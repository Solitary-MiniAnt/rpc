// =============================================================================
// rpc_server_test.cpp
// RPC 服务端测试程序：注册一个 Add 方法，然后启动服务
// =============================================================================

#include "servers.h"  // RpcServer、SDescribeFactory
#include "fields.hpp" // VType、Address
#include "logger.h"   // ILOG
#include <jsoncpp/json/json.h>

// ---------- 业务回调：加法 ----------
// 输入：req 是客户端传来的参数对象 {num1, num2}
// 输出：rsp 是返回给客户端的结果
void Add(const Json::Value &req, Json::Value &rsp)
{
    int num1 = req["num1"].asInt();
    int num2 = req["num2"].asInt();
    rsp = num1 + num2;
}

int main()
{
    // 1. 创建服务描述工厂，一步步设置 Add 方法的元信息
    bitrpc::server::SDescribeFactory factory;
    factory.setMethodName("Add");
    factory.setParamsDesc("num1", bitrpc::server::VType::INTEGRAL);
    factory.setParamsDesc("num2", bitrpc::server::VType::INTEGRAL);
    factory.setReturnType(bitrpc::server::VType::INTEGRAL);
    factory.setCallback(Add);

    // 2. 创建 RPC 服务端，监听 127.0.0.1:9090
    //    参数 2（enableRegistry）= false：本次测试不启用注册中心
    bitrpc::server::RpcServer server(bitrpc::Address("127.0.0.1", 9090));

    // 3. 把 Add 方法注册进路由器
    server.registerMethod(factory.build());

    // 4. 启动（阻塞，进入事件循环）
    server.start();
    return 0;
}