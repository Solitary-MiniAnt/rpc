#include "rpc_router.h"
#include <arpa/inet.h>

namespace bitrpc
{
    namespace server
    {

        // ============================================================
        // ServiceDescribe
        // ============================================================

        // 校验请求参数：每个字段都必须存在且类型匹配
        bool ServiceDescribe::paramCheck(const Json::Value &params)
        {
            for (auto &desc : _params_desc)
            {
                // 字段必须存在
                if (!params.isMember(desc.first))
                {
                    ELOG("参数字段完整性校验失败！%s 字段缺失！", desc.first.c_str());
                    return false;
                }
                // 字段类型必须匹配
                if (!check(desc.second, params[desc.first]))
                {
                    ELOG("%s 参数类型校验失败！", desc.first.c_str());
                    return false;
                }
            }
            return true;
        }

        // 调用业务回调，并校验返回值类型
        bool ServiceDescribe::call(const Json::Value &params, Json::Value &result)
        {
            _callback(params, result);
            if (!rtypeCheck(result))
            {
                ELOG("回调处理函数中的响应信息校验失败！");
                return false;
            }
            return true;
        }

        // 校验返回值类型
        bool ServiceDescribe::rtypeCheck(const Json::Value &val)
        {
            return check(_return_type, val);
        }

        // 通用类型校验：根据 VType 判断 Json 值的类型
        bool ServiceDescribe::check(VType vtype, const Json::Value &val)
        {
            switch (vtype)
            {
            case VType::BOOL:
                return val.isBool();
            case VType::INTEGRAL:
                return val.isIntegral();
            case VType::NUMERIC:
                return val.isNumeric();
            case VType::STRING:
                return val.isString();
            case VType::ARRAY:
                return val.isArray();
            case VType::OBJECT:
                return val.isObject();
            }
            return false;
        }

        // ============================================================
        // ServiceManager
        // ============================================================

        void ServiceManager::insert(const ServiceDescribe::ptr &desc)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _services.insert(std::make_pair(desc->method(), desc));
        }

        ServiceDescribe::ptr ServiceManager::select(const std::string &method_name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _services.find(method_name);
            if (it == _services.end())
            {
                return ServiceDescribe::ptr();
            }
            return it->second;
        }

        void ServiceManager::remove(const std::string &method_name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _services.erase(method_name);
        }

        // ============================================================
        // RpcRouter
        // ============================================================

        RpcRouter::RpcRouter()
            : _service_manager(std::make_shared<ServiceManager>()) {}

        // 处理 RPC 请求：查服务 → 校验参数 → 调用业务 → 发送响应
        void RpcRouter::onRpcRequest(const BaseConnection::ptr &conn, RpcRequest::ptr &request)
        {
            // 1. 查询方法描述：服务端能否提供该服务
            auto service = _service_manager->select(request->method());
            if (service.get() == nullptr)
            {
                ELOG("%s 服务未找到！", request->method().c_str());
                response(conn, request, Json::Value(), RCode::RCODE_NOT_FOUND_SERVICE);
                return;
            }
            // 2. 参数校验
            if (!service->paramCheck(request->params()))
            {
                ELOG("%s 服务参数校验失败！", request->method().c_str());
                response(conn, request, Json::Value(), RCode::RCODE_INVALID_PARAMS);
                return;
            }
            // 3. 执行业务回调
            Json::Value result;
            if (!service->call(request->params(), result))
            {
                ELOG("%s 服务执行失败！", request->method().c_str());
                response(conn, request, Json::Value(), RCode::RCODE_INTERNAL_ERROR);
                return;
            }
            // 4. 组织并发送响应
            response(conn, request, result, RCode::RCODE_OK);
        }

        // 注册一个 RPC 方法
        void RpcRouter::registerMethod(const ServiceDescribe::ptr &service)
        {
            _service_manager->insert(service);
        }

        // 组织并发送响应
        void RpcRouter::response(const BaseConnection::ptr &conn,
                                 const RpcRequest::ptr &req,
                                 const Json::Value &res,
                                 RCode rcode)
        {
            auto msg = std::make_shared<RpcResponse>();
            msg->setId(req->rid());        // 响应 id 对应请求 id
            msg->setMType(MType::RSP_RPC); // 消息类型：RPC 响应
            msg->setRCode(rcode);          // 响应码
            msg->setResult(res);           // 结果
            conn->send(msg);
        }

    } // namespace server
} // namespace bitrpc