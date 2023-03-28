CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
project(bedrock_level)
set(CMAKE_CXX_STANDARD 17)
include_directories(src/include)
include_directories(third/)
add_executable(main src/main.cpp)
target_link_libraries(main
        ${PROJECT_SOURCE_DIR}/libs/libleveldb.a
        ${PROJECT_SOURCE_DIR}/libs/libz.a
        )