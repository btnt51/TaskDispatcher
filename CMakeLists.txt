cmake_minimum_required(VERSION 3.25)
project(priority_task_dispatcher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Configure local githooks
execute_process(
    COMMAND git config core.hooksPath .githooks
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE GIT_COMMAND_RESULT
)
include(FetchContent)

# Версию при желании зафиксируй
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
# Для macOS/GCC ничего особого; для MSVC common: set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

find_package(GTest REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

set(target dispatcher)

add_executable(${target} main.cpp)
target_include_directories(${target} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${target}
    PRIVATE
        task_dispatcher
)

include(CTest)
enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
