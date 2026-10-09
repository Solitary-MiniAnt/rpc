#pragma once

// ---------- 本项目其他模块 ----------
#include "dispatcher.h" // Dispatcher（7.7）
#include "muduo_impl.h" // ClientFactory（7.5）
#include "message.h"    // 消息类型
#include "fields.hpp"   // MType、Address
#include "logger.h"     // ELOG

#include "requestor.h"            // Requestor（7.12）
#include "rpc_caller.h"           // RpcCaller（7.13）
#include "client_registry.h"      // client::Provider、client::Discoverer（7.15）
#include "client_topic_manager.h" // client::TopicManager（7.14）

// ---------- 标准库 ----------
#include <memory>        // shared_ptr
#include <functional>    // std::function、std::bind
#include <mutex>         // mutex
#include <unordered_map> // 客户端连接池
#include <string>        // string

namespace bitrpc
{
    namespace client
    {

        // ============================================================
        // RegistryClient：服务端用来向注册中心注册自己
        // 职责：连接注册中心，把方法名和地址上报。
        // 用途：RpcServer 启动时使用（7.11 里留的坑现在填上）。
        // ============================================================
        class RegistryClient
        {
        public:
            using ptr = std::shared_ptr<RegistryClient>;

            // ip / port：注册中心地址
            RegistryClient(const std::string &ip, int port);

            // 上报一个方法（方法名 + 本服务对外地址）
            bool registryMethod(const std::string &method, const Address &host);

        private:
            Requestor::ptr _requestor;   // 请求管理器
            Provider::ptr _provider;     // 服务注册工具
            Dispatcher::ptr _dispatcher; // 消息分发器
            BaseClient::ptr _client;     // 与注册中心的连接
        };

        // ============================================================
        // DiscoveryClient：客户端用来从注册中心查询服务地址
        // 职责：连接注册中心，发现服务、接收上下线通知。
        // 用途：RpcClient 启用服务发现时使用。
        // ============================================================
        class DiscoveryClient
        {
        public:
            using ptr = std::shared_ptr<DiscoveryClient>;

            // ip / port：注册中心地址
            // cb：服务下线时的回调
            DiscoveryClient(const std::string &ip, int port,
                            const Discoverer::OfflineCallback &cb);

            // 按方法名查询一个可用主机地址
            bool serviceDiscovery(const std::string &method, Address &host);

        private:
            Requestor::ptr _requestor;
            Discoverer::ptr _discoverer;
            Dispatcher::ptr _dispatcher;
            BaseClient::ptr _client;
        };

        // ============================================================
        // RpcClient：最终对用户暴露的 RPC 客户端
        // 支持两种模式：
        //   1. enableDiscovery = true：连接注册中心，动态发现服务端
        //   2. enableDiscovery = false：直连一个固定的服务端地址
        // 提供三种调用方式：同步、异步 future、异步回调
        // ============================================================
        class RpcClient
        {
        public:
            using ptr = std::shared_ptr<RpcClient>;

            // enableDiscovery：是否启用服务发现
            // ip / port：若开启发现，是注册中心地址；否则是服务端地址
            RpcClient(bool enableDiscovery, const std::string &ip, int port);

            // 同步调用
            bool call(const std::string &method, const Json::Value &params,
                      Json::Value &result);

            // 异步 future 调用
            bool call(const std::string &method, const Json::Value &params,
                      RpcCaller::JsonAsyncResponse &result);

            // 异步回调调用
            bool call(const std::string &method, const Json::Value &params,
                      const RpcCaller::JsonResponseCallback &cb);

        private:
            // 创建并连接一个新的 RPC 服务端客户端（服务发现模式下用）
            BaseClient::ptr newClient(const Address &host);

            // 按地址查已有的客户端连接
            BaseClient::ptr getClient(const Address &host);

            // 按方法名查客户端：服务发现模式走 discovery，否则返回固定 client
            BaseClient::ptr getClient(const std::string method);

            // 把新创建的客户端存入连接池
            void putClient(const Address &host, BaseClient::ptr &client);

            // 服务下线时删除对应的客户端连接
            void delClient(const Address &host);

        private:
            // AddressHash：让 Address（std::pair）能作为 unordered_map 的 key
            struct AddressHash
            {
                size_t operator()(const Address &host) const
                {
                    std::string addr = host.first + std::to_string(host.second);
                    return std::hash<std::string>{}(addr);
                }
            };

        private:
            bool _enableDiscovery;                                                  // 是否启用服务发现
            DiscoveryClient::ptr _discovery_client;                                 // 服务发现客户端（启用时用）
            Requestor::ptr _requestor;                                              // 请求管理器
            RpcCaller::ptr _caller;                                                 // RPC 调用器
            Dispatcher::ptr _dispatcher;                                            // 消息分发器
            BaseClient::ptr _rpc_client;                                            // 固定连接（未启用发现时用）
            std::mutex _mutex;                                                      // 保护 _rpc_clients
            std::unordered_map<Address, BaseClient::ptr, AddressHash> _rpc_clients; // 连接池
        };

        // ============================================================
        // TopicClient：发布订阅客户端
        // 职责：连接发布订阅服务端，提供主题操作接口
        // ============================================================
        class TopicClient
        {
        public:
            using ptr = std::shared_ptr<TopicClient>;

            TopicClient(const std::string &ip, int port);

            bool create(const std::string &key); // 创建主题
            bool remove(const std::string &key); // 删除主题
            bool subscribe(const std::string &key,
                           const TopicManager::SubCallback &cb);          // 订阅主题
            bool cancel(const std::string &key);                          // 取消订阅
            bool publish(const std::string &key, const std::string &msg); // 发布消息
            void shutdown();                                              // 关闭连接

        private:
            Requestor::ptr _requestor;
            TopicManager::ptr _topic_manager;
            Dispatcher::ptr _dispatcher;
            BaseClient::ptr _rpc_client;
        };

    } // namespace client
} // namespace bitrpc