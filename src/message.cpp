#include "message.h"

namespace bitrpc
{

    // ============================================================
    // JsonMessage
    // ============================================================
    std::string JsonMessage::serialize()
    {
        std::string body;
        if (!JSON::serialize(_body, body))
        {
            return std::string();
        }
        return body;
    }

    bool JsonMessage::unserialize(const std::string &msg)
    {
        return JSON::unserialize(msg, _body);
    }

    // ============================================================
    // JsonRequest / JsonResponse
    // ============================================================
    bool JsonRequest::check()
    {
        // 默认只校验 method 字段
        if (_body[KEY_METHOD].isNull() || !_body[KEY_METHOD].isString())
        {
            ELOG("请求中没有方法名或方法名类型错误！");
            return false;
        }
        return true;
    }

    bool JsonResponse::check()
    {
        // 默认只校验 rcode 字段
        if (_body[KEY_RCODE].isNull() || !_body[KEY_RCODE].isIntegral())
        {
            ELOG("响应中没有响应状态码或状态码类型错误！");
            return false;
        }
        return true;
    }

    RCode JsonResponse::rcode()
    {
        return (RCode)_body[KEY_RCODE].asInt();
    }

    void JsonResponse::setRCode(RCode rcode)
    {
        _body[KEY_RCODE] = (int)rcode;
    }

    // ============================================================
    // RpcRequest
    // ============================================================
    bool RpcRequest::check()
    {
        // method 必须是字符串
        if (_body[KEY_METHOD].isNull() || !_body[KEY_METHOD].isString())
        {
            ELOG("RPC请求中没有方法名称或方法名称类型错误！");
            return false;
        }
        // params 必须是对象
        if (_body[KEY_PARAMS].isNull() || !_body[KEY_PARAMS].isObject())
        {
            ELOG("RPC请求中没有参数信息或参数信息类型错误！");
            return false;
        }
        return true;
    }

    std::string RpcRequest::method() { return _body[KEY_METHOD].asString(); }
    void RpcRequest::setMethod(const std::string &method) { _body[KEY_METHOD] = method; }
    Json::Value RpcRequest::params() { return _body[KEY_PARAMS]; }
    void RpcRequest::setParams(const Json::Value &params) { _body[KEY_PARAMS] = params; }

    // ============================================================
    // TopicRequest
    // ============================================================
    bool TopicRequest::check()
    {
        // topic_key 必须是字符串
        if (_body[KEY_TOPIC_KEY].isNull() || !_body[KEY_TOPIC_KEY].isString())
        {
            ELOG("主题请求中没有主题名称或主题名称类型错误！");
            return false;
        }
        // optype 必须是整数
        if (_body[KEY_OPTYPE].isNull() || !_body[KEY_OPTYPE].isIntegral())
        {
            ELOG("主题请求中没有操作类型或操作类型类型错误！");
            return false;
        }
        // 如果是 PUBLISH，必须有 topic_msg
        if (_body[KEY_OPTYPE].asInt() == (int)TopicOptype::TOPIC_PUBLISH &&
            (_body[KEY_TOPIC_MSG].isNull() || !_body[KEY_TOPIC_MSG].isString()))
        {
            ELOG("主题消息发布请求中没有消息内容字段或消息内容类型错误！");
            return false;
        }
        return true;
    }

    std::string TopicRequest::topicKey() { return _body[KEY_TOPIC_KEY].asString(); }
    void TopicRequest::setTopicKey(const std::string &key) { _body[KEY_TOPIC_KEY] = key; }
    TopicOptype TopicRequest::optype() { return (TopicOptype)_body[KEY_OPTYPE].asInt(); }
    void TopicRequest::setOptype(TopicOptype optype) { _body[KEY_OPTYPE] = (int)optype; }
    std::string TopicRequest::topicMsg() { return _body[KEY_TOPIC_MSG].asString(); }
    void TopicRequest::setTopicMsg(const std::string &msg) { _body[KEY_TOPIC_MSG] = msg; }

