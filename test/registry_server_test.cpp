// registry_server_test.cpp
// 注册中心服务端：监听 8080 端口

#include "servers.h"
#include "logger.h"

int main()
{
    bitrpc::server::RegistryServer reg_server(8080);
    ILOG("RegistryServer starting on port 8080 ...");
    reg_server.start();
    return 0;
}