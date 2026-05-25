#include "Connection.h"
#include "EventLoop.h"
#include "Channel.h"
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <errno.h> // 必须引入，用于判断 EAGAIN

Connection::Connection(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd), channel_(std::make_unique<Channel>(loop, fd)) {
    channel_->setReadCallback(std::bind(&Connection::handleRead, this));
}

Connection::~Connection() {
    ::close(fd_);
}

void Connection::connectEstablished() {
    channel_->enableReading();
}

void Connection::send(const std::string& msg) {
    ::write(fd_, msg.data(), msg.size());
}

void Connection::handleRead() {
    char buf[4096]; // 稍微调大单次读取缓冲区
    
    // 【核心修复】：必须使用 while 循环将内核数据一次性全部读完
    while (true) {
        ssize_t n = ::read(fd_, buf, sizeof(buf));
        
        if (n > 0) {
            std::string message(buf, n);
            if (messageCallback_) {
                // 向上层（main.cpp 的缓冲区）投递这一小段数据
                messageCallback_(shared_from_this(), message);
            }
            // 如果读取的长度等于 buf 大小，说明可能还有后续数据，继续循环读
            if (static_cast<size_t>(n) < sizeof(buf)) {
                // 如果读到的比 buf 小，虽然不一定读完，但通常可以退出了，
                // 或者保守一点，继续读到返回 -1 且为 EAGAIN。
            }
        } else if (n == 0) {
            // 客户端主动关闭连接
            handleClose();
            break;
        } else {
            // n < 0 的情况
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 【关键】：这代表当前内核缓冲区已经没数据了，可以安全退出循环
                break;
            } else if (errno == EINTR) {
                // 被信号中断，需要重试
                continue;
            } else {
                // 真正的读取错误
                std::cerr << "Read error on fd " << fd_ << " errno: " << errno << std::endl;
                handleClose();
                break;
            }
        }
    }
}

void Connection::handleClose() {
    channel_->disableAll();
    if (closeCallback_) {
        closeCallback_(shared_from_this());
    }
}