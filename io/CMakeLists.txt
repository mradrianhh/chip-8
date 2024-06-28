file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/**.c")

add_library(io SHARED "${SOURCES}")

target_include_directories(io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
