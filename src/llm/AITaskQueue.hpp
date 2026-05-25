// src/ai/AITaskQueue.hpp
#pragma once
#include "BaseLLM.h"
#include "QwenVisionProvider.hpp"
#include "../network/Connection.h"
#include "../db/DBManager.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <iostream>
#include <regex>
#include <sstream>
#include <vector>
#include <jsoncpp/json/json.h> // 需要 jsoncpp 处理前端发来的 JSON 数据

struct AITask {
    std::shared_ptr<Connection> conn;
    std::string prompt;
};

class AITaskQueue {
public:
    explicit AITaskQueue(std::unique_ptr<BaseLLM> llm) 
        : llm_(std::move(llm)), stop_(false) {
        worker_ = std::thread(&AITaskQueue::processQueue, this);
    }

    ~AITaskQueue() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_one();
        if (worker_.joinable()) worker_.join();
    }

    void pushTask(const std::shared_ptr<Connection>& conn, const std::string& prompt) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            tasks_.push({conn, prompt});
        }
        cv_.notify_one();
    }

private:
    void processQueue() {
        DBManager db;
        if (!db.connect()) {
            std::cerr << "[AI Worker] 数据库连接失败！" << std::endl;
        }

        while (true) {
            AITask task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }

            // --- 1. 解析任务类型（视觉 vs 文本） ---
            bool isVisionTask = false;
            std::string userPrompt = task.prompt; // 默认将全部内容当做用户提问
            std::vector<std::string> base64Images; // 核心修改：使用 vector 存储多图

            if (task.prompt.find("VISION_TASK:") == 0) {
                isVisionTask = true;
                std::string jsonBody = task.prompt.substr(12); // 剥离头部标识
                
                Json::CharReaderBuilder reader;
                Json::Value root;
                std::string errs;
                std::istringstream s(jsonBody);
                
                if (Json::parseFromStream(reader, s, &root, &errs)) {
                    userPrompt = root["prompt"].asString(); // 提取纯净的提示词用于后续 RAG
                    
                    // 核心修改：解析前端传来的 images 数组
                    if (root.isMember("images") && root["images"].isArray()) {
                        for (const auto& imgNode : root["images"]) {
                            base64Images.push_back(imgNode.asString());
                        }
                    }
                    std::cout << "[AI Worker] 接收到视觉任务，包含 " << base64Images.size() << " 张照片。" << std::endl;
                } else {
                    std::cerr << "[AI Worker] 视觉任务 JSON 解析失败: " << errs << std::endl;
                    continue; // 数据损坏，直接跳过本次任务
                }
            } else {
                std::cout << "[AI Worker] 正在处理普通文本任务: " << userPrompt << std::endl;
            }

            // --- 2. 核心逻辑：智能 RAG 检索增强 ---
            std::string backgroundContext = "";
            
            if (userPrompt.find("哪些") != std::string::npos || 
                userPrompt.find("所有") != std::string::npos || 
                userPrompt.find("列表") != std::string::npos ||
                userPrompt.find("清单") != std::string::npos) {
                
                backgroundContext = db.getAllDevicesSummary();
                std::cout << "[AI Worker] 检测到全局查询意图，已提取设备列表。" << std::endl;
            } 
            else {
                std::regex id_regex("[A-Za-z0-9_]+");
                std::smatch match;
                if (std::regex_search(userPrompt, match, id_regex)) {
                    std::string possibleId = match.str();
                    DeviceData info = db.getDeviceInfo(possibleId);
                    if (!info.id.empty()) {
                        // 【新增逻辑】：将数据库的英文 ENUM 状态翻译为友好的中文
                        std::string displayStatus = info.status;
                        if (info.status == "NORMAL") displayStatus = "运行正常";
                        else if (info.status == "FAULTY") displayStatus = "设备故障/异常";
                        else displayStatus = "状态未知/离线";

                        backgroundContext = "【已知实验设备实时信息】:\n"
                                          " - 编号: " + info.id + "\n"
                                          " - 名称: " + info.name + "\n"
                                          " - 状态: " + displayStatus + "\n"
                                          " - 更新时间: " + info.last_update + "\n";
                        std::cout << "[AI Worker] 查到特定设备数据，已增强 Prompt。" << std::endl;
                    }
                }
            }

            std::string finalPrompt = "";
            if (!backgroundContext.empty()) {
                finalPrompt = "【当前实验室实时状态参考】：\n" + backgroundContext + "\n\n" +
                              "【用户问题】：" + userPrompt + "\n\n" +
                              "（请结合参考信息回答，如果参考信息中没有对应内容，请利用你的通用知识解答。）";
            } else {
                finalPrompt = userPrompt;
            }

            // --- 3. 调用大模型 ---
            std::string answer = "";
            if (isVisionTask) {
                std::cout << "[AI Worker] 触发真实视觉诊断！正在调用 Qwen-VL API..." << std::endl;
                
                // 实例化视觉大模型
                QwenVisionProvider visionLLM("sk-67c33d5870594b0196fa9a4a8e33bc5f"); 
                
                // 发送图片数组和 Prompt 给阿里云并获取诊断结果
                answer = visionLLM.askVision(base64Images, finalPrompt);
            } else {
                // 如果是纯文本聊天，依然走原来的 DeepSeek
                answer = llm_->ask(finalPrompt);
            }

            // --- 4. 处理结果并封装为 HTTP 格式 ---
            if (!answer.empty()) {
                // 成功响应：加入跨域头和长度
                std::string httpResponse = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain; charset=utf-8\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Content-Length: " + std::to_string(answer.size()) + "\r\n"
                    "\r\n" + 
                    answer;

                task.conn->send(httpResponse);
                
                // 注意：由于转成了英文，这里写回故障记录的触发词依然兼容 "故障" 和 "FAULTY"
                if (finalPrompt.find("FAULTY") != std::string::npos || 
                    finalPrompt.find("故障") != std::string::npos) {
                    std::regex id_regex("[A-Za-z0-9_]+");
                    std::smatch match;
                    if (std::regex_search(userPrompt, match, id_regex)) { 
                        db.addFaultRecord(match.str(), "AI自动诊断: " + answer);
                    }
                }
            } else {
                // 错误响应：加入跨域头和长度，防止浏览器死等
                std::string errorMsg = "⚠️ AI 思考时发生错误，请稍后再试。";
                std::string errorResponse = 
                    "HTTP/1.1 500 Internal Server Error\r\n"
                    "Content-Type: text/plain; charset=utf-8\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Content-Length: " + std::to_string(errorMsg.size()) + "\r\n"
                    "\r\n" + 
                    errorMsg;
                task.conn->send(errorResponse);
            }

            std::cout << "[AI Worker] 任务处理完成。" << std::endl;
        }
    }

    std::unique_ptr<BaseLLM> llm_;
    std::queue<AITask> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    bool stop_;
};