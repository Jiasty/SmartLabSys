// src/network/TcpServer.cpp
#include "TcpServer.h"
#include <iostream>

TcpServer::TcpServer(EventLoop* loop, const std::string& ip, uint16_t port, int numThreads)
    : mainLoop_(loop),
      acceptor_(std::make_unique<Acceptor>(loop, ip, port)),
      threadPool_(std::make_unique<EventLoopThreadPool>(loop, numThreads)) {
    
    // 注册 Acceptor 拿到新连接后的处理动作
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1));
}

void TcpServer::start() {
    threadPool_->start(); // 启动工作子线程
    acceptor_->listen();  // 主线程开始监听新连接
}

void TcpServer::newConnection(int sockfd) {
    // 1. 从线程池里按轮询算法拿一个子线程的 EventLoop
    EventLoop* ioLoop = threadPool_->getNextLoop();
    
    // 2. 将这个 fd 包装成 Connection，并交给拿到的 ioLoop 去管理
    std::shared_ptr<Connection> conn = std::make_shared<Connection>(ioLoop, sockfd);
    connections_[sockfd] = conn; // 保存进 map

    std::cout << "[TcpServer] New Connection fd=" << sockfd << " assigned to a worker thread.\n";

    // 3. 设置业务回调和关闭清理回调
    conn->setMessageCallback(messageCallback_);
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // 4. 让 Connection 在子线程中把自己挂载到 epoll 上
    conn->connectEstablished(); 
}

void TcpServer::removeConnection(const std::shared_ptr<Connection>& conn) {
    int fd = conn->fd();
    connections_.erase(fd); // 从 map 中移除，触发 shared_ptr 析构释放资源
    std::cout << "[TcpServer] Connection fd=" << fd << " closed and removed.\n";
}