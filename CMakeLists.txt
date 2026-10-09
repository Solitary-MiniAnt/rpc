cmake_minimum_required(VERSION 3.10)
project(RpcProject)

set(CMAKE_CXX_STANDARD 11)

# 默认使用 Debug 模式（开发时快、好调试）
# 想要发布时，在终端执行 cmake -DCMAKE_BUILD_TYPE=Release ..
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Debug：不优化，带调试信息
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall")
# Release：二级优化，用于最终发布
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -Wall")

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(/usr/local/include)
link_directories(/usr/local/lib)

file(GLOB SRC_FILES "src/*.cpp")
file(GLOB TEST_FILES "test/*.cpp")

foreach(test_file ${TEST_FILES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file} ${SRC_FILES})
    target_link_libraries(${test_name} muduo_net muduo_base pthread jsoncpp)
endforeach()