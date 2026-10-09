#pragma once
#include "message.h"

namespace bitrpc
{

    // 根据消息类型创建对应的消息对象
    class MessageFactory
    {
    public:
        static BaseMessage::ptr create(MType mtype)
        {
            switch (mtype)
            {
            case MType::REQ_RPC:
                return std::make_shared<RpcRequest>();
            case MType::RSP_RPC:
                return std::make_shared<RpcResponse>();
            case MType::REQ_TOPIC:
                return std::make_shared<TopicRequest>();
            case MType::RSP_TOPIC:
                return std::make_shared<TopicResponse>();
            case MType::REQ_SERVICE:
                return std::make_shared<ServiceRequest>();
            case MType::RSP_SERVICE:
                return std::make_shared<ServiceResponse>();
            }
            return BaseMessage::ptr();
        }
    };

} // namespace bitrpc