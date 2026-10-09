#include "muduo_impl.h" // 本模块声明，包含所有类定义
#include <iostream>     // std::cout，调试输出用
#include <arpa/inet.h>  // htonl：主机字节序转网络字节序

namespace bitrpc
{

    // ============================================================
    // MuduoBuffer
    // ============================================================

    size_t MuduoBuffer::readableSize() { return _buf->readableBytes(); }
    int32_t MuduoBuffer::peekInt32() { return _buf->peekInt32(); }
    void MuduoBuffer::retrieveInt32() { _buf->retrieveInt32(); }
    int32_t MuduoBuffer::readInt32() { return _buf->readInt32(); }
    std::string MuduoBuffer::retrieveAsString(size_t len) { return _buf->retrieveAsString(len); }

    // ============================================================
    // LVProtocol
    // ============================================================

    // 判断是否攒够一条完整消息：
    // 先看够不够 4 字节长度头，再看可读数据够不够“总长度 + 4”
    bool LVProtocol::canProcessed(const BaseBuffer::ptr &buf)
    {
        if (buf->readableSize() < lenFieldsLength)
            return false;
        int32_t total_len = buf->peekInt32();
        if (buf->readableSize() < (size_t)(total_len + (int32_t)lenFieldsLength))
            return false;
        return true;
    }

    // 从缓冲区提取一条完整消息并反序列化
    bool LVProtocol::onMessage(const BaseBuffer::ptr &buf, BaseMessage::ptr &msg)
    {
        // 依次读取头部各字段
        int32_t total_len = buf->readInt32();  // 总长度
        MType mtype = (MType)buf->readInt32(); // 消息类型
        int32_t idlen = buf->readInt32();      // id 长度
        // body 长度 = 总长度 - id 长度 - 两个固定字段长度
        int32_t body_len = total_len - idlen - (int32_t)idlenFieldsLength - (int32_t)mtypeFieldsLength;
        // 取出 id 和正文
        std::string id = buf->retrieveAsString(idlen);
        std::string body = buf->retrieveAsString(body_len);

        // 根据消息类型创建具体对象
        msg = MessageFactory::create(mtype);
        if (msg.get() == nullptr)
        {
            ELOG("消息类型错误，构造消息对象失败！");
            return false;
        }
        // 反序列化正文
        if (!msg->unserialize(body))
        {
            ELOG("消息正文反序列化失败！");
            return false;
        }
        msg->setId(id);
        msg->setMType(mtype);
        return true;
    }

    // 把消息打包成 LV 字节流
    std::string LVProtocol::serialize(const BaseMessage::ptr &msg)
    {
        std::string body = msg->serialize(); // 先序列化正文
        std::string id = msg->rid();         // 取消息 id

        // 转换成网络字节序
        int32_t mtype = htonl((int32_t)msg->mtype());
        int32_t idlen = htonl((int32_t)id.size());
        int32_t h_total_len = (int32_t)(mtypeFieldsLength + idlenFieldsLength + id.size() + body.size());
        int32_t n_total_len = htonl(h_total_len);

        // 按协议顺序拼接字节流
        std::string result;
        result.reserve(h_total_len);
        result.append((char *)&n_total_len, lenFieldsLength); // 总长度
        result.append((char *)&mtype, mtypeFieldsLength);     // 消息类型
        result.append((char *)&idlen, idlenFieldsLength);     // id 长度
        result.append(id);                                    // id
        result.append(body);                                  // 正文
        return result;
    }

    // ============================================================
    // MuduoConnection
    // ============================================================

    void MuduoConnection::send(const BaseMessage::ptr &msg)
    {
        std::string body = _protocol->serialize(msg); // 先序列化
        _conn->send(body);                            // 通过 muduo 发送
    }

    void MuduoConnection::shutdown() { _conn->shutdown(); }
    bool MuduoConnection::connected() { return _conn->connected(); }

    // ============================================================
    // MuduoServer
    // ============================================================

    // 初始化 muduo TcpServer 和协议对象
    MuduoServer::MuduoServer(int port)
        : _server(&_baseloop, muduo::net::InetAddress("0.0.0.0", port),
                  "MuduoServer", muduo::net::TcpServer::kReusePort),
          _protocol(ProtocolFactory::create()) {}

