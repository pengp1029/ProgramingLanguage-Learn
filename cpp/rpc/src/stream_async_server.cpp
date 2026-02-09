#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <grpcpp/grpcpp.h>
#include "proto/streamservice.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerAsyncWriter;
using grpc::ServerAsyncReader;
using grpc::ServerAsyncReaderWriter;
using grpc::ServerCompletionQueue;
using grpc::Status;
using namespace rpc::proto::streamservice;

// 流式异步服务器实现
class AsyncStreamServiceImpl {
private:
    StreamService::AsyncService service_;
    std::unique_ptr<ServerCompletionQueue> cq_;
    std::unique_ptr<Server> server_;

public:
    ~AsyncStreamServiceImpl() {
        server_->Shutdown();
        cq_->Shutdown();
    }

    void Run() {
        std::string server_address("0.0.0.0:50052");

        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service_);
        cq_ = builder.AddCompletionQueue();
        server_ = builder.BuildAndStart();

        std::cout << "Async Stream Server listening on " << server_address << std::endl;

        HandleRpcs();
    }

private:
    // 服务端流式 RPC 的处理类
    class ServerStreamCallData {
    public:
        ServerStreamCallData(StreamService::AsyncService* service, ServerCompletionQueue* cq)
            : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE), sequence_(0) {
            Proceed();
        }

        void Proceed() {
            if (status_ == CREATE) {
                status_ = PROCESS;
                service_->RequestServerStream(&ctx_, &request_, &responder_, cq_, cq_, this);
            } else if (status_ == PROCESS) {
                new ServerStreamCallData(service_, cq_);

                std::cout << "ServerStream: Processing request for " << request_.name()
                         << " with count " << request_.count() << std::endl;

                status_ = WRITE;
                WriteNext();
            } else if (status_ == WRITE) {
                if (sequence_ < request_.count()) {
                    WriteNext();
                } else {
                    status_ = FINISH;
                    responder_.Finish(Status::OK, this);
                }
            } else {
                GPR_ASSERT(status_ == FINISH);
                delete this;
            }
        }

    private:
        void WriteNext() {
            StreamResponse response;
            response.set_message("Stream message " + std::to_string(sequence_) +
                               " for " + request_.name());
            response.set_sequence(sequence_);

            sequence_++;
            responder_.Write(response, this);
        }

        StreamService::AsyncService* service_;
        ServerCompletionQueue* cq_;
        ServerContext ctx_;
        StreamRequest request_;
        ServerAsyncWriter<StreamResponse> responder_;
        enum CallStatus { CREATE, PROCESS, WRITE, FINISH };
        CallStatus status_;
        int sequence_;
    };

    // 双向流式 RPC 的处理类
    class BidirectionalStreamCallData {
    public:
        BidirectionalStreamCallData(StreamService::AsyncService* service, ServerCompletionQueue* cq)
            : service_(service), cq_(cq), stream_(&ctx_), status_(CREATE) {
            Proceed();
        }

        void Proceed() {
            if (status_ == CREATE) {
                status_ = PROCESS;
                service_->RequestBidirectionalStream(&ctx_, &stream_, cq_, cq_, this);
            } else if (status_ == PROCESS) {
                new BidirectionalStreamCallData(service_, cq_);
                status_ = READ;
                stream_.Read(&request_, this);
            } else if (status_ == READ) {
                if (ctx_.IsCancelled()) {
                    status_ = FINISH;
                    stream_.Finish(Status::CANCELLED, this);
                } else {
                    // 回显收到的消息
                    StreamResponse response;
                    response.set_message("Echo: " + request_.name());
                    response.set_sequence(request_.count());

                    status_ = WRITE;
                    stream_.Write(response, this);
                }
            } else if (status_ == WRITE) {
                status_ = READ;
                stream_.Read(&request_, this);
            } else {
                GPR_ASSERT(status_ == FINISH);
                delete this;
            }
        }

    private:
        StreamService::AsyncService* service_;
        ServerCompletionQueue* cq_;
        ServerContext ctx_;
        StreamRequest request_;
        ServerAsyncReaderWriter<StreamResponse, StreamRequest> stream_;
        enum CallStatus { CREATE, PROCESS, READ, WRITE, FINISH };
        CallStatus status_;
    };

    void HandleRpcs() {
        new ServerStreamCallData(&service_, cq_.get());
        new BidirectionalStreamCallData(&service_, cq_.get());

        void* tag;
        bool ok;

        while (true) {
            GPR_ASSERT(cq_->Next(&tag, &ok));

            if (!ok) {
                std::cout << "Server shutting down." << std::endl;
                break;
            }

            static_cast<ServerStreamCallData*>(tag)->Proceed();
        }
    }
};

int main(int argc, char** argv) {
    AsyncStreamServiceImpl server;
    server.Run();
    return 0;
}