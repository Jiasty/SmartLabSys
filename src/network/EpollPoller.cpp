// src/network/EpollPoller.cpp
#include "EpollPoller.h"
#include "Channel.h"
#include <unistd.h>
#include <cstring>
#include <iostream>

const int kInitEventListSize = 16;

EpollPoller::EpollPoller()
    : epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {
    if (epollfd_ < 0) {
        std::cerr << "Failed to create epoll fd" << std::endl;
        exit(EXIT_FAILURE);
    }
}

EpollPoller::~EpollPoller() {
    ::close(epollfd_);
}

void EpollPoller::poll(int timeoutMs, std::vector<Channel*>& activeChannels) {
    // 调用底层的 epoll_wait
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int savedErrno = errno;

    if (numEvents > 0) {
        for (int i = 0; i < numEvents; ++i) {
            // 从 epoll_event 的 data.ptr 中获取绑定的 Channel 指针
            Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
            // 将实际发生的事件赋值给 Channel
            channel->set_revents(events_[i].events);
            activeChannels.push_back(channel);
        }
        // 如果事件数组满了，动态扩容
        if (numEvents == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (numEvents == 0) {
        // 超时，无事发生
    } else {
        if (savedErrno != EINTR) {
            std::cerr << "EpollPoller::poll() error!" << std::endl;
        }
    }
}

void EpollPoller::updateChannel(Channel* channel) {
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    
    // 配置事件：采用 EPOLLET (边缘触发) 模式
    event.events = channel->events() | EPOLLET; 
    event.data.ptr = channel; // 绑定 Channel 指针

    int fd = channel->fd();
    int op = EPOLL_CTL_MOD; 

    // 如果是第一次添加（可以通过 Channel 内部维护一个状态来判断，这里为简化逻辑假设需要添加时捕获错误）
    if (::epoll_ctl(epollfd_, op, fd, &event) < 0) {
        if (errno == ENOENT) { // 不存在，说明是新 fd，改为 ADD
            op = EPOLL_CTL_ADD;
            ::epoll_ctl(epollfd_, op, fd, &event);
        }
    }
}

void EpollPoller::removeChannel(Channel* channel) {
    int fd = channel->fd();
    ::epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, nullptr);
}