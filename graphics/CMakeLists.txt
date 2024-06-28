file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/**.c")

add_library(graphics SHARED "${SOURCES}")

target_include_directories(graphics PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
