option(BUILD_LIB "Build freestanding shared library" ON)
option(FLTO "FLTO" ON)
option(SANITIZE "Enable Sanitizer" OFF)
option(VERBOSE "Verbose" OFF)
option(RUN_CPPCHECK "Run cppcheck" OFF)
option(HW_ACCEL "Use SDL's hardware rendering path for applicable platforms" ON)

# I don't use CMAKE_BUILD_TYPE because it scares and confuses me (and I don't trust it)
# set(BUILD_MODE STRING "Release" "Debug/Fast/Release")

# add_subdirectory(tools/png2bin)
# add_subdirectory(tools/holyass)
# add_subdirectory(tools/wadmaker)
# add_subdirectory(tools/ericw-tools)

include(ExternalProject)

ExternalProject_Add(png2bin-proj
	SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tools/png2bin
	# INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR} -DCMAKE_BUILD_TYPE=Release
	BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/bin/png2bin
	)
ExternalProject_Add(holyass-proj
	SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tools/holyass
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR} -DCMAKE_BUILD_TYPE=Release
	BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/bin/holyass
	)
ExternalProject_Add(wadmaker-proj
	SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tools/wadmaker
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR} -DCMAKE_BUILD_TYPE=Release
	BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/bin/wadmaker
	)
ExternalProject_Add(holytone-proj
	SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tools/holytone
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR} -DCMAKE_BUILD_TYPE=Release
	BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/bin/holytone
	)

ExternalProject_Add(ericw-tools-proj
	# GIT_REPOSITORY https://github.com/ericwa/ericw-tools.git
	SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tools/ericw-tools
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR} -DCMAKE_BUILD_TYPE=Release -DSKIP_EMBREE_INSTALL=ON -DSKIP_TBB_INSTALL=ON -DDISABLE_TESTS=ON -DDISABLE_DOCS=ON -DDISABLE_TESTS=ON
	# BUILD_ALWAYS TRUE
	BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/bin/qbsp
)

# ExternalProject_Get_Property(holyass-proj BINARY_DIR)
add_executable(holyass IMPORTED)
add_dependencies(holyass holyass-proj)
set_property(TARGET holyass PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/bin/holyass)

add_executable(png2bin IMPORTED)
add_dependencies(png2bin png2bin-proj)
set_property(TARGET png2bin PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/bin/png2bin)

add_executable(wadmaker IMPORTED)
add_dependencies(wadmaker wadmaker-proj)
set_property(TARGET wadmaker PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/bin/wadmaker)

add_executable(holytone IMPORTED)
add_dependencies(holytone holytone-proj)
set_property(TARGET holytone PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/bin/holytone)

# ExternalProject_Get_Property(ericw-tools-proj BINARY_DIR)
add_executable(qbsp IMPORTED)
add_executable(vis IMPORTED)
add_executable(light IMPORTED)
set_property(TARGET qbsp PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/bin/qbsp)
set_property(TARGET vis PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/bin/vis)
set_property(TARGET light PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/bin/light)
add_dependencies(qbsp ericw-tools-proj)
add_dependencies(vis ericw-tools-proj)
add_dependencies(light ericw-tools-proj)

