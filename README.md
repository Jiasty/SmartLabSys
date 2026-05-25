# SmartLabSys
Intelligent Laboratory Equipment Management System




# 当前需要改进的
### 1. 后端实现 vs 设计书
- 设计书强调：
  - “C++ 大模型 SDK + OpenCV 为核心”
  - “Linux 后端开发”
  - “基于 epoll + 线程池 实现高并发服务”
- 实际项目：
  - main.cpp 里是自定义的 `EventLoop` + `TcpServer`，确实是 C++ 后端网络服务
  - 但没有看到 OpenCV 使用代码
  - 大模型调用是通过 HTTP 接口（`DeepSeekProvider`、`QwenVisionProvider`）远程 API，而不是本地 C++ SDK 直接调用本地模型

### 2. 视觉功能 vs 设计书
- 设计书提到：
  - “OpenCV 核心应用：设备标签识别、设备外观状态检测、提取关键特征”
- 实际项目：
  - 前端 index.html 有摄像头拍照和上传图片功能
  - 后端 AITaskQueue.hpp 会将图片 Base64 发给 `QwenVisionProvider`
  - 但代码里没有看到本地图像处理、OpenCV 识别、标签识别或特征提取逻辑

### 3. 前端功能 vs 设计书
- 设计书要求：
  - “设备扫码盘点”
  - “状态查询”
  - “故障上报”
  - “咨询问答”
  - “操作手册查看”
- 实际项目：
  - index.html 实现了：
    - 设备台账列表
    - 设备状态显示
    - AI 智能助手对话
    - 摄像头拍照上传
  - 但没有明显的：
    - QR 码/条码扫码盘点
    - 专门的“故障上报表单”
    - “操作手册查看”页面或文档浏览功能

### 4. 设备数据库 / 知识库 vs 设计书
- 设计书说要构建“设备知识库”
- 实际项目：
  - DBManager.hpp 连接 `lab_system` 数据库
  - 提供 `getAllDevicesJson()`、`getDeviceInfo()`、`addFaultRecord()` 等接口
  - 说明有设备台账、状态、故障记录
  - 但这一“知识库”更像是传统设备表 + AI prompt 增强，不是完整的设备知识库体系

### 5. 大模型与智能部分
- 设计书目标是“统一调用本地轻量化大模型”
- 实际项目是“用远程大模型 API 做智能问答/视觉诊断”
- AI 逻辑存在“RAG 检索增强”思路，但核心模型仍靠网络 API

## 结论
这个项目已经实现了：
- C++ 后端服务
- Web 前端界面
- 数据库设备台账查询
- AI 问答与图像诊断入口

但和设计书的差异主要在于：
- 没有本地 OpenCV 图像处理
- 没有本地 C++ 大模型 SDK
- 前端功能没有完全覆盖“扫码盘点、故障上报、操作手册查看”
- 视觉识别更多依赖远程模型 API，而不是本地视觉算法

如果你想，我可以继续帮你把这个项目对照设计书补齐缺失点，或者给出一个更具体的改进建议清单。





# 具体更改
### 1. 让项目更贴近“C++ SDK + OpenCV + 本地视觉”设计
当前问题：
- 目前视觉诊断由前端拍照后，后端 QwenVisionProvider.hpp 把 Base64 直接发到云端接口。
- 没有本地 OpenCV 图像处理、设备标签识别、缺陷检测逻辑。

建议：
- 在 vision 新增模块，比如 `VisionManager.hpp/cpp`
- 使用 OpenCV 实现：
  - 图像预处理：去噪、灰度、边缘检测
  - 设备标签/二维码识别：`cv::QRCodeDetector`
  - 设备外观状态检测：简单的模板匹配、颜色异常、裂纹检测
- 在 main.cpp 里 `/api/vision` 路由增加本地处理步骤：
  - 先本地分析图片是否存在明显损伤
  - 再把结果或关键特征发送给 `QwenVisionProvider` 做“智能诊断”
- 这样就能把“OpenCV 核心应用”部分落地。

---

