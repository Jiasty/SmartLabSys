#include "src/network/EventLoop.h"
#include "src/network/TcpServer.h"
#include "src/llm/AITaskQueue.hpp"
#include "src/llm/DeepSeekProvider.hpp"
#include "src/db/DBManager.hpp"  // <--- 极其关键的引用
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <mutex>
#include <cctype>

// 全局 AI 任务队列
std::unique_ptr<AITaskQueue> g_aiQueue;

// 缓冲区与线程锁
std::map<void*, std::string> g_recvBuffer;
std::mutex g_bufferMutex;

std::string readFile(const std::string& filePath) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) return "";
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

void onMessage(const std::shared_ptr<Connection>& conn, const std::string& msg) {
    std::string currentData;
    
    // 1. 线程安全地拼接网络碎片
    {
        std::lock_guard<std::mutex> lock(g_bufferMutex);
        void* connKey = conn.get();
        g_recvBuffer[connKey] += msg;
        currentData = g_recvBuffer[connKey];
    }

    std::cout << "\n[TCP 监控] fd=" << conn->fd() << " 收到新数据: " << msg.size() << " 字节 | 当前总缓冲: " << currentData.size() << " 字节\n";

    // 2. 循环解析
    while (true) {
        size_t headerEnd = currentData.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            std::cout << "[TCP 监控] HTTP 头尚未收完，继续等待...\n";
            break; 
        }

        // 提取头部并全转为小写，彻底终结大小写解析 Bug
        std::string headerStr = currentData.substr(0, headerEnd);
        for (auto& c : headerStr) c = std::tolower(c);

        size_t contentLength = 0;
        size_t clPos = headerStr.find("content-length:");
        if (clPos != std::string::npos) {
            size_t valStart = clPos + 15;
            size_t clEnd = headerStr.find("\r\n", valStart);
            if (clEnd == std::string::npos) clEnd = headerStr.size();
            try {
                std::string clStr = headerStr.substr(valStart, clEnd - valStart);
                contentLength = std::stoull(clStr);
            } catch (...) {
                contentLength = 0;
            }
        }

        size_t totalExpectedLength = headerEnd + 4 + contentLength;

        if (currentData.size() < totalExpectedLength) {
            std::cout << "[TCP 监控] 包没收全！期待总长: " << totalExpectedLength 
                      << "，还差 " << (totalExpectedLength - currentData.size()) << " 字节，继续死等...\n";
            break; 
        }

        std::cout << "[TCP 监控] 🎯 完美拼接出一个 HTTP 请求！长度: " << totalExpectedLength << "\n";

        // 3. 剥离完整的请求
        std::string fullRequest = currentData.substr(0, totalExpectedLength);
        std::string body = fullRequest.substr(headerEnd + 4);
        currentData.erase(0, totalExpectedLength);

        {
            std::lock_guard<std::mutex> lock(g_bufferMutex);
            g_recvBuffer[conn.get()] = currentData;
        }

        // 4. 路由分发
        if (fullRequest.find("OPTIONS") == 0) {
            std::cout << "[HTTP 路由] 处理 OPTIONS 跨域预检请求\n";
            std::string cors = "HTTP/1.1 204 No Content\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
                               "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
                               "\r\n";
            conn->send(cors);
        }
        else if (fullRequest.find("POST /api/vision") == 0) {
            std::cout << "[HTTP 路由] 转发至视觉分析模块，负载大小: " << body.size() << " 字节\n";
            g_aiQueue->pushTask(conn, "VISION_TASK:" + body);
        }
        else if (fullRequest.find("GET /api/devices") == 0) {
            std::cout << "[HTTP 路由] 请求设备台账列表，正在查询 MySQL...\n";
            
            // 局部实例化 DBManager，保证多线程环境下的连接安全
            DBManager db;
            std::string jsonResponse = "[]";
            if (db.connect()) {
                jsonResponse = db.getAllDevicesJson();
            } else {
                std::cerr << "[HTTP 错误] 数据库连接失败！\n";
            }

            // 组装带 CORS 头的标准 JSON 响应
            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json; charset=utf-8\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Length: " + std::to_string(jsonResponse.size()) + "\r\n"
                "\r\n" + jsonResponse;
            
            conn->send(response);
        }
        else if (fullRequest.find("POST /") == 0) {
            g_aiQueue->pushTask(conn, body);
        }
        else if (fullRequest.find("GET / ") == 0 || fullRequest.find("GET /index.html") == 0) {
            std::string htmlContent = readFile("../index.html"); 
            if (!htmlContent.empty()) {
                std::string response = 
                    "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                    "Content-Length: " + std::to_string(htmlContent.size()) + "\r\n\r\n" + htmlContent;
                conn->send(response);
            }
        }
    }
}

int main() {
    std::cout << "--- SmartLabSys AI Server Starting ---" << std::endl;

    auto provider = std::make_unique<DeepSeekProvider>("sk-2f175811093d45af9fc2387770460384");
    g_aiQueue = std::make_unique<AITaskQueue>(std::move(provider));

    EventLoop mainLoop;
    TcpServer server(&mainLoop, "0.0.0.0", 12577, 4);
    server.setMessageCallback(onMessage);
    
    server.start();

    std::cout << "🚀 智能实验室 AI 后端已启动！正在监听..." << std::endl;
    mainLoop.loop();

    return 0;
}