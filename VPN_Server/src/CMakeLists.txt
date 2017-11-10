cmake_minimum_required(VERSION 2.8)
 
project(vpn_service)			

set(SOURCE_EXE main.cpp)

set(SOURCE_LIB vpn_server.hpp)			

add_library(vpn_lib STATIC ${SOURCE_LIB})	
set_target_properties(vpn_lib PROPERTIES LINKER_LANGUAGE CXX)
add_executable(main ${SOURCE_EXE})

target_link_libraries(main vpn_lib)		
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -lpthread -lwolfssl -std=c++11")