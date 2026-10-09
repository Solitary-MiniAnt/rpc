#pragma once

// ---------- 本项目其他模块 ----------
#include "requestor.h" // Requestor
#include "message.h"   // ServiceRequest、ServiceResponse
#include "fields.hpp"  // MType、RCode、ServiceOptype、Address
#include "logger.h"    // ELOG
#include "uuid.h"      // uuid()

// ---------- 标准库 ----------
#include <memory>        // shared_ptr
#include <functional>    // std::function
#include <mutex>         // mutex、unique_lock
#include <unordered_map> // method -> MethodHost
#include <vector>        // vector
#include <string>        // string

namespace bitrpc
{
    namespace client
    {

        // ============================================================
        // Provider：客户端侧的“服务提供者工具”
        // 用途：RPC 服务端启动时，用它把自己的方法注册到注册中心
        // 注意：服务端 7.10 也有 Provider 类，但那是“注册中心内部管理
        //       服务端的对象”，这里是“服务端主动注册自己时用的工具”。
        // ============================================================
        class Provider
        {
        public:
            using ptr = std::shared_ptr<Provider>;

            explicit Provider(const Requestor::ptr &requestor) : _requestor(requestor) {}

            // 向注册中心注册一个方法
            // conn：连向注册中心的连接
            // method：方法名
            // host：本服务对外可访问的地址
            bool registryMethod(const BaseConnection::ptr &conn,
                                const std::string &method,
                                const Address &host);

        private:
            Requestor::ptr _requestor;
        };

        // ============================================================
        // MethodHost：管理某个方法的所有主机地址
        // 支持：新增主机、删除主机、轮询选择主机（负载均衡）
        // ============================================================
        class MethodHost
        {
        public:
            using ptr = std::shared_ptr<MethodHost>;

            // 默认构造：空列表
            MethodHost();

            // 从地址列表构造
            explicit MethodHost(const std::vector<Address> &hosts);

            // 新增一个主机（收到服务上线通知时调用）
            void appendHost(const Address &host);

            // 删除一个主机（收到服务下线通知时调用）
            void removeHost(const Address &host);

            // 轮询选择一个主机（负载均衡）
            // 注意：调用前必须确保列表非空，否则会除零崩溃
            Address chooseHost();

            // 是否为空
            bool empty();

        private:
            std::mutex _mutex;           // 保护 _hosts 和 _idx
            size_t _idx;                 // 轮询下标
            std::vector<Address> _hosts; // 所有主机地址
        };

        // ============================================================
        // Discoverer：客户端侧的“服务发现器”
        // 用途：
        //   1. 提供 serviceDiscovery：按方法名找到可用的服务端地址
        //   2. 提供 onServiceRequest：作为 Dispatcher 的回调，
        //      处理服务端上线/下线通知
        // ============================================================
        class Discoverer
        {
        public:
            using ptr = std::shared_ptr<Discoverer>;

            // 服务下线时的回调（由用户提供，用来通知上层某主机不可用）
            using OfflineCallback = std::function<void(const Address &)>;

            Discoverer(const Requestor::ptr &requestor, const OfflineCallback &cb)
                : _requestor(requestor), _offline_callback(cb) {}

            // 服务发现：按方法名找一个可用主机地址
            // 优先从本地缓存取；缓存没有则向注册中心查询
            bool serviceDiscovery(const BaseConnection::ptr &conn,
                                  const std::string &method,
                                  Address &host);

            // 处理服务上线/下线通知（注册到 Dispatcher）
            void onServiceRequest(const BaseConnection::ptr &conn,
                                  const ServiceRequest::ptr &msg);

        private:
            OfflineCallback _offline_callback;                              // 下线回调
            std::mutex _mutex;                                              // 保护 _method_hosts
            std::unordered_map<std::string, MethodHost::ptr> _method_hosts; // 方法 -> 主机列表
            Requestor::ptr _requestor;
        };

    } // namespace client
} // namespace bitrpc