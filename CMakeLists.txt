
CMAKE_MINIMUM_REQUIRED(VERSION 3.14)


project(bedrock_level)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    MESSAGE(FATAL_ERROR "Unsupported tool-chain, please use mingw64 instead")
endif ()


include_directories(src/include)
include_directories(third/)

enable_testing()
add_library(bedrock-level
        STATIC
        src/sub_chunk.cpp
        src/utils.cpp
        src/palette.cpp
        src/bedrock_level.cpp
        src/bedrock_key.cpp
        src/data_3d.cpp
        src/chunk.cpp
        src/actor.cpp
        src/player.cpp
        src/scoreboard.cpp
        src/global.cpp
        src/color.cpp
        src/level_dat.cpp
)


if (WIN32)
    MESSAGE(STATUS "Current compiler is Mingw")
    set(EXTRA_LIBS ${PROJECT_SOURCE_DIR}/libs/libleveldb-mingw64.a ${PROJECT_SOURCE_DIR}/libs/libz-mingw64.a)
    add_compile_options(-std=c++17 -O2 -Wall -g)
    target_link_libraries(bedrock-level ${EXTRA_LIBS})
endif (WIN32)


if (UNIX)
    add_compile_options(-std=c++17 -Wall -g)
    MESSAGE(STATUS "Current compiler is GUN gcc")
    set(EXTRA_LIBS ${PROJECT_SOURCE_DIR}/libs/libleveldb-gnu.a)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
    find_package(ZLIB)
    target_link_libraries(bedrock-level ${EXTRA_LIBS} ZLIB::ZLIB)
endif (UNIX)


function(create_app name files)
    add_executable(
            ${name}
            ${files}
    )
    target_link_libraries(${name} PRIVATE bedrock-level)
endfunction(create_app)

create_app(biome_png app/biome_png.cpp)
create_app(mem_leak_check app/mem_leak_check.cpp)
create_app(map_to_png app/map_to_png.cpp)


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
create_test(data_dump_level_test tests/data_dump_test.cpp)
create_test(palette_test tests/palettes_test.cpp)
create_test(actor_test tests/actor_test.cpp)
create_test(key_test tests/key_test.cpp)
create_test(bedrock_level_test tests/bedrock_level_test.cpp)
create_test(chunk_test tests/chunk_test.cpp)
create_test(stb_image_test tests/stb_image_test.cpp)
create_test(color_test tests/color_test.cpp)
create_test(level_dat_test tests/level_dat_test.cpp)
#create_test(nbt_test tests/nbt_test.cpp)

