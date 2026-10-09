#include "protocol.h"
#include <iostream>
#include <sstream>
#include <memory>
#include <unordered_map>

// ============================================================
// JSON 命名空间：JSON 序列化 / 反序列化
// ============================================================
namespace JSON
{

    // Json::Value → std::string
    bool serialize(const Json::Value &val, std::string &body)
    {
        std::stringstream ss;
        Json::StreamWriterBuilder swb;                                 // 工厂
        std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter()); // 生产 writer
        int ret = sw->write(val, &ss);                                 // 写入 ss
        if (ret != 0)
        {
            std::cout << "json serialize failed!\n";
            return false;
        }
        body = ss.str(); // 取出字符串
        return true;
    }

    // std::string → Json::Value
    bool unserialize(const std::string &body, Json::Value &val)
    {
        Json::CharReaderBuilder crb;                               // 工厂
        std::string errs;                                          // 保存错误信息
        std::unique_ptr<Json::CharReader> cr(crb.newCharReader()); // 生产 reader
        bool ret = cr->parse(body.c_str(), body.c_str() + body.size(), &val, &errs);
        if (ret == false)
        {
            std::cout << "json unserialize failed : " << errs << std::endl;
            return false;
        }
        return true;
    }

} // namespace JSON

// ============================================================
// bitrpc 命名空间：errReason 实现
// ============================================================
namespace bitrpc
{

    // 为 RCode 枚举提供哈希算法
    // std::unordered_map 默认不支持 enum class 作 key，必须自定义
    struct RCodeHash
    {
        std::size_t operator()(RCode code) const
        {
            return static_cast<std::size_t>(code);
        }
    };

    // 响应码转错误文本
    std::string errReason(RCode code)
    {
        // static：只初始化一次，之后复用，避免重复构造 map
        static std::unordered_map<RCode, std::string, RCodeHash> err_map = {
            {RCode::RCODE_OK, "成功处理！"},
            {RCode::RCODE_PARSE_FAILED, "消息解析失败！"},
            {RCode::RCODE_ERROR_MSGTYPE, "消息类型错误！"},
            {RCode::RCODE_INVALID_MSG, "无效消息"},
            {RCode::RCODE_DISCONNECTED, "连接已断开！"},
            {RCode::RCODE_INVALID_PARAMS, "无效的Rpc参数！"},
            {RCode::RCODE_NOT_FOUND_SERVICE, "没有找到对应的服务！"},
            {RCode::RCODE_INVALID_OPTYPE, "无效的操作类型"},
            {RCode::RCODE_NOT_FOUND_TOPIC, "没有找到对应的主题！"},
            {RCode::RCODE_INTERNAL_ERROR, "内部错误！"}};
        auto it = err_map.find(code);
        if (it == err_map.end())
            return "未知错误！";
        return it->second;
    }

} // namespace bitrpc