### 2. 完善“设备扫码盘点 + 故障上报 + 操作手册”前端功能
当前问题：
- 前端 index.html 有设备列表和 AI 聊天，但没有“扫码盘点”和“操作手册查看”这两个核心功能模块。
- “故障上报”只存在 AI 诊断入口，没有结构化上报流程。

建议：
- index.html 增加：
  - `扫码盘点` 按钮
  - `故障上报` 表单或弹窗，包含：设备 ID、故障类型、描述、照片附件
  - `操作手册` 页面/弹窗，展示设备说明文档
- 加入 JS 库：
  - `jsQR` / `ZXing` 实现摄像头二维码/条码扫描
- 设备操作流程：
  - 扫码后自动填充设备 ID
  - 显示该设备当前状态和最近故障记录
  - 允许直接提交“故障上报”

---

### 3. 强化后端数据库接口和数据同步
当前已有：
- DBManager.hpp 提供 `getAllDevicesJson()`、`getDeviceInfo()`、`addFaultRecord()`

建议补足：
- 在 DBManager.hpp 增加：
  - `bool updateDeviceStatus(const std::string& id, const std::string& status)`
  - `bool addDevice(const DeviceData& device)` / `bool deleteDevice(...)`
  - `std::string getDeviceManual(const std::string& id)`（返回操作手册文本或 URL）
- 在 main.cpp 新增 HTTP 路由：
  - `/api/device` 查询单个设备详情
  - `/api/fault-report` 接收故障上报
  - `/api/manual` 返回操作手册内容
- 这样前端就能真正做到：
  - 设备状态查询
  - 上报故障
  - 查看手册

---

### 4. 弥补“线程池 + 高并发”实现细节
当前问题：
- main.cpp 使用 `TcpServer` 的 4 个线程监听，但 AI 任务队列实际只有一个工作线程 `AITaskQueue::worker_`。
- 设计书里强调“高并发服务”。

建议：
- 修改 AITaskQueue.hpp
- 用 `std::vector<std::thread> workers_` 变成线程池，比如 2~4 个工作线程
- 保持任务队列共享，避免单线程成为瓶颈
- 这样后端更符合“Linux 后端 + 线程池高并发”的描述

---

### 5. 让“大模型 SDK”更真实
当前问题：
- 你现在使用的是远程 HTTP API 接口，而不是本地 C++ SDK。
- 设计书里说“基于 C++ SDK 封装大模型”。

建议：
- 把 DeepSeekProvider.hpp 和 QwenVisionProvider.hpp 改成封装层接口
- 以后如果需要替换成本地 SDK，只需改写这两个 provider，不影响上层逻辑
- 具体做法：
  - 将 `DeepSeekProvider` 改为通用 `LLMProvider`
  - 用 `BaseLLM` 作为抽象
  - 本地 SDK 实现时直接继承 `BaseLLM`

---

### 6. 直接落地的改动优先级
如果你要做最少改动但效果最大，我建议先做这两项：

1. index.html 添加“扫码盘点”和“故障上报”界面
   - 这部分用户可见，差异最大
2. vision 加入 OpenCV 图像预处理
   - 让“视觉诊断”从“纯云API”变为“本地视觉 + AI诊断”

---

## 具体文件建议
- main.cpp
  - 新增 `/api/fault-report`、`/api/manual`、`/api/device`
  - 视觉路由加入本地 OpenCV处理
- AITaskQueue.hpp
  - 改成线程池
  - 增加图片任务区分、设备 ID 匹配更精准
- DBManager.hpp
  - 增加设备手册/故障上报接口
- `src/vision/VisionManager.hpp/cpp`
  - 实现 OpenCV 本地视觉能力
- index.html
  - 添加扫码盘点、故障上报、操作手册界面
  - 保持现有设备列表和 AI 聊天逻辑

---

## 总结
你现在的项目已经有“智能实验室管理系统”的雏形，但距离设计书里的“本地 OpenCV + SDK +扫码盘点+故障上报+操作手册”还有差距。  
如果你愿意，我可以继续帮你一步一步写出“扫码盘点 + 故障上报”界面，或者直接帮你改 main.cpp / DBManager.hpp / index.html。