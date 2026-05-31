cmake_minimum_required(VERSION 3.16)
project(jc_ftpserver VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(jc_ftpserver
        src/main.cpp
        src/config.cpp
        src/ftp_server.cpp
        src/ftp_session.cpp
        src/string_utils.cpp
)

if(WIN32)
    target_sources(jc_ftpserver PRIVATE src/socket_windows.cpp)
    target_link_libraries(jc_ftpserver PRIVATE ws2_32)
else()
    target_sources(jc_ftpserver PRIVATE src/socket_posix.cpp)
endif()

target_include_directories(jc_ftpserver PRIVATE include)

target_compile_options(jc_ftpserver PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_compile_definitions(jc_ftpserver PRIVATE
        JC_FTPSERVER_VERSION="${PROJECT_VERSION}"
)
