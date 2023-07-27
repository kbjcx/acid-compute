cmake_minimum_required(VERSION 3.16)
project(DF)
include(cmake/util.cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (CMAKE_BUILD_TYPE)
else ()
    set(CMAKE_BUILD_TYPE DEBUG)
endif ()
message("------------ Options -------------")
message("  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("  CMAKE_COMMAND: ${CMAKE_COMMAND}")
# 定义一个选项option(<option_name> "<option_description>" <default_value>)
option(ENABLE_DEBUGGER "enable debugger" ON)
if (ENABLE_DEBUGGER)
    set(ENABLE_DEBUGGER 1)
    # 用于向编译器添加预定义的宏定义ACID_ENABLE_DEBUGGER替换为0
    add_definitions(-DACID_ENABLE_DEBUGGER=1s)
    message ("  enable_debugger: yes")
else ()
    set(ENABLE_DEBUGGER 0)
    add_definitions(-DACID_ENABLE_DEBUGGER=0)
    message ("  enable_debugger: no")
endif ()

option(BUILD_DYNAMIC "build dynamic" OFF)
if (BUILD_DYNAMIC)
    message ("  build dynamic lib: yes")
else ()
    message ("  build dynamic lib: no")
endif ()

message("-------------- Env ---------------")
message("  CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message("  CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message("----------------------------------")

set(CMAKE_CXX_COMPILER "/usr/bin/g++-11")
set(CMAKE_CXX_STANDARD 20)
add_definitions(-D__const__=__unused__)
set(CMAKE_CXX_FLAGS "-fPIC -fno-strict-aliasing -Wall ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O3 -DNDEBUG")

message("------------ Cxx flags -------------")
message("  CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message("  CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}: ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")
message("------------------------------------")

#third_party
#json
#include_directories("${PROJECT_SOURCE_DIR}/third_party/json/single_include")
#libgo
#include_directories("${PROJECT_SOURCE_DIR}/third_party/libgo")
#include_directories("${PROJECT_SOURCE_DIR}/third_party/libgo/libgo")
#include_directories("${PROJECT_SOURCE_DIR}/third_party/libgo/libgo/linux")
#link_directories("${PROJECT_SOURCE_DIR}/third_party/libgo/build")
#spdlog
#include_directories("${PROJECT_SOURCE_DIR}/third_party/spdlog/include")
#link_directories("${PROJECT_SOURCE_DIR}/third_party/spdlog/build")
#yaml-cpp
include_directories("${PROJECT_SOURCE_DIR}/third_party/yaml-cpp/include")
link_directories("${PROJECT_SOURCE_DIR}/third_party/yaml-cpp/build")

include_directories("${PROJECT_SOURCE_DIR}/third_party/parser")

include_directories("${PROJECT_SOURCE_DIR}/third_party/lua/include")
link_directories("${PROJECT_SOURCE_DIR}/lib")

include_directories(${PROJECT_SOURCE_DIR})

aux_source_directory(${PROJECT_SOURCE_DIR}/acid/rpc SRC_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/acid/http SRC_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/acid/net SRC_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/acid/common SRC_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/acid/logger SRC_LIST)
set(SRC_LIST ${SRC_LIST} ${PROJECT_SOURCE_DIR}/third_party/parser/http_parser.c)

#编译的库生成在lib
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
message("------------ LIBRARY_OUTPUT_PATH -------------")
message(${PROJECT_SOURCE_DIR}/lib)

#set(HTTPPARSER ${PROJECT_SOURCE_DIR}/third_party/parser/http_parser.c)
#if (BUILD_DYNAMIC)
#    add_library("httpparser" SHARED ${HTTPPARSER})
#    set_target_properties("httpparser" PROPERTIES OUTPUT_NAME "${HTTPPARSER}")
#    # 将生成的共享库文件和静态库文件安装到指定的目录中
#    install(TARGETS "httpparser" LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")
#else ()
#    add_library("httpparser" STATIC ${HTTPPARSER})
#    set_target_properties("httpparser" PROPERTIES OUTPUT_NAME "${HTTPPARSER}")
#endif ()

set(LINK_ARGS pthread yaml-cpp dl)
set(TARGET "acid")
set(STATIC_T "acid_static")
set(SHARED_T "acid_dynamic")

message("---------------------------SRC_LIST---------------------------------")
message(${SRC_LIST})

# 编译为动态库
if (BUILD_DYNAMIC)
    add_library("${SHARED_T}" SHARED "${SRC_LIST}")
    set_target_properties("${SHARED_T}" PROPERTIES OUTPUT_NAME "${TARGET}")
    target_link_libraries("${SHARED_T}" -Wl,--start-group ${LINK_ARGS} -Wl,--end-group)
    # 将生成的共享库文件和静态库文件安装到指定的目录中
    install(TARGETS ${SHARED_T} LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")
else ()
    # 默认编译静态库
    add_library("${STATIC_T}" STATIC ${SRC_LIST})
    set_target_properties("${STATIC_T}" PROPERTIES OUTPUT_NAME "${TARGET}")
    target_link_libraries("${STATIC_T}" -Wl,--start-group ${LINK_ARGS} -Wl,--end-group)
endif ()

link_directories(${PROJECT_SOURCE_DIR}/lib)

acid_add_test(/test/fiber FIBER)

acid_add_test(/test/thread THREAD)

acid_add_test(/test/http HTTP)

acid_add_test(/test/logger LOGGER)

acid_add_test(/test/rpc RPC)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)