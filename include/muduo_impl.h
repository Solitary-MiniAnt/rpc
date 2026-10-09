#pragma once

// ---------- muduo 网络库 ----------
#include <muduo/net/TcpServer.h>       // TcpServer：服务器监听、接受连接
#include <muduo/net/EventLoop.h>       // EventLoop：事件循环，驱动网络事件
#include <muduo/net/TcpConnection.h>   // TcpConnectionPtr：代表一条已建立的连接
#include <muduo/net/Buffer.h>          // Buffer：muduo 的读写缓冲区
#include <muduo/base/CountDownLatch.h> // CountDownLatch：用于阻塞等待连接建立
#include <muduo/net/EventLoopThread.h> // EventLoopThread：在独立线程里跑事件循环
#include <muduo/net/TcpClient.h>       // TcpClient：客户端，主动连接服务器

// ---------- 本项目其他模块 ----------
#include "detail.hpp" // MessageFactory：根据 MType 创建具体的消息对象
#include "fields.hpp" // 宏定义 + 枚举（MType、RCode、TopicOptype 等）
#include "abstract.h" // 抽象基类：BaseBuffer / BaseProtocol / BaseConnection / BaseServer / BaseClient
#include "message.h"  // JsonMessage / JsonRequest / JsonResponse

// ---------- 标准库 ----------
#include <mutex>         // std::mutex：保护 _conns 的互斥锁
#include <unordered_map> // std::unordered_map：muduo 连接 -> BaseConnection 的映射表

namespace bitrpc
{

    // ============================================================
    // MuduoBuffer
    // 把 muduo::Buffer 包装成 BaseBuffer 接口。
    // ============================================================
    class MuduoBuffer : public BaseBuffer
    {
    public:
        using ptr = std::shared_ptr<MuduoBuffer>;
        explicit MuduoBuffer(muduo::net::Buffer *buf) : _buf(buf) {}

        virtual size_t readableSize() override;                    // 还有多少可读
        virtual int32_t peekInt32() override;                      // 窥视前4字节
        virtual void retrieveInt32() override;                     // 取走前4字节
        virtual int32_t readInt32() override;                      // 读取前4字节
        virtual std::string retrieveAsString(size_t len) override; // 取走len字节转字符串

    private:
        muduo::net::Buffer *_buf;
    };

    // 创建 BaseBuffer 的工厂
    class BufferFactory
    {
    public:
        template <typename... Args>
        static BaseBuffer::ptr create(Args &&...args)
        {
            return std::make_shared<MuduoBuffer>(std::forward<Args>(args)...);
        }
    };

    // ============================================================
    // LVProtocol：长度-值协议，解决 TCP 粘包
    // ============================================================
    class LVProtocol : public BaseProtocol
    {
    public:
        using ptr = std::shared_ptr<LVProtocol>;

        virtual bool canProcessed(const BaseBuffer::ptr &buf) override;                     // 判断是否够一条完整消息
        virtual bool onMessage(const BaseBuffer::ptr &buf, BaseMessage::ptr &msg) override; // 提取消息
        virtual std::string serialize(const BaseMessage::ptr &msg) override;                // 打包消息

    private:
        const size_t lenFieldsLength = 4;   // 总长度字段长度
        const size_t mtypeFieldsLength = 4; // 消息类型字段长度
        const size_t idlenFieldsLength = 4; // id长度字段长度
    };

    // 创建 BaseProtocol 的工厂
    class ProtocolFactory
    {
    public:
        template <typename... Args>
        static BaseProtocol::ptr create(Args &&...args)
        {
            return std::make_shared<LVProtocol>(std::forward<Args>(args)...);
        }
    };

    // ============================================================
    // MuduoConnection：包装 muduo::TcpConnection
    // ============================================================
    class MuduoConnection : public BaseConnection
    {
    public:
        using ptr = std::shared_ptr<MuduoConnection>;
        MuduoConnection(const muduo::net::TcpConnectionPtr &conn,
                        const BaseProtocol::ptr &protocol)
            : _protocol(protocol), _conn(conn) {}

        virtual void send(const BaseMessage::ptr &msg) override; // 发送消息
        virtual void shutdown() override;                        // 关闭连接
        virtual bool connected() override;                       // 是否已连接

    private:
        BaseProtocol::ptr _protocol;
        muduo::net::TcpConnectionPtr _conn;
    };

    // 创建 BaseConnection 的工厂
    class ConnectionFactory
    {
    public:
        template <typename... Args>
        static BaseConnection::ptr create(Args &&...args)
        {
            return std::make_shared<MuduoConnection>(std::forward<Args>(args)...);
        }
    };

    // ============================================================
    // MuduoServer：基于 muduo::TcpServer 实现
    // ============================================================
    class MuduoServer : public BaseServer
    {
    public:
        using ptr = std::shared_ptr<MuduoServer>;
        explicit MuduoServer(int port);

        virtual void start() override; // 启动服务器

    private:
        void onConnection(const muduo::net::TcpConnectionPtr &conn); // 连接建立/断开回调
        void onMessage(const muduo::net::TcpConnectionPtr &conn,     // 收到数据回调
                       muduo::net::Buffer *buf, muduo::Timestamp);

        const size_t maxDataSize = RPC_MAX_BUFFER_SIZE;                                         // 缓冲区上限
        BaseProtocol::ptr _protocol;                                                  // 协议对象
        muduo::net::EventLoop _baseloop;                                              // 事件循环
        muduo::net::TcpServer _server;                                                // muduo 服务器
        std::mutex _mutex;                                                            // 保护 _conns 的锁
        std::unordered_map<muduo::net::TcpConnectionPtr, BaseConnection::ptr> _conns; // 连接映射表
    };

    // 创建 BaseServer 的工厂
    class ServerFactory
    {
    public:
        template <typename... Args>
        static BaseServer::ptr create(Args &&...args)
        {
            return std::make_shared<MuduoServer>(std::forward<Args>(args)...);
        }
    };

    // ============================================================
    // MuduoClient：基于 muduo::TcpClient 实现
    // ============================================================
    class MuduoClient : public BaseClient
    {
    public:
        using ptr = std::shared_ptr<MuduoClient>;
        MuduoClient(const std::string &sip, int sport);

        virtual void connect() override;                         // 连接服务器
        virtual void shutdown() override;                        // 断开连接
        virtual bool send(const BaseMessage::ptr &msg) override; // 发送消息
        virtual BaseConnection::ptr connection() override;       // 获取连接对象
        virtual bool connected() override;                       // 是否已连接

    private:
        void onConnection(const muduo::net::TcpConnectionPtr &conn);
        void onMessage(const muduo::net::TcpConnectionPtr &conn,
                       muduo::net::Buffer *buf, muduo::Timestamp);

        const size_t maxDataSize = RPC_MAX_BUFFER_SIZE;
        BaseProtocol::ptr _protocol;             // 协议对象
        BaseConnection::ptr _conn;               // 当前连接
        muduo::CountDownLatch _downlatch;        // 用于等待连接建立
        muduo::net::EventLoopThread _loopthread; // 独立的事件循环线程
        muduo::net::EventLoop *_baseloop;        // 事件循环指针
        muduo::net::TcpClient _client;           // muduo 客户端
    };

    // 创建 BaseClient 的工厂
    class ClientFactory
    {
    public:
        template <typename... Args>
        static BaseClient::ptr create(Args &&...args)
        {
            return std::make_shared<MuduoClient>(std::forward<Args>(args)...);
        }
    };

} // namespace bitrpc