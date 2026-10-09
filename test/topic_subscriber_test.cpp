// =============================================================================
// topic_subscriber_test.cpp
// 订阅者测试：创建主题、订阅主题，等待接收推送
// =============================================================================

#include "clients.h" // TopicClient
#include "logger.h"  // ILOG、ELOG
#include <thread>
#include <chrono>

// ---------- 订阅回调 ----------
// 收到某主题推送时会触发，参数是（主题名，消息内容）
void callback(const std::string &key, const std::string &msg)
{
    ILOG("%s 主题收到推送过来的消息： %s", key.c_str(), msg.c_str());
}

int main()
{
    // 1. 创建发布订阅客户端，连接 127.0.0.1:7070
    auto client = std::make_shared<bitrpc::client::TopicClient>("127.0.0.1", 7070);

    // 2. 创建主题 "hello"
    bool ret = client->create("hello");
    if (ret == false)
    {
        ELOG("创建主题失败！");
    }

    // 3. 订阅主题 "hello"，并注册回调
    ret = client->subscribe("hello", callback);

    // 4. 保持运行，等待接收推送
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 5. 关闭连接
    client->shutdown();
    return 0;
}