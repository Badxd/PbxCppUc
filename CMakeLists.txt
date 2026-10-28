cmake_minimum_required(VERSION 3.20)
project(cppgram_userbot CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Paths injected by Dockerfile
# -DCPPGRAM_LIB_DIR=/src/bot/cppgram_lib/lib
# -DCPPGRAM_INCLUDE_DIR=/src/bot/cppgram_lib/include
if(NOT DEFINED CPPGRAM_LIB_DIR OR NOT DEFINED CPPGRAM_INCLUDE_DIR)
    message(FATAL_ERROR "Set -DCPPGRAM_LIB_DIR and -DCPPGRAM_INCLUDE_DIR")
endif()

add_library(cppgram STATIC IMPORTED)
set_target_properties(cppgram PROPERTIES
    IMPORTED_LOCATION "${CPPGRAM_LIB_DIR}/libcppgram.a"
)

add_executable(cppgram_userbot src/main.cpp)

target_include_directories(cppgram_userbot PRIVATE
    "${CPPGRAM_INCLUDE_DIR}"
)

find_package(OpenSSL REQUIRED)
find_package(ZLIB    REQUIRED)

target_link_libraries(cppgram_userbot PRIVATE
    cppgram
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    sqlite3
    curl
    pthread
)
