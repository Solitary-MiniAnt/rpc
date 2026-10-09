#include "dispatcher.h"

namespace bitrpc {

// 按消息类型找到对应回调并执行
void Dispatcher::onMessage(const BaseConnection::ptr &conn, BaseMessage::ptr &msg) {
    std::unique_lock<std::mutex> lock(_mutex);   // 加锁保护 _handlers
    auto it = _handlers.find(msg->mtype());      // 按类型查找
    if (it != _handlers.end()) {
        // 找到：调用对应回调
        it->second->onMessage(conn, msg);
        return;
    }
    // 没找到：说明收到了未注册的消息类型
    ELOG("收到未知类型的消息: %d！", (int)msg->mtype());
    conn->shutdown();                            // 关闭连接，防止继续收到未知消息
}

}  // namespace bitrpc