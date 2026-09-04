
CMAKE_MINIMUM_REQUIRED(VERSION 3.14)


project(bedrock_level)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Pure C++ library: never inherit a parent project's Qt AUTOGEN settings.
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

include_directories(src/include)
include_directories(third/)

enable_testing()

option(BEDROCK_LEVEL_BUILD_APPS "Build bedrock-level sample apps" OFF)
option(BEDROCK_LEVEL_BUILD_TESTS "Build bedrock-level unit tests" OFF)

add_library(bedrock-level
        STATIC
        src/sub_chunk.cpp
        src/palette.cpp
        src/utils.cpp
        src/nbt.cpp
        src/bedrock_level.cpp
        src/bedrock_key.cpp
        src/data_3d.cpp
        src/chunk.cpp
        src/actor.cpp
        src/global.cpp
        src/color.cpp
        src/level_dat.cpp
        src/config.cpp
        #third
        third/loguru/loguru.cpp
        third/stb/stb_image_write.cpp
        )


if (WIN32)
    if (MSVC)
        MESSAGE(STATUS "Current compiler is MSVC")
        set(EXTRA_LIBS ${PROJECT_SOURCE_DIR}/libs/leveldb.lib ${PROJECT_SOURCE_DIR}/libs/zs.lib)
        add_compile_options(/O2)
    elseif (MINGW)
        MESSAGE(STATUS "Current compiler is MinGW")
        set(EXTRA_LIBS ${PROJECT_SOURCE_DIR}/libs/libleveldb.a ${PROJECT_SOURCE_DIR}/libs/libzs.a)
        add_compile_options(-std=c++17 -O2 -Wall -g)
    endif ()
    target_link_libraries(bedrock-level ${EXTRA_LIBS})
endif (WIN32)


if (UNIX)
    MESSAGE(STATUS "Current compiler is GUN gcc, not support yet, please compile libz and leveldb-mcpe manually")
endif (UNIX)

message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")

if (BEDROCK_LEVEL_BUILD_APPS)
    function(create_app name files)
        add_executable(
                ${name}
                ${files}
        )
        target_link_libraries(${name} PRIVATE bedrock-level)
    endfunction(create_app)

    create_app(biome_png app/biome_png.cpp)
    create_app(dump_chunks app/dump_chunks.cpp)
    create_app(level_stater app/level_stater.cpp)
endif ()

if (BEDROCK_LEVEL_BUILD_TESTS)
    #testing
    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
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
        target_compile_definitions(${name} PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
        #    add_test(NAME ${name} COMMAND ${name})
    endfunction(create_test)

    create_test(utils_test tests/utils_test.cpp)
    create_test(data3d_test tests/data3d_test.cpp)
    create_test(palette_test tests/palettes_test.cpp)
    create_test(nbt_test tests/nbt_test.cpp)
    create_test(mcstructure_test tests/mcstructure_test.cpp)
    create_test(key_test tests/key_test.cpp)
    create_test(chunk_test tests/chunk_test.cpp)
    create_test(stb_image_test tests/stb_image_test.cpp)
    create_test(color_test tests/color_test.cpp)
    create_test(large_nbt_test tests/large_nbt_test.cpp)
endif ()

