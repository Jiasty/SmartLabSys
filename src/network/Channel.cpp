// src/network/Channel.cpp
#include "Channel.h"
#include "EventLoop.h"
#include <sys/epoll.h>

const int kNoneEvent = 0;
const int kReadEvent = EPOLLIN | EPOLLPRI;
const int kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0) {
}

// 核心分发逻辑
void Channel::handleEvent() {
    // 处理对方关闭连接或错误事件
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (closeCallback_) closeCallback_();
    }
    if (revents_ & EPOLLERR) {
        // 发生错误，一般也调用 close
        if (closeCallback_) closeCallback_();
    }
    // 处理读事件
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (readCallback_) readCallback_();
    }
    // 处理写事件
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) writeCallback_();
    }
}

void Channel::enableReading() {
    events_ |= kReadEvent;
    loop_->updateChannel(this);
}

void Channel::enableWriting() {
    events_ |= kWriteEvent;
    loop_->updateChannel(this);
}

void Channel::disableAll() {
    events_ = kNoneEvent;
    loop_->removeChannel(this); // 直接从 epoll 中移除
}