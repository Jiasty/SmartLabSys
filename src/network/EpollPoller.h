#pragma once
#include <vector>
#include <sys/epoll.h>

class Channel;

class EpollPoller {
public:
    EpollPoller();
    ~EpollPoller();

    // 核心：调用 epoll_wait，将活跃的 Channel 填充到 activeChannels 中
    void poll(int timeoutMs, std::vector<Channel*>& activeChannels);

    // 修改/添加/删除 Channel 上的关注事件 (底层调用 epoll_ctl)
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

private:
    int epollfd_;
    std::vector<struct epoll_event> events_; // 存放 epoll_wait 返回的事件数组
};