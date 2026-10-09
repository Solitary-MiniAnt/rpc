// =============================================================================
// rpc_client_test.cpp
// RPC 客户端测试程序：依次测试同步、异步 future、异步回调三种调用
// =============================================================================

#include "clients.h"  // RpcClient
#include "fields.hpp" // Address
#include "logger.h"   // ILOG、DLOG
#include <jsoncpp/json/json.h>
#include <thread> // std::this_thread::sleep_for
#include <chrono> // std::chrono::seconds

// ---------- 异步回调函数 ----------
// 响应到达后会被 RpcCaller 调用
void callback(const Json::Value &result)
{
    ILOG("callback result: %d", result.asInt());
}

int main()
{
    // 1. 创建 RPC 客户端
    //    false：不启用服务发现，直连固定服务端
    //    127.0.0.1:9090：服务端地址
    bitrpc::client::RpcClient client(false, "127.0.0.1", 9090);

    // ---------- 方式1：同步调用 ----------
    Json::Value params, result;
    params["num1"] = 11;
    params["num2"] = 22;
    bool ret = client.call("Add", params, result);
    if (ret != false)
    {
        ILOG("同步调用 result: %d", result.asInt()); // 期望 33
    }

    // ---------- 方式2：异步 future 调用 ----------
    bitrpc::client::RpcCaller::JsonAsyncResponse res_future;
    params["num1"] = 33;
    params["num2"] = 44;
    ret = client.call("Add", params, res_future);
    if (ret != false)
    {
        result = res_future.get();                      // 阻塞直到响应到达
        ILOG("future 调用 result: %d", result.asInt()); // 期望 77
    }

    // ---------- 方式3：异步回调调用 ----------
    params["num1"] = 55;
    params["num2"] = 66;
    ret = client.call("Add", params, callback); // 响应到达时自动触发 callback
    DLOG("-------\n");

    // 等回调完成
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}