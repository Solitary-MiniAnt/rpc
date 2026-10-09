#pragma once
#include "abstract.h"
#include "protocol.h"
#include "logger.h"
#include "fields.hpp"
#include <jsoncpp/json/json.h>
#include <vector>

namespace bitrpc
{

    // ============================================================
    // JsonMessage：所有 JSON 消息的基类
    // ============================================================
    class JsonMessage : public BaseMessage
    {
    public:
        using ptr = std::shared_ptr<JsonMessage>;
        virtual std::string serialize() override;
        virtual bool unserialize(const std::string &msg) override;

    protected:
        Json::Value _body; // 消息正文
    };

    // ============================================================
    // JsonRequest：JSON 请求基类（无额外逻辑）
    // ============================================================
    class JsonRequest : public JsonMessage
    {
    public:
        using ptr = std::shared_ptr<JsonRequest>;
        virtual bool check() override;
    };

    // ============================================================
    // JsonResponse：JSON 响应基类
    // ============================================================
    class JsonResponse : public JsonMessage
    {
    public:
        using ptr = std::shared_ptr<JsonResponse>;
        virtual bool check() override;
        virtual RCode rcode();                                                  // 读取响应码
        virtual void setRCode(RCode rcode);                                     // 设置响应码
        virtual Json::Value result() { return _body[KEY_RESULT]; }              // 读取结果
        virtual void setResult(const Json::Value &r) { _body[KEY_RESULT] = r; } // 设置结果
    };

    // ============================================================
    // RpcRequest：RPC 调用请求
    // ============================================================
    class RpcRequest : public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<RpcRequest>;

        // 校验：必须包含 method（字符串）和 params（对象）
        virtual bool check() override;

        std::string method();                      // 读取方法名
        void setMethod(const std::string &method); // 设置方法名
        Json::Value params();                      // 读取参数
        void setParams(const Json::Value &params); // 设置参数
    };

    // ============================================================
    // TopicRequest：发布订阅主题操作请求
    // ============================================================
    class TopicRequest : public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<TopicRequest>;

        // 校验：必须有 topic_key（字符串）和 optype（整数）
        // 如果是 PUBLISH 操作，还必须有 topic_msg（字符串）
        virtual bool check() override;

        std::string topicKey();                   // 读取主题名
        void setTopicKey(const std::string &key); // 设置主题名
        TopicOptype optype();                     // 读取操作类型
        void setOptype(TopicOptype optype);       // 设置操作类型
        std::string topicMsg();                   // 读取主题消息
        void setTopicMsg(const std::string &msg); // 设置主题消息
    };

    // ============================================================
    // ServiceRequest：服务注册/发现请求
    // ============================================================
    class ServiceRequest : public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<ServiceRequest>;

        // 校验：必须有 method（字符串）和 optype（整数）
        // 如果不是 DISCOVERY 操作，还必须有 host（对象，含 ip 和 port）
        virtual bool check() override;

        std::string method();                    // 读取方法名
        void setMethod(const std::string &name); // 设置方法名
        ServiceOptype optype();                  // 读取操作类型
        void setOptype(ServiceOptype optype);    // 设置操作类型
        Address host();                          // 读取主机地址
        void setHost(const Address &host);       // 设置主机地址
    };

    // ============================================================
    // RpcResponse：RPC 调用响应
    // ============================================================
    class RpcResponse : public JsonResponse
    {
    public:
        using ptr = std::shared_ptr<RpcResponse>;

        // 校验：必须有 rcode（整数）和 result
        virtual bool check() override;

        Json::Value result();                      // 读取调用结果
        void setResult(const Json::Value &result); // 设置调用结果
    };

    // ============================================================
    // TopicResponse：发布订阅操作响应（暂时无额外字段）
    // ============================================================
    class TopicResponse : public JsonResponse
    {
    public:
        using ptr = std::shared_ptr<TopicResponse>;
    };

    // ============================================================
    // ServiceResponse：服务操作响应
    // ============================================================
    class ServiceResponse : public JsonResponse
    {
    public:
        using ptr = std::shared_ptr<ServiceResponse>;

        // 校验：必须有 rcode 和 optype
        // 如果是 DISCOVERY，还要有 method（字符串）和 host（数组）
        virtual bool check() override;

        ServiceOptype optype();                    // 读取操作类型
        void setOptype(ServiceOptype optype);      // 设置操作类型
        std::string method();                      // 读取方法名
        void setMethod(const std::string &method); // 设置方法名
        void setHost(std::vector<Address> addrs);  // 设置主机地址列表
        std::vector<Address> hosts();              // 读取主机地址列表
    };

} // namespace bitrpc