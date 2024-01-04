cmake_minimum_required(VERSION 3.21)
project(png2bin)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(${PROJECT_NAME} main.c flags.c lodepng.c)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 17)

if(NOT MSVC)
	target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wextra -pedantic)
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
