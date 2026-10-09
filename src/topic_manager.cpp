#include "topic_manager.h"

namespace bitrpc
{
    namespace server
    {

        // ============================================================
        // Subscriber
        // ============================================================

        void Subscriber::appendTopic(const std::string &topic_name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            topics.insert(topic_name);
        }

        void Subscriber::removeTopic(const std::string &topic_name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            topics.erase(topic_name);
        }

        // ============================================================
        // Topic
        // ============================================================

        void Topic::appendSubscriber(const Subscriber::ptr &subscriber)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            subscribers.insert(subscriber);
        }

        void Topic::removeSubscriber(const Subscriber::ptr &subscriber)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            subscribers.erase(subscriber);
        }

        // 把消息推送给该主题的所有订阅者
        void Topic::pushMessage(const BaseMessage::ptr &msg)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            for (auto &subscriber : subscribers)
            {
                subscriber->conn->send(msg);
            }
        }

        // ============================================================
        // TopicManager
        // ============================================================

        // 分发主题请求：按 optype 走不同处理分支
        void TopicManager::onTopicRequest(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
        {
            TopicOptype optype = msg->optype();
            bool ret = true;
            switch (optype)
            {
            case TopicOptype::TOPIC_CREATE:
                topicCreate(conn, msg);
                break;
            case TopicOptype::TOPIC_REMOVE:
                topicRemove(conn, msg);
                break;
            case TopicOptype::TOPIC_SUBSCRIBE:
                ret = topicSubscribe(conn, msg);
                break;
            case TopicOptype::TOPIC_CANCEL:
                topicCancel(conn, msg);
                break;
            case TopicOptype::TOPIC_PUBLISH:
                ret = topicPublish(conn, msg);
                break;
            default:
                // 未知操作类型
                errorResponse(conn, msg, RCode::RCODE_INVALID_OPTYPE);
                return;
            }
            if (!ret)
            {
                // subscribe / publish 失败，说明主题不存在
                errorResponse(conn, msg, RCode::RCODE_NOT_FOUND_TOPIC);
                return;
            }
            // 其余情况视为成功
            topicResponse(conn, msg);
        }

        // 客户端断开时清理订阅者数据
        void TopicManager::onShutdown(const BaseConnection::ptr &conn)
        {
            std::vector<Topic::ptr> topics; // 受影响的主题对象
            Subscriber::ptr subscriber;     // 该连接对应的订阅者
            {
                std::unique_lock<std::mutex> lock(_mutex);
                // 不是订阅者，直接返回
                auto it = _subscribers.find(conn);
                if (it == _subscribers.end())
                    return;
                subscriber = it->second;

                // 找出该订阅者订阅的所有主题对象
                for (auto &topic_name : subscriber->topics)
                {
                    auto topic_it = _topics.find(topic_name);
                    if (topic_it == _topics.end())
                        continue;
                    topics.push_back(topic_it->second);
                }
                // 从订阅者映射中删除该订阅者
                _subscribers.erase(it);
            }
            // 从每个主题的订阅者列表中移除该订阅者
            for (auto &topic : topics)
            {
                topic->removeSubscriber(subscriber);
            }
        }

        // 发送错误响应
        void TopicManager::errorResponse(const BaseConnection::ptr &conn,
                                         const TopicRequest::ptr &msg,
                                         RCode rcode)
        {
            auto rsp = std::make_shared<TopicResponse>();
            rsp->setId(msg->rid());
            rsp->setMType(MType::RSP_TOPIC);
            rsp->setRCode(rcode);
            conn->send(rsp);
        }

        // 发送成功响应
        void TopicManager::topicResponse(const BaseConnection::ptr &conn,
                                         const TopicRequest::ptr &msg)
        {
            auto rsp = std::make_shared<TopicResponse>();
            rsp->setId(msg->rid());
            rsp->setMType(MType::RSP_TOPIC);
            rsp->setRCode(RCode::RCODE_OK);
            conn->send(rsp);
        }

        // 创建主题
        void TopicManager::topicCreate(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            std::string topic_name = msg->topicKey();
            auto topic = std::make_shared<Topic>(topic_name);
            _topics.insert(std::make_pair(topic_name, topic));
        }

        // 删除主题：先找出受影响的订阅者，再删除主题，最后清理订阅者的数据
        void TopicManager::topicRemove(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
        {
            std::string topic_name = msg->topicKey();
            std::unordered_set<Subscriber::ptr> subscribers;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _topics.find(topic_name);
                if (it == _topics.end())
                    return;
                // 先暂存受影响的订阅者
                subscribers = it->second->subscribers;
                // 删除主题
                _topics.erase(it);
            }
            // 从每个订阅者的订阅集合中移除该主题
            for (auto &subscriber : subscribers)
            {
                subscriber->removeTopic(topic_name);
            }
        }

        // 订阅主题
        bool TopicManager::topicSubscribe(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
        {
            Topic::ptr topic;
            Subscriber::ptr subscriber;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                // 主题必须已存在
                auto topic_it = _topics.find(msg->topicKey());
                if (topic_it == _topics.end())
                    return false;
                topic = topic_it->second;

                // 该连接如果是第一次订阅，则创建 Subscriber；否则复用
                auto sub_it = _subscribers.find(conn);
                if (sub_it != _subscribers.end())
                {
                    subscriber = sub_it->second;
                }
                else
                {
                    subscriber = std::make_shared<Subscriber>(conn);
                    _subscribers.insert(std::make_pair(conn, subscriber));
                }
            }
            // 双向绑定：主题记录订阅者，订阅者记录主题
            topic->appendSubscriber(subscriber);
            subscriber->appendTopic(msg->topicKey());
            return true;
        }

        // 取消订阅
        void TopicManager::topicCancel(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
        {
            Topic::ptr topic;
            Subscriber::ptr subscriber;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto topic_it = _topics.find(msg->topicKey());
                if (topic_it != _topics.end())
                    topic = topic_it->second;

                auto sub_it = _subscribers.find(conn);
                if (sub_it != _subscribers.end())
                    subscriber = sub_it->second;
            }
            // 双向解除绑定
            if (subscriber)
                subscriber->removeTopic(msg->topicKey());
            if (topic && subscriber)
                topic->removeSubscriber(subscriber);
        }

        // 发布消息
        bool TopicManager::topicPublish(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
        {
            Topic::ptr topic;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto topic_it = _topics.find(msg->topicKey());
                if (topic_it == _topics.end())
                    return false;
                topic = topic_it->second;
            }
            // 推送给该主题的所有订阅者
            topic->pushMessage(msg);
            return true;
        }

    } // namespace server
} // namespace bitrpc