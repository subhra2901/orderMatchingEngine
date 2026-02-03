cmake_minimum_required(VERSION 3.20)
project(OrderMatchingEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimize for performance
add_compile_options(-Wall -Wextra -O2 -march=native -flto)

# Find packages (Ubuntu packages)
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Core library
add_library(ome
    src/order_book.cpp
    src/matching_engine.cpp
    logging/logger.cpp
)
target_include_directories(ome PUBLIC include)
target_compile_features(ome PUBLIC cxx_std_20)

# Main executable
add_executable(ome_main src/main.cpp)
target_link_libraries(ome_main PRIVATE ome)

# Tests
enable_testing()
add_executable(test_ome tests/test_matching_engine.cpp)
target_link_libraries(test_ome PRIVATE ome GTest::gtest GTest::gtest_main Threads::Threads)
target_compile_options(test_ome PRIVATE -g)
add_test(NAME OMETests COMMAND test_ome)

# Benchmarks (optional)
find_package(benchmark QUIET)
if(benchmark_FOUND)
    add_executable(bench_ome benchmarks/perf_benchmark.cpp)
    target_link_libraries(bench_ome PRIVATE ome benchmark::benchmark Threads::Threads)
endif()

# Install targets
install(TARGETS ome_main test_ome
    RUNTIME DESTINATION bin
)

add_definitions(-DPROJECT_ROOT_PATH="${CMAKE_SOURCE_DIR}")

# Print summary
message(STATUS "Order Matching Engine Configuration:")
message(STATUS "  C++ Standard:         ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:             ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build Type:           ${CMAKE_BUILD_TYPE}")
message(STATUS "  GTest:                ${GTEST_FOUND}")
message(STATUS "  Benchmark:            ${benchmark_FOUND}")
