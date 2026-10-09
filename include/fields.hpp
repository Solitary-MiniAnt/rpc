#pragma once
#include <string>
#include <utility> // std::pair
#include <cstddef> // size_t

namespace bitrpc
{

// ================== 请求字段宏定义 ==================
// 宏不受命名空间影响，放在这里只是为了整齐
#define KEY_METHOD "method"       // 方法名
#define KEY_PARAMS "parameters"   // 方法参数
#define KEY_TOPIC_KEY "topic_key" // 主题名
#define KEY_TOPIC_MSG "topic_msg" // 主题消息内容
#define KEY_OPTYPE "optype"       // 操作类型
#define KEY_HOST "host"           // 主机信息（对象）
#define KEY_HOST_IP "ip"          // 主机 IP
#define KEY_HOST_PORT "port"      // 主机端口
#define KEY_RCODE "rcode"         // 响应码
#define KEY_RESULT "result"       // 调用结果

// 补充字段（代码里用到）
#define KEY_TYPE "type"       // 消息类型
#define KEY_ID "id"           // 消息 id
#define KEY_SERVICE "service" // 服务名
#define KEY_ERROR "error"     // 错误描述

    // ================== 消息类型 ==================
    // 区分请求/响应的大类，决定 Dispatcher 分发给哪个业务模块
    enum class MType
    {
        REQ_RPC = 0, // RPC 请求
        RSP_RPC,     // RPC 响应
        REQ_TOPIC,   // 主题操作请求
        RSP_TOPIC,   // 主题操作响应
        REQ_SERVICE, // 服务操作请求
        RSP_SERVICE  // 服务操作响应
    };

    // ================== 响应码 ==================
    // 标记一次操作的结果，OK 表示成功，其余表示各种失败原因
    enum class RCode
    {
        RCODE_OK = 0,            // 成功
        RCODE_PARSE_FAILED,      // 消息解析失败
        RCODE_ERROR_MSGTYPE,     // 消息类型错误
        RCODE_INVALID_MSG,       // 无效消息
        RCODE_DISCONNECTED,      // 连接已断开
        RCODE_INVALID_PARAMS,    // 无效的 RPC 参数
        RCODE_NOT_FOUND_SERVICE, // 服务未找到
        RCODE_INVALID_OPTYPE,    // 无效的操作类型
        RCODE_NOT_FOUND_TOPIC,   // 主题未找到
        RCODE_INTERNAL_ERROR     // 内部错误
    };

    // 根据响应码返回人类可读的错误描述（实现在 protocol.cpp）
    std::string errReason(RCode code);

    // ================== RPC 请求类型 ==================
    // 客户端发起请求时，标记它想要哪种响应方式
    enum class RType
    {
        REQ_ASYNC = 0, // 异步（同步本质是拿到 future 后立即 get）
        REQ_CALLBACK   // 回调
    };

    // ================== 主题操作类型 ==================
    enum class TopicOptype
    {
        TOPIC_CREATE = 0, // 创建主题
        TOPIC_REMOVE,     // 删除主题
        TOPIC_SUBSCRIBE,  // 订阅主题
        TOPIC_CANCEL,     // 取消订阅
        TOPIC_PUBLISH     // 发布消息
    };

    // ================== 服务操作类型 ==================
    enum class ServiceOptype
    {
        SERVICE_REGISTRY = 0, // 服务注册
        SERVICE_DISCOVERY,    // 服务发现
        SERVICE_ONLINE,       // 服务上线通知
        SERVICE_OFFLINE,      // 服务下线通知
        SERVICE_UNKNOW        // 未知操作（保留文档原拼写）
    };

    // ================== 网络地址 ==================
    // first = IP，second = 端口
    using Address = std::pair<std::string, int>;

    // ================== 通信缓冲区上限 ==================
    // 单个连接缓冲区最大字节数（64KB），超过则断开，防止恶意攻击
    static constexpr size_t RPC_MAX_BUFFER_SIZE = (1 << 16);

} // namespace bitrpc