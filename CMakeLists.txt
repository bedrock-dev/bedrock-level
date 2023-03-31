
CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

project(bedrock_level)
set(CMAKE_CXX_STANDARD 17)
include_directories(src/include)
include_directories(third/)
include_directories(third/nbt-cpp)


# find_package(Catch2 REQUIRED)

#Catch2 Test lib
enable_testing()


add_library(bedrock-level
        src/sub_chunk.cpp
        src/utils.cpp
        src/bit_tools.cpp
        src/palette.cpp
        src/bedrock_level.cpp
        src/bedrock_key.cpp
        src/data_3d.cpp
        src/chunk.cpp
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


#testing


include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

function(create_test name files)
    add_executable(
            ${name}
            ${files}
    )
    target_link_libraries(${name} PRIVATE GTest::gtest_main bedrock-level)
    #    add_test(NAME ${name} COMMAND ${name})
endfunction(create_test)

create_test(sub_chunk_test tests/sub_chunk_test.cpp)
create_test(utils_test tests/utils_test.cpp)
create_test(bit_tools_test tests/bit_tools_test.cpp)
create_test(data3d_test tests/data3d_test.cpp)
create_test(bedrock_level_test tests/bedrock_level_test.cpp)