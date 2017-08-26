CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
project(funkeymonkey)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

include_directories(include)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_executable(funkeymonkey src/main.cpp)
target_link_libraries(funkeymonkey dl pthread)
install(TARGETS funkeymonkey DESTINATION sbin COMPONENT binaries)

add_library(testmodule SHARED modules/testmodule.cpp)
install(TARGETS testmodule DESTINATION lib/funkeymonkey)
add_library(toymodule SHARED modules/toymodule.cpp)
install(TARGETS toymodule DESTINATION lib/funkeymonkey)
add_library(keyboard SHARED modules/keyboard.cpp)
install(TARGETS keyboard DESTINATION lib/funkeymonkey)
add_library(cavestorysnes SHARED modules/cavestorysnes.cpp)
install(TARGETS cavestorysnes DESTINATION lib/funkeymonkey)
add_library(modalgamepad SHARED modules/modalgamepad.cpp)
install(TARGETS modalgamepad DESTINATION lib/funkeymonkey)
add_library(ctrlbackdel SHARED modules/ctrlbackdel.cpp)
install(TARGETS ctrlbackdel DESTINATION lib/funkeymonkey)

install(FILES "include/funkeymonkeymodule.h" DESTINATION include/funkeymonkey)
install(FILES "include/uinputdevice.h" DESTINATION include/funkeymonkey)
install(FILES "include/evdevdevice.h" DESTINATION include/funkeymonkey)
