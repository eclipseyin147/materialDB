#include "database_manager.h"
#include "material.h"
#include <iostream>
#include <nlohmann/json.hpp>

using namespace CFD_MaterialDB;

DatabaseManager::DatabaseManager(const std::string &dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error("无法打开数据库: " + std::string(sqlite3_errmsg(db)));
    }
}

DatabaseManager::~DatabaseManager() {
    sqlite3_close(db);
}

void DatabaseManager::createTables() {
    // 检查表是否存在
    const char *checkTableSql = "SELECT name FROM sqlite_master WHERE type='table' AND name='materials';";

    // 创建表SQL
    const char *createTableSql =
            "CREATE TABLE IF NOT EXISTS materials ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT UNIQUE NOT NULL,"
            "chinese_name TEXT UNIQUE NOT NULL,"
            "type INTEGER NOT NULL,"
            "properties TEXT NOT NULL);";

    // 检查表结构SQL
    const char *checkSchemaSql = "PRAGMA table_info(materials);";

    try {
        // 执行表检查
        executeSQL(checkTableSql);

        // 如果表不存在则创建
        executeSQL(createTableSql);

        // 验证表结构
        executeSQL(checkSchemaSql);
    } catch (const std::exception &e) {
        throw std::runtime_error("初始化数据库表失败: " + std::string(e.what()));
    }
}

void DatabaseManager::insertMaterial(const Material &material) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO materials (name, type, chinese_name, properties) VALUES (?, ?, ?, ?);";
    try {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("SQL preparation failed : " + std::string(sqlite3_errmsg(db)));
        }
        sqlite3_bind_text(stmt, 1, material.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, static_cast<int>(material.type.state));
        sqlite3_bind_text(stmt, 3, material.chinese_name.c_str(), -1, SQLITE_TRANSIENT);
        std::string json = nlohmann::json(material).dump();
        sqlite3_bind_text(stmt, 4, json.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Material inserted failed: " + std::string(sqlite3_errmsg(db)));
        }
    }
    catch (const std::exception &e) {
        throw std::runtime_error("Material inserted failed : " + std::string(e.what()));
    }

    sqlite3_finalize(stmt);
}

void DatabaseManager::executeSQL(const std::string &sql) {
    char *errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string error = "SQL执行错误: " + std::string(errMsg);
        sqlite3_free(errMsg);
        throw std::runtime_error(error);
    }
}

Material DatabaseManager::getMaterialByName(const std::string &name) {
    Material material;
    sqlite3_stmt *stmt;
    const char *sql = "SELECT * FROM materials WHERE name = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("准备SQL语句失败: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        material.name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        material.type.state = static_cast<MaterialState>(sqlite3_column_int(stmt, 2));
        ///< TODO
        //material.fromJson(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
    } else {
        sqlite3_finalize(stmt);
        throw std::runtime_error("未找到材料: " + name);
    }

    sqlite3_finalize(stmt);
    return material;
}

void DatabaseManager::updateMaterial(const Material &material) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE materials SET type = ?, properties = ? WHERE name = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("准备SQL语句失败: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_bind_int(stmt, 1, static_cast<int>(material.type.state));
    std::string json = nlohmann::json(material).dump();
    sqlite3_bind_text(stmt, 2, json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, material.name.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("更新数据失败: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_finalize(stmt);
}

void DatabaseManager::deleteMaterial(const std::string &name) {
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM materials WHERE name = ?;";

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

int DatabaseManager::callback(void *contents, size_t size, size_t nmemb, std::string *s) {
    size_t newLength = size * nmemb;
    s->append((char *) contents, newLength);
    return newLength;
}

std::string DatabaseManager::TranslateText(const std::string &text) {
    CURL *curl = curl_easy_init();
    std::string result;
    std::string BAIDU_APPID = "20250422002339827";
    std::string BAIDU_KEY = "2hEn69vKVRn_L9bhH2jw";
    std::string TRANSLATE_URL = "https://fanyi-api.baidu.com/api/trans/vip/translate";

    if (!curl) {
        throw std::runtime_error("CURL failed");
    }

#if(0)
    // 初始化随机数种子
    srand(static_cast<unsigned int>(time(nullptr)));
    
    // 生成随机盐和签名
    std::string salt = std::to_string(rand());
    std::string sign = BAIDU_APPID + text + salt + BAIDU_KEY;
    std::string sign_md5 = md5(sign);

    // 构造POST参数
    std::string postFields = "q=" + std::string(curl_easy_escape(curl, text.c_str(), text.length())) +
                             "&from=en&to=zh&appid=" + BAIDU_APPID +
                             "&salt=" + salt + "&sign=" + sign_md5;

    // 设置CURL选项
    curl_easy_setopt(curl, CURLOPT_URL, TRANSLATE_URL.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // 禁用SSL验证
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    CURLcode res = curl_easy_perform(curl);

#endif
#if(1)
    {
        CURLcode res;
        char myurl[1000] = "http://api.fanyi.baidu.com/api/trans/vip/translate?";
        char *appid = "20250422002339827";    //replace myAppid with your own appid
        char *q = const_cast<char *>(text.c_str());          //replace apple with your own text to be translate, ensure that the input text is encoded with UTF-8!
        char *from = "en";          //replace en with your own language type of input text
        char *to = "zh";            //replace zh with your own language type of output text
        char salt[60];
        int a = rand();
        sprintf(salt, "%d", a);
        char *secret_key = "2hEn69vKVRn_L9bhH2jw";   //replace mySecretKey with your own mySecretKey
        char sign[120] = "";
        strcat(sign, appid);
        strcat(sign, q);
        strcat(sign, salt);
        strcat(sign, secret_key);
        printf("%s\n", sign);
        unsigned char md[16];
        int i;
        char tmp[3] = {'\0'}, buf[33] = {'\0'};
        MD5((unsigned char *) sign, strlen(reinterpret_cast<const char *>((unsigned char *) sign)), md);
        for (i = 0; i < 16; i++) {
            sprintf(tmp, "%2.2x", md[i]);
            strcat(buf, tmp);
        }
        printf("%s\n", buf);
        strcat(myurl, "appid=");
        strcat(myurl, appid);
        strcat(myurl, "&q=");
        strcat(myurl, q);
        strcat(myurl, "&from=");
        strcat(myurl, from);
        strcat(myurl, "&to=");
        strcat(myurl, to);
        strcat(myurl, "&salt=");
        strcat(myurl, salt);
        strcat(myurl, "&sign=");
        strcat(myurl, buf);
        printf("%s\n", myurl);
        curl_easy_setopt(curl, CURLOPT_URL, myurl);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            curl_easy_cleanup(curl);
            //throw std::runtime_error("Translation request failed: " + std::string(curl_easy_strerror(res)));
        }

        curl_easy_cleanup(curl);
    }
#endif
    // 解析JSON结果
    try {
        auto json = nlohmann::json::parse(result);
        if (json.contains("trans_result")) {
            result = json["trans_result"][0]["dst"];
        }
    }
    catch (const std::exception &e) {
        std::cout << "Translation failed: " << e.what() << "\n";
    }
    return result;
}

std::string DatabaseManager::md5(const std::string &text) {
    uint8_t digest[MD5_DIGEST_LENGTH];
    MD5((const uint8_t *) text.c_str(), text.size(), digest);

    std::ostringstream oss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int) digest[i];
    return oss.str();
}


