// =============================================================================
// topic_publisher_test.cpp
// 发布者测试：创建主题、连续发布 10 条消息
// =============================================================================

#include "clients.h" // TopicClient
#include "logger.h"  // ILOG、ELOG
#include <string>

int main()
{
    // 1. 创建发布订阅客户端，连接 127.0.0.1:7070
    auto client = std::make_shared<bitrpc::client::TopicClient>("127.0.0.1", 7070);

    // 2. 创建主题 "hello"（如果已存在，服务端会再次 insert，不影响）
    bool ret = client->create("hello");
    if (ret == false)
    {
        ELOG("创建主题失败！");
    }

    // 3. 连续发布 10 条消息
    for (int i = 0; i < 10; i++)
    {
        client->publish("hello", "Hello World-" + std::to_string(i));
    }

    // 4. 关闭连接
    client->shutdown();
    return 0;
}