#pragma once
#include <memory>
#include <functional>
#include "fields.hpp" // 之前定义的枚举

namespace bitrpc
{

    // ================= 消息基类 =================
    class BaseMessage
    {
    public:
        using ptr = std::shared_ptr<BaseMessage>; // 智能指针别名
        virtual ~BaseMessage() {}                 // 虚析构，确保子类能正确释放

        // 消息ID的读写
        virtual void setId(const std::string &id) { _rid = id; }
        virtual std::string rid() { return _rid; }

        // 消息类型（RPC请求、响应、服务操作等）的读写
        virtual void setMType(MType mtype) { _mtype = mtype; }
        virtual MType mtype() { return _mtype; }

        // 纯虚函数：子类必须实现
        virtual std::string serialize() = 0;                  // 序列化成字符串
        virtual bool unserialize(const std::string &msg) = 0; // 从字符串还原
        virtual bool check() = 0;                             // 校验数据合法性

    private:
        MType _mtype;     // 消息类型
        std::string _rid; // 消息ID
    };

    // ================= 缓冲区基类 =================
    class BaseBuffer
    {
    public:
        using ptr = std::shared_ptr<BaseBuffer>;
        virtual size_t readableSize() = 0;                    // 还有多少可读
        virtual int32_t peekInt32() = 0;                      // 看一眼4字节整数（不取出）
        virtual void retrieveInt32() = 0;                     // 吃掉4字节整数
        virtual int32_t readInt32() = 0;                      // 读取4字节整数并返回
        virtual std::string retrieveAsString(size_t len) = 0; // 吃掉指定长度并转成字符串
    };

    // ================= 协议基类 =================
    class BaseProtocol
    {
    public:
        using ptr = std::shared_ptr<BaseProtocol>;
        virtual bool canProcessed(const BaseBuffer::ptr &buf) = 0;                     // 判断缓冲区里是否有完整消息
        virtual bool onMessage(const BaseBuffer::ptr &buf, BaseMessage::ptr &msg) = 0; // 从缓冲区提取消息
        virtual std::string serialize(const BaseMessage::ptr &msg) = 0;                // 将消息序列化
    };

    // ================= 连接基类 =================
    class BaseConnection
    {
    public:
        using ptr = std::shared_ptr<BaseConnection>;
        virtual void send(const BaseMessage::ptr &msg) = 0; // 发送消息
        virtual void shutdown() = 0;                        // 关闭连接
        virtual bool connected() = 0;                       // 是否已连接
    };

    // ================= 回调函数类型定义 =================
    using ConnectionCallback = std::function<void(const BaseConnection::ptr &)>;
    using CloseCallback = std::function<void(const BaseConnection::ptr &)>;
    using MessageCallback = std::function<void(const BaseConnection::ptr &, BaseMessage::ptr &)>;

    // ================= 服务端基类 =================
    class BaseServer
    {
    public:
        using ptr = std::shared_ptr<BaseServer>;
        // 设置三个回调（子类可直接使用这几个成员变量）
        virtual void setConnectionCallback(const ConnectionCallback &cb) { _cb_connection = cb; }
        virtual void setCloseCallback(const CloseCallback &cb) { _cb_close = cb; }
        virtual void setMessageCallback(const MessageCallback &cb) { _cb_message = cb; }
        virtual void start() = 0; // 纯虚：启动服务器
    protected:
        ConnectionCallback _cb_connection; // 新连接回调
        CloseCallback _cb_close;           // 断开连接回调
        MessageCallback _cb_message;       // 收到消息回调
    };

    // ================= 客户端基类 =================
    class BaseClient
    {
    public:
        using ptr = std::shared_ptr<BaseClient>;
        // 设置三个回调
        virtual void setConnectionCallback(const ConnectionCallback &cb) { _cb_connection = cb; }
        virtual void setCloseCallback(const CloseCallback &cb) { _cb_close = cb; }
        virtual void setMessageCallback(const MessageCallback &cb) { _cb_message = cb; }

        virtual void connect() = 0;                      // 发起连接
        virtual void shutdown() = 0;                     // 关闭自己
        virtual bool send(const BaseMessage::ptr &) = 0; // 发送消息
        virtual BaseConnection::ptr connection() = 0;    // 获取当前连接对象
        virtual bool connected() = 0;                    // 是否已连接
    protected:
        ConnectionCallback _cb_connection;
        CloseCallback _cb_close;
        MessageCallback _cb_message;
    };

}