#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <string>
#include <vector>
#include <iostream>

class VisionManager {
public:
    std::string decodeQRCode(const std::vector<uchar>& buffer) {
        try {
            // 核心拦截：如果数据为空，直接拒绝，防止 OpenCV 断言崩溃
            if (buffer.empty()) {
                std::cerr << "[Vision] 收到空图片数据，拒绝解码。" << std::endl;
                return "";
            }

            cv::Mat img = cv::imdecode(buffer, cv::IMREAD_COLOR);
            if (img.empty()) {
                std::cerr << "[Vision] 图片解码失败，数据可能损坏或不完整。" << std::endl;
                return "";
            }

            cv::QRCodeDetector detector;
            std::string data = detector.detectAndDecode(img);
            
            if (!data.empty()) {
                std::cout << "[Vision] 成功识别二维码内容: " << data << std::endl;
            }
            return data;
        } catch (const std::exception& e) {
            std::cerr << "[Vision] OpenCV 异常: " << e.what() << std::endl;
            return "";
        }
    }
};