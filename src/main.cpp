//
// Created by siqi on 2025/4/8.
//
#include "scm_parser.h"
#include "database_manager.h"
#include <fstream>
#include <sstream>
#include <sqlite3.h>

int main()
{
    // 读取SCM文件内容
//    std::ifstream scmFile("../propdb.scm");
//    std::stringstream buffer;
//    buffer << scmFile.rdbuf();
//    std::string scmContent = buffer.str();
    
    // 解析SCM文件
    ScmParser parser;
    auto materials = parser.parse("propdb.scm");
    
    // 创建数据库连接
    sqlite3* db;
    int rc = sqlite3_open("materials.db", &db);
    
    // 创建材料表
    const char* createTableSQL = "CREATE TABLE IF NOT EXISTS materials ("
                                "name TEXT PRIMARY KEY,"
                                "type TEXT,"
                                "properties TEXT"
                                ");";
    sqlite3_exec(db, createTableSQL, 0, 0, 0);
    
    // 插入材料数据
    CFD_MaterialDB::DatabaseManager dbManager("materials.db");
    for (const auto& material : materials) {
        dbManager.insertMaterial(material);
    }
    
    sqlite3_close(db);
    return 0;
}