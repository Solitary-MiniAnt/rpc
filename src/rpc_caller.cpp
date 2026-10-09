#include "rpc_caller.h"
#include "uuid.h" // uuid()

namespace bitrpc
{
    namespace client
    {

        // ============================================================
        // 方式1：同步调用
        // ============================================================
        bool RpcCaller::call(const BaseConnection::ptr &conn,
                             const std::string &method,
                             const Json::Value &params,
                             Json::Value &result)
        {
            DLOG("开始同步 RPC 调用...");

            // 1. 组织 RpcRequest
            auto req_msg = std::make_shared<RpcRequest>();
            req_msg->setId(uuid());
            req_msg->setMType(MType::REQ_RPC);
            req_msg->setMethod(method);
            req_msg->setParams(params);

            // 2. 通过 Requestor 发送请求（同步方式会阻塞等待响应）
            BaseMessage::ptr rsp_msg;
            bool ret = _requestor->send(conn,
                                        std::dynamic_pointer_cast<BaseMessage>(req_msg),
                                        rsp_msg);
            if (ret == false)
            {
                ELOG("同步 RPC 请求失败！");
                return false;
            }
            DLOG("收到响应，进行解析，获取结果！");

            // 3. 把响应转成 RpcResponse，检查响应码
            auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(rsp_msg);
            if (!rpc_rsp_msg)
            {
                ELOG("RPC 响应，向下类型转换失败！");
                return false;
            }
            if (rpc_rsp_msg->rcode() != RCode::RCODE_OK)
            {
                ELOG("RPC 请求出错：%s", errReason(rpc_rsp_msg->rcode()).c_str());
                return false;
            }
            // 4. 取出 result 返回
            result = rpc_rsp_msg->result();
            DLOG("结果设置完毕！");
            return true;
        }

        // ============================================================
        // 方式2：异步 future
        // ============================================================
        bool RpcCaller::call(const BaseConnection::ptr &conn,
                             const std::string &method,
                             const Json::Value &params,
                             JsonAsyncResponse &result)
        {
            // 1. 组织 RpcRequest
            auto req_msg = std::make_shared<RpcRequest>();
            req_msg->setId(uuid());
            req_msg->setMType(MType::REQ_RPC);
            req_msg->setMethod(method);
            req_msg->setParams(params);

            // 2. 创建一个 promise，用来在响应到达时设置结果
            auto json_promise = std::make_shared<std::promise<Json::Value>>();
            result = json_promise->get_future();

            // 3. 把 promise 包进 Requestor 的回调里，响应到达时由 Callback 设置 promise
            Requestor::RequestCallback cb = std::bind(&RpcCaller::Callback,
                                                      this,
                                                      json_promise,
                                                      std::placeholders::_1);

            // 4. 发送请求（不阻塞，立即返回）
            bool ret = _requestor->send(conn,
                                        std::dynamic_pointer_cast<BaseMessage>(req_msg),
                                        cb);
            if (ret == false)
            {
                ELOG("异步 RPC 请求失败！");
                return false;
            }
            return true;
        }

        // ============================================================
        // 方式3：异步回调
        // ============================================================
        bool RpcCaller::call(const BaseConnection::ptr &conn,
                             const std::string &method,
                             const Json::Value &params,
                             const JsonResponseCallback &cb)
        {
            // 1. 组织 RpcRequest
            auto req_msg = std::make_shared<RpcRequest>();
            req_msg->setId(uuid());
            req_msg->setMType(MType::REQ_RPC);
            req_msg->setMethod(method);
            req_msg->setParams(params);

            // 2. 把用户回调包进 Callback1 里，响应到达时由 Callback1 调用用户回调
            Requestor::RequestCallback req_cb = std::bind(&RpcCaller::Callback1,
                                                          this,
                                                          cb,
                                                          std::placeholders::_1);

            // 3. 发送请求（不阻塞）
            bool ret = _requestor->send(conn,
                                        std::dynamic_pointer_cast<BaseMessage>(req_msg),
                                        req_cb);
            if (ret == false)
            {
                ELOG("回调 RPC 请求失败！");
                return false;
            }
            return true;
        }

        // ============================================================
        // 私有方法
        // ============================================================

        // 异步回调的内部包装：响应到达后，校验 rcode 并调用用户回调
        void RpcCaller::Callback1(const JsonResponseCallback &cb,
                                  const BaseMessage::ptr &msg)
        {
            // 1. 把 BaseMessage 转成 RpcResponse
            auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(msg);
            if (!rpc_rsp_msg)
            {
                ELOG("RPC 响应，向下类型转换失败！");
                return;
            }
            // 2. 检查响应码
            if (rpc_rsp_msg->rcode() != RCode::RCODE_OK)
            {
                ELOG("RPC 回调请求出错：%s", errReason(rpc_rsp_msg->rcode()).c_str());
                return;
            }
            // 3. 把 result 交给用户回调
            cb(rpc_rsp_msg->result());
        }

        // 异步 future 的内部包装：响应到达后，校验 rcode 并设置 promise
        void RpcCaller::Callback(std::shared_ptr<std::promise<Json::Value>> result,
                                 const BaseMessage::ptr &msg)
        {
            // 1. 把 BaseMessage 转成 RpcResponse
            auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(msg);
            if (!rpc_rsp_msg)
            {
                ELOG("RPC 响应，向下类型转换失败！");
                return;
            }
            // 2. 检查响应码
            if (rpc_rsp_msg->rcode() != RCode::RCODE_OK)
            {
                ELOG("RPC 异步请求出错：%s", errReason(rpc_rsp_msg->rcode()).c_str());
                return;
            }
            // 3. 设置 promise 的值，唤醒等待的 future.get()
            result->set_value(rpc_rsp_msg->result());
        }

    } // namespace client
} // namespace bitrpc