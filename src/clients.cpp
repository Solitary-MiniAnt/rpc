#include "clients.h"

namespace bitrpc
{
    namespace client
    {

        // ============================================================
        // RegistryClient
        // ============================================================

        RegistryClient::RegistryClient(const std::string &ip, int port)
            : _requestor(std::make_shared<Requestor>()),
              _provider(std::make_shared<Provider>(_requestor)),
              _dispatcher(std::make_shared<Dispatcher>())
        {
            // 把 Requestor::onResponse 注册为 RSP_SERVICE 的处理回调
            // 即：收到服务端响应 → 交给 Requestor 处理
            auto rsp_cb = std::bind(&Requestor::onResponse,
                                    _requestor.get(),
                                    std::placeholders::_1, std::placeholders::_2);
            _dispatcher->registerHandler<BaseMessage>(MType::RSP_SERVICE, rsp_cb);

            // 创建底层客户端，把 Dispatcher::onMessage 设为消息回调
            auto message_cb = std::bind(&Dispatcher::onMessage,
                                        _dispatcher.get(),
                                        std::placeholders::_1, std::placeholders::_2);
            _client = ClientFactory::create(ip, port);
            _client->setMessageCallback(message_cb);
            _client->connect();
        }

        bool RegistryClient::registryMethod(const std::string &method, const Address &host)
        {
            return _provider->registryMethod(_client->connection(), method, host);
        }

        // ============================================================
        // DiscoveryClient
        // ============================================================

        DiscoveryClient::DiscoveryClient(const std::string &ip, int port,
                                         const Discoverer::OfflineCallback &cb)
            : _requestor(std::make_shared<Requestor>()),
              _discoverer(std::make_shared<Discoverer>(_requestor, cb)),
              _dispatcher(std::make_shared<Dispatcher>())
        {
            // 收到服务端响应 → 交给 Requestor 处理
            auto rsp_cb = std::bind(&Requestor::onResponse,
                                    _requestor.get(),
                                    std::placeholders::_1, std::placeholders::_2);
            _dispatcher->registerHandler<BaseMessage>(MType::RSP_SERVICE, rsp_cb);

            // 收到服务端推送的上线/下线通知 → 交给 Discoverer 处理
            auto req_cb = std::bind(&Discoverer::onServiceRequest,
                                    _discoverer.get(),
                                    std::placeholders::_1, std::placeholders::_2);
            _dispatcher->registerHandler<ServiceRequest>(MType::REQ_SERVICE, req_cb);

            auto message_cb = std::bind(&Dispatcher::onMessage,
                                        _dispatcher.get(),
                                        std::placeholders::_1, std::placeholders::_2);
            _client = ClientFactory::create(ip, port);
            _client->setMessageCallback(message_cb);
            _client->connect();
        }

        bool DiscoveryClient::serviceDiscovery(const std::string &method, Address &host)
        {
            return _discoverer->serviceDiscovery(_client->connection(), method, host);
        }

        // ============================================================
        // RpcClient
        // ============================================================

        RpcClient::RpcClient(bool enableDiscovery, const std::string &ip, int port)
            : _enableDiscovery(enableDiscovery),
              _requestor(std::make_shared<Requestor>()),
              _dispatcher(std::make_shared<Dispatcher>()),
              _caller(std::make_shared<RpcCaller>(_requestor))
        {
            // 收到 RSP_RPC 响应 → 交给 Requestor 处理
            auto rsp_cb = std::bind(&Requestor::onResponse,
                                    _requestor.get(),
                                    std::placeholders::_1, std::placeholders::_2);
            _dispatcher->registerHandler<BaseMessage>(MType::RSP_RPC, rsp_cb);

            if (_enableDiscovery)
            {
                // 启用服务发现：创建 DiscoveryClient，通过它查询服务端地址
                auto offline_cb = std::bind(&RpcClient::delClient, this, std::placeholders::_1);
                _discovery_client = std::make_shared<DiscoveryClient>(ip, port, offline_cb);
            }
            else
            {
                // 未启用：直连给定的服务端地址
                auto message_cb = std::bind(&Dispatcher::onMessage,
                                            _dispatcher.get(),
                                            std::placeholders::_1, std::placeholders::_2);
                _rpc_client = ClientFactory::create(ip, port);
                _rpc_client->setMessageCallback(message_cb);
                _rpc_client->connect();
            }
        }

        // 三种 call：都是先找到服务端 client，再委托给 RpcCaller

        bool RpcClient::call(const std::string &method, const Json::Value &params,
                             Json::Value &result)
        {
            BaseClient::ptr client = getClient(method);
            if (client.get() == nullptr)
                return false;
            return _caller->call(client->connection(), method, params, result);
        }

