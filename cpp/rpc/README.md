# C++ gRPC Hello Service 教程

## 项目结构

```
cpp/rpc/
├── proto/
│   └── helloservice.proto          # Protocol Buffers 服务定义
├── src/
│   ├── proto/                      # 生成的 protobuf 文件
│   │   ├── helloservice.pb.h
│   │   ├── helloservice.pb.cc
│   │   ├── helloservice.grpc.pb.h
│   │   └── helloservice.grpc.pb.cc
│   ├── hello_server.cpp            # gRPC 服务端实现
│   └── hello_client.cpp            # gRPC 客户端实现
├── CMakeLists.txt                  # CMake 构建配置
├── Makefile                        # Make 构建配置
├── build.sh                        # 构建脚本
└── README.md                       # 本文档
```

## 实现步骤

### 1. 定义 Protocol Buffers 服务 (proto/helloservice.proto)

```protobuf
syntax = "proto3";

package rpc.proto.helloservice;

// 请求消息
message HelloRequest {
    string name = 1;
}

// 响应消息
message HelloResponse {
    string reply = 1;
}

// gRPC 服务定义
service HelloService {
    rpc SayHello(HelloRequest) returns (HelloResponse);
}
```

### 2. 生成 C++ 代码

使用 protoc 编译器生成 C++ 代码：

```bash
# 生成 protobuf 文件
protoc --cpp_out=src/proto --grpc_out=src/proto \
       --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` \
       -I proto proto/helloservice.proto
```

### 3. 实现服务端 (src/hello_server.cpp)

```cpp
#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "proto/helloservice.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using namespace rpc::proto::helloservice;

class HelloServiceImpl final : public HelloService::Service {
    grpc::Status SayHello(ServerContext* context,
                         const HelloRequest* request,
                         HelloResponse* response) override {
        std::string prefix("Hello ");
        response->set_reply(prefix + request->name());

        std::cout << "Received: " << request->name() << std::endl;
        std::cout << "Sent: " << response->reply() << std::endl;

        return Status::OK;
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    HelloServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address,
                           grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}
```

### 4. 实现客户端 (src/hello_client.cpp)

```cpp
#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "proto/helloservice.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using namespace rpc::proto::helloservice;

class HelloClient {
public:
    HelloClient(std::shared_ptr<Channel> channel)
        : stub_(HelloService::NewStub(channel)) {}

    std::string SayHello(const std::string& name) {
        HelloRequest request;
        request.set_name(name);

        HelloResponse response;
        ClientContext context;

        Status status = stub_->SayHello(&context, request, &response);

        if (status.ok()) {
            return response.reply();
        } else {
            std::cout << "RPC failed: " << status.error_code()
                     << ": " << status.error_message() << std::endl;
            return "RPC failed";
        }
    }

private:
    std::unique_ptr<HelloService::Stub> stub_;
};

int main(int argc, char** argv) {
    std::string server_address("localhost:50051");
    HelloClient client(grpc::CreateChannel(server_address,
                                          grpc::InsecureChannelCredentials()));

    std::string name("World");
    if (argc > 1) {
        name = argv[1];
    }

    std::string response = client.SayHello(name);
    std::cout << "Client received: " << response << std::endl;

    return 0;
}
```

## 构建项目

### 安装依赖 (macOS)

```bash
# 使用 Homebrew 安装依赖
brew install protobuf grpc abseil
```

### 方法 1: 使用 CMake (推荐)

```bash
# 安装 CMake
brew install cmake

# 构建
mkdir build && cd build
cmake ..
make
```

### 方法 2: 使用 Make

```bash
make
```

### 方法 3: 手动编译 (如果遇到链接问题)

```bash
# 编译服务端
g++ -std=c++17 -I/opt/homebrew/opt/protobuf/include \
    -I/opt/homebrew/opt/grpc/include \
    -I/opt/homebrew/opt/abseil/include -Isrc \
    src/hello_server.cpp src/proto/helloservice.pb.cc \
    src/proto/helloservice.grpc.pb.cc -o hello_server \
    -L/opt/homebrew/opt/grpc/lib -L/opt/homebrew/opt/protobuf/lib \
    -lgrpc++ -lprotobuf

# 编译客户端
g++ -std=c++17 -I/opt/homebrew/opt/protobuf/include \
    -I/opt/homebrew/opt/grpc/include \
    -I/opt/homebrew/opt/abseil/include -Isrc \
    src/hello_client.cpp src/proto/helloservice.pb.cc \
    src/proto/helloservice.grpc.pb.cc -o hello_client \
    -L/opt/homebrew/opt/grpc/lib -L/opt/homebrew/opt/protobuf/lib \
    -lgrpc++ -lprotobuf
```

## 运行测试

```bash
# 终端 1: 启动服务端
./hello_server

# 终端 2: 运行客户端
./hello_client World
# 或者自定义名字
./hello_client "Your Name"
```

## 关键概念

### 1. Protocol Buffers
- 语言中性的数据序列化格式
- 定义消息结构和服务接口
- 比 JSON/XML 更高效

### 2. gRPC
- Google 开发的 RPC 框架
- 基于 HTTP/2，支持流式传输
- 跨语言支持

### 3. 服务端实现
- 继承生成的 Service 类
- 实现虚拟方法
- 使用 ServerBuilder 构建服务器

### 4. 客户端实现
- 创建 Stub（存根）
- 调用远程方法如本地方法
- 处理网络错误

## 扩展功能

1. **流式 RPC**: 支持客户端流、服务端流、双向流
2. **认证**: 添加 TLS、OAuth 等安全机制
3. **拦截器**: 日志记录、监控、验证
4. **负载均衡**: 客户端负载均衡
5. **健康检查**: 服务健康状态监控

## 故障排除

### 常见链接错误
- 确保所有依赖库都已安装
- 检查库路径是否正确
- 可能需要额外的 abseil 库

### VS Code 配置
- 在 `.vscode/c_cpp_properties.json` 中添加包含路径
- 包含 protobuf、grpc、abseil 的头文件目录

这个项目展示了 C++ 中使用 protobuf 和 gRPC 实现 RPC 通信的完整流程。