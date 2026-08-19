cmake_minimum_required(VERSION 3.14)
project(OnlineCompiler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(Threads REQUIRED)

include(FetchContent)

# Fetch Asio first (header-only networking library)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
)
FetchContent_MakeAvailable(asio)

# Set ASIO_INCLUDE_DIR so Crow's find_package(asio) succeeds
set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include CACHE PATH "" FORCE)

# Fetch Crow (header-only web framework)
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG        v1.2.0
)
FetchContent_MakeAvailable(crow)

# Fetch SQLite amalgamation (single-file, no external dependency)
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip
)
FetchContent_MakeAvailable(sqlite3)

# Build sqlite3 as a static library
add_library(sqlite3_lib STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3_lib PUBLIC ${sqlite3_SOURCE_DIR})
set_target_properties(sqlite3_lib PROPERTIES LINKER_LANGUAGE C)

# Main executable
add_executable(online_compiler
    src/main.cpp
    src/executor.cpp
    src/rate_limiter.cpp
    src/database.cpp
)

target_include_directories(online_compiler PRIVATE
    ${crow_SOURCE_DIR}/include
    ${asio_SOURCE_DIR}/asio/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${sqlite3_SOURCE_DIR}
)

target_link_libraries(online_compiler PRIVATE
    Threads::Threads
    sqlite3_lib
)

# Copy public/ to build directory
add_custom_command(TARGET online_compiler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/public $<TARGET_FILE_DIR:online_compiler>/public
)
