// src/network/TcpServer.h
#pragma once
#include "Acceptor.h"
#include "EventLoopThreadPool.hpp"
#include "Connection.h"
#include <unordered_map>
#include <memory>
#include <string>

class TcpServer {
public:
    using MessageCallback = Connection::MessageCallback;

    TcpServer(EventLoop* loop, const std::string& ip, uint16_t port, int numThreads);
    ~TcpServer() = default;

    void start();
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }

private:
    // Acceptor 抛出新连接 fd 时触发此函数
    void newConnection(int sockfd);
    // Connection 断开时触发此函数
    void removeConnection(const std::shared_ptr<Connection>& conn);

    EventLoop* mainLoop_;
    std::unique_ptr<Acceptor> acceptor_;
    std::unique_ptr<EventLoopThreadPool> threadPool_;
    
    MessageCallback messageCallback_;
    
    // 保存所有持有的连接，防止智能指针被提前释放
    std::unordered_map<int, std::shared_ptr<Connection>> connections_; 
};