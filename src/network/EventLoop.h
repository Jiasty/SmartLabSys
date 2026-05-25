// src/network/EventLoop.h
#pragma once
#include <vector>
#include <memory>
#include <atomic>
#include <functional>

class EpollPoller;
class Channel;

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();

    // 给 EventLoop 所在线程执行任务（用于跨线程唤醒）
    void runInLoop(std::function<void()> cb);
    
    // 更新 Poller 中的 Channel
    void updateChannel(Channel* channel);

    void removeChannel(Channel* channel);

private:
    std::atomic<bool> looping_;
    std::atomic<bool> quit_;
    
    std::unique_ptr<EpollPoller> poller_;
    std::vector<Channel*> activeChannels_; // 活跃的连接
};