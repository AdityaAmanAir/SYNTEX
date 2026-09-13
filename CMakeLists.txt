cmake_minimum_required(VERSION 3.10)
project(StudentWorkspace)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)

add_executable(server
    src/main.cpp
    src/pdf_extract.cpp
    src/llm_client.cpp
    src/notes_builder.cpp
)

target_include_directories(server PRIVATE include)
target_link_libraries(server PRIVATE CURL::libcurl pthread)

# Standalone CLI test utility for local end-to-end verification
add_executable(cli_test
    src/cli_test.cpp
    src/pdf_extract.cpp
    src/llm_client.cpp
    src/notes_builder.cpp
)
target_include_directories(cli_test PRIVATE include)
target_link_libraries(cli_test PRIVATE CURL::libcurl pthread)
