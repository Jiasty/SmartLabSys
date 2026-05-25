// src/ai/DeepSeekProvider.hpp
#pragma once
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "BaseLLM.h"
#include <httplib.h>
#include <jsoncpp/json/json.h>
#include <iostream>
#include <sstream>

class DeepSeekProvider : public BaseLLM {
public:
    explicit DeepSeekProvider(const std::string& apiKey) : apiKey_(apiKey) {}

    std::string ask(const std::string& prompt) override {
        // DeepSeek API 域名
        httplib::Client cli("https://api.deepseek.com");
        cli.set_follow_location(true);

        // 1. 构造 JSON 请求体
        Json::Value root;
        root["model"] = "deepseek-chat";
        root["stream"] = false;
        root["temperature"] = 0.0;

        // --- 核心改进：添加 System Prompt ---
        // 第一条消息：设定 AI 的身份和行为准则
        Json::Value systemMsg;
        systemMsg["role"] = "system";
        systemMsg["content"] = 
            "你是一个名为‘智理’的实验室智能助手。你的大脑由两部分组成：\n"
            "1. 【专业实验室数据库】：你连接了 lab_system。当用户查询特定设备（如 DEV001）或实验室清单时，请务必以数据库提供的实时数据为准。\n"
            "2. 【全知大模型知识】：你拥有丰富的计算机科学、历史和通用常识。当用户询问非实验室具体业务的问题（如‘计算机之父’、‘如何学习C++’）时，请直接利用你的通用知识热情回答。\n\n"
            "【回复原则】：\n"
            "- 结合实际：如果用户问‘DEV001’，结合数据库说它是示波器且正处于故障中。\n"
            "- 灵活变通：如果用户问‘计算机设备有哪些’，先列举常见的（示波器、开发板等），再顺带提一句数据库里目前已登记了哪些。\n"
            "- 拒绝生硬：不要总是复读‘我只负责管理设备’，要像真正的助手一样交流。"
            "- 首要规则：只要你的‘参考信息’中包含数据库查询结果，你必须直接、立即回答数据内容。\n"
            "- 如果没有任何参考信息，再进行通用的知识回答。";
        root["messages"].append(systemMsg);

        // 第二条消息：用户的实际提问
        Json::Value userMsg;
        userMsg["role"] = "user";
        userMsg["content"] = prompt;
        root["messages"].append(userMsg);

        Json::StreamWriterBuilder writer;
        std::string payload = Json::writeString(writer, root);

        // 2. 设置 HTTP Header
        httplib::Headers headers = {
            {"Authorization", "Bearer " + apiKey_},
            {"Content-Type", "application/json"}
        };

        // 3. 发起请求
        auto res = cli.Post("/chat/completions", headers, payload, "application/json");

        // 4. 解析结果
        if (res && res->status == 200) {
            Json::CharReaderBuilder reader;
            Json::Value resJson;
            std::istringstream s(res->body);
            std::string errs;

            if (Json::parseFromStream(reader, s, &resJson, &errs)) {
                return resJson["choices"][0]["message"]["content"].asString();
            }
            return "[JSON解析失败]";
        } else {
            return "[DeepSeek API 错误] 状态码: " + (res ? std::to_string(res->status) : "超时");
        }
    }

private:
    std::string apiKey_;
};