#pragma once

// ---------- 本项目其他模块 ----------
#include "requestor.h" // Requestor：请求管理器
#include "message.h"   // RpcRequest、RpcResponse
#include "fields.hpp"  // MType、RCode
#include "logger.h"    // DLOG、ELOG

// ---------- 标准库 ----------
#include <jsoncpp/json/json.h> // Json::Value
#include <future>              // std::promise、std::future
#include <functional>          // std::function
#include <memory>              // shared_ptr
#include <string>              // string

namespace bitrpc
{
    namespace client
    {

        // ============================================================
        // RpcCaller：RPC 调用器
        // 职责：把“调用某个远程方法”这件事包装成三种接口
        //       1. 同步调用：发完请求阻塞等结果
        //       2. 异步 future：发完立刻返回 future
        //       3. 异步回调：发完不管，响应到达时触发用户回调
        // 依赖：Requestor（负责底层的请求-响应匹配）
        // ============================================================
        class RpcCaller
        {
        public:
            using ptr = std::shared_ptr<RpcCaller>;

            // 异步 future 的返回值类型
            using JsonAsyncResponse = std::future<Json::Value>;
            // 异步回调的函数类型
            using JsonResponseCallback = std::function<void(const Json::Value &)>;

            // 构造：必须传入 Requestor
            explicit RpcCaller(const Requestor::ptr &requestor) : _requestor(requestor) {}

            // ---------- 方式1：同步调用 ----------
            // 发送 RPC 请求，阻塞等待响应，结果写入 result
            // 成功返回 true，失败返回 false
            bool call(const BaseConnection::ptr &conn,
                      const std::string &method,
                      const Json::Value &params,
                      Json::Value &result);

            // ---------- 方式2：异步 future ----------
            // 发送请求，立刻返回一个 future，用户将来用 get() 拿结果
            bool call(const BaseConnection::ptr &conn,
                      const std::string &method,
                      const Json::Value &params,
                      JsonAsyncResponse &result);

            // ---------- 方式3：异步回调 ----------
            // 发送请求，响应到达时自动调用 cb
            bool call(const BaseConnection::ptr &conn,
                      const std::string &method,
                      const Json::Value &params,
                      const JsonResponseCallback &cb);

        private:
            // 异步回调的内部包装：把 BaseMessage 转成 RpcResponse，校验 rcode 后调用用户回调
            void Callback1(const JsonResponseCallback &cb, const BaseMessage::ptr &msg);

            // 异步 future 的内部包装：把 BaseMessage 转成 RpcResponse，校验 rcode 后设置 promise
            void Callback(std::shared_ptr<std::promise<Json::Value>> result,
                          const BaseMessage::ptr &msg);

        private:
            Requestor::ptr _requestor; // 底层请求管理器
        };

    } // namespace client
} // namespace bitrpc