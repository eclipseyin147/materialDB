//
// Created by siqi on 2025/4/8.
//
#include "scm_parser.h"
#include "database_manager.h"
#include <fstream>
#include <sstream>
// 移除标准SQLite头文件，只保留SQLCipher头文件

int main() {
    // 解析SCM文件
    ScmParser parser;
    auto materials = parser.parse("propdb.scm");
    // 创建数据库连接
    if (std::filesystem::exists("materials.db")) {
        std::filesystem::remove("materials.db");
    }
    //sqlite3_key(db, "MyStrongPassword!", 17); // 设置密钥
    // 使用try-catch块处理可能的异常
    try {
        CFD_MaterialDB::DatabaseManager matDict("materialsDict.db");
        // 确保materialsDict.db有正确的表结构
        matDict.createTables();

        // 插入材料数据
        CFD_MaterialDB::DatabaseManager dbManager("materials.db");
        dbManager.createTables();
        for (auto &material: materials) {
            try {
                // 尝试从字典获取中文名
                material.chinese_name = matDict.getMaterialByName(material.name).chinese_name;
            } catch (const std::exception &e) {
                // 如果获取失败，使用翻译API或默认值
                std::cerr << "获取中文名失败: " << e.what() << std::endl;
                //material.chinese_name = CFD_MaterialDB::DatabaseManager::TranslateText(material.name);
                material.chinese_name = material.name; // 临时使用英文名作为中文名
            }

            ///< translate name into chinese
            std::cout << "Chinese name: " << material.chinese_name << std::endl;
            dbManager.insertMaterial(material);
        }
    } catch (const std::exception &e) {
        std::cerr << "处理数据库时发生错误: " << e.what() << std::endl;
    }
    return 0;
}