    // ============================================================
    // ServiceRequest
    // ============================================================
    bool ServiceRequest::check()
    {
        // method 必须是字符串
        if (_body[KEY_METHOD].isNull() || !_body[KEY_METHOD].isString())
        {
            ELOG("服务请求中没有方法名称或方法名称类型错误！");
            return false;
        }
        // optype 必须是整数
        if (_body[KEY_OPTYPE].isNull() || !_body[KEY_OPTYPE].isIntegral())
        {
            ELOG("服务请求中没有操作类型或操作类型类型错误！");
            return false;
        }
        // 如果不是 DISCOVERY，必须有 host 对象，且含 ip 和 port
        if (_body[KEY_OPTYPE].asInt() != (int)ServiceOptype::SERVICE_DISCOVERY &&
            (_body[KEY_HOST].isNull() || !_body[KEY_HOST].isObject() ||
             _body[KEY_HOST][KEY_HOST_IP].isNull() || !_body[KEY_HOST][KEY_HOST_IP].isString() ||
             _body[KEY_HOST][KEY_HOST_PORT].isNull() || !_body[KEY_HOST][KEY_HOST_PORT].isIntegral()))
        {
            ELOG("服务请求中主机地址信息错误！");
            return false;
        }
        return true;
    }

    std::string ServiceRequest::method() { return _body[KEY_METHOD].asString(); }
    void ServiceRequest::setMethod(const std::string &name) { _body[KEY_METHOD] = name; }
    ServiceOptype ServiceRequest::optype() { return (ServiceOptype)_body[KEY_OPTYPE].asInt(); }
    void ServiceRequest::setOptype(ServiceOptype optype) { _body[KEY_OPTYPE] = (int)optype; }

    Address ServiceRequest::host()
    {
        Address addr;
        addr.first = _body[KEY_HOST][KEY_HOST_IP].asString();
        addr.second = _body[KEY_HOST][KEY_HOST_PORT].asInt();
        return addr;
    }

    void ServiceRequest::setHost(const Address &host)
    {
        Json::Value val;
        val[KEY_HOST_IP] = host.first;
        val[KEY_HOST_PORT] = host.second;
        _body[KEY_HOST] = val;
    }

    // ============================================================
    // RpcResponse
    // ============================================================
    bool RpcResponse::check()
    {
        if (_body[KEY_RCODE].isNull() || !_body[KEY_RCODE].isIntegral())
        {
            ELOG("响应中没有响应状态码或状态码类型错误！");
            return false;
        }
        if (_body[KEY_RESULT].isNull())
        {
            ELOG("响应中没有RPC调用结果或结果类型错误！");
            return false;
        }
        return true;
    }

    Json::Value RpcResponse::result() { return _body[KEY_RESULT]; }
    void RpcResponse::setResult(const Json::Value &result) { _body[KEY_RESULT] = result; }

    // ============================================================
    // ServiceResponse
    // ============================================================
    bool ServiceResponse::check()
    {
        if (_body[KEY_RCODE].isNull() || !_body[KEY_RCODE].isIntegral())
        {
            ELOG("响应中没有响应状态码或状态码类型错误！");
            return false;
        }
        if (_body[KEY_OPTYPE].isNull() || !_body[KEY_OPTYPE].isIntegral())
        {
            ELOG("响应中没有操作类型或操作类型类型错误！");
            return false;
        }
        // 如果是 DISCOVERY，必须有 method 和 host（数组）
        if (_body[KEY_OPTYPE].asInt() == (int)ServiceOptype::SERVICE_DISCOVERY &&
            (_body[KEY_METHOD].isNull() || !_body[KEY_METHOD].isString() ||
             _body[KEY_HOST].isNull() || !_body[KEY_HOST].isArray()))
        {
            ELOG("服务发现响应中响应信息字段错误！");
            return false;
        }
        return true;
    }

    ServiceOptype ServiceResponse::optype() { return (ServiceOptype)_body[KEY_OPTYPE].asInt(); }
    void ServiceResponse::setOptype(ServiceOptype optype) { _body[KEY_OPTYPE] = (int)optype; }
    std::string ServiceResponse::method() { return _body[KEY_METHOD].asString(); }
    void ServiceResponse::setMethod(const std::string &method) { _body[KEY_METHOD] = method; }

    void ServiceResponse::setHost(std::vector<Address> addrs)
    {
        for (auto &addr : addrs)
        {
            Json::Value val;
            val[KEY_HOST_IP] = addr.first;
            val[KEY_HOST_PORT] = addr.second;
            _body[KEY_HOST].append(val);
        }
    }

    std::vector<Address> ServiceResponse::hosts()
    {
        std::vector<Address> addrs;
        int sz = _body[KEY_HOST].size();
        for (int i = 0; i < sz; i++)
        {
            Address addr;
            addr.first = _body[KEY_HOST][i][KEY_HOST_IP].asString();
            addr.second = _body[KEY_HOST][i][KEY_HOST_PORT].asInt();
            addrs.push_back(addr);
        }
        return addrs;
    }

} // namespace bitrpc