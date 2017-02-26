CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
project(funkeymonkey)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

include_directories(include)
list(APPEND CMAKE_CXX_FLAGS -std=c++11)

add_executable(funkeymonkey src/main.cpp)
target_link_libraries(funkeymonkey dl pthread)

add_library(testmodule SHARED modules/testmodule.cpp)
add_library(toymodule SHARED modules/toymodule.cpp)
add_library(keyboard SHARED modules/keyboard.cpp)
add_library(cavestorysnes SHARED modules/cavestorysnes.cpp)
add_library(modalgamepad SHARED modules/modalgamepad.cpp)
add_library(ctrlbackdel SHARED modules/ctrlbackdel.cpp)
