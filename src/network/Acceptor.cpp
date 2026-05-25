// src/network/Acceptor.cpp
#include "Acceptor.h"
#include "EventLoop.h"
#include "Channel.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

Acceptor::Acceptor(EventLoop* loop, const std::string& ip, uint16_t port)
    : loop_(loop),
      listenfd_(createListenSocket(ip, port)),
      acceptChannel_(new Channel(loop, listenfd_)) {
    
    // 设置 Channel 的读回调，当有新连接时，epoll 会触发它
    acceptChannel_->setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
    acceptChannel_->disableAll();
    delete acceptChannel_;
    ::close(listenfd_);
}

void Acceptor::listen() {
    if (::listen(listenfd_, 128) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        exit(1);
    }
    acceptChannel_->enableReading(); // 将 listenfd 注册到 main loop 的 epoll 中
    std::cout << "[Acceptor] Listening for new connections..." << std::endl;
}

void Acceptor::handleRead() {
    struct sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);
    // 接收新连接
    int connfd = ::accept4(listenfd_, (struct sockaddr*)&clientAddr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    
    if (connfd >= 0) {
        if (newConnectionCallback_) {
            // 将拿到的新连接 fd 抛出去
            newConnectionCallback_(connfd); 
        } else {
            ::close(connfd);
        }
    } else {
        std::cerr << "Accept failed" << std::endl;
    }
}

int Acceptor::createListenSocket(const std::string& ip, uint16_t port) {
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    int optval = 1;
    ::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)); // 端口复用

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (::bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind socket" << std::endl;
        exit(1);
    }
    return sockfd;
}