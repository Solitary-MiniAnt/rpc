#pragma once

// ---------- 本项目其他模块 ----------
#include "abstract.h" // BaseConnection：连接对象
#include "message.h"  // RpcRequest、RpcResponse
#include "fields.hpp" // MType、RCode
#include "logger.h"   // ELOG

// ---------- 标准库 ----------
#include <jsoncpp/json/json.h> // Json::Value
#include <functional>          // std::function
#include <memory>              // shared_ptr
#include <mutex>               // std::mutex
#include <unordered_map>       // unordered_map
#include <vector>              // vector
#include <string>              // string
#include <utility>             // std::pair

namespace bitrpc
{
    namespace server
    {

        // ============================================================
        // VType：参数/返回值的类型枚举
        // 用于描述 RPC 方法的参数类型和返回值类型
        // ============================================================
        enum class VType
        {
            BOOL = 0, // 布尔
            INTEGRAL, // 整数
            NUMERIC,  // 数字（整数或浮点）
            STRING,   // 字符串
            ARRAY,    // 数组
            OBJECT,   // 对象
        };

        // ============================================================
        // ServiceDescribe：单个 RPC 服务的描述信息
        // 包含：方法名、业务回调、参数描述、返回值类型
        // 作用：既能调用业务逻辑，又能校验参数和返回值类型
        // ============================================================
        class ServiceDescribe
        {
        public:
            using ptr = std::shared_ptr<ServiceDescribe>;
            // 业务回调：接收参数对象，输出结果对象
            using ServiceCallback = std::function<void(const Json::Value &, Json::Value &)>;
            // 参数描述：字段名 + 期望类型
            using ParamsDescribe = std::pair<std::string, VType>;

            // 构造：方法名、参数描述列表、返回值类型、业务回调
            ServiceDescribe(std::string &&mname,
                            std::vector<ParamsDescribe> &&desc,
                            VType vtype,
                            ServiceCallback &&handler)
                : _method_name(std::move(mname)),
                  _callback(std::move(handler)),
                  _params_desc(std::move(desc)),
                  _return_type(vtype) {}

            // 返回方法名
            const std::string &method() { return _method_name; }

            // 校验请求中的参数是否符合描述
            bool paramCheck(const Json::Value &params);

            // 调用业务回调，并校验返回值类型
            bool call(const Json::Value &params, Json::Value &result);

        private:
            // 校验返回值类型
            bool rtypeCheck(const Json::Value &val);

            // 通用类型校验：判断 val 是否是 vtype 描述的类型
            bool check(VType vtype, const Json::Value &val);

        private:
            std::string _method_name;                 // 方法名
            ServiceCallback _callback;                // 业务回调
            std::vector<ParamsDescribe> _params_desc; // 参数字段描述
            VType _return_type;                       // 返回值类型描述
        };

        // ============================================================
        // SDescribeFactory：ServiceDescribe 的建造者
        // 作用：让用户一步步设置方法名、参数、返回值、回调，
        //       最后 build() 出 ServiceDescribe 对象。
        // ============================================================
        class SDescribeFactory
        {
        public:
            void setMethodName(const std::string &name) { _method_name = name; }
            void setReturnType(VType vtype) { _return_type = vtype; }
            void setParamsDesc(const std::string &pname, VType vtype)
            {
                _params_desc.push_back(ServiceDescribe::ParamsDescribe(pname, vtype));
            }
            void setCallback(const ServiceDescribe::ServiceCallback &cb) { _callback = cb; }

            // 生成 ServiceDescribe 对象
            ServiceDescribe::ptr build()
            {
                return std::make_shared<ServiceDescribe>(
                    std::move(_method_name),
                    std::move(_params_desc),
                    _return_type,
                    std::move(_callback));
            }

        private:
            std::string _method_name;
            ServiceDescribe::ServiceCallback _callback;
            std::vector<ServiceDescribe::ParamsDescribe> _params_desc;
            VType _return_type;
        };

        // ============================================================
        // ServiceManager：服务管理器
        // 作用：维护“方法名 -> ServiceDescribe”的映射表
        // 用法：RpcRouter 通过它查询/注册/删除服务
        // ============================================================
        class ServiceManager
        {
        public:
            using ptr = std::shared_ptr<ServiceManager>;

            // 注册一个服务
            void insert(const ServiceDescribe::ptr &desc);

            // 按方法名查询服务，找不到返回空指针
            ServiceDescribe::ptr select(const std::string &method_name);

            // 按方法名删除服务
            void remove(const std::string &method_name);

        private:
            std::mutex _mutex;                                               // 保护 _services
            std::unordered_map<std::string, ServiceDescribe::ptr> _services; // 方法名 -> 服务描述
        };

        // ============================================================
        // RpcRouter：RPC 路由器
        // 作用：作为 Dispatcher 中 REQ_RPC 类型的处理回调，
        //       负责查服务 → 校验参数 → 调用业务 → 组织响应。
        // 用法：
        //   router.onRpcRequest 注册到 Dispatcher；
        //   router.registerMethod 注册新的 RPC 方法。
        // ============================================================
        class RpcRouter
        {
        public:
            using ptr = std::shared_ptr<RpcRouter>;

            RpcRouter();

            // 处理 RPC 请求（注册到 Dispatcher 的回调）
            void onRpcRequest(const BaseConnection::ptr &conn, RpcRequest::ptr &request);

            // 注册一个 RPC 方法
            void registerMethod(const ServiceDescribe::ptr &service);

        private:
            // 组织并发送响应
            void response(const BaseConnection::ptr &conn,
                          const RpcRequest::ptr &req,
                          const Json::Value &res,
                          RCode rcode);

        private:
            ServiceManager::ptr _service_manager; // 服务管理器
        };

    } // namespace server
} // namespace bitrpc