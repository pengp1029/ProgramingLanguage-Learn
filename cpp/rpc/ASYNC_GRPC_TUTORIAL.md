# gRPC AsyncService 使用教程

## 什么是 gRPC AsyncService？

gRPC AsyncService 是 gRPC 的异步编程模型，它提供：
- **非阻塞 I/O**: 不会阻塞调用线程
- **更好的性能**: 可以处理更多并发请求
- **资源效率**: 一个线程可以处理多个 RPC
- **灵活的控制**: 完全控制 RPC 的生命周期

## 核心概念

### 1. 完成队列 (CompletionQueue)
```cpp
ServerCompletionQueue* cq_;  // 服务端
CompletionQueue cq;          // 客户端
```
- 所有异步操作的事件中心
- 使用 `Next()` 获取完成的操作

### 2. 异步服务 (AsyncService)
```cpp
HelloService::AsyncService service_;
```
- 代替同步的 `Service` 类
- 提供 `RequestXXX()` 方法注册 RPC 处理

### 3. 状态机模型
每个 RPC 通过状态机管理：
- **CREATE**: 注册新的 RPC
- **PROCESS**: 处理业务逻辑
- **FINISH**: 完成响应

## 异步服务端实现

### 基本结构

```cpp
class AsyncHelloServiceImpl {
private:
    HelloService::AsyncService service_;
    std::unique_ptr<ServerCompletionQueue> cq_;
    std::unique_ptr<Server> server_;

    class CallData {
        enum CallStatus { CREATE, PROCESS, FINISH };
        CallStatus status_;

        void Proceed() {
            switch (status_) {
                case CREATE:
                    // 注册新 RPC
                    service_->RequestSayHello(&ctx_, &request_, &responder_, cq_, cq_, this);
                    status_ = PROCESS;
                    break;
                case PROCESS:
                    // 创建新实例处理下一个请求
                    new CallData(service_, cq_);
                    // 处理当前请求
                    ProcessRequest();
                    status_ = FINISH;
                    responder_.Finish(reply_, Status::OK, this);
                    break;
                case FINISH:
                    delete this;
                    break;
            }
        }
    };
};
```

### 关键步骤

1. **注册服务**
```cpp
builder.RegisterService(&service_);  // 注册 AsyncService
cq_ = builder.AddCompletionQueue();  // 添加完成队列
```

2. **启动 RPC 处理**
```cpp
service_->RequestSayHello(&ctx_, &request_, &responder_, cq_, cq_, this);
```

3. **事件循环**
```cpp
void* tag;
bool ok;
while (cq_->Next(&tag, &ok)) {
    static_cast<CallData*>(tag)->Proceed();
}
```

## 异步客户端实现

### 单个异步请求

```cpp
class AsyncHelloClient {
    std::string SayHello(const std::string& name) {
        HelloRequest request;
        request.set_name(name);

        CompletionQueue cq;

        // 发起异步调用
        auto rpc = stub_->AsyncSayHello(&context, request, &cq);

        // 注册完成回调
        rpc->Finish(&reply, &status, (void*)1);

        // 等待完成
        void* got_tag;
        bool ok;
        cq.Next(&got_tag, &ok);

        return reply.reply();
    }
};
```

### 并发异步请求

```cpp
void SayHelloMultiple(const std::vector<std::string>& names) {
    CompletionQueue cq;
    std::vector<std::unique_ptr<AsyncCall>> calls;

    // 发起多个异步调用
    for (const auto& name : names) {
        auto call = std::make_unique<AsyncCall>();
        call->reader = stub_->AsyncSayHello(&call->context, request, &cq);
        call->reader->Finish(&call->reply, &call->status, call.get());
        calls.push_back(std::move(call));
    }

    // 收集所有响应
    for (size_t i = 0; i < names.size(); ++i) {
        void* tag;
        bool ok;
        cq.Next(&tag, &ok);
        // 处理响应...
    }
}
```

## 流式 RPC 异步处理

### 服务端流式 RPC

```cpp
class ServerStreamCallData {
    void Proceed() {
        if (status_ == CREATE) {
            service_->RequestServerStream(&ctx_, &request_, &responder_, cq_, cq_, this);
            status_ = PROCESS;
        } else if (status_ == PROCESS) {
            new ServerStreamCallData(service_, cq_);  // 处理下一个请求
            status_ = WRITE;
            WriteNext();
        } else if (status_ == WRITE) {
            if (hasMoreData) {
                WriteNext();  // 继续写入
            } else {
                responder_.Finish(Status::OK, this);  // 完成流
                status_ = FINISH;
            }
        } else {
            delete this;
        }
    }

    void WriteNext() {
        StreamResponse response;
        // 填充响应数据...
        responder_.Write(response, this);
    }
};
```

### 双向流式 RPC

