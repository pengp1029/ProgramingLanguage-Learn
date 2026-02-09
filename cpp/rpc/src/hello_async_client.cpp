#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "proto/helloservice.grpc.pb.h"

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using namespace rpc::proto::helloservice;

// 异步客户端实现
class AsyncHelloClient {
public:
    explicit AsyncHelloClient(std::shared_ptr<Channel> channel)
        : stub_(HelloService::NewStub(channel)) {}

    // 发送异步请求
    std::string SayHello(const std::string& name) {
        // 准备数据
        HelloRequest request;
        request.set_name(name);

        HelloResponse reply;
        ClientContext context;

        // 完成队列用于异步操作
        CompletionQueue cq;

        // 发起异步 RPC 调用
        Status status;
        std::unique_ptr<ClientAsyncResponseReader<HelloResponse>> rpc(
            stub_->AsyncSayHello(&context, request, &cq));

        // 请求 RPC 完成时通知我们
        rpc->Finish(&reply, &status, (void*)1);

        void* got_tag;
        bool ok = false;

        // 阻塞直到下一个结果从完成队列中可用
        GPR_ASSERT(cq.Next(&got_tag, &ok));

        // 验证结果
        GPR_ASSERT(got_tag == (void*)1);
        GPR_ASSERT(ok);

        if (status.ok()) {
            return reply.reply();
        } else {
            std::cout << "RPC failed: " << status.error_code() << ": "
                     << status.error_message() << std::endl;
            return "RPC failed";
        }
    }

    // 发送多个异步请求
    void SayHelloMultiple(const std::vector<std::string>& names) {
        struct AsyncClientCall {
            HelloResponse reply;
            ClientContext context;
            Status status;
            std::unique_ptr<ClientAsyncResponseReader<HelloResponse>> response_reader;
        };

        CompletionQueue cq;
        std::vector<std::unique_ptr<AsyncClientCall>> calls;

        // 发起多个异步调用
        for (size_t i = 0; i < names.size(); ++i) {
            auto call = std::make_unique<AsyncClientCall>();

            HelloRequest request;
            request.set_name(names[i]);

            call->response_reader = stub_->AsyncSayHello(&call->context, request, &cq);
            call->response_reader->Finish(&call->reply, &call->status, (void*)call.get());

            calls.push_back(std::move(call));
        }

        // 等待所有响应
        for (size_t i = 0; i < names.size(); ++i) {
            void* got_tag;
            bool ok = false;

            GPR_ASSERT(cq.Next(&got_tag, &ok));
            GPR_ASSERT(ok);

            auto* call = static_cast<AsyncClientCall*>(got_tag);

            if (call->status.ok()) {
                std::cout << "Async response " << (i+1) << ": "
                         << call->reply.reply() << std::endl;
            } else {
                std::cout << "RPC " << (i+1) << " failed: "
                         << call->status.error_code() << ": "
                         << call->status.error_message() << std::endl;
            }
        }
    }

private:
    std::unique_ptr<HelloService::Stub> stub_;
};

int main(int argc, char** argv) {
    // 连接到服务器
    std::string server_address("localhost:50051");
    AsyncHelloClient client(grpc::CreateChannel(
        server_address, grpc::InsecureChannelCredentials()));

    // 测试单个异步调用
    std::cout << "=== 单个异步调用测试 ===" << std::endl;
    std::string name = "AsyncWorld";
    if (argc > 1) {
        name = argv[1];
    }

    std::string response = client.SayHello(name);
    std::cout << "Client received: " << response << std::endl;

    // 测试多个异步调用
    std::cout << "\n=== 多个异步调用测试 ===" << std::endl;
    std::vector<std::string> names = {"Alice", "Bob", "Charlie", "David", "Eve"};
    client.SayHelloMultiple(names);

    return 0;
}