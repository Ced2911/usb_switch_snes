cmake_minimum_required(VERSION 3.0)
project (testexample) 

enable_testing()

include_directories(../src)
include_directories(../include)

add_library(test_main OBJECT test_main.cpp)

add_executable(test1 joystick.test.cpp $<TARGET_OBJECTS:test_main>)
add_test(NAME test1 COMMAND test1)