cmake_minimum_required(VERSION 3.21)
project(cyberchud LANGUAGES C ASM)
set(CMAKE_C_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
	message(FATAL_ERROR "MSVC is not supported, try implementing the required compiler flags yourself if you want.")
endif()

include(csrc.cmake)
include(assets.cmake)
include(fonts.cmake)
include(pxrgb.cmake)

set(DATA_BUILD_DIRS generated data/bsp data/fonts/matchup data/px/rgb data/models/anim data/models/static)
add_custom_target(build-time-make-directory ALL COMMAND ${CMAKE_COMMAND} -E make_directory ${DATA_BUILD_DIRS})

set(BSP_BUILT "")
foreach(BSP IN LISTS BSP_LIST)
	set(COUT data/bsp/${BSP}.bsp)
	list(APPEND BSP_BUILT ${COUT} data/bsp/${BSP}.lux)
	add_custom_command(
		OUTPUT ${COUT} data/bsp/${BSP}.lux
		DEPENDS assets/bsp/${BSP}.map qbsp
		COMMAND qbsp -bsp2 -leaktest -notex ${CMAKE_CURRENT_SOURCE_DIR}/assets/bsp/${BSP}.map ${COUT}
		COMMAND vis ${COUT}
		COMMAND light -lux -dirt -extra4 -bounce ${COUT}
		)
endforeach()

set(PX_RGB_BUILT "")
foreach(PX IN LISTS PX_RGB)
	set(CIN assets/png/rgb/${PX}.png)
	set(COUT data/px/rgb/${PX}.bin)
	list(APPEND PX_RGB_BUILT ${COUT})
	add_custom_command(
		OUTPUT ${COUT}
		DEPENDS ${CIN} png2bin
		COMMAND png2bin -c ${CMAKE_CURRENT_SOURCE_DIR}/${CIN} ${COUT}
		)
endforeach()

set(PX_GRAY_BUILT "")
foreach(PX IN LISTS PX_GRAY)
	set(CIN assets/png/gray/${PX}.png)
	set(COUT data/px/gray/${PX}.bin)
	list(APPEND PX_GRAY_BUILT ${COUT})
	add_custom_command(
		OUTPUT ${COUT}
		DEPENDS ${CIN} png2bin
		COMMAND png2bin -g ${CMAKE_CURRENT_SOURCE_DIR}/${CIN} ${COUT}
		)
endforeach()

foreach(PX IN LISTS PX_GRAY_ALT)
	set(CIN assets/png/gray/${PX}.png)
	set(COUT data/px/gray/${PX}.bin)
	list(APPEND PX_GRAY_BUILT ${COUT})
	add_custom_command(
		OUTPUT ${COUT}
		DEPENDS ${CIN} png2bin
		COMMAND png2bin -g -a ${CMAKE_CURRENT_SOURCE_DIR}/${CIN} ${COUT}
		)
endforeach()

set(PX_PAL_BUILT "")
foreach(PX IN LISTS PX_PAL)
	set(CIN assets/png/pal/${PX}.png)
	set(COUT data/px/pal/${PX}.bin)
	list(APPEND PX_PAL_BUILT ${COUT})
	add_custom_command(
		OUTPUT ${COUT}
		DEPENDS ${CIN} png2bin
		COMMAND png2bin ${CMAKE_CURRENT_SOURCE_DIR}/${CIN} ${COUT}
		)
endforeach()

set(FONT_BUILT "")
foreach(PX IN LISTS ALL_FONTS)
	set(CIN assets/fonts/${PX}.png)
	set(COUT data/fonts/${PX}.bin)
	list(APPEND FONT_BUILT ${COUT})
	add_custom_command(
		OUTPUT ${COUT}
		DEPENDS ${CIN} png2bin
		COMMAND png2bin ${CMAKE_CURRENT_SOURCE_DIR}/${CIN} ${COUT}
		)
endforeach()

set(STATIC_MODELS_BUILT "")
foreach(MODEL IN LISTS STATIC_MODELS)
	set(CIN assets/gltf/static/${MODEL}.gltf)
	set(COUT data/models/static/${MODEL}.bin)
	list(APPEND STATIC_MODELS_BUILT ${COUT})
	add_custom_command(
		OUTPUT ${COUT}
		DEPENDS ${CIN} holyass
		COMMAND holyass ${CMAKE_CURRENT_SOURCE_DIR}/${CIN} ${COUT}
		)
endforeach()

set(ANIM_MODELS_BUILT "")
foreach(MODEL IN LISTS ANIM_MODELS)
	set(CIN assets/gltf/anim/${MODEL}.gltf)
	set(COUT data/models/anim/${MODEL}.bin)
	list(APPEND ANIM_MODELS_BUILT ${COUT})
	add_custom_command(
		OUTPUT ${COUT}
		DEPENDS ${CIN} holyass
		COMMAND holyass -s ${CMAKE_CURRENT_SOURCE_DIR}/${CIN} ${COUT}
		)
endforeach()

set(SNDS_BUILT "")
foreach(SND IN LISTS SNDS)
	set(CIN assets/snd/${SND}.txt)
	set(COUT data/snd/${SND}.bin)
	list(APPEND SNDS_BUILT ${COUT})
	add_custom_command(
		OUTPUT ${COUT}
		DEPENDS ${CIN} holytone
		COMMAND holytone -v -ts -te ${CMAKE_CURRENT_SOURCE_DIR}/${CIN} ${COUT}
		)
endforeach()

set(ASSETS ${BSP_BUILT} ${PX_RGB_BUILT} ${PX_GRAY_BUILT} ${PX_PAL_BUILT} ${FONT_BUILT} ${STATIC_MODELS_BUILT} ${ANIM_MODELS_BUILT} ${SNDS_BUILT})
add_custom_command(
	OUTPUT data.wad generated/wad.h
	DEPENDS ${ASSETS} wadmaker
	COMMAND wadmaker -g -d data ${ASSETS} data.wad generated/wad.h
	)

configure_file(scripts/qemu-create.sh.in qemu-create.sh @ONLY)
configure_file(scripts/qemu-update.sh.in qemu-update.sh @ONLY)
configure_file(scripts/vmware.sh.in vmware.sh @ONLY)

# -fvisibility=hidden
# -Wno-type-limits
# -Wno-unused-parameter
set(COMMON_CFLAGS -Wall -Wextra -Wno-type-limits -Wno-unused-parameter -fdiagnostics-color=always -fno-math-errno -ffinite-math-only -fno-rounding-math -fno-trapping-math -fno-signed-zeros -fomit-frame-pointer )
if (NOT EMSCRIPTEN)
	list(APPEND COMMON_CFLAGS -fallow-store-data-races)
endif()

if(VERBOSE)
	list(APPEND COMMON_CFLAGS -DVERBOSE)
endif()
set(TOS_CFLAGS -march=x86-64-v2 -s -fcf-protection=none -mno-shstk -fno-stack-protector -mno-red-zone -fno-asynchronous-unwind-tables -fno-exceptions -nostartfiles)
set(INC_DIRS external/cglm/include include src ${CMAKE_CURRENT_BINARY_DIR}/generated)

if(RUN_CPPCHECK)
	find_program(CMAKE_C_CPPCHECK NAMES cppcheck)
	if (CMAKE_C_CPPCHECK)
		message(STATUS "${CMAKE_CXX_CPPCHECK}")
		list(APPEND CMAKE_CXX_CPPCHECK "--enable=all")
	endif()
endif(RUN_CPPCHECK)

set(UBERSHADERS
	ubershader0
	ubershader1
	ubershader2
	ubershader3
	)

set(VSHADERS
	vshader0
	vshader1
	vshader2
	vshader3
	)

set(UBERSHADER_OBJS "")
set(UBERSHADER_TOS_OBJS "")
set(VSHADER_OBJS "")
set(VSHADER_TOS_OBJS "")

foreach(SHADER IN LISTS UBERSHADERS)
	add_library(${SHADER} OBJECT src/shaders/ubershader.c generated/wad.h)
	list(APPEND UBERSHADER_OBJS $<TARGET_OBJECTS:${SHADER}>)

	if(BUILD_LIB)
		add_library(tos_${SHADER} OBJECT src/shaders/ubershader.c generated/wad.h)
		list(APPEND UBERSHADER_TOS_OBJS $<TARGET_OBJECTS:tos_${SHADER}>)
	endif()
endforeach()

foreach(SHADER IN LISTS VSHADERS)
	add_library(${SHADER} OBJECT src/vshaders.c generated/wad.h)
	list(APPEND VSHADER_OBJS $<TARGET_OBJECTS:${SHADER}>)

	if(BUILD_LIB)
		add_library(tos_${SHADER} OBJECT src/vshaders.c generated/wad.h)
		list(APPEND VSHADER_TOS_OBJS $<TARGET_OBJECTS:tos_${SHADER}>)
	endif()
endforeach()

message(STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})
# set(COMMON_DEPS ${BSP_BUILT} ${FONT_BUILT} ${PX_RGB_BUILT} ${PX_GRAY_BUILT} ${PX_PAL_BUILT} ${BSP_BUILT} ${C_SRC} $<TARGET_OBJECTS:ubershader0>)
set(COMMON_DEPS ${BSP_BUILT} ${FONT_BUILT} ${PX_RGB_BUILT} ${PX_GRAY_BUILT} ${PX_PAL_BUILT} ${BSP_BUILT} ${C_SRC})
if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
	add_executable(cyberchud-bin data.wad generated/wad.h ${COMMON_DEPS} ${UBERSHADER_OBJS} ${VSHADER_OBJS} ${MUSL_SRC} ${ASM_SRC} src/desktop.c)
	if (BUILD_LIB)
		add_library(cyberchud SHARED data.wad generated/wad.h ${COMMON_DEPS} ${UBERSHADER_TOS_OBJS} ${VSHADER_TOS_OBJS} ${MUSL_SRC} ${ASM_SRC} ${TOS_SRC})
	endif()
