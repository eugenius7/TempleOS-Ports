cmake_minimum_required(VERSION 3.16)
project(holyass)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(${PROJECT_NAME} main.cpp export.cpp bones.cpp flags.cpp animation.cpp)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
target_link_libraries(${PROJECT_NAME} PRIVATE assimp)

if(NOT MSVC)
	target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wextra -pedantic -fno-exceptions -fno-rtti)
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
