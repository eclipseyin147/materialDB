#include "database_manager.h"
#include "material.h"
#include <iostream>

using namespace CFD_MaterialDB;

DatabaseManager::DatabaseManager(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error("无法打开数据库: " + std::string(sqlite3_errmsg(db)));
    }
}

DatabaseManager::~DatabaseManager() {
    sqlite3_close(db);
}

void DatabaseManager::createTables() {
    // 检查表是否存在
    const char* checkTableSql = "SELECT name FROM sqlite_master WHERE type='table' AND name='materials';";
    
    // 创建表SQL
    const char* createTableSql = 
        "CREATE TABLE IF NOT EXISTS materials ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT UNIQUE NOT NULL,"
        "type INTEGER NOT NULL,"
        "properties TEXT NOT NULL);";
    
    // 检查表结构SQL
    const char* checkSchemaSql = "PRAGMA table_info(materials);";
    
    try {
        // 执行表检查
        executeSQL(checkTableSql);
        
        // 如果表不存在则创建
        executeSQL(createTableSql);
        
        // 验证表结构
        executeSQL(checkSchemaSql);
    } catch (const std::exception& e) {
        throw std::runtime_error("初始化数据库表失败: " + std::string(e.what()));
    }
}

void DatabaseManager::insertMaterial(const Material& material) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO materials (name, type, properties) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("准备SQL语句失败: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_bind_text(stmt, 1, material.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, static_cast<int>(material.type.state));
    std::string json = material.toJson();
    sqlite3_bind_text(stmt, 3, json.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("插入数据失败: " + std::string(sqlite3_errmsg(db)));
    }
    sqlite3_finalize(stmt);
}

void DatabaseManager::executeSQL(const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string error = "SQL执行错误: " + std::string(errMsg);
        sqlite3_free(errMsg);
        throw std::runtime_error(error);
    }
}

Material DatabaseManager::getMaterialByName(const std::string& name) {
    Material material;
    sqlite3_stmt* stmt;
    const char* sql = "SELECT * FROM materials WHERE name = ?;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("准备SQL语句失败: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        material.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        material.type.state = static_cast<MaterialState>(sqlite3_column_int(stmt, 2));
        material.fromJson(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
    } else {
        sqlite3_finalize(stmt);
        throw std::runtime_error("未找到材料: " + name);
    }
    
    sqlite3_finalize(stmt);
    return material;
}

void DatabaseManager::updateMaterial(const Material& material) {
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE materials SET type = ?, properties = ? WHERE name = ?;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("准备SQL语句失败: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_bind_int(stmt, 1, static_cast<int>(material.type.state));
    std::string json = material.toJson();
    sqlite3_bind_text(stmt, 2, json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, material.name.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("更新数据失败: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_finalize(stmt);
}

void DatabaseManager::deleteMaterial(const std::string& name) {
    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM materials WHERE name = ?;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("准备SQL语句失败: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("删除数据失败: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_finalize(stmt);
}

int DatabaseManager::callback(void* data, int argc, char** argv, char** colName) {
    Material* material = static_cast<Material*>(data);
    for (int i = 0; i < argc; i++) {
        if (strcmp(colName[i], "name") == 0) {
            material->name = argv[i];
        } else if (strcmp(colName[i], "type") == 0) {
            material->type = static_cast<MaterialType>(atoi(argv[i]));
        } else if (strcmp(colName[i], "properties") == 0) {
            material->fromJson(argv[i]);
        }
    }
    return 0;
}