// src/network/Acceptor.h
#pragma once
#include <functional>
#include <string>

class EventLoop;
class Channel;

class Acceptor {
public:
    // 回调函数：当拿到新连接 fd 时，抛给 TcpServer 处理
    using NewConnectionCallback = std::function<void(int sockfd)>;

    Acceptor(EventLoop* loop, const std::string& ip, uint16_t port);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb) {
        newConnectionCallback_ = cb;
    }

    void listen();

private:
    void handleRead(); // 当 listenfd 触发读事件时调用
    int createListenSocket(const std::string& ip, uint16_t port);

    EventLoop* loop_;
    int listenfd_;
    Channel* acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
};