// src/network/EventLoop.cpp
#include "EventLoop.h"
#include "EpollPoller.h"
#include "Channel.h"

EventLoop::EventLoop()
    : looping_(false), 
      quit_(false), 
      poller_(std::make_unique<EpollPoller>()) {
}

EventLoop::~EventLoop() {
}

void EventLoop::loop() {
    looping_ = true;
    quit_ = false;

    while (!quit_) {
        activeChannels_.clear();
        // 1. 获取所有活跃的连接，超时时间设为 10000ms (10秒)
        poller_->poll(10000, activeChannels_);

        // 2. 遍历处理所有活跃连接的事件
        for (Channel* channel : activeChannels_) {
            channel->handleEvent();
        }

        // 3. 执行跨线程投递过来的任务 (例如：主线程把新连接投递给子线程)
        // 这里的 doPendingFunctors() 实现略，通常通过 eventfd 唤醒并执行队列中的回调
    }

    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    // 注意：如果跨线程调用 quit()，需要通过 eventfd 唤醒 epoll_wait，防止其一直阻塞
}

void EventLoop::updateChannel(Channel* channel) {
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
    poller_->removeChannel(channel);
}