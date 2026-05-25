// src/ai/OllamaProvider.hpp
#pragma once
#include "BaseLLM.h"
#include <httplib.h>
#include <jsoncpp/json/json.h>
#include <iostream>

class OllamaProvider : public BaseLLM {
public:
    OllamaProvider(const std::string& host = "localhost", 
                   int port = 11434, 
                   const std::string& modelName = "qwen")
        : host_(host), port_(port), modelName_(modelName) {}

    std::string ask(const std::string& prompt) override {
        httplib::Client cli(host_, port_);
        
        // 1. 使用 jsoncpp 构造发给 Ollama 的请求体
        Json::Value reqJson;
        reqJson["model"] = modelName_;
        reqJson["prompt"] = prompt;
        reqJson["stream"] = false;

        Json::StreamWriterBuilder writer;
        std::string payload = Json::writeString(writer, reqJson);

        // 2. 发起 HTTP POST 请求
        auto res = cli.Post("/api/generate", payload, "application/json");

        // 3. 处理响应
        if (res && res->status == 200) {
            Json::CharReaderBuilder reader;
            Json::Value resJson;
            std::string errs;
            std::istringstream s(res->body);
            
            // 解析返回的 JSON 字符串
            if (Json::parseFromStream(reader, s, &resJson, &errs)) {
                return resJson["response"].asString();
            } else {
                return "[JSON Parse Error] " + errs;
            }
        } else {
            std::string err = res ? std::to_string(res->status) : "Connection Failed";
            return "[HTTP Request Error] " + err;
        }
    }

private:
    std::string host_;
    int port_;
    std::string modelName_;
};