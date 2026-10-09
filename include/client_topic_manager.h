#pragma once

// ---------- 本项目其他模块 ----------
#include "requestor.h" // Requestor
#include "message.h"   // TopicRequest、TopicResponse
#include "fields.hpp"  // MType、RCode、TopicOptype
#include "logger.h"    // ELOG

// ---------- 标准库 ----------
#include <memory>        // shared_ptr
#include <functional>    // std::function
#include <mutex>         // mutex、unique_lock
#include <unordered_map> // 订阅回调表
#include <string>        // string

namespace bitrpc
{
    namespace client
    {

        // ============================================================
        // TopicManager：客户端的发布订阅管理器
        // 与服务端的 TopicManager 不同：
        //   服务端负责管理主题、订阅者、推送消息；
        //   客户端只负责“发请求 + 本地保存订阅回调”，
        //   收到推送消息时按主题找到回调触发。
        // ============================================================
        class TopicManager
        {
        public:
            using ptr = std::shared_ptr<TopicManager>;

            // 订阅回调：收到某主题推送时调用，参数是（主题名，消息内容）
            using SubCallback = std::function<void(const std::string &, const std::string &)>;

            explicit TopicManager(const Requestor::ptr &requestor) : _requestor(requestor) {}

            // ---------- 对外接口：主题操作 ----------

            // 创建主题
            bool create(const BaseConnection::ptr &conn, const std::string &key);

            // 删除主题
            bool remove(const BaseConnection::ptr &conn, const std::string &key);

            // 订阅主题：本地先记录回调，再发请求；失败则回滚本地记录
            bool subscribe(const BaseConnection::ptr &conn,
                           const std::string &key,
                           const SubCallback &cb);

            // 取消订阅：本地先删回调，再发请求
            bool cancel(const BaseConnection::ptr &conn, const std::string &key);

            // 发布消息到主题
            bool publish(const BaseConnection::ptr &conn,
                         const std::string &key,
                         const std::string &msg);

            // 收到服务端推送的发布消息时被调用：
            //   按主题名找到本地注册的回调并触发
            void onPublish(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg);

        private:
            // 本地记录一个主题的回调
            void addSubscribe(const std::string &key, const SubCallback &cb);

            // 本地删除一个主题的回调
            void delSubscribe(const std::string &key);

            // 按主题名查询回调
            const SubCallback getSubscribe(const std::string &key);

            // 通用请求：把主题操作封装成一次 RPC 调用，等待服务端响应
            bool commonRequest(const BaseConnection::ptr &conn,
                               const std::string &key,
                               TopicOptype type,
                               const std::string &msg = "");

        private:
            std::mutex _mutex;                                             // 保护 _topic_callbacks
            std::unordered_map<std::string, SubCallback> _topic_callbacks; // 主题名 -> 回调
            Requestor::ptr _requestor;                                     // 底层请求管理器
        };

    } // namespace client
} // namespace bitrpc