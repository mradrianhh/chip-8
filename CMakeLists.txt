cmake_minimum_required(VERSION 3.16)
project(chip-8)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_INSTALL_PREFIX "/home/adrian/local")
set(CMAKE_CXX_FLAGS "-fsanitize=address,undefined")
set(RESOURCE_INSTALL_DIR "" CACHE PATH "Path to install resources to (leave empty for running uninstalled)")

# Asset and shader path selection
if(RESOURCE_INSTALL_DIR)
	add_definitions(-DVK_EXAMPLE_ROMS_DIR=\"${RESOURCE_INSTALL_DIR}/roms\")
	install(DIRECTORY roms/ DESTINATION ${RESOURCE_INSTALL_DIR}/roms)
else()
	if(NOT USE_RELATIVE_ASSET_PATH)
		add_definitions(-DVK_EXAMPLE_ROMS_DIR=\"${CMAKE_SOURCE_DIR}/roms/\")
	endif()
endif()

file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/**.c")
add_executable(chip-8 "${SOURCES}")
target_include_directories(chip-8 PUBLIC "${PROJECT_SOURCE_DIR}/include")

install(TARGETS chip-8 DESTINATION bin)
