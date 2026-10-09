#include "requestor.h"

namespace bitrpc
{
    namespace client
    {

        // ============================================================
        // onResponse：收到响应时被调用
        // ============================================================
        void Requestor::onResponse(const BaseConnection::ptr &conn, BaseMessage::ptr &msg)
        {
            // 取出响应 id，找到对应的请求描述
            std::string rid = msg->rid();
            RequestDescribe::ptr rdp = getDescribe(rid);
            if (rdp.get() == nullptr)
            {
                ELOG("收到响应 - %s，但是未找到对应的请求描述！", rid.c_str());
                return;
            }

            // 根据请求类型决定如何处理：
            if (rdp->rtype == RType::REQ_ASYNC)
            {
                // 异步 future 方式：设置 promise 的值，唤醒等待的 get()
                rdp->response.set_value(msg);
            }
            else if (rdp->rtype == RType::REQ_CALLBACK)
            {
                // 回调方式：直接调用回调函数
                if (rdp->callback)
                    rdp->callback(msg);
            }
            else
            {
                ELOG("请求类型未知！！");
            }

            // 处理完毕，从映射表中删除该请求
            delDescribe(rid);
        }

        // ============================================================
        // send（异步 future）：发送请求，立刻返回 future
        // ============================================================
        bool Requestor::send(const BaseConnection::ptr &conn,
                             const BaseMessage::ptr &req,
                             AsyncResponse &async_rsp)
        {
            // 1. 创建请求描述，类型标记为异步
            RequestDescribe::ptr rdp = newDescribe(req, RType::REQ_ASYNC);
            if (rdp.get() == nullptr)
            {
                ELOG("构造请求描述对象失败！");
                return false;
            }
            // 2. 发送请求
            conn->send(req);
            // 3. 把 future 交出去（用户用 future.get() 等结果）
            async_rsp = rdp->response.get_future();
            return true;
        }

        // ============================================================
        // send（同步）：发送请求，阻塞等待响应
        // ============================================================
        bool Requestor::send(const BaseConnection::ptr &conn,
                             const BaseMessage::ptr &req,
                             BaseMessage::ptr &rsp)
        {
            // 内部先调异步版本拿到 future
            AsyncResponse rsp_future;
            bool ret = send(conn, req, rsp_future);
            if (ret == false)
                return false;
            // 然后立即 get() 阻塞等待结果——这就变成了同步
            rsp = rsp_future.get();
            return true;
        }

        // ============================================================
        // send（异步回调）：发送请求，响应到达时调用 cb
        // ============================================================
        bool Requestor::send(const BaseConnection::ptr &conn,
                             const BaseMessage::ptr &req,
                             const RequestCallback &cb)
        {
            // 1. 创建请求描述，类型标记为回调
            RequestDescribe::ptr rdp = newDescribe(req, RType::REQ_CALLBACK, cb);
            if (rdp.get() == nullptr)
            {
                ELOG("构造请求描述对象失败！");
                return false;
            }
            // 2. 发送请求（响应到达时，onResponse 会触发回调）
            conn->send(req);
            return true;
        }

        // ============================================================
        // 私有方法
        // ============================================================

        // 创建请求描述并存入映射表
        Requestor::RequestDescribe::ptr
        Requestor::newDescribe(const BaseMessage::ptr &req,
                               RType rtype,
                               const RequestCallback &cb)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            // 创建描述对象
            RequestDescribe::ptr rd = std::make_shared<RequestDescribe>();
            rd->request = req;
            rd->rtype = rtype;
            // 只有回调方式才需要保存回调
            if (rtype == RType::REQ_CALLBACK && cb)
            {
                rd->callback = cb;
            }
            // 用请求 id 作为 key，存入映射表
            _request_desc.insert(std::make_pair(req->rid(), rd));
            return rd;
        }

        // 按 id 查询请求描述
        Requestor::RequestDescribe::ptr
        Requestor::getDescribe(const std::string &rid)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _request_desc.find(rid);
            if (it == _request_desc.end())
                return RequestDescribe::ptr();
            return it->second;
        }

        // 按 id 删除请求描述
        void Requestor::delDescribe(const std::string &rid)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _request_desc.erase(rid);
        }

    } // namespace client
} // namespace bitrpc