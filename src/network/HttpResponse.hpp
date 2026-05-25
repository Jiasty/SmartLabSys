// src/network/HttpResponse.hpp
#pragma once
#include <string>
#include <unordered_map>

class HttpResponse {
public:
    HttpResponse() : statusCode_(200), statusMessage_("OK") {
        // 默认设置为 JSON 格式返回，解决中文乱码
        headers_["Content-Type"] = "application/json; charset=utf-8";
        // 允许跨域请求 (CORS)，方便后续 Vue3 前端本地联调
        headers_["Access-Control-Allow-Origin"] = "*";
    }

    void setStatusCode(int code, const std::string& msg) {
        statusCode_ = code;
        statusMessage_ = msg;
    }

    void addHeader(const std::string& key, const std::string& value) {
        headers_[key] = value;
    }

    void setBody(const std::string& body) {
        body_ = body;
        headers_["Content-Length"] = std::to_string(body_.size());
    }

    // 核心：将这些属性拼接成符合 HTTP 标准的文本流
    std::string toString() const {
        std::string response = "HTTP/1.1 " + std::to_string(statusCode_) + " " + statusMessage_ + "\r\n";
        
        for (const auto& header : headers_) {
            response += header.first + ": " + header.second + "\r\n";
        }
        
        response += "\r\n"; // 头部和响应体之间必须有一个空行
        response += body_;
        
        return response;
    }

private:
    int statusCode_;
    std::string statusMessage_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};