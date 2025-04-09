cmake_minimum_required(VERSION 3.12)
project(CFD_MaterialDB LANGUAGES CXX)


# 设置C++17标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# 查找依赖库
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(Boost 1.82 REQUIRED COMPONENTS filesystem spirit regex)

include_directories(${SQLITECPP_INCLUDE_DIR} ${SQLite3_INCLUDE_DIRS})


#add_subdirectory(src/thirdParty/tinyscheme)

# 可执行文件配置
add_executable(material_db
        src/main.cpp
        src/models/src/material.cpp
        src/database/src/database_manager.cpp
        src/scm_parser/src/scm_parser.cpp
        src/scm_parser/include/scm_parser.h
)

target_include_directories(material_db
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/models/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/database/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scm_parser/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/thirdParty/tinyscheme/include
        ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(material_db
        PRIVATE
        unofficial::sqlite3::sqlite3
        SQLiteCpp
        Boost::filesystem
        Boost::spirit
)

# 包含目录