elseif(EMSCRIPTEN)
	add_executable(cyberchud-bin data.wad ${COMMON_DEPS} ${UBERSHADER_OBJS} ${VSHADER_OBJS} src/desktop.c generated/wad.h)
else()
	add_executable(cyberchud-bin data.wad ${COMMON_DEPS} ${UBERSHADER_OBJS} ${VSHADER_OBJS} ${MUSL_SRC} src/desktop.c generated/wad.h)
	if (BUILD_LIB)
		add_library(cyberchud SHARED data.wad generated/wad.h ${COMMON_DEPS} ${UBERSHADER_TOS_OBJS} ${VSHADER_TOS_OBJS} ${MUSL_SRC})
	endif()
endif()

set(ZSTD_COMPILE_DEFINES ZSTDLIB_VISIBILITY=ZSTDLIB_HIDDEN ZSTDERRORLIB_VISIBILITY=ZSTDERRORLIB_HIDDEN)

if(NOT EMSCRIPTEN)
	set(BIN_DEFS PLATFORM_DESKTOP)
	set(COMMON_CFLAGS ${COMMON_CFLAGS} -fcx-limited-range -fno-signaling-nans)
endif()

if(HW_ACCEL)
	list(APPEND BIN_DEFS HW_ACCEL)
endif()

