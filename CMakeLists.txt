cmake_minimum_required(VERSION 3.20)
project(OrderMatchingEngine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")

# Main library
add_library(ome
    src/order_book.cpp
    src/matching_engine.cpp
)
target_include_directories(ome PUBLIC include)

# Main executable
add_executable(ome_main src/main.cpp)
target_link_libraries(ome_main ome)

# Testing
# enable_testing()
# find_package(GTest REQUIRED)

# add_executable(test_ome tests/test_matching_engine.cpp)
# target_link_libraries(test_ome ome GTest::gtest GTest::gtest_main)
# add_test(NAME OMETests COMMAND test_ome)

# Benchmarking
# find_package(benchmark REQUIRED)
# add_executable(bench_ome benchmarks/perf_benchmark.cpp)
# target_link_libraries(bench_ome ome benchmark::benchmark)