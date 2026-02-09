#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <cassert>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include "proto/helloservice.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerCompletionQueue;
using grpc::Status;
using namespace rpc::proto::helloservice;

// 异步服务器实现
class AsyncHelloServiceImpl {
private:
    // 异步服务和完成队列
    HelloService::AsyncService service_;
    std::unique_ptr<ServerCompletionQueue> cq_;
    std::unique_ptr<Server> server_;

public:
    ~AsyncHelloServiceImpl() {
        server_->Shutdown();
        // 始终在服务器关闭后关闭完成队列
        cq_->Shutdown();
    }

    // 启动服务器
    void Run() {
        std::string server_address("0.0.0.0:50051");

        ServerBuilder builder;
        // 监听端口
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        // 注册异步服务
        builder.RegisterService(&service_);
        // 获取完成队列
        cq_ = builder.AddCompletionQueue();
        // 构建并启动服务器
        server_ = builder.BuildAndStart();

        std::cout << "Async Server listening on " << server_address << std::endl;

        // 开始处理 RPC
        HandleRpcs();
    }

private:
    // 这个类表示一个 RPC 的状态
    class CallData {
    public:
        // 构造函数，开始处理新的 RPC
        CallData(HelloService::AsyncService* service, ServerCompletionQueue* cq)
            : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE) {
            // 调用 Proceed 进入状态机
            Proceed();
        }

        void Proceed() {
            if (status_ == CREATE) {
                // 开始新的 RPC，状态变为 PROCESS
                status_ = PROCESS;

                // 向服务注册，准备接收 SayHello 请求
                service_->RequestSayHello(&ctx_, &request_, &responder_, cq_, cq_, this);
            } else if (status_ == PROCESS) {
                // 生成新的 CallData 实例来处理下一个请求
                new CallData(service_, cq_);

                // 实际的处理逻辑
                std::string prefix("Hello ");
                reply_.set_reply(prefix + request_.name());

                std::cout << "Async Server: Processing request from "
                         << request_.name() << std::endl;

                // 状态变为 FINISH
                status_ = FINISH;
                // 发送响应，状态机完成
                responder_.Finish(reply_, Status::OK, this);
            } else {
                // FINISH 状态，删除自己
                assert(status_ == FINISH);
                delete this;
            }
        }

    private:
        // 服务的异步版本
        HelloService::AsyncService* service_;
        // 完成队列，用于异步通信
        ServerCompletionQueue* cq_;
        // RPC 上下文
        ServerContext ctx_;

        // 用于与 gRPC 运行时通信的变量
        HelloRequest request_;
        HelloResponse reply_;

        // 用于发送响应的对象
        ServerAsyncResponseWriter<HelloResponse> responder_;

        // 当前 RPC 的状态
        enum CallStatus { CREATE, PROCESS, FINISH };
        CallStatus status_;
    };

    // 处理 RPC 的主循环
    void HandleRpcs() {
        // 生成新的 CallData 实例来处理新的 RPC
        new CallData(&service_, cq_.get());
        void* tag;  // 唯一标识一个请求的标签
        bool ok;

        while (true) {
            // 从完成队列获取下一个事件，阻塞直到有事件到达
            if (!cq_->Next(&tag, &ok)) break;

            if (!ok) {
                std::cout << "Server shutting down." << std::endl;
                break;
            }

            // tag 实际上是一个 CallData 指针
            static_cast<CallData*>(tag)->Proceed();
        }
    }
};

int main(int argc, char** argv) {
    AsyncHelloServiceImpl server;
    server.Run();

    return 0;
}