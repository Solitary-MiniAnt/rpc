// =============================================================================
// topic_server_test.cpp
// 发布订阅服务端测试：监听 7070 端口，管理所有主题和订阅者
// =============================================================================

#include "servers.h" // TopicServer
#include "logger.h"  // ILOG

int main()
{
    // 创建发布订阅服务端，监听 7070 端口
    bitrpc::server::TopicServer server(7070);

    // 启动（阻塞，进入事件循环）
    ILOG("TopicServer starting on port 7070 ...");
    server.start();
    return 0;
}