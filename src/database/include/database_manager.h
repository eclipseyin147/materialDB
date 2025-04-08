#pragma once
#include <string>
#include <sqlite3.h>
#include "material.h"
namespace CFD_MaterialDB {

class DatabaseManager {
public:
    DatabaseManager(const std::string& dbPath);
    ~DatabaseManager();

    void createTables();
    void insertMaterial(const Material& material);
    Material getMaterialByName(const std::string& name);
    void updateMaterial(const Material& material);
    void deleteMaterial(const std::string& name);

private:
    sqlite3* db;
    
    void executeSQL(const std::string& sql);
    static int callback(void* data, int argc, char** argv, char** colName);
};

} // namespace CFD_MaterialDB