if(BUILD_MODE STREQUAL "Debug")
	set(FINAL_COMPILE_OPTS -g -Og)
	set(FINAL_LINK_OPTS -g -Og)
elseif(BUILD_MODE STREQUAL "Test")
	set(FINAL_COMPILE_OPTS -g -O3)
	set(FINAL_LINK_OPTS -g -O3)
elseif(BUILD_MODE STREQUAL "Fast")
	set(FINAL_COMPILE_OPTS -g -O3)
	set(FINAL_LINK_OPTS -g -O3)
	set(BIN_DEFS ${BIN_DEFS} NDEBUG)
	if (BUILD_LIB)
		target_compile_options(cyberchud PRIVATE ${FAST_OPTS})
		target_link_options(cyberchud PRIVATE ${FAST_OPTS})
	endif()
elseif(BUILD_MODE STREQUAL "Release")
	set(FINAL_COMPILE_OPTS -O3)
	set(FINAL_LINK_OPTS -O3)
	set(BIN_DEFS ${BIN_DEFS} NDEBUG)
	if(NOT EMSCRIPTEN)
		list(APPEND FINAL_LINK_OPTS -s)
	endif()
endif()

if(NOT EMSCRIPTEN)
	list(APPEND FINAL_COMPILE_OPTS -ffreestanding)
	list(APPEND FINAL_LINK_OPTS -ffreestanding)
endif()

if(FLTO)
	set(FINAL_COMPILE_OPTS ${FINAL_COMPILE_OPTS} -flto=auto)
	set(FINAL_LINK_OPTS ${FINAL_LINK_OPTS} -flto=auto)
endif()