```cpp
class BidirectionalStreamCallData {
    enum CallStatus { CREATE, PROCESS, READ, WRITE, FINISH };

    void Proceed() {
        switch (status_) {
            case CREATE:
                service_->RequestBidirectionalStream(&ctx_, &stream_, cq_, cq_, this);
                status_ = PROCESS;
                break;
            case PROCESS:
                new BidirectionalStreamCallData(service_, cq_);
                status_ = READ;
                stream_.Read(&request_, this);
                break;
            case READ:
                if (!ctx_.IsCancelled()) {
                    // 处理读取的数据，准备写入
                    status_ = WRITE;
                    stream_.Write(response, this);
                } else {
                    stream_.Finish(Status::CANCELLED, this);
                    status_ = FINISH;
                }
                break;
            case WRITE:
                status_ = read;
                stream_.Read(&request_, this);  // 继续读取
                break;
            case FINISH:
                delete this;
                break;
        }
    }
};
```

## 最佳实践

### 1. 内存管理
```cpp
// CallData 通过自删除管理生命周期
class CallData {
    void Proceed() {
        if (status_ == FINISH) {
            delete this;  // 自删除
        }
    }
};

// 在 PROCESS 状态创建新实例
new CallData(service_, cq_);
```

### 2. 错误处理
```cpp
void HandleRpcs() {
    while (cq_->Next(&tag, &ok)) {
        if (!ok) {
            // 服务器正在关闭
            break;
        }
        static_cast<CallData*>(tag)->Proceed();
    }
}
```

### 3. 优雅关闭
```cpp
~AsyncServiceImpl() {
    server_->Shutdown();        // 先关闭服务器
    cq_->Shutdown();           // 再关闭完成队列
}
```

### 4. 线程安全
```cpp
// 完成队列是线程安全的
// 可以从多个线程调用 Next()
void HandleRpcs() {
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this]() {
            void* tag;
            bool ok;
            while (cq_->Next(&tag, &ok)) {
                if (ok) {
                    static_cast<CallData*>(tag)->Proceed();
                }
            }
        });
    }
}
```

## 性能优化技巧

### 1. 预分配 CallData
```cpp
// 在服务器启动时预创建多个 CallData
for (int i = 0; i < INITIAL_CALL_DATA_COUNT; ++i) {
    new CallData(&service_, cq_.get());
}
```

### 2. 使用对象池
```cpp
class CallDataPool {
    std::queue<std::unique_ptr<CallData>> pool_;
    std::mutex mutex_;

public:
    std::unique_ptr<CallData> Get() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pool_.empty()) {
            return std::make_unique<CallData>();
        }
        auto obj = std::move(pool_.front());
        pool_.pop();
        return obj;
    }

    void Return(std::unique_ptr<CallData> obj) {
        std::lock_guard<std::mutex> lock(mutex_);
        obj->Reset();
        pool_.push(std::move(obj));
    }
};
```

### 3. 多线程处理
```cpp
// 多个线程处理完成队列
const int kNumThreads = std::thread::hardware_concurrency();
std::vector<std::thread> threads;

for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back(&AsyncServiceImpl::HandleRpcs, this);
}
```

## 与同步 API 对比

| 特性 | 同步 API | 异步 API |
|------|----------|----------|
| 编程复杂度 | 简单 | 复杂 |
| 性能 | 中等 | 高 |
| 内存使用 | 高（每个请求一个线程） | 低 |
| 并发能力 | 受线程数限制 | 几乎无限 |
| 调试难度 | 简单 | 困难 |
| 延迟 | 稍高 | 低 |

## 使用场景

### 适合异步 API：
- 高并发服务
- 长连接/流式服务
- 资源受限环境
- 需要精确控制的场景

### 适合同步 API：
- 简单的请求-响应模式
- 原型开发
- 业务逻辑复杂的服务

## 示例代码编译和运行

```bash
# 生成 proto 文件
protoc --cpp_out=src/proto --grpc_out=src/proto \
       --plugin=protoc-gen-grpc=grpc_cpp_plugin \
       -I proto proto/streamservice.proto

# 编译异步服务端
g++ -std=c++17 -I/opt/homebrew/opt/grpc/include \
    -I/opt/homebrew/opt/protobuf/include \
    -Isrc src/hello_async_server.cpp src/proto/*.cc \
    -o hello_async_server -lgrpc++ -lprotobuf

# 编译异步客户端
g++ -std=c++17 -I/opt/homebrew/opt/grpc/include \
    -I/opt/homebrew/opt/protobuf/include \
    -Isrc src/hello_async_client.cpp src/proto/*.cc \
    -o hello_async_client -lgrpc++ -lprotobuf

# 运行
./hello_async_server  # 终端1
./hello_async_client  # 终端2
```

异步 gRPC 提供了强大的性能和灵活性，但需要仔细的设计和实现。掌握这些概念后，你就能构建高性能的分布式服务了！