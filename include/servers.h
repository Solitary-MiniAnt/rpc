#pragma once

// ---------- 本项目其他模块 ----------
#include "abstract.h"   // BaseConnection、BaseServer
#include "dispatcher.h" // Dispatcher：按消息类型分发
#include "muduo_impl.h" // ServerFactory：创建底层 muduo 服务器
#include "message.h"    // 各类消息类型
#include "fields.hpp"   // MType、Address
#include "logger.h"     // ILOG、ELOG

#include "rpc_router.h"    // RpcRouter：RPC 请求处理器
#include "topic_manager.h" // 服务端 TopicManager：发布订阅处理
#include "pd_manager.h"    // PDManager：注册/发现处理
#include "clients.h"       // client::RegistryClient：向注册中心上报

// ---------- 标准库 ----------
#include <memory> // shared_ptr、make_shared
#include <string> // string

namespace bitrpc
{
    namespace server
    {

        // ============================================================
        // RegistryServer：注册中心服务端
        // 职责：只处理服务注册与发现请求
        // 用法：直接 start() 就能跑，客户端连它进行注册/发现
        // ============================================================
        class RegistryServer
        {
        public:
            using ptr = std::shared_ptr<RegistryServer>;

            // port：注册中心监听端口
            explicit RegistryServer(int port);

            void start(); // 启动服务器（阻塞，进入事件循环）

        private:
            // 连接断开时被调用：清理该连接作为提供者/发现者的数据
            void onConnShutdown(const BaseConnection::ptr &conn);

        private:
            PDManager::ptr _pd_manager;  // 注册/发现管理器（业务）
            Dispatcher::ptr _dispatcher; // 消息分发器（按类型分发）
            BaseServer::ptr _server;     // 底层 muduo 服务器（抽象接口）
        };

        // ============================================================
        // RpcServer：RPC 服务端
        // 职责：处理 RPC 请求，可选向注册中心注册自己的方法
        // ============================================================
        class RpcServer
        {
        public:
            using ptr = std::shared_ptr<RpcServer>;

            // access_addr：本 RPC 服务对外可访问的地址（IP + 端口）
            // enableRegistry：是否启用注册（true 时会连注册中心）
            // registry_server_addr：注册中心地址（enableRegistry 时有效）
            explicit RpcServer(const Address &access_addr,
                               bool enableRegistry = false,
                               const Address &registry_server_addr = Address());

            // 注册一个 RPC 方法：会登记到本地路由器，
            // 若启用了注册，还会上报给注册中心
            void registerMethod(const ServiceDescribe::ptr &service);

            void start(); // 启动服务器（阻塞）

        private:
            bool _enableRegistry;                    // 是否启用注册
            Address _access_addr;                    // 本服务对外访问地址
            client::RegistryClient::ptr _reg_client; // 注册中心客户端（启用时非空）
            RpcRouter::ptr _router;                  // RPC 路由器（本地方法表）
            Dispatcher::ptr _dispatcher;             // 消息分发器
            BaseServer::ptr _server;                 // 底层 muduo 服务器
        };

        // ============================================================
        // TopicServer：发布订阅服务端
        // 职责：处理主题的创建/删除/订阅/取消/发布
        // ============================================================
        class TopicServer
        {
        public:
            using ptr = std::shared_ptr<TopicServer>;

            // port：发布订阅服务监听端口
            explicit TopicServer(int port);

            void start(); // 启动服务器（阻塞）

        private:
            // 连接断开时被调用：清理该连接作为订阅者的数据
            void onConnShutdown(const BaseConnection::ptr &conn);

        private:
            TopicManager::ptr _topic_manager; // 发布订阅管理器（业务）
            Dispatcher::ptr _dispatcher;      // 消息分发器
            BaseServer::ptr _server;          // 底层 muduo 服务器
        };

    } // namespace server
} // namespace bitrpc