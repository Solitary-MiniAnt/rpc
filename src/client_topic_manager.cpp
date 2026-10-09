#include "client_topic_manager.h"
#include "uuid.h" // uuid()

namespace bitrpc
{
    namespace client
    {

        // ============================================================
        // 对外接口
        // ============================================================

        // 创建主题
        bool TopicManager::create(const BaseConnection::ptr &conn, const std::string &key)
        {
            return commonRequest(conn, key, TopicOptype::TOPIC_CREATE);
        }

        // 删除主题
        bool TopicManager::remove(const BaseConnection::ptr &conn, const std::string &key)
        {
            return commonRequest(conn, key, TopicOptype::TOPIC_REMOVE);
        }

        // 订阅主题
        bool TopicManager::subscribe(const BaseConnection::ptr &conn,
                                     const std::string &key,
                                     const SubCallback &cb)
        {
            // 1. 先在本地保存回调，方便将来收到推送时能找到处理函数
            addSubscribe(key, cb);
            // 2. 向服务端发订阅请求
            bool ret = commonRequest(conn, key, TopicOptype::TOPIC_SUBSCRIBE);
            if (ret == false)
            {
                // 3. 订阅失败则回滚本地记录
                delSubscribe(key);
                return false;
            }
            return true;
        }

        // 取消订阅
        bool TopicManager::cancel(const BaseConnection::ptr &conn, const std::string &key)
        {
            // 1. 本地先删回调，后续不会再处理该主题推送
            delSubscribe(key);
            // 2. 通知服务端
            return commonRequest(conn, key, TopicOptype::TOPIC_CANCEL);
        }

        // 发布消息
        bool TopicManager::publish(const BaseConnection::ptr &conn,
                                   const std::string &key,
                                   const std::string &msg)
        {
            return commonRequest(conn, key, TopicOptype::TOPIC_PUBLISH, msg);
        }

        // 收到服务端推送的发布消息
        void TopicManager::onPublish(const BaseConnection::ptr &conn,
                                     const TopicRequest::ptr &msg)
        {
            // 1. 检查操作类型必须是 PUBLISH
            auto type = msg->optype();
            if (type != TopicOptype::TOPIC_PUBLISH)
            {
                ELOG("收到了错误类型的主题操作！");
                return;
            }
            // 2. 取出主题名和消息内容
            std::string topic_key = msg->topicKey();
            std::string topic_msg = msg->topicMsg();
            // 3. 查本地回调
            auto callback = getSubscribe(topic_key);
            if (!callback)
            {
                ELOG("收到了 %s 主题消息，但是该消息无主题处理回调！", topic_key.c_str());
                return;
            }
            // 4. 触发回调
            callback(topic_key, topic_msg);
        }

        // ============================================================
        // 私有方法
        // ============================================================

        // 本地新增订阅回调
        void TopicManager::addSubscribe(const std::string &key, const SubCallback &cb)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _topic_callbacks.insert(std::make_pair(key, cb));
        }

        // 本地删除订阅回调
        void TopicManager::delSubscribe(const std::string &key)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _topic_callbacks.erase(key);
        }

        // 本地查询订阅回调
        const TopicManager::SubCallback TopicManager::getSubscribe(const std::string &key)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _topic_callbacks.find(key);
            if (it == _topic_callbacks.end())
                return SubCallback();
            return it->second;
        }

        // 通用请求：把主题操作封装成一次请求-响应
        bool TopicManager::commonRequest(const BaseConnection::ptr &conn,
                                         const std::string &key,
                                         TopicOptype type,
                                         const std::string &msg)
        {
            // 1. 构造 TopicRequest
            auto req = std::make_shared<TopicRequest>();
            req->setId(uuid());
            req->setMType(MType::REQ_TOPIC);
            req->setOptype(type);
            req->setTopicKey(key);
            // 只有 PUBLISH 才需要携带消息内容
            if (type == TopicOptype::TOPIC_PUBLISH)
            {
                req->setTopicMsg(msg);
            }

            // 2. 通过 Requestor 同步发送，等待响应
            BaseMessage::ptr rsp;
            bool ret = _requestor->send(conn, req, rsp);
            if (ret == false)
            {
                ELOG("主题操作请求失败！");
                return false;
            }

            // 3. 校验响应
            auto topic_rsp = std::dynamic_pointer_cast<TopicResponse>(rsp);
            if (!topic_rsp)
            {
                ELOG("主题操作响应，向下类型转换失败！");
                return false;
            }
            if (topic_rsp->rcode() != RCode::RCODE_OK)
            {
                ELOG("主题操作请求出错：%s", errReason(topic_rsp->rcode()).c_str());
                return false;
            }
            return true;
        }

    } // namespace client
} // namespace bitrpc