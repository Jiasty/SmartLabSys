// src/network/Connection.h
#pragma once
#include <memory>
#include <functional>
#include <string>

class EventLoop;
class Channel;

// 继承 enable_shared_from_this 是为了在回调中能安全地传递自己的智能指针
class Connection : public std::enable_shared_from_this<Connection> {
public:
    using MessageCallback = std::function<void(const std::shared_ptr<Connection>&, const std::string& message)>;
    using CloseCallback = std::function<void(const std::shared_ptr<Connection>&)>;

    Connection(EventLoop* loop, int fd);
    ~Connection();

    // 暴露给外界的回调设置
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }

    // 建立连接后，将自己的 Channel 注册到对应的 subLoop 中
    void connectEstablished();
    
    // 简单的发送数据接口
    void send(const std::string& msg);

    int fd() const { return fd_; }

private:
    void handleRead();  // 绑在 Channel 上的读事件回调
    void handleClose(); // 处理断开连接

    EventLoop* loop_;
    int fd_;
    std::unique_ptr<Channel> channel_;
    
    MessageCallback messageCallback_;
    CloseCallback closeCallback_;
};