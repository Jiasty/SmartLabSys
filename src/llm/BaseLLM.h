// src/ai/BaseLLM.h
#pragma once
#include <string>

class BaseLLM {
public:
    virtual ~BaseLLM() = default;
    
    // 纯虚函数：子类必须实现
    // 传入问题 prompt，返回大模型的纯文本回答
    virtual std::string ask(const std::string& prompt) = 0;
};