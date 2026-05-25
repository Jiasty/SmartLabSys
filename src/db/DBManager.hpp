#pragma once
#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <iostream>

// 匹配新的规范化三表架构的设备数据结构体
struct DeviceData {
    std::string id;
    std::string name;
    std::string lab;          // 所属实验室名称 (通过 LEFT JOIN 关联拉取)
    std::string status;
    std::string last_update;
};

class DBManager {
public:
    DBManager() {
        mysql_init(&conn_);
    }

    ~DBManager() {
        mysql_close(&conn_);
    }

    // 连接数据库
    bool connect() {
        if (mysql_real_connect(&conn_, "localhost", "root", "PGMG7464", "lab_system", 3306, NULL, 0)) {
            mysql_set_character_set(&conn_, "utf8mb4");
            return true;
        }
        std::cerr << "MySQL Connection Error: " << mysql_error(&conn_) << std::endl;
        return false;
    }

    // 根据 ID 查询设备详细状态（用于 AI 的 RAG 检索，升级为连表查询）
    DeviceData getDeviceInfo(const std::string& deviceId) {
        DeviceData data;
        // 使用 LEFT JOIN 联合查询 device_info 和 lab_info 表
        std::string sql = "SELECT d.id, d.name, l.lab_name, d.status, d.last_update "
                          "FROM device_info d "
                          "LEFT JOIN lab_info l ON d.lab_id = l.lab_id "
                          "WHERE d.id = '" + deviceId + "'";
        
        if (mysql_query(&conn_, sql.c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(&conn_);
            if (MYSQL_ROW row = mysql_fetch_row(res)) {
                // 安全校验，防止 MySQL 返回 NULL 导致 std::string 赋值崩溃
                data.id = row[0] ? row[0] : "";
                data.name = row[1] ? row[1] : "";
                data.lab = row[2] ? row[2] : "未分配实验室";
                data.status = row[3] ? row[3] : "";
                data.last_update = row[4] ? row[4] : "";
            }
            mysql_free_result(res);
        }
        return data;
    }

    // 记录 AI 产生的故障分析结果到 fault_record 表
    void addFaultRecord(const std::string& deviceId, const std::string& desc) {
        std::string sql = "INSERT INTO fault_record (device_id, description) VALUES ('" 
                          + deviceId + "', '" + desc + "')";
        mysql_query(&conn_, sql.c_str());
    }

    // 获取纯文本摘要（同样升级连表查询，让 AI 在 RAG 检索时知道设备到底在哪个物理房间）
    std::string getAllDevicesSummary() {
        std::string summary = "当前实验室设备清单：\n";
        std::string sql = "SELECT d.id, d.name, l.lab_name, d.status "
                          "FROM device_info d "
                          "LEFT JOIN lab_info l ON d.lab_id = l.lab_id";
        
        if (mysql_query(&conn_, sql.c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(&conn_);
            while (MYSQL_ROW row = mysql_fetch_row(res)) {
                std::string id = row[0] ? row[0] : "";
                std::string name = row[1] ? row[1] : "";
                std::string lab = row[2] ? row[2] : "未分配";
                std::string status = row[3] ? row[3] : "";
                
                summary += " - 设备ID: " + id + 
                           ", 名称: " + name + 
                           ", 所属实验室: " + lab +
                           ", 状态: " + status + "\n";
            }
            mysql_free_result(res);
        }
        return summary;
    }

    // 专门为前端后台管理页面提供 JSON 格式的数据接口（同步升级为 LEFT JOIN 连表）
    std::string getAllDevicesJson() {
        std::string jsonStr = "[";
        std::string sql = "SELECT d.id, d.name, l.lab_name, d.status, d.last_update "
                          "FROM device_info d "
                          "LEFT JOIN lab_info l ON d.lab_id = l.lab_id";
        
        if (mysql_query(&conn_, sql.c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(&conn_);
            bool first = true;
            
            while (MYSQL_ROW row = mysql_fetch_row(res)) {
                // ==========================================
                // 【核心修复】：把这里的逗号拼接逻辑改一下
                if (!first) {
                    jsonStr += ","; 
                }
                // ==========================================
                
                std::string id = row[0] ? row[0] : "";
                std::string name = row[1] ? row[1] : "";
                std::string lab = row[2] ? row[2] : "未分配实验室"; 
                std::string status = row[3] ? row[3] : "UNKNOWN";
                std::string last_update = row[4] ? row[4] : "";

                jsonStr += "{";
                jsonStr += "\"id\":\"" + id + "\",";
                jsonStr += "\"name\":\"" + name + "\",";
                jsonStr += "\"lab\":\"" + lab + "\",";
                jsonStr += "\"status\":\"" + status + "\",";
                jsonStr += "\"time\":\"" + last_update + "\"";
                jsonStr += "}";
                
                // 【新增】：在循环的最后，把 first 置为 false
                first = false;
            }
            mysql_free_result(res);
        } else {
            std::cerr << "MySQL Query Error in getAllDevicesJson: " << mysql_error(&conn_) << std::endl;
        }
        
        jsonStr += "]";
        return jsonStr;
    }

private:
    MYSQL conn_;
};