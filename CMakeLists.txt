CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
project(funkeymonkey)

include_directories(include)
list(APPEND CMAKE_CXX_FLAGS -std=c++11)

add_executable(funkeymonkey src/main.cpp)
target_link_libraries(funkeymonkey dl)

add_library(testmodule SHARED modules/testmodule.cpp)
add_library(toymodule SHARED modules/toymodule.cpp)
