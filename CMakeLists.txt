cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(alex_fifo LANGUAGES CXX)

find_package(Threads)

# set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(alex_fifo test.cpp)
target_compile_features(alex_fifo PRIVATE cxx_std_20)
target_include_directories(alex_fifo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(alex_fifo PRIVATE Threads::Threads dl)