    // 设置回调 → 开始监听 → 进入事件循环
    void MuduoServer::start()
    {
        _server.setConnectionCallback(
            std::bind(&MuduoServer::onConnection, this, std::placeholders::_1));
        _server.setMessageCallback(
            std::bind(&MuduoServer::onMessage, this,
                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        _server.start();
        _baseloop.loop();
    }

    // 连接建立/断开处理
    void MuduoServer::onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            // 新连接：创建 BaseConnection 并保存到 _conns
            auto muduo_conn = ConnectionFactory::create(conn, _protocol);
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _conns.insert(std::make_pair(conn, muduo_conn));
            }
            // 通知业务层
            if (_cb_connection)
                _cb_connection(muduo_conn);
        }
        else
        {
            // 连接断开：从 _conns 中取出并删除，然后通知业务层
            BaseConnection::ptr muduo_conn;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(conn);
                if (it == _conns.end())
                    return;
                muduo_conn = it->second;
                _conns.erase(conn);
            }
            if (_cb_close)
                _cb_close(muduo_conn);
        }
    }

    // 收到数据时：循环拆包 → 反序列化 → 交给业务层
    void MuduoServer::onMessage(const muduo::net::TcpConnectionPtr &conn,
                                muduo::net::Buffer *buf, muduo::Timestamp)
    {
        auto base_buf = BufferFactory::create(buf); // 包装缓冲区
        while (1)
        {
            // 不够一条完整消息就退出循环
            if (!_protocol->canProcessed(base_buf))
            {
                // 但如果缓冲区已经过大，直接断开
                if (base_buf->readableSize() > maxDataSize)
                {
                    conn->shutdown();
                    ELOG("缓冲区中数据过大！");
                    return;
                }
                break;
            }
            // 提取一条消息
            BaseMessage::ptr msg;
            if (!_protocol->onMessage(base_buf, msg))
            {
                conn->shutdown();
                ELOG("缓冲区中数据错误！");
                return;
            }
            // 找到对应的 BaseConnection
            BaseConnection::ptr base_conn;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(conn);
                if (it == _conns.end())
                {
                    conn->shutdown();
                    return;
                }
                base_conn = it->second;
            }
            // 回调业务层
            if (_cb_message)
                _cb_message(base_conn, msg);
        }
    }

    // ============================================================
    // MuduoClient
    // ============================================================

    // 初始化：创建协议对象，启动事件循环线程，创建 muduo 客户端
    MuduoClient::MuduoClient(const std::string &sip, int sport)
        : _protocol(ProtocolFactory::create()),
          _baseloop(_loopthread.startLoop()), // 启动事件循环并拿到指针
          _downlatch(1),
          _client(_baseloop, muduo::net::InetAddress(sip, sport), "MuduoClient")
    {
    }

    // 连接服务器：设置回调 → 发起连接 → 阻塞等待连接建立
    void MuduoClient::connect()
    {
        _client.setConnectionCallback(
            std::bind(&MuduoClient::onConnection, this, std::placeholders::_1));
        _client.setMessageCallback(
            std::bind(&MuduoClient::onMessage, this,
                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        _client.connect();
        _downlatch.wait(); // 等 onConnection 里 countDown
    }

    void MuduoClient::shutdown() { _client.disconnect(); }

    // 发送消息：先判断连接是否有效
    bool MuduoClient::send(const BaseMessage::ptr &msg)
    {
        if (!connected())
        {
            ELOG("连接已断开！");
            return false;
        }
        _conn->send(msg);
        return true;
    }

    BaseConnection::ptr MuduoClient::connection() { return _conn; }
    bool MuduoClient::connected() { return (_conn && _conn->connected()); }

    // 连接建立/断开处理
    void MuduoClient::onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            _downlatch.countDown(); // 唤醒 connect() 中等待的线程
            _conn = ConnectionFactory::create(conn, _protocol);
        }
        else
        {
            _conn.reset();
        }
    }

    // 收到数据时：循环拆包 → 反序列化 → 交给业务层
    void MuduoClient::onMessage(const muduo::net::TcpConnectionPtr &conn,
                                muduo::net::Buffer *buf, muduo::Timestamp)
    {
        auto base_buf = BufferFactory::create(buf);
        while (1)
        {
            if (!_protocol->canProcessed(base_buf))
            {
                if (base_buf->readableSize() > maxDataSize)
                {
                    conn->shutdown();
                    ELOG("缓冲区中数据过大！");
                    return;
                }
                break;
            }
            BaseMessage::ptr msg;
            if (!_protocol->onMessage(base_buf, msg))
            {
                conn->shutdown();
                ELOG("缓冲区中数据错误！");
                return;
            }
            if (_cb_message)
                _cb_message(_conn, msg);
        }
    }

} // namespace bitrpc