# RPC 框架

基于 C++11 + muduo + JsonCpp 实现的轻量级分布式 RPC 框架。支持远程方法调用、发布订阅、服务注册与发现三大功能。

## 功能特性

- **RPC 调用**：支持同步调用、异步 future 调用、异步回调调用
- **发布订阅**：基于主题的消息广播，服务端主动推送
- **服务注册与发现**：注册中心管理服务提供者，客户端动态发现服务端地址
- **负载均衡**：多个服务提供者时，按轮询策略选择
- **服务上下线通知**：服务端断线时，注册中心主动通知客户端
- **自定义通信协议**：采用 LV（Length-Value）协议解决 TCP 粘包问题
- **JSON 序列化**：请求和响应正文使用 JSON 格式

## 技术栈

| 技术 | 用途 |
|------|------|
| C++11 | 语言标准 |
| muduo | 高性能网络库 |
| JsonCpp | JSON 序列化 / 反序列化 |
| CMake | 构建系统 |
| WSL2 + Ubuntu | 开发环境 |

## 项目结构

    rpc_demo/
    ├── CMakeLists.txt
    ├── README.md
    ├── include/                          # 头文件
    │   ├── abstract.h                    # 通信抽象层
    │   ├── fields.hpp                    # 协议字段宏 + 枚举
    │   ├── logger.h                      # 日志宏
    │   ├── uuid.h                        # UUID 生成
    │   ├── protocol.h                    # JSON 序列化工具
    │   ├── message.h                     # 具体消息类型
    │   ├── detail.hpp                    # MessageFactory
    │   ├── muduo_impl.h                  # muduo 封装
    │   ├── dispatcher.h                  # 消息分发器
    │   ├── rpc_router.h                  # 服务端 RPC 路由器
    │   ├── topic_manager.h               # 服务端发布订阅管理器
    │   ├── pd_manager.h                  # 服务端注册/发现管理器
    │   ├── requestor.h                   # 客户端请求管理器
    │   ├── rpc_caller.h                  # 客户端 RPC 调用器
    │   ├── client_registry.h             # 客户端注册/发现工具
    │   ├── client_topic_manager.h        # 客户端发布订阅管理器
    │   ├── servers.h                     # 服务端整合
    │   └── clients.h                     # 客户端整合
    ├── src/                              # 源文件
    └── test/                             # 测试程序

## 编译

### 依赖

- g++ 支持 C++11
- CMake >= 3.10
- muduo（需自行编译安装）
- libjsoncpp-dev

Ubuntu 下安装：

    sudo apt install build-essential cmake libjsoncpp-dev libboost-all-dev

muduo 安装：

    git clone https://github.com/chenshuo/muduo.git
    cd muduo
    mkdir build && cd build
    cmake .. -DMUDUO_BUILD_EXAMPLES=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    make -j$(nproc) muduo_base muduo_net
    sudo make install

### 构建

    mkdir build && cd build
    cmake ..
    make

## 使用示例

### 1. 简单 RPC 调用（直连模式）

服务端：

    void Add(const Json::Value &req, Json::Value &rsp) {
        rsp = req["num1"].asInt() + req["num2"].asInt();
    }

    int main() {
        bitrpc::server::SDescribeFactory factory;
        factory.setMethodName("Add");
        factory.setParamsDesc("num1", bitrpc::server::VType::INTEGRAL);
        factory.setParamsDesc("num2", bitrpc::server::VType::INTEGRAL);
        factory.setReturnType(bitrpc::server::VType::INTEGRAL);
        factory.setCallback(Add);

        bitrpc::server::RpcServer server(bitrpc::Address("127.0.0.1", 9090));
        server.registerMethod(factory.build());
        server.start();
    }

客户端：

    bitrpc::client::RpcClient client(false, "127.0.0.1", 9090);

    Json::Value params, result;
    params["num1"] = 11;
    params["num2"] = 22;

    // 同步调用
    client.call("Add", params, result);

    // 异步 future 调用
    bitrpc::client::RpcCaller::JsonAsyncResponse fut;
    client.call("Add", params, fut);
    result = fut.get();

    // 异步回调调用
    client.call("Add", params, [](const Json::Value &r) {
        std::cout << r.asInt() << std::endl;
    });

运行：

    # 终端 1
    ./build/rpc_server_test

    # 终端 2
    ./build/rpc_client_test

### 2. 发布订阅

    # 终端 1：发布订阅服务端
    ./build/topic_server_test

    # 终端 2：订阅者
    ./build/topic_subscriber_test

    # 终端 3：发布者
    ./build/topic_publisher_test

### 3. 服务注册与发现

    # 终端 1：注册中心
    ./build/registry_server_test

    # 终端 2：带注册的 RPC 服务端
    ./build/rpc_server_registry_test

    # 终端 3：带服务发现的 RPC 客户端
    ./build/rpc_client_registry_test

## 架构设计

### 分层结构

    业务层   RpcServer / RpcClient
             TopicServer / TopicClient
             RegistryServer / RegistryClient
             ↓
    路由层   RpcRouter / TopicManager / PDManager
             ↓
    调度层   Dispatcher
             ↓
    通信层   MuduoServer / MuduoClient / LVProtocol
             ↓
    抽象层   BaseMessage / BaseConnection / BaseProtocol

### 通信协议（LV 格式）

    |--Len(4)--|--mtype(4)--|--idlen(4)--|--id(idlen)--|--body--|

- `Len`：后面所有字段的总长度
- `mtype`：消息类型
- `idlen`：请求 id 的长度
- `id`：请求唯一标识（UUID）
- `body`：JSON 序列化后的消息正文

### 三种请求类型

| 类型 | 特点 | 用法 |
|------|------|------|
| 同步调用 | 发送后阻塞等待结果 | 简单场景 |
| 异步 future | 返回 future，可稍后取结果 | 并发多个请求 |
| 异步回调 | 响应到达时自动触发回调 | 不阻塞 |

## 性能优化

- 编译时开启 -O2 优化
- 缓冲区大小上限 64KB，防止恶意攻击
- 使用 shared_ptr 管理连接与消息

## 后续扩展

- 服务提供者健康检查
- 更多负载均衡策略（随机、一致性哈希）
- Protobuf 序列化支持
- 基于 ZooKeeper / etcd 的分布式注册中心

## 参考

- [muduo 网络库](https://github.com/chenshuo/muduo)
- [JsonCpp](https://github.com/open-source-parsers/jsoncpp)

## License

MIT
