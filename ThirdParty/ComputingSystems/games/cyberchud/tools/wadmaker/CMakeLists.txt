cmake_minimum_required(VERSION 3.21)
project(wadmaker)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(${PROJECT_NAME} zstd zstd.c main.cpp flags.cpp)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 17)

if(NOT MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
