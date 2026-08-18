cmake_minimum_required(VERSION 3.21)
project(Rezo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(rezo src/main.cpp)

enable_testing()
add_executable(rezo_tests tests/test_main.cpp)
add_test(NAME rezo_tests COMMAND rezo_tests)
