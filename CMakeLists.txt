cmake_minimum_required(VERSION 3.27)
project(memory_pool_xs)

set(CMAKE_CXX_STANDARD 23)

# ===================================================================
# Main Executable
# ===================================================================
add_executable(memory_pool_xs main.cpp
        src/thread_cache.cpp
        src/utils.cpp
        src/central_cache.cpp
        src/page_cache.cpp)

# Include directories for the main executable
target_include_directories(memory_pool_xs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Link libraries for the main executable
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(memory_pool_xs PRIVATE
        spdlog::spdlog)
target_compile_definitions(memory_pool_xs PRIVATE SPDLOG_ENABLE_SOURCE_LOCATION)


# ===================================================================
# Testing with Google Test
# ===================================================================
# Enable testing for the project
enable_testing()

# Add Google Test using FetchContent
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For projects using C++11 or later, it's recommended to build googletest
# with the same C++ standard.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


# Create the test executable, now including BOTH test files
add_executable(run_tests
        test/thread_cache_test.cpp
        test/central_cache_test.cpp
        test/page_cache_test.cpp
        src/thread_cache.cpp
        src/central_cache.cpp
        src/page_cache.cpp
        src/utils.cpp
)

# Add include directories for the test executable so it can find your headers
target_include_directories(run_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link the test executable against GTest (gtest_main includes gtest and a main function)
target_link_libraries(run_tests PRIVATE
    gtest_main
)

# Discover tests and add them to CTest for running
#include(GoogleTest)
#gtest_discover_tests(run_tests)
