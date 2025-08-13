set(target queue_test)

add_executable(${target}
    bounded_queue.cpp
    unbounded_queue.cpp
    priority_queue.cpp
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
        queue
)

add_test(NAME ${target} COMMAND ${target})