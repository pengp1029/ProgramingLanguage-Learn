# gRPC AsyncService 服务器请求处理执行流程

## 整体架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                        gRPC AsyncServer                         │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐    ┌──────────────┐    ┌─────────────────────┐ │
│  │   Network   │────│   gRPC Core  │────│  AsyncService      │ │
│  │  Listener   │    │   Runtime    │    │  (Your Code)       │ │
│  └─────────────┘    └──────────────┘    └─────────────────────┘ │
│                             │                       │           │
│                             ▼                       ▼           │
│                    ┌─────────────────┐     ┌─────────────────┐  │
│                    │ CompletionQueue │◄────┤   CallData     │  │
│                    │      (CQ)       │     │  (State Machine)│  │
│                    └─────────────────┘     └─────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## 详细请求处理流程

### 1. 服务器启动阶段

```
┌─────────────────┐
│  Server Start   │
└─────────────────┘
          │
          ▼
┌─────────────────────────────────────┐
│ 1. RegisterService(&service_)       │  ◄─── 注册异步服务
│ 2. cq_ = builder.AddCompletionQueue()│  ◄─── 创建完成队列
│ 3. server_ = builder.BuildAndStart() │  ◄─── 启动服务器
└─────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────┐
│    new CallData(&service_, cq_)     │  ◄─── 创建初始 CallData
└─────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────┐
│     CallData::Proceed()             │  ◄─── 进入状态机
│     status_ = CREATE                │
└─────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────┐
│ service_->RequestSayHello(...)      │  ◄─── 注册 RPC 请求监听
│ 服务器开始等待客户端请求               │
└─────────────────────────────────────┘
```

### 2. 客户端请求到达处理流程

```
┌─────────────────┐
│ Client Request  │  ◄─── 客户端发送请求
│   Arrives       │
└─────────────────┘
          │
          ▼
┌─────────────────────────────────────┐
│        gRPC Runtime                 │
│   1. 接收网络数据                    │
│   2. 解析 protobuf                   │  ◄─── gRPC 内部处理
│   3. 匹配到 RequestSayHello          │
│   4. 触发 CompletionQueue 事件       │
└─────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────┐
│     CompletionQueue Event           │
│   cq_->Next(&tag, &ok)              │  ◄─── 事件循环获取事件
│   tag = CallData* (this)            │      tag 指向对应的 CallData
│   ok = true (请求成功到达)            │
└─────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────┐
│  static_cast<CallData*>(tag)        │
│        ->Proceed()                  │  ◄─── 调用对应 CallData 的 Proceed
└─────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────┐
│    CallData 状态机处理              │
│    (详见下面的状态机流程)             │
└─────────────────────────────────────┘
```

### 3. CallData 状态机详细流程

```
CallData 状态机流程:

初始状态: CREATE
┌─────────────────┐
│     CREATE      │  ◄─── 初始状态，准备注册 RPC
└─────────────────┘
          │ Proceed() 被调用
          ▼
┌─────────────────────────────────────┐
│ service_->RequestSayHello(          │
│   &ctx_, &request_, &responder_,    │  ◄─── 向 gRPC 注册 RPC 处理
│   cq_, cq_, this)                   │      this 作为 tag 传递
│ status_ = PROCESS                   │
└─────────────────────────────────────┘
          │ 等待客户端请求...
          │
          ▼ (当请求到达时)
┌─────────────────┐
│    PROCESS      │  ◄─── 实际请求到达，开始处理
└─────────────────┘
          │ Proceed() 再次被调用
          ▼
┌─────────────────────────────────────┐
│ 1. new CallData(service_, cq_)      │  ◄─── 创建新的 CallData 处理下一个请求
│                                     │
│ 2. // 业务逻辑处理                   │  ◄─── 执行实际的业务逻辑
│    reply_.set_reply("Hello " +      │      (这里是 Hello + name)
│                    request_.name()) │
│                                     │
│ 3. status_ = FINISH                 │  ◄─── 设置状态为 FINISH
│                                     │
│ 4. responder_.Finish(reply_,        │  ◄─── 发送响应给客户端
│                     Status::OK,     │      这也是异步操作
│                     this)           │
└─────────────────────────────────────┘
          │ 等待响应发送完成...
          │
          ▼ (当响应发送完成时)
┌─────────────────┐
│     FINISH      │  ◄─── 响应发送完成
└─────────────────┘
          │ Proceed() 最后一次被调用
          ▼
┌─────────────────────────────────────┐
│         delete this;                │  ◄─── 删除当前 CallData 实例
│      (释放内存)                      │
└─────────────────────────────────────┘
```

### 4. 完整的请求处理时序图