if(EMSCRIPTEN)
	message(STATUS "EMSCRIPTEN")
	set(EM_CFLAGS -sUSE_SDL=2 -pthread)
	set(EM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/emscripten)
	# -sPROXY_TO_PTHREAD causes mouse input issues in Firefox
	# exactly 2GB of INITIAL_MEMORY causes Firefox to error, so we allocate 2GB - 64KB (minimum alignment)
	set(EM_LDFLAGS -sEXPORTED_FUNCTIONS=_main,_window_focus_callback -sUSE_SDL=2 -pthread -sINITIAL_MEMORY=2147418112 -sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency -sWASM=1 "--embed-file data.wad" "--shell-file ${EM_DIR}/shell.html")
	if(HW_ACCEL)
		list(APPEND EM_LDFLAGS -sUSE_WEBGL2=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2)
	endif()
	if(BUILD_MODE STREQUAL "Debug")
		# -sALLOW_MEMORY_GROWTH=1
		list(APPEND EM_LDFLAGS -sASSERTIONS=1 -sSAFE_HEAP_LOG=1)
	endif()
endif()

target_compile_options(cyberchud-bin PRIVATE ${COMMON_CFLAGS} ${FINAL_COMPILE_OPTS})
target_link_options(cyberchud-bin PRIVATE ${COMMON_CFLAGS} ${FINAL_LINK_OPTS})
target_compile_definitions(cyberchud-bin PRIVATE ${ZSTD_COMPILE_DEFINES} ${BIN_DEFS})
target_include_directories(cyberchud-bin PRIVATE ${INC_DIRS})

foreach(SHADER IN LISTS UBERSHADERS)
	target_compile_options(${SHADER} PRIVATE ${COMMON_CFLAGS} ${FINAL_COMPILE_OPTS} ${EM_CFLAGS})
	target_link_options(${SHADER} PRIVATE ${COMMON_CFLAGS} ${FINAL_LINK_OPTS} ${EM_LDFLAGS})
	target_include_directories(${SHADER} PRIVATE ${INC_DIRS})

	if(BUILD_LIB)
		target_compile_options(tos_${SHADER} PRIVATE ${COMMON_CFLAGS} ${FINAL_COMPILE_OPTS} ${TOS_CFLAGS} -fPIC)
		target_link_options(tos_${SHADER} PRIVATE ${COMMON_CFLAGS} ${FINAL_LINK_OPTS} ${TOS_CFLAGS} -fPIC)
		target_include_directories(tos_${SHADER} PRIVATE ${INC_DIRS})
	endif()
endforeach()

foreach(SHADER IN LISTS VSHADERS)
	target_compile_options(${SHADER} PRIVATE ${COMMON_CFLAGS} ${FINAL_COMPILE_OPTS} ${EM_CFLAGS})
	target_link_options(${SHADER} PRIVATE ${COMMON_CFLAGS} ${FINAL_LINK_OPTS})
	target_include_directories(${SHADER} PRIVATE ${INC_DIRS})

	if(BUILD_LIB)
		target_compile_options(tos_${SHADER} PRIVATE ${COMMON_CFLAGS} ${FINAL_COMPILE_OPTS} ${TOS_CFLAGS} -fPIC)
		target_link_options(tos_${SHADER} PRIVATE ${COMMON_CFLAGS} ${FINAL_LINK_OPTS} ${TOS_CFLAGS} -fPIC)
		target_include_directories(tos_${SHADER} PRIVATE ${INC_DIRS})
	endif()
endforeach()

set(TOS_COMMON_DEFS TEMPLEOS TOSLIKE NDEBUG)

target_compile_definitions(ubershader0 PRIVATE ${ZSTD_COMPILE_DEFINES} ${BIN_DEFS} SHADER_NAME_DEFAULT SHADER_PL SHADER_SC)
target_compile_definitions(ubershader1 PRIVATE ${ZSTD_COMPILE_DEFINES} ${BIN_DEFS} SHADER_NAME_NO_PL SHADER_SC)
target_compile_definitions(ubershader2 PRIVATE ${ZSTD_COMPILE_DEFINES} ${BIN_DEFS} SHADER_NAME_NO_SC SHADER_PL)
target_compile_definitions(ubershader3 PRIVATE ${ZSTD_COMPILE_DEFINES} ${BIN_DEFS} SHADER_NAME_NO_PLSC)
target_compile_definitions(vshader0 PRIVATE ${ZSTD_COMPILE_DEFINES} ${BIN_DEFS} SHADER_NAME_DEFAULT SHADER_PL SHADER_SC)
target_compile_definitions(vshader1 PRIVATE ${ZSTD_COMPILE_DEFINES} ${BIN_DEFS} SHADER_NAME_NO_PL SHADER_SC)
target_compile_definitions(vshader2 PRIVATE ${ZSTD_COMPILE_DEFINES} ${BIN_DEFS} SHADER_NAME_NO_SC SHADER_PL)
target_compile_definitions(vshader3 PRIVATE ${ZSTD_COMPILE_DEFINES} ${BIN_DEFS} SHADER_NAME_NO_PLSC)

