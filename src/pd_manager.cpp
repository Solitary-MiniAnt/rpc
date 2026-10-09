#include "pd_manager.h"

namespace bitrpc
{
    namespace server
    {

        // ============================================================
        // Provider
        // ============================================================
        void Provider::appendMethod(const std::string &method)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            methods.emplace_back(method);
        }

        // ============================================================
        // Discoverer
        // ============================================================
        void Discoverer::appendMethod(const std::string &method)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            methods.push_back(method);
        }

        // ============================================================
        // ProviderManager
        // ============================================================

        // 新增服务提供者（注册时调用）
        void ProviderManager::addProvider(const BaseConnection::ptr &c, const Address &h, const std::string &method)
        {
            Provider::ptr provider;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                // 该连接第一次注册？已经注册过别的？
                auto it = _conns.find(c);
                if (it != _conns.end())
                {
                    provider = it->second;
                }
                else
                {
                    provider = std::make_shared<Provider>(c, h);
                    _conns.insert(std::make_pair(c, provider));
                }
                // 把该提供者加入“这个方法对应的提供者集合”
                auto &providers = _providers[method];
                providers.insert(provider);
            }
            // 提供者对象也记录一份方法名
            provider->appendMethod(method);
        }

        // 按连接查提供者
        Provider::ptr ProviderManager::getProvider(const BaseConnection::ptr &c)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _conns.find(c);
            if (it != _conns.end())
                return it->second;
            return Provider::ptr();
        }

        // 删除提供者（连接断开时调用）
        void ProviderManager::delProvider(const BaseConnection::ptr &c)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _conns.find(c);
            if (it == _conns.end())
                return; // 该连接不是提供者

            // 从“方法 -> 提供者集合”里挨个删除
            for (auto &method : it->second->methods)
            {
                auto &providers = _providers[method];
                providers.erase(it->second);
            }
            // 删除连接与提供者的关联
            _conns.erase(it);
        }

        // 查询某个方法的所有主机地址
        std::vector<Address> ProviderManager::methodHosts(const std::string &method)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _providers.find(method);
            if (it == _providers.end())
                return std::vector<Address>();

            std::vector<Address> result;
            for (auto &provider : it->second)
            {
                result.push_back(provider->host);
            }
            return result;
        }

        // ============================================================
        // DiscovererManager
        // ============================================================

        // 新增发现者
        Discoverer::ptr DiscovererManager::addDiscoverer(const BaseConnection::ptr &c, const std::string &method)
        {
            Discoverer::ptr discoverer;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(c);
                if (it != _conns.end())
                {
                    discoverer = it->second;
                }
                else
                {
                    discoverer = std::make_shared<Discoverer>(c);
                    _conns.insert(std::make_pair(c, discoverer));
                }
                auto &discoverers = _discoverers[method];
                discoverers.insert(discoverer);
            }
            discoverer->appendMethod(method);
            return discoverer;
        }

        // 删除发现者
        void DiscovererManager::delDiscoverer(const BaseConnection::ptr &c)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _conns.find(c);
            if (it == _conns.end())
                return; // 该连接不是发现者

            for (auto &method : it->second->methods)
            {
                auto &discoverers = _discoverers[method];
                discoverers.erase(it->second);
            }
            _conns.erase(it);
        }

        // 服务上线通知
        void DiscovererManager::onlineNotify(const std::string &method, const Address &host)
        {
            notify(method, host, ServiceOptype::SERVICE_ONLINE);
        }

        // 服务下线通知
        void DiscovererManager::offlineNotify(const std::string &method, const Address &host)
        {
            notify(method, host, ServiceOptype::SERVICE_OFFLINE);
        }

        // 统一通知逻辑
        void DiscovererManager::notify(const std::string &method, const Address &host, ServiceOptype optype)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _discoverers.find(method);
            if (it == _discoverers.end())
                return; // 没有发现者关注这个方法

            // 构造一条通知消息（复用 ServiceRequest 结构）
            auto msg = std::make_shared<ServiceRequest>();
            msg->setId(uuid());
            msg->setMType(MType::REQ_SERVICE);
            msg->setMethod(method);
            msg->setHost(host);
            msg->setOptype(optype);

            // 推给所有关注该方法的发现者
            for (auto &discoverer : it->second)
            {
                discoverer->conn->send(msg);
            }
        }

        // ============================================================
        // PDManager
        // ============================================================

        PDManager::PDManager()
            : _providers(std::make_shared<ProviderManager>()),
              _discoverers(std::make_shared<DiscovererManager>()) {}

        // 处理服务操作请求
        void PDManager::onServiceRequest(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg)
        {
            ServiceOptype optype = msg->optype();

            if (optype == ServiceOptype::SERVICE_REGISTRY)
            {
                // 服务注册：新增提供者 + 通知发现者（上线）
                ILOG("%s:%d 注册服务 %s", msg->host().first.c_str(), msg->host().second, msg->method().c_str());
                _providers->addProvider(conn, msg->host(), msg->method());
                _discoverers->onlineNotify(msg->method(), msg->host());
                registryResponse(conn, msg);
            }
            else if (optype == ServiceOptype::SERVICE_DISCOVERY)
            {
                // 服务发现：新增发现者
                ILOG("客户端要进行 %s 服务发现！", msg->method().c_str());
                _discoverers->addDiscoverer(conn, msg->method());
                discoveryResponse(conn, msg);
            }
            else
            {
                // 其它类型：报错
                ELOG("收到服务操作请求，但是操作类型错误！");
                errorResponse(conn, msg);
            }
        }

        // 连接断开时清理
        void PDManager::onConnShutdown(const BaseConnection::ptr &conn)
        {
            // 1. 如果断开的是提供者，触发下线通知
            auto provider = _providers->getProvider(conn);
            if (provider.get() != nullptr)
            {
                ILOG("%s:%d 服务下线", provider->host.first.c_str(), provider->host.second);
                for (auto &method : provider->methods)
                {
                    _discoverers->offlineNotify(method, provider->host);
                }
                _providers->delProvider(conn);
            }
            // 2. 如果断开的是发现者，清理数据
            _discoverers->delDiscoverer(conn);
        }

        // 响应：错误
        void PDManager::errorResponse(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg)
        {
            auto rsp = std::make_shared<ServiceResponse>();
            rsp->setId(msg->rid());
            rsp->setMType(MType::RSP_SERVICE);
            rsp->setRCode(RCode::RCODE_INVALID_OPTYPE);
            rsp->setOptype(ServiceOptype::SERVICE_UNKNOW);
            conn->send(rsp);
        }

        // 响应：注册成功
        void PDManager::registryResponse(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg)
        {
            auto rsp = std::make_shared<ServiceResponse>();
            rsp->setId(msg->rid());
            rsp->setMType(MType::RSP_SERVICE);
            rsp->setRCode(RCode::RCODE_OK);
            rsp->setOptype(ServiceOptype::SERVICE_REGISTRY);
            conn->send(rsp);
        }

        // 响应：发现结果
        void PDManager::discoveryResponse(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg)
        {
            auto rsp = std::make_shared<ServiceResponse>();
            rsp->setId(msg->rid());
            rsp->setMType(MType::RSP_SERVICE);
            rsp->setOptype(ServiceOptype::SERVICE_DISCOVERY);

            auto hosts = _providers->methodHosts(msg->method());
            if (hosts.empty())
            {
                rsp->setRCode(RCode::RCODE_NOT_FOUND_SERVICE);
                conn->send(rsp);
                return;
            }
            rsp->setRCode(RCode::RCODE_OK);
            rsp->setMethod(msg->method());
            rsp->setHost(hosts);
            conn->send(rsp);
        }

    } // namespace server
} // namespace bitrpc