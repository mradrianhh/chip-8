cmake_minimum_required(VERSION 3.16)
project(chip-8)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "-fsanitize=address,undefined")

add_definitions(-DCH8_EXAMPLE_ROMS_DIR=\"${CMAKE_SOURCE_DIR}/assets/roms/\")
add_definitions(-DCH8_PNGS_DIR=\"${CMAKE_SOURCE_DIR}/assets/pngs/\")
add_definitions(-DCH8_LOGS_DIR=\"${CMAKE_SOURCE_DIR}/logs/\")
add_definitions(-DCH8_SHADERS_DIR=\"${CMAKE_SOURCE_DIR}/assets/shaders/\")
add_definitions(-DCH8_SOUNDS_DIR=\"${CMAKE_SOURCE_DIR}/assets/sounds/\")

add_subdirectory(app)
add_subdirectory(core)
add_subdirectory(ui)
add_subdirectory(graphio)
add_subdirectory(audiosys)
add_subdirectory(common)
add_subdirectory(external/events)
add_subdirectory(external/logger)
add_subdirectory(external/cglm)
