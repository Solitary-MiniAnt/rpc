#include "servers.h"

namespace bitrpc
{
    namespace server
    {

        // ============================================================
        // RegistryServer
        // ============================================================

        RegistryServer::RegistryServer(int port)
            : _pd_manager(std::make_shared<PDManager>()),
              _dispatcher(std::make_shared<Dispatcher>())
        {
            // 把 PDManager::onServiceRequest 注册为 REQ_SERVICE 的处理回调
            auto service_cb = std::bind(&PDManager::onServiceRequest,
                                        _pd_manager.get(),
                                        std::placeholders::_1, std::placeholders::_2);
            _dispatcher->registerHandler<ServiceRequest>(MType::REQ_SERVICE, service_cb);

            // 创建底层服务器，监听 port
            _server = ServerFactory::create(port);

            // 把 Dispatcher::onMessage 注册为底层服务器的消息回调
            // 收到消息 → Dispatcher 按类型分发 → PDManager 处理
            auto message_cb = std::bind(&Dispatcher::onMessage,
                                        _dispatcher.get(),
                                        std::placeholders::_1, std::placeholders::_2);
            _server->setMessageCallback(message_cb);

            // 连接断开时通知 PDManager（用于触发服务下线通知）
            auto close_cb = std::bind(&RegistryServer::onConnShutdown,
                                      this, std::placeholders::_1);
            _server->setCloseCallback(close_cb);
        }

        void RegistryServer::start()
        {
            _server->start();
        }

        void RegistryServer::onConnShutdown(const BaseConnection::ptr &conn)
        {
            _pd_manager->onConnShutdown(conn);
        }

        // ============================================================
        // RpcServer
        // ============================================================

        RpcServer::RpcServer(const Address &access_addr,
                             bool enableRegistry,
                             const Address &registry_server_addr)
            : _enableRegistry(enableRegistry),
              _access_addr(access_addr),
              _router(std::make_shared<RpcRouter>()),
              _dispatcher(std::make_shared<Dispatcher>())
        {
            // 启用注册：创建注册中心客户端（连上注册中心）
            if (enableRegistry)
            {
                _reg_client = std::make_shared<client::RegistryClient>(
                    registry_server_addr.first, registry_server_addr.second);
            }

            // 把 RpcRouter::onRpcRequest 注册为 REQ_RPC 的处理回调
            auto rpc_cb = std::bind(&RpcRouter::onRpcRequest,
                                    _router.get(),
                                    std::placeholders::_1, std::placeholders::_2);
            _dispatcher->registerHandler<RpcRequest>(MType::REQ_RPC, rpc_cb);

            // 创建底层服务器，监听 access_addr 的端口
            _server = ServerFactory::create(access_addr.second);

            // 把 Dispatcher::onMessage 注册为底层服务器的消息回调
            auto message_cb = std::bind(&Dispatcher::onMessage,
                                        _dispatcher.get(),
                                        std::placeholders::_1, std::placeholders::_2);
            _server->setMessageCallback(message_cb);
        }

        // 注册 RPC 方法：可选上报给注册中心，然后登记到本地路由器
        void RpcServer::registerMethod(const ServiceDescribe::ptr &service)
        {
            if (_enableRegistry)
            {
                _reg_client->registryMethod(service->method(), _access_addr);
            }
            _router->registerMethod(service);
        }

        void RpcServer::start()
        {
            _server->start();
        }

        // ============================================================
        // TopicServer
        // ============================================================

        TopicServer::TopicServer(int port)
            : _topic_manager(std::make_shared<TopicManager>()),
              _dispatcher(std::make_shared<Dispatcher>())
        {
            // 把 TopicManager::onTopicRequest 注册为 REQ_TOPIC 的处理回调
            auto topic_cb = std::bind(&TopicManager::onTopicRequest,
                                      _topic_manager.get(),
                                      std::placeholders::_1, std::placeholders::_2);
            _dispatcher->registerHandler<TopicRequest>(MType::REQ_TOPIC, topic_cb);

            // 创建底层服务器，监听 port
            _server = ServerFactory::create(port);

            // 把 Dispatcher::onMessage 注册为底层服务器的消息回调
            auto message_cb = std::bind(&Dispatcher::onMessage,
                                        _dispatcher.get(),
                                        std::placeholders::_1, std::placeholders::_2);
            _server->setMessageCallback(message_cb);

            // 连接断开时通知 TopicManager（清理订阅者数据）
            auto close_cb = std::bind(&TopicServer::onConnShutdown,
                                      this, std::placeholders::_1);
            _server->setCloseCallback(close_cb);
        }

        void TopicServer::start()
        {
            _server->start();
        }

        void TopicServer::onConnShutdown(const BaseConnection::ptr &conn)
        {
            _topic_manager->onShutdown(conn);
        }

    } // namespace server
} // namespace bitrpc