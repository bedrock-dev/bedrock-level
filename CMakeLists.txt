CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
project(bedrock_level)
set(CMAKE_CXX_STANDARD 17)
include_directories(src/include)
include_directories(third/)
include_directories(third/nbt-cpp)

#Catch2 Test lib
#add_library(Catch2 third/catch2/catch_amalgamated.cpp)
enable_testing()

add_library(bedrock-level
        src/sub_chunk.cpp
        src/utils.cpp
        src/bit_tools.cpp
        src/palette.cpp
        src/bedrock_level.cpp
        )

add_library(nbt-cpp
        third/nbt-cpp/nbt.cpp
        third/nbt-cpp/nbt_internal.hpp
        third/nbt-cpp/nbt.hpp
        )

target_link_libraries(bedrock-level
        ${PROJECT_SOURCE_DIR}/libs/libleveldb.a
        ${PROJECT_SOURCE_DIR}/libs/libz.a
        nbt-cpp
        )


add_executable(main src/main.cpp)
target_link_libraries(main bedrock-level)


function(create_test name files)
    add_executable(
            ${name}
            ${files}
    )

    target_link_libraries(${name} bedrock-level ${PROJECT_SOURCE_DIR}/libs/libCatch2.a)
    target_include_directories(${name} PRIVATE third/catch2)
    set_target_properties(${name}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_BINARY_DIR}/tests_bin"
            )
endfunction(create_test)

create_test(sub_chunk_test tests/sub_chunk_test.cpp)
create_test(utils_test tests/utils_test.cpp)
create_test(bit_tools tests/bit_tools_test.cpp)