        bool RpcClient::call(const std::string &method, const Json::Value &params,
                             RpcCaller::JsonAsyncResponse &result)
        {
            BaseClient::ptr client = getClient(method);
            if (client.get() == nullptr)
                return false;
            return _caller->call(client->connection(), method, params, result);
        }

        bool RpcClient::call(const std::string &method, const Json::Value &params,
                             const RpcCaller::JsonResponseCallback &cb)
        {
            BaseClient::ptr client = getClient(method);
            if (client.get() == nullptr)
                return false;
            return _caller->call(client->connection(), method, params, cb);
        }

        // 创建一个新的 RPC 服务端客户端连接
        BaseClient::ptr RpcClient::newClient(const Address &host)
        {
            auto message_cb = std::bind(&Dispatcher::onMessage,
                                        _dispatcher.get(),
                                        std::placeholders::_1, std::placeholders::_2);
            auto client = ClientFactory::create(host.first, host.second);
            client->setMessageCallback(message_cb);
            client->connect();
            putClient(host, client);
            return client;
        }

        // 按地址查已有连接
        BaseClient::ptr RpcClient::getClient(const Address &host)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _rpc_clients.find(host);
            if (it == _rpc_clients.end())
                return BaseClient::ptr();
            return it->second;
        }

        // 按方法名查客户端
        BaseClient::ptr RpcClient::getClient(const std::string method)
        {
            BaseClient::ptr client;
            if (_enableDiscovery)
            {
                // 走服务发现
                Address host;
                bool ret = _discovery_client->serviceDiscovery(method, host);
                if (ret == false)
                {
                    ELOG("当前 %s 服务，没有找到服务提供者！", method.c_str());
                    return BaseClient::ptr();
                }
                // 已有连接就用，没有就新建
                client = getClient(host);
                if (client.get() == nullptr)
                {
                    client = newClient(host);
                }
            }
            else
            {
                // 固定连接
                client = _rpc_client;
            }
            return client;
        }

        // 把新连接存入连接池
        void RpcClient::putClient(const Address &host, BaseClient::ptr &client)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _rpc_clients.insert(std::make_pair(host, client));
        }

        // 删除一个连接（服务下线时被调用）
        void RpcClient::delClient(const Address &host)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _rpc_clients.erase(host);
        }

        // ============================================================
        // TopicClient
        // ============================================================

        TopicClient::TopicClient(const std::string &ip, int port)
            : _requestor(std::make_shared<Requestor>()),
              _dispatcher(std::make_shared<Dispatcher>()),
              _topic_manager(std::make_shared<TopicManager>(_requestor))
        {
            // 收到 RSP_TOPIC 响应 → 交给 Requestor 处理
            auto rsp_cb = std::bind(&Requestor::onResponse,
                                    _requestor.get(),
                                    std::placeholders::_1, std::placeholders::_2);
            _dispatcher->registerHandler<BaseMessage>(MType::RSP_TOPIC, rsp_cb);

            // 收到服务端推送的 REQ_TOPIC 消息 → 交给 TopicManager::onPublish 处理
            auto msg_cb = std::bind(&TopicManager::onPublish,
                                    _topic_manager.get(),
                                    std::placeholders::_1, std::placeholders::_2);
            _dispatcher->registerHandler<TopicRequest>(MType::REQ_TOPIC, msg_cb);

            auto message_cb = std::bind(&Dispatcher::onMessage,
                                        _dispatcher.get(),
                                        std::placeholders::_1, std::placeholders::_2);
            _rpc_client = ClientFactory::create(ip, port);
            _rpc_client->setMessageCallback(message_cb);
            _rpc_client->connect();
        }

        bool TopicClient::create(const std::string &key)
        {
            return _topic_manager->create(_rpc_client->connection(), key);
        }
        bool TopicClient::remove(const std::string &key)
        {
            return _topic_manager->remove(_rpc_client->connection(), key);
        }
        bool TopicClient::subscribe(const std::string &key, const TopicManager::SubCallback &cb)
        {
            return _topic_manager->subscribe(_rpc_client->connection(), key, cb);
        }
        bool TopicClient::cancel(const std::string &key)
        {
            return _topic_manager->cancel(_rpc_client->connection(), key);
        }
        bool TopicClient::publish(const std::string &key, const std::string &msg)
        {
            return _topic_manager->publish(_rpc_client->connection(), key, msg);
        }
        void TopicClient::shutdown()
        {
            _rpc_client->shutdown();
        }

    } // namespace client
} // namespace bitrpc