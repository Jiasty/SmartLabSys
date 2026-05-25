#pragma once
#include <functional>

class EventLoop; // 前向声明

class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel() = default;

    // 处理触发的事件（核心逻辑，由 EventLoop 调用）
    void handleEvent();

    // 设置回调函数
    void setReadCallback(EventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }

    // 设置感兴趣的事件并向 epoll 注册
    void enableReading();
    void enableWriting();
    void disableAll();

    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; } // epoll 返回的实际发生事件

private:
    EventLoop* loop_; // 当前 Channel 属于哪个 EventLoop
    const int fd_;
    int events_;      // 用户关心的 epoll 事件 (如 EPOLLIN)
    int revents_;     // 目前 fd 实际触发的事件

    // 具体的回调函数
    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
};

