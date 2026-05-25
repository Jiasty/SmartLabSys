#pragma once
#include <string>
#include <iostream>
#include <memory>
#include <array>
#include <vector>
#include <sstream>
#include <jsoncpp/json/json.h> 

class QwenVisionProvider {
public:
    explicit QwenVisionProvider(const std::string& apiKey) : apiKey_(apiKey) {}

    // 核心修改：接收 std::vector<std::string> 多图数组
    std::string askVision(const std::vector<std::string>& base64Images, const std::string& prompt) {
        Json::Value root;
        root["model"] = "qwen-vl-plus";
        
        Json::Value contentArray(Json::arrayValue);

        // 核心修改：遍历每一张图片，按阿里云格式装填
        for (const auto& imgBase64 : base64Images) {
            Json::Value imageObj;
            imageObj["type"] = "image_url";
            Json::Value urlObj;
            urlObj["url"] = imgBase64;
            imageObj["image_url"] = urlObj;
            contentArray.append(imageObj);
        }

        // 把文本内容加在图片后面
        Json::Value textObj;
        textObj["type"] = "text";
        textObj["text"] = prompt;
        contentArray.append(textObj);

        Json::Value messageObj;
        messageObj["role"] = "user";
        messageObj["content"] = contentArray;
        root["messages"].append(messageObj);

        Json::FastWriter writer;
        std::string jsonBody = writer.write(root);

        std::string tempFile = "/tmp/qwen_req.json";
        FILE* fp = fopen(tempFile.c_str(), "w");
        if (fp) {
            fwrite(jsonBody.c_str(), 1, jsonBody.size(), fp);
            fclose(fp);
        } else {
            return "⚠️ 后端临时文件写入失败。";
        }

        std::string cmd = "curl -s -X POST https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions "
                          "-H 'Authorization: Bearer " + apiKey_ + "' "
                          "-H 'Content-Type: application/json' "
                          "-d @" + tempFile;

        std::cout << "[Qwen-VL] 正在上传 " << base64Images.size() << " 张照片至云端视觉大模型..." << std::endl;

        std::array<char, 4096> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        
        if (!pipe) return "⚠️ 调用底层 curl 失败。";
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }

        Json::CharReaderBuilder reader;
        Json::Value responseJson;
        std::string errs;
        std::istringstream s(result);
        
        if (Json::parseFromStream(reader, s, &responseJson, &errs)) {
            if (responseJson.isMember("choices")) {
                return responseJson["choices"][0]["message"]["content"].asString();
            } else {
                std::cerr << "[Qwen-VL API 报错]: " << result << std::endl;
                return "⚠️ 接口拒绝了请求，请检查 API Key 或图片质量。";
            }
        }

        return "⚠️ 解析视觉模型返回结果失败。";
    }

private:
    std::string apiKey_;
};