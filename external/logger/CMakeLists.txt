file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/**.c")

add_library(logger SHARED "${SOURCES}")

target_include_directories(logger PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
