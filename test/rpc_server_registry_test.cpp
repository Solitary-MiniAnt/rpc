// =============================================================================
// rpc_server_registry_test.cpp
// RPC 服务端（带注册）：注册 Add 方法，并向注册中心上报自己
// =============================================================================

#include "servers.h"          // RpcServer、SDescribeFactory
#include "fields.hpp"         // VType、Address
#include "logger.h"           // ILOG
#include <jsoncpp/json/json.h>

// 业务回调：加法
void Add(const Json::Value &req, Json::Value &rsp) {
    int num1 = req["num1"].asInt();
    int num2 = req["num2"].asInt();
    rsp = num1 + num2;
}

int main() {
    // 1. 配置 Add 方法
    bitrpc::server::SDescribeFactory factory;
    factory.setMethodName("Add");
    factory.setParamsDesc("num1", bitrpc::server::VType::INTEGRAL);
    factory.setParamsDesc("num2", bitrpc::server::VType::INTEGRAL);
    factory.setReturnType(bitrpc::server::VType::INTEGRAL);
    factory.setCallback(Add);

    // 2. 创建 RPC 服务端
    //    参数说明：
    //      127.0.0.1:9090 —— 本 RPC 服务对外可访问的地址
    //      true           —— 启用注册中心
    //      127.0.0.1:8080 —— 注册中心地址
    bitrpc::server::RpcServer server(bitrpc::Address("127.0.0.1", 9090),
                                     true,
                                     bitrpc::Address("127.0.0.1", 8080));

    // 3. 注册方法（会同时上报给注册中心）
    server.registerMethod(factory.build());

    // 4. 启动（阻塞）
    ILOG("RpcServer with registry starting ...");
    server.start();
    return 0;
}