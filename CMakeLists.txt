cmake_minimum_required(VERSION 3.10)
project(SmartLabSys)

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# 1. 设置 C++17 标准（必须，因为我们用了 std::unique_ptr, std::regex 等）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 2. 开启 HTTPS 支持宏（让 cpp-httplib 知道我们要用 SSL）
add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)

# 3. 添加头文件搜索路径
# 包含项目内部 include、系统 jsoncpp 路径和 MySQL 路径
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    /usr/include/jsoncpp
    /usr/include/mysql
    ${OpenCV_INCLUDE_DIRS}
)

# 4. 收集所有的源文件 (.cpp)
# 自动扫描 src 目录下所有子目录的 cpp 文件
file(GLOB_RECURSE ALL_SOURCES "src/*.cpp")

# 5. 生成可执行文件
add_executable(SmartLabSys main.cpp ${ALL_SOURCES})

# 6. 链接各大“神兽”库（顺序很重要！）
target_link_libraries(SmartLabSys
    ${OpenCV_LIBS} 
    pthread      # 多线程支持
    jsoncpp      # JSON 解析
    mysqlclient  # MySQL 数据库连接
    ssl          # OpenSSL 加密
    crypto       # OpenSSL 算法库
)