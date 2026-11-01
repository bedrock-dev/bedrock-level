
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
option(BEDROCK_LEVEL_BUILD_BENCHMARKS "Build bedrock-level benchmarks" ON)
option(BEDROCK_LEVEL_BUILD_WEB_DEMO "Build the browser NBT viewer" OFF)

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
elseif (EMSCRIPTEN)
    if (NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "bedrock-level provides Emscripten libraries only for wasm32")
    endif ()
    message(STATUS "Current platform is Emscripten wasm32 (pthread)")
    set(wasm_version_file "${BEDROCK_LEVEL_LIB_DIR}/wasm32-emscripten-version.txt")
    if (NOT EXISTS "${wasm_version_file}")
        message(FATAL_ERROR "Missing wasm toolchain version file; rebuild dependencies with leveldb-mcpe/build-wasm.ps1")
    endif ()
    file(READ "${wasm_version_file}" wasm_dependency_version)
    string(STRIP "${wasm_dependency_version}" wasm_dependency_version)
    if (NOT wasm_dependency_version STREQUAL EMSCRIPTEN_VERSION)
        message(FATAL_ERROR
                "Wasm dependencies use Emscripten ${wasm_dependency_version}, but this toolchain is ${EMSCRIPTEN_VERSION}. Copy matching prebuilt archives into libs; see libs/README.md")
    endif ()
    set(EXTRA_LIBS
            "${BEDROCK_LEVEL_LIB_DIR}/libleveldb-wasm32.a"
            "${BEDROCK_LEVEL_LIB_DIR}/libz-wasm32.a"
            "${BEDROCK_LEVEL_LIB_DIR}/libdeflate-wasm32.a")
    set_target_properties(bedrock-level PROPERTIES OUTPUT_NAME bedrock-level-wasm32)
    target_compile_definitions(bedrock-level PRIVATE LOGURU_STACKTRACES=0)
    # Match the JavaScript exception mode used by the Qt 6.11.1 prebuilt kit.
    target_compile_options(bedrock-level PUBLIC -pthread -fexceptions)
    # Propagate the runtime requirements to executables linking this static library.
    target_link_options(bedrock-level PUBLIC
            -pthread -fexceptions
            "-sPTHREAD_POOL_SIZE=2"
            "-sSTACK_SIZE=5242880"
            "-sDEFAULT_PTHREAD_STACK_SIZE=2097152"
            "-sINITIAL_MEMORY=33554432"
            "-sALLOW_MEMORY_GROWTH=1")
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

if (BEDROCK_LEVEL_BUILD_WEB_DEMO)
    if (NOT EMSCRIPTEN)
        message(FATAL_ERROR "The NBT web demo requires Emscripten; use build_wasm.ps1 -WebDemo")
    endif ()
    add_executable(nbt_viewer webdemo/nbt_viewer.cpp)
    target_link_libraries(nbt_viewer PRIVATE bedrock-level)
    set_target_properties(nbt_viewer PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/webdemo")
    target_link_options(nbt_viewer PRIVATE
            --no-entry
            "-sMODULARIZE=1"
            "-sEXPORT_NAME=createNbtViewer"
            "-sENVIRONMENT=web,worker,node"
            "-sEXPORTED_FUNCTIONS=_malloc,_free,_nbt_to_string,_nbt_result_size,_nbt_last_error"
            "-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU8")
    foreach (asset index.html viewer.js nbt_worker.js)
        configure_file("webdemo/${asset}" "${CMAKE_BINARY_DIR}/webdemo/${asset}" COPYONLY)
    endforeach ()
endif ()

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

if (BEDROCK_LEVEL_BUILD_BENCHMARKS)
    function(create_benchmark name files)
        add_executable(${name} ${files})
        target_link_libraries(${name} PRIVATE bedrock-level)
        set_target_properties(${name} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmark")
    endfunction(create_benchmark)

    create_benchmark(nbt_benchmark benchmark/nbt_benchmark.cpp)
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
        set_target_properties(${name} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
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

