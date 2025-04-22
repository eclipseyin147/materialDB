#pragma once
#include <string>
#include <sqlite3.h>
#include "material.h"
#include <openssl/md5.h>
#include <curl/curl.h>
#include <sstream>
#include <iomanip>
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
    static std::string TranslateText(const std::string &text);

private:
    sqlite3* db;
    
    void executeSQL(const std::string& sql);
    static int callback(void *contents, size_t size, size_t nmemb, std::string *s);



    static std::string md5(const std::string &text);

};

} // namespace CFD_MaterialDB