if(BUILD_LIB)
	target_compile_definitions(tos_ubershader0 PRIVATE ${ZSTD_COMPILE_DEFINES} ${TOS_COMMON_DEFS} SHADER_NAME_DEFAULT SHADER_PL SHADER_SC)
	target_compile_definitions(tos_ubershader1 PRIVATE ${ZSTD_COMPILE_DEFINES} ${TOS_COMMON_DEFS} SHADER_NAME_NO_PL SHADER_SC)
	target_compile_definitions(tos_ubershader2 PRIVATE ${ZSTD_COMPILE_DEFINES} ${TOS_COMMON_DEFS} SHADER_NAME_NO_SC SHADER_PL)
	target_compile_definitions(tos_ubershader3 PRIVATE ${ZSTD_COMPILE_DEFINES} ${TOS_COMMON_DEFS} SHADER_NAME_NO_PLSC)
	target_compile_definitions(tos_vshader0 PRIVATE ${ZSTD_COMPILE_DEFINES} ${TOS_COMMON_DEFS} SHADER_NAME_DEFAULT SHADER_PL SHADER_SC)
	target_compile_definitions(tos_vshader1 PRIVATE ${ZSTD_COMPILE_DEFINES} ${TOS_COMMON_DEFS} SHADER_NAME_NO_PL SHADER_SC)
	target_compile_definitions(tos_vshader2 PRIVATE ${ZSTD_COMPILE_DEFINES} ${TOS_COMMON_DEFS} SHADER_NAME_NO_SC SHADER_PL)
	target_compile_definitions(tos_vshader3 PRIVATE ${ZSTD_COMPILE_DEFINES} ${TOS_COMMON_DEFS} SHADER_NAME_NO_PLSC)

	target_compile_options(cyberchud PRIVATE ${COMMON_CFLAGS} ${FINAL_COMPILE_OPTS})
	target_link_options(cyberchud PRIVATE ${COMMON_CFLAGS} ${FINAL_LINK_OPTS})
	target_compile_definitions(cyberchud PRIVATE ${ZSTD_COMPILE_DEFINES} ${TOS_COMMON_DEFS})
	target_include_directories(cyberchud PRIVATE ${INC_DIRS})
endif()

if(SANITIZE)
	# -fstack-protector
	# -fsanitize=undefined,memory
	target_link_options(cyberchud-bin PRIVATE -fsanitize=address,undefined)
	target_compile_options(cyberchud-bin PRIVATE -fsanitize=address,undefined)
endif()

if(EMSCRIPTEN)
	target_compile_options(cyberchud-bin PRIVATE ${EM_CFLAGS})
	target_link_libraries(cyberchud-bin PRIVATE ${EM_LDFLAGS})
	set_target_properties(cyberchud-bin PROPERTIES SUFFIX ".html")
else()
	target_compile_options(cyberchud-bin PRIVATE -march=x86-64-v2)
	target_link_libraries(cyberchud-bin PRIVATE SDL2)
	if(BUILD_LIB)
		target_compile_options(cyberchud PRIVATE ${TOS_CFLAGS})
		target_link_options(cyberchud PRIVATE ${TOS_CFLAGS})
	endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "TinyCC")
	target_compile_definitions(cyberchud PRIVATE SDL_DISABLE_IMMINTRIN_H)
	target_compile_definitions(cyberchud-bin PRIVATE SDL_DISABLE_IMMINTRIN_H)
endif()
