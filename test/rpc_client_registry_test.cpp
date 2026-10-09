// =============================================================================
// rpc_client_registry_test.cpp
// RPC 客户端（带服务发现）：从注册中心查询 Add 服务，然后调用
// =============================================================================

#include "clients.h"  // RpcClient
#include "fields.hpp" // Address
#include "logger.h"   // ILOG、DLOG
#include <jsoncpp/json/json.h>
#include <thread>
#include <chrono>

// 异步回调
void callback(const Json::Value &result)
{
    ILOG("callback result: %d", result.asInt());
}

int main()
{
    // 创建 RPC 客户端
    // 参数说明：
    //   true           —— 启用服务发现
    //   127.0.0.1:8080 —— 注册中心地址（不是服务端地址！）
    bitrpc::client::RpcClient client(true, "127.0.0.1", 8080);

    // ---------- 同步调用 ----------
    Json::Value params, result;
    params["num1"] = 11;
    params["num2"] = 22;
    bool ret = client.call("Add", params, result);
    if (ret != false)
    {
        ILOG("同步调用 result: %d", result.asInt()); // 期望 33
    }

    // ---------- 异步 future ----------
    bitrpc::client::RpcCaller::JsonAsyncResponse res_future;
    params["num1"] = 33;
    params["num2"] = 44;
    ret = client.call("Add", params, res_future);
    if (ret != false)
    {
        result = res_future.get();
        ILOG("future 调用 result: %d", result.asInt()); // 期望 77
    }

    // ---------- 异步回调 ----------
    params["num1"] = 55;
    params["num2"] = 66;
    ret = client.call("Add", params, callback); // 期望 121
    DLOG("-------\n");

    // 等回调完成
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}