```
Time ──────────────────────────────────────────────────────────►

Client          Network        gRPC Runtime       CompletionQueue        CallData(A)        CallData(B)
  │                │               │                     │                    │                 │
  │ Send Request   │               │                     │                    │                 │
  ├──────────────► │               │                     │                    │                 │
  │                │ Forward       │                     │                    │                 │
  │                ├─────────────► │                     │                    │                 │
  │                │               │ Trigger Event       │                    │                 │
  │                │               ├───────────────────► │                    │                 │
  │                │               │                     │ Next() returns     │                 │
  │                │               │                     ├──────────────────► │                 │
  │                │               │                     │                    │ Proceed()       │
  │                │               │                     │                    │ status_=PROCESS │
  │                │               │                     │                    │                 │
  │                │               │                     │                    │ new CallData ──┼►│
  │                │               │                     │                    │                 │ (Constructor)
  │                │               │                     │                    │                 │ status_=CREATE
  │                │               │                     │                    │                 │ Proceed()
  │                │               │                     │                    │                 │ RequestSayHello()
  │                │               │                     │                    │                 │
  │                │               │                     │                    │ Process Request │
  │                │               │                     │                    │ status_=FINISH  │
  │                │               │                     │                    │ responder_.     │
  │                │               │                     │                    │ Finish(...)     │
  │                │               │                     │                    │                 │
  │                │               │ Response Sent       │                    │                 │
  │                │               ├───────────────────► │                    │                 │
  │                │ Send Response │                     │ Next() returns     │                 │
  │ ◄────────────── │               │                     ├──────────────────► │                 │
  │                │               │                     │                    │ Proceed()       │
  │                │               │                     │                    │ delete this     │
  │                │               │                     │                    ✗                 │
  │                │               │                     │                    (destroyed)        │
  │                │               │                     │                                      │
  │                │               │                     │                                      │
  │                │               │                     │          ┌─ 等待下一个请求...        │
  │                │               │                     │          │                          │
```

### 5. 内存管理和并发处理

```
内存管理模式:

每个请求 ──► 一个 CallData 实例
                │
                ├─ 在 PROCESS 状态创建新的 CallData (处理下一个请求)
                │
                └─ 在 FINISH 状态自删除 (delete this)

并发处理:

Request 1 ──► CallData A ──► [CREATE] ──► [PROCESS] ──► [FINISH] ──► delete
                                  │           │
Request 2 ──────────────────────► CallData B ──► [PROCESS] ──► [FINISH] ──► delete
                                                      │
Request 3 ──────────────────────────────────────────► CallData C ──► [PROCESS] ──► ...

多个 CallData 同时存在，每个处理一个请求
```

### 6. 关键代码映射

```cpp
// 事件循环 - 对应上图中的 "CompletionQueue Event"
void HandleRpcs() {
    void* tag;  // 这就是 CallData* 指针
    bool ok;

    while (true) {
        // 阻塞等待下一个事件
        GPR_ASSERT(cq_->Next(&tag, &ok));  // ◄─── 这里获取完成的事件

        if (!ok) break;

        // 调用对应 CallData 的 Proceed 方法
        static_cast<CallData*>(tag)->Proceed();  // ◄─── 驱动状态机
    }
}

// CallData 状态机 - 对应上图中的状态转换
void Proceed() {
    if (status_ == CREATE) {
        status_ = PROCESS;
        // 注册新的 RPC - 对应 "RequestSayHello" 步骤
        service_->RequestSayHello(&ctx_, &request_, &responder_, cq_, cq_, this);

    } else if (status_ == PROCESS) {
        // 创建新实例处理下一个请求 - 对应 "new CallData" 步骤
        new CallData(service_, cq_);

        // 处理当前请求 - 对应 "Process Request" 步骤
        std::string prefix("Hello ");
        reply_.set_reply(prefix + request_.name());

        // 发送响应 - 对应 "responder_.Finish" 步骤
        status_ = FINISH;
        responder_.Finish(reply_, Status::OK, this);

    } else {
        // 清理 - 对应 "delete this" 步骤
        GPR_ASSERT(status_ == FINISH);
        delete this;
    }
}
```

## 核心理解要点

1. **每个 RPC 请求都有独立的 CallData 实例**
2. **状态机驱动整个处理流程** (CREATE → PROCESS → FINISH)
3. **完成队列是事件中心**，所有异步操作完成时都会在这里通知
4. **tag 参数就是 CallData 指针**，用于将事件与对应的处理实例关联
5. **在 PROCESS 状态创建新 CallData**，确保可以处理下一个请求
6. **自删除模式管理内存**，避免内存泄漏

这种设计使得一个线程可以处理大量并发请求，而不需要为每个请求创建单独的线程！