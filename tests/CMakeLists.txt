set(target dispatcher_test)

add_executable(${target}
    task_dispatcher.cpp
)
include(FetchContent)

# Версию при желании зафиксируй
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
# Для macOS/GCC ничего особого; для MSVC common: set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

target_link_libraries(${target}
    PRIVATE
        GTest::GTest
        GTest::Main
        task_dispatcher
)

add_test(NAME ${target} COMMAND ${target})

add_subdirectory(queue)