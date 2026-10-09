#pragma once

// ---------- 本项目其他模块 ----------
#include "abstract.h" // BaseConnection：连接对象；BaseMessage：消息基类
#include "message.h"  // TopicRequest、TopicResponse：主题请求/响应
#include "fields.hpp" // RCode、TopicOptype、MType
#include "logger.h"   // ELOG

// ---------- 标准库 ----------
#include <memory>        // shared_ptr
#include <mutex>         // mutex, unique_lock
#include <unordered_map> // 映射表
#include <unordered_set> // 集合
#include <string>        // string
#include <vector>        // vector

namespace bitrpc
{
    namespace server
    {

        // ============================================================
        // Subscriber：一个订阅者（对应一条客户端连接）
        // 保存它订阅的所有主题名
        // ============================================================
        class Subscriber
        {
        public:
            using ptr = std::shared_ptr<Subscriber>;

            BaseConnection::ptr conn;               // 订阅者对应的连接
            std::unordered_set<std::string> topics; // 订阅者订阅的主题集合

            explicit Subscriber(const BaseConnection::ptr &c) : conn(c) {}

            void appendTopic(const std::string &topic_name); // 订阅时调用
            void removeTopic(const std::string &topic_name); // 取消订阅或主题删除时调用

        private:
            std::mutex _mutex; // 保护 topics
        };

        // ============================================================
        // Topic：一个主题
        // 保存该主题的所有订阅者，并提供推送消息的方法
        // ============================================================
        class Topic
        {
        public:
            using ptr = std::shared_ptr<Topic>;

            std::string topic_name;                          // 主题名
            std::unordered_set<Subscriber::ptr> subscribers; // 订阅该主题的所有订阅者

            explicit Topic(const std::string &name) : topic_name(name) {}

            void appendSubscriber(const Subscriber::ptr &subscriber); // 新增订阅时调用
            void removeSubscriber(const Subscriber::ptr &subscriber); // 取消订阅/连接断开时调用
            void pushMessage(const BaseMessage::ptr &msg);            // 发布消息，推送给所有订阅者

        private:
            std::mutex _mutex; // 保护 subscribers
        };

        // ============================================================
        // TopicManager：发布订阅管理器
        // 作用：处理所有 TOPIC 相关请求（创建/删除/订阅/取消/发布），
        //       并维护主题和订阅者的映射。
        // 用法：注册到 Dispatcher 处理 REQ_TOPIC 类型的消息。
        // ============================================================
        class TopicManager
        {
        public:
            using ptr = std::shared_ptr<TopicManager>;

            TopicManager() = default;

            // 处理来自客户端的主题请求（注册到 Dispatcher 的回调）
            void onTopicRequest(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg);

            // 客户端连接断开时调用，清理该连接作为订阅者的相关数据
            void onShutdown(const BaseConnection::ptr &conn);

        private:
            // 发送错误响应
            void errorResponse(const BaseConnection::ptr &conn,
                               const TopicRequest::ptr &msg,
                               RCode rcode);

            // 发送成功响应
            void topicResponse(const BaseConnection::ptr &conn,
                               const TopicRequest::ptr &msg);

            // 创建主题
            void topicCreate(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg);

            // 删除主题
            void topicRemove(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg);

            // 订阅主题，成功返回 true，主题不存在返回 false
            bool topicSubscribe(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg);

            // 取消订阅
            void topicCancel(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg);

            // 发布消息，成功返回 true，主题不存在返回 false
            bool topicPublish(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg);

        private:
            std::mutex _mutex;                                                     // 保护两张映射表
            std::unordered_map<std::string, Topic::ptr> _topics;                   // 主题名 -> 主题
            std::unordered_map<BaseConnection::ptr, Subscriber::ptr> _subscribers; // 连接 -> 订阅者
        };

    } // namespace server
} // namespace bitrpc