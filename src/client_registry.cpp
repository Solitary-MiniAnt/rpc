#include "client_registry.h"

namespace bitrpc
{
    namespace client
    {

        // ============================================================
        // Provider
        // ============================================================

        // 向注册中心注册一个方法
        bool Provider::registryMethod(const BaseConnection::ptr &conn,
                                      const std::string &method,
                                      const Address &host)
        {
            // 1. 构造注册请求
            auto req = std::make_shared<ServiceRequest>();
            req->setId(uuid());
            req->setMType(MType::REQ_SERVICE);
            req->setMethod(method);
            req->setHost(host);
            req->setOptype(ServiceOptype::SERVICE_REGISTRY);

            // 2. 同步发送请求，等待响应
            BaseMessage::ptr rsp;
            bool ret = _requestor->send(conn, req, rsp);
            if (ret == false)
            {
                ELOG("%s 服务注册失败！", method.c_str());
                return false;
            }

            // 3. 校验响应类型
            auto service_rsp = std::dynamic_pointer_cast<ServiceResponse>(rsp);
            if (service_rsp.get() == nullptr)
            {
                ELOG("响应类型向下转换失败！");
                return false;
            }
            // 4. 校验响应码
            if (service_rsp->rcode() != RCode::RCODE_OK)
            {
                ELOG("服务注册失败，原因：%s", errReason(service_rsp->rcode()).c_str());
                return false;
            }
            return true;
        }

        // ============================================================
        // MethodHost
        // ============================================================

        MethodHost::MethodHost() : _idx(0) {}

        MethodHost::MethodHost(const std::vector<Address> &hosts)
            : _hosts(hosts.begin(), hosts.end()), _idx(0) {}

        // 新增一个主机（收到上线通知）
        void MethodHost::appendHost(const Address &host)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _hosts.push_back(host);
        }

        // 删除一个主机（收到下线通知）
        void MethodHost::removeHost(const Address &host)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            for (auto it = _hosts.begin(); it != _hosts.end(); ++it)
            {
                if (*it == host)
                {
                    _hosts.erase(it);
                    break;
                }
            }
        }

        // 轮询选一个主机（负载均衡）
        Address MethodHost::chooseHost()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            size_t pos = _idx++ % _hosts.size();
            return _hosts[pos];
        }

        // 是否为空
        bool MethodHost::empty()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            return _hosts.empty();
        }

        // ============================================================
        // Discoverer
        // ============================================================

        // 服务发现：优先查本地缓存，缓存没有则向注册中心查
        bool Discoverer::serviceDiscovery(const BaseConnection::ptr &conn,
                                          const std::string &method,
                                          Address &host)
        {
            // 1. 先查本地缓存
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _method_hosts.find(method);
                if (it != _method_hosts.end())
                {
                    if (it->second->empty() == false)
                    {
                        host = it->second->chooseHost();
                        return true;
                    }
                }
            }

            // 2. 本地没有缓存，向注册中心查
            auto req = std::make_shared<ServiceRequest>();
            req->setId(uuid());
            req->setMType(MType::REQ_SERVICE);
            req->setMethod(method);
            req->setOptype(ServiceOptype::SERVICE_DISCOVERY);

            BaseMessage::ptr rsp;
            bool ret = _requestor->send(conn, req, rsp);
            if (ret == false)
            {
                ELOG("服务发现失败！");
                return false;
            }

            // 3. 校验响应
            auto service_rsp = std::dynamic_pointer_cast<ServiceResponse>(rsp);
            if (!service_rsp)
            {
                ELOG("服务发现失败！响应类型转换失败！");
                return false;
            }
            if (service_rsp->rcode() != RCode::RCODE_OK)
            {
                ELOG("服务发现失败！%s", errReason(service_rsp->rcode()).c_str());
                return false;
            }

            // 4. 把查询到的主机列表存入本地缓存
            std::unique_lock<std::mutex> lock(_mutex);
            auto method_host = std::make_shared<MethodHost>(service_rsp->hosts());
            if (method_host->empty())
            {
                ELOG("%s 服务发现失败！没有能够提供服务的主机！", method.c_str());
                return false;
            }
            host = method_host->chooseHost();
            _method_hosts[method] = method_host;
            return true;
        }

        // 处理服务上线/下线通知
        void Discoverer::onServiceRequest(const BaseConnection::ptr &conn,
                                          const ServiceRequest::ptr &msg)
        {
            auto optype = msg->optype();
            std::string method = msg->method();

            std::unique_lock<std::mutex> lock(_mutex);

            if (optype == ServiceOptype::SERVICE_ONLINE)
            {
                // 服务上线：往本地缓存里加一个主机
                auto it = _method_hosts.find(method);
                if (it == _method_hosts.end())
                {
                    // 首次出现这个方法，创建一个新的 MethodHost
                    auto method_host = std::make_shared<MethodHost>();
                    method_host->appendHost(msg->host());
                    _method_hosts[method] = method_host;
                }
                else
                {
                    // 已有记录，直接追加主机
                    it->second->appendHost(msg->host());
                }
            }
            else if (optype == ServiceOptype::SERVICE_OFFLINE)
            {
                // 服务下线：从本地缓存里移除主机
                auto it = _method_hosts.find(method);
                if (it == _method_hosts.end())
                    return;
                it->second->removeHost(msg->host());
                // 通知上层：某主机下线了
                _offline_callback(msg->host());
            }
        }

    } // namespace client
} // namespace bitrpc