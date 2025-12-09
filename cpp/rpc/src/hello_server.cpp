#include<iostream>
#include<CLI/CLI.hpp>
#include<absl/log/initialize.h>
#include<grpcpp/grpcpp.h> // grpc所在库
#include "proto/helloservice.grpc.pb.h"
using namespace rpc::proto::helloservice;
class HelloClient {
    public:
    HelloClient(std::shared_ptr<grpc::Channel> channel):stub_(HelloService::NewStub(channel)){}
    grpc::Status SayHello(const std::string& user, std::string& reply){
        HelloRequest request;
        request.set_name(user);

        HelloResponse response;

        grpc::ClientContext context;

        grpc::Status status = stub_->SayHello(&context, request, &response);

        if(status.ok()){
            reply = response.reply();
        } else{
            std::cout << "RPC failed: " << status.error_code() << ": " << status.error_message() << std::endl;
        }
        return status;
    }
    private:
        std::unique_ptr<HelloService::Stub> stub_;

};

class HelloServer:public HelloService::Service{
    public:
    grpc::Status SayHello(grpc::ServerContext* context, const HelloRequest* request, HelloResponse* response) override{
        std::string prefix("Hello ");
        response->set_reply(prefix + request->name());
        return grpc::Status::OK;
    }
};

class HelloArguments{
    public:
    HelloArguments(bool client, bool server): _client(client), _server(server){}
    bool isClient(){return _client;}
    bool isServer(){return _server;}
    private:
    bool _client;
    bool _server;
};

int main(int argc, char** argv){
    CLI::App app{"Hello World"};
    absl::InitializeLog();
    bool is_client = false;
    bool is_server = false;
    app.add_flag("-c", is_client, "Choose client mode");
    app.add_flag("-s", is_server, "Choose server mode");
    CLI11_PARSE(app, argc, argv);
    
    HelloArguments args(is_client, is_server);
    if(args.isClient()){
        std::string ip_port = "127.0.0.1:50051";
        auto channel = grpc::CreateChannel(ip_port, grpc::InsecureChannelCredentials());
        HelloClient client(channel);
        std::string name = "World";
        std::string reply;
        auto status = client.SayHello(name, reply);
        if(status.ok()){
            std::cout << "Reply: " << reply << std::endl;
        }else{
            std::cout << "RPC failed: " << status.error_code() << ": " << status.error_message() << std::endl;
        }
    }else{
        std::string ip_port = "0.0.0.0:50051";
        HelloServer server;
        grpc::ServerBuilder builder;
        builder.AddListeningPort(ip_port, grpc::InsecureServerCredentials());
        builder.RegisterService(&server);

        std::unique_ptr<grpc::Server> grpc_server(builder.BuildAndStart());
        if(!grpc_server){
            std::cout << "Failed to start server" << std::endl;
            return -1;
        }
        std::cout << "Server listening on " << ip_port << std::endl;
        grpc_server->Wait();
    }
    return 0;
}