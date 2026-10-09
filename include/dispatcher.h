#pragma once

// ---------- 本项目其他模块 ----------
#include "abstract.h"   // BaseConnection、BaseMessage
#include "message.h"    // MType（在 fields.hpp 里，被 message.h 间接包含）
#include "fields.hpp"   // MType 枚举
#include "logger.h"     // ELOG

// ---------- 标准库 ----------
#include <memory>            // shared_ptr
#include <functional>        // std::function
#include <mutex>             // std::mutex
#include <unordered_map>     // std::unordered_map

namespace bitrpc {

// ============================================================
// Callback：抽象回调基类
// 作用：让 Dispatcher 能以统一方式持有各种类型的回调，
//       不关心具体消息类型是什么。
// ============================================================
class Callback {
public:
    using ptr = std::shared_ptr<Callback>;
    virtual ~Callback() = default;

    // 处理消息的接口，由子类实现
    virtual void onMessage(const BaseConnection::ptr &conn, BaseMessage::ptr &msg) = 0;
};

// ============================================================
// CallbackT<T>：针对具体消息类型 T 的回调包装
// 作用：把用户提供的“处理某种具体消息”的函数包装成
//       Callback 的子类，内部自动把 BaseMessage 转成 T。
// 注意：这是模板类，实现必须放在头文件里。
// ============================================================
template<typename T>
class CallbackT : public Callback {
public:
    using ptr = std::shared_ptr<CallbackT<T>>;
    // 用户提供的回调函数类型：参数是具体消息类型 T
    using MessageCallback = std::function<void(const BaseConnection::ptr &conn, std::shared_ptr<T> &msg)>;

    // 构造：保存用户提供的回调函数
    CallbackT(const MessageCallback &handler) : _handler(handler) {}

    // 收到消息时被调用：把 msg 转成 T，再调用用户回调
    void onMessage(const BaseConnection::ptr &conn, BaseMessage::ptr &msg) override {
        // dynamic_pointer_cast：把基类指针转成具体子类指针
        auto type_msg = std::dynamic_pointer_cast<T>(msg);
        _handler(conn, type_msg);
    }

private:
    MessageCallback _handler;
};

// ============================================================
// Dispatcher：消息分发器
// 作用：维护“消息类型 -> 回调”的映射表，
//       收到消息时按类型找到对应回调并调用。
// 用法：
//   dispatcher.registerHandler<RpcRequest>(MType::REQ_RPC, handler);
//   收到 REQ_RPC 消息时，handler 会被调用。
// ============================================================
class Dispatcher {
public:
    using ptr = std::shared_ptr<Dispatcher>;

    // 注册某个消息类型的处理函数
    // T：具体的消息类型（如 RpcRequest）
    // mtype：MType 枚举值（如 REQ_RPC）
    // handler：用户提供的处理函数
    template<typename T>
    void registerHandler(MType mtype, const typename CallbackT<T>::MessageCallback &handler) {
        std::unique_lock<std::mutex> lock(_mutex);
        auto cb = std::make_shared<CallbackT<T>>(handler); // 包装成 CallbackT<T>
        _handlers.insert(std::make_pair(mtype, cb));       // 存入映射表
    }

    // 收到消息时调用：按消息类型找到回调并执行
    // 若找不到对应类型，记录错误并关闭连接
    void onMessage(const BaseConnection::ptr &conn, BaseMessage::ptr &msg);

private:
    std::mutex _mutex;                                    // 保护 _handlers
    std::unordered_map<MType, Callback::ptr> _handlers;   // 消息类型 -> 回调
};

}  // namespace bitrpc