// src/network/EventLoopThreadPool.hpp
#pragma once
#include "EventLoop.h"
#include <vector>
#include <thread>
#include <memory>
#include <future>
#include <iostream>

class EventLoopThreadPool {
public:
    EventLoopThreadPool(EventLoop* mainLoop, int numThreads)
        : mainLoop_(mainLoop), numThreads_(numThreads), next_(0) {}

    ~EventLoopThreadPool() {
        // 实际工程中这里需要优雅退出子线程，为简化毕设代码，暂略
    }

    void start() {
        for (int i = 0; i < numThreads_; ++i) {
            // 使用 promise/future 机制，确保子线程的 loop 初始化完成后，主线程再继续
            std::promise<EventLoop*> promise;
            std::future<EventLoop*> future = promise.get_future();

            threads_.emplace_back([&promise]() {
                EventLoop loop;
                promise.set_value(&loop); // 将跑在子线程栈上的 loop 指针传出去
                loop.loop();              // 死循环，子线程阻塞在这里处理事件
            });

            loops_.push_back(future.get()); // 阻塞等待子线程初始化完毕
        }
        std::cout << "[ThreadPool] Started " << numThreads_ << " worker threads." << std::endl;
    }

    // 核心：轮询算法 (Round-Robin) 分发连接
    EventLoop* getNextLoop() {
        EventLoop* loop = mainLoop_; // 默认情况下，如果没有子线程，就用主线程
        if (!loops_.empty()) {
            loop = loops_[next_];
            next_ = (next_ + 1) % loops_.size();
        }
        return loop;
    }

private:
    EventLoop* mainLoop_;
    int numThreads_;
    int next_;
    std::vector<std::thread> threads_;
    std::vector<EventLoop*> loops_;
};