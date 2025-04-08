project(CFD_MaterialDB)


file(GLOB_RECURSE MODULE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(tinyscheme STATIC
        ${MODULE_SOURCES}
)


