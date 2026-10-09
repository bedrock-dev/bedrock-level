
CMAKE_MINIMUM_REQUIRED(VERSION 3.14)


project(bedrock_level)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Pure C++ library: never inherit a parent project's Qt AUTOGEN settings.
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

enable_testing()

option(BEDROCK_LEVEL_BUILD_APPS "Build bedrock-level sample apps" OFF)
option(BEDROCK_LEVEL_BUILD_TESTS "Build bedrock-level unit tests" OFF)

add_library(bedrock-level
        STATIC
        src/sub_chunk.cpp
        src/palette.cpp
        src/mcstructure.cpp
        src/utils.cpp
        src/nbt.cpp
        src/geometry.cpp
        src/bedrock_level.cpp
        src/bedrock_key.cpp
        src/chunk_data_position.cpp
        src/data_3d.cpp
        src/chunk.cpp
        src/raw_chunk.cpp
        src/actor.cpp
        src/global.cpp
        src/color.cpp
        src/level_dat.cpp
        src/config.cpp
        #third
        third/loguru/loguru.cpp
        third/stb/stb_image_write.cpp
        )

target_include_directories(bedrock-level PUBLIC
        ${PROJECT_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/third
        )


set(BEDROCK_LEVEL_LIB_DIR "${PROJECT_SOURCE_DIR}/libs")

if (MINGW)
    message(STATUS "Current compiler is MinGW/GCC")
    set(EXTRA_LIBS
            "${BEDROCK_LEVEL_LIB_DIR}/libleveldb-win64.a"
            "${BEDROCK_LEVEL_LIB_DIR}/libzs-win64.a"
            "${BEDROCK_LEVEL_LIB_DIR}/libdeflate-win64.a")
    target_compile_options(bedrock-level PRIVATE -O2 -Wall)
elseif (ANDROID)
    if (NOT ANDROID_ABI STREQUAL "arm64-v8a")
        message(FATAL_ERROR
                "bedrock-level currently provides Android libraries only for arm64-v8a; got '${ANDROID_ABI}'")
    endif ()
    message(STATUS "Current platform is Android ${ANDROID_ABI}")
    set(EXTRA_LIBS
            "${BEDROCK_LEVEL_LIB_DIR}/libleveldb-android-arm64.a"
            "${BEDROCK_LEVEL_LIB_DIR}/libz-android-arm64.a"
            "${BEDROCK_LEVEL_LIB_DIR}/libdeflate-android-arm64.a")
else ()
    message(FATAL_ERROR "Unsupported platform for bedrock-level: ${CMAKE_SYSTEM_NAME}")
endif ()

foreach (extra_lib IN LISTS EXTRA_LIBS)
    if (NOT EXISTS "${extra_lib}")
        message(FATAL_ERROR "Missing prebuilt dependency: ${extra_lib}")
    endif ()
endforeach ()

target_link_libraries(bedrock-level PRIVATE ${EXTRA_LIBS})

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
    function(create_test name files)
        add_executable(
                ${name}
                ${files}
        )
        target_link_libraries(${name} PRIVATE GTest::gtest_main bedrock-level)
        target_compile_definitions(${name} PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
        add_test(NAME ${name} COMMAND ${name})
    endfunction(create_test)

    create_test(data3d_test tests/data3d_test.cpp)
    create_test(palette_test tests/palettes_test.cpp)
    create_test(nbt_test tests/nbt_test.cpp)
    create_test(mcstructure_test tests/mcstructure_test.cpp)
    create_test(key_test tests/key_test.cpp)
    create_test(chunk_test tests/chunk_test.cpp)
    create_test(sub_chunk_test tests/sub_chunk_test.cpp)
    create_test(color_test tests/color_test.cpp)
    create_test(large_nbt_test tests/large_nbt_test.cpp)
endif ()

