#pragma once

// ---------- 本项目其他模块 ----------
#include "abstract.h" // BaseConnection
#include "message.h"  // ServiceRequest、ServiceResponse
#include "fields.hpp" // ServiceOptype、RCode、MType
#include "logger.h"   // ILOG、ELOG
#include "uuid.h"     // uuid()：生成通知消息的 id

// ---------- 标准库 ----------
#include <memory>        // shared_ptr
#include <mutex>         // mutex、unique_lock
#include <unordered_map> // 映射表
#include <set>           // 集合
#include <vector>        // vector
#include <string>        // string

namespace bitrpc
{
    namespace server
    {

        // ============================================================
        // Provider：一个服务提供者（对应一条服务端连接）
        // 记录：连接、主机地址、能提供的方法列表
        // ============================================================
        class Provider
        {
        public:
            using ptr = std::shared_ptr<Provider>;

            BaseConnection::ptr conn;         // 提供者对应的连接
            Address host;                     // 主机地址（IP + 端口）
            std::vector<std::string> methods; // 该提供者能提供的方法列表

            Provider(const BaseConnection::ptr &c, const Address &h) : conn(c), host(h) {}

            void appendMethod(const std::string &method); // 新增一个可提供的方法

        private:
            std::mutex _mutex; // 保护 methods
        };

        // ============================================================
        // Discoverer：一个服务发现者（对应一条客户端连接）
        // 记录：连接、关注过的方法列表
        // ============================================================
        class Discoverer
        {
        public:
            using ptr = std::shared_ptr<Discoverer>;

            BaseConnection::ptr conn;         // 发现者对应的连接
            std::vector<std::string> methods; // 该发现者关注过的方法列表

            explicit Discoverer(const BaseConnection::ptr &c) : conn(c) {}

            void appendMethod(const std::string &method); // 新增一个关注的方法

        private:
            std::mutex _mutex; // 保护 methods
        };

        // ============================================================
        // ProviderManager：服务提供者管理器
        //   _providers：方法名 -> 提供该方法的提供者集合
        //   _conns：连接 -> 该连接对应的提供者
        // ============================================================
        class ProviderManager
        {
        public:
            using ptr = std::shared_ptr<ProviderManager>;

            void addProvider(const BaseConnection::ptr &c, const Address &h, const std::string &method); // 新增提供者
            Provider::ptr getProvider(const BaseConnection::ptr &c);                                     // 按连接查提供者
            void delProvider(const BaseConnection::ptr &c);                                              // 删除提供者
            std::vector<Address> methodHosts(const std::string &method);                                 // 查某个方法的所有主机地址

        private:
            std::mutex _mutex;                                                   // 保护两张表
            std::unordered_map<std::string, std::set<Provider::ptr>> _providers; // 方法名 -> 提供者集合
            std::unordered_map<BaseConnection::ptr, Provider::ptr> _conns;       // 连接 -> 提供者
        };

        // ============================================================
        // DiscovererManager：服务发现者管理器
        //   _discoverers：方法名 -> 关注该方法的发现者集合
        //   _conns：连接 -> 该连接对应的发现者
        // 同时负责在服务上下线时通知相关发现者
        // ============================================================
        class DiscovererManager
        {
        public:
            using ptr = std::shared_ptr<DiscovererManager>;

            Discoverer::ptr addDiscoverer(const BaseConnection::ptr &c, const std::string &method); // 新增发现者
            void delDiscoverer(const BaseConnection::ptr &c);                                       // 删除发现者

            void onlineNotify(const std::string &method, const Address &host);  // 服务上线通知
            void offlineNotify(const std::string &method, const Address &host); // 服务下线通知

        private:
            void notify(const std::string &method, const Address &host, ServiceOptype optype); // 统一通知逻辑

        private:
            std::mutex _mutex;                                                       // 保护两张表
            std::unordered_map<std::string, std::set<Discoverer::ptr>> _discoverers; // 方法名 -> 发现者集合
            std::unordered_map<BaseConnection::ptr, Discoverer::ptr> _conns;         // 连接 -> 发现者
        };

        // ============================================================
        // PDManager：注册/发现总管理器
        // 作用：处理所有 SERVICE 类型请求（注册、发现），
        //       并在服务上下线时通知发现者。
        // 用法：注册到 Dispatcher 处理 REQ_SERVICE 类型消息。
        // ============================================================
        class PDManager
        {
        public:
            using ptr = std::shared_ptr<PDManager>;

            PDManager();

            void onServiceRequest(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg); // 处理服务操作请求
            void onConnShutdown(const BaseConnection::ptr &conn);                                   // 连接断开时清理

        private:
            void errorResponse(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg);     // 错误响应
            void registryResponse(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg);  // 注册成功响应
            void discoveryResponse(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg); // 发现结果响应

        private:
            ProviderManager::ptr _providers;     // 提供者管理
            DiscovererManager::ptr _discoverers; // 发现者管理
        };

    } // namespace server
} // namespace bitrpc