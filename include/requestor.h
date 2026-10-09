#pragma once

// ---------- 本项目其他模块 ----------
#include "abstract.h" // BaseConnection、BaseMessage
#include "fields.hpp" // RType
#include "logger.h"   // ELOG

// ---------- 标准库 ----------
#include <future>        // std::promise、std::future
#include <functional>    // std::function
#include <memory>        // shared_ptr
#include <mutex>         // mutex、unique_lock
#include <unordered_map> // 请求描述表
#include <string>        // string

namespace bitrpc
{
    namespace client
    {

        // ============================================================
        // Requestor：客户端请求管理器
        // 职责：
        //   1. 提供三种发送请求的方式（同步、异步 future、异步回调）
        //   2. 维护“请求 id -> 请求描述”的映射表
        //   3. 收到响应时，按 id 找到对应的请求，唤醒等待者或触发回调
        // ============================================================
        class Requestor
        {
        public:
            using ptr = std::shared_ptr<Requestor>;

            // 回调函数类型：处理收到的响应
            using RequestCallback = std::function<void(const BaseMessage::ptr &)>;
            // 异步 future 类型：将来会拿到响应
            using AsyncResponse = std::future<BaseMessage::ptr>;

            // 描述一个正在进行中的请求
            struct RequestDescribe
            {
                using ptr = std::shared_ptr<RequestDescribe>;
                BaseMessage::ptr request;                // 原始请求
                RType rtype;                             // 请求类型：异步 or 回调
                std::promise<BaseMessage::ptr> response; // 用于异步 future 方式
                RequestCallback callback;                // 用于回调方式
            };

            // 收到响应时调用：按 id 找到请求描述，唤醒等待者或触发回调
            void onResponse(const BaseConnection::ptr &conn, BaseMessage::ptr &msg);

            // 方式1：异步 future。发送请求，立刻返回 future
            bool send(const BaseConnection::ptr &conn,
                      const BaseMessage::ptr &req,
                      AsyncResponse &async_rsp);

            // 方式2：同步。发送请求，阻塞等待响应
            bool send(const BaseConnection::ptr &conn,
                      const BaseMessage::ptr &req,
                      BaseMessage::ptr &rsp);

            // 方式3：异步回调。发送请求，响应到达时调用 cb
            bool send(const BaseConnection::ptr &conn,
                      const BaseMessage::ptr &req,
                      const RequestCallback &cb);

        private:
            // 创建一个请求描述并存入映射表
            RequestDescribe::ptr newDescribe(const BaseMessage::ptr &req,
                                             RType rtype,
                                             const RequestCallback &cb = RequestCallback());

            // 按 id 查询请求描述
            RequestDescribe::ptr getDescribe(const std::string &rid);

            // 按 id 删除请求描述
            void delDescribe(const std::string &rid);

        private:
            std::mutex _mutex;                                                   // 保护 _request_desc
            std::unordered_map<std::string, RequestDescribe::ptr> _request_desc; // id -> 请求描述
        };

    } // namespace client
} // namespace bitrpc