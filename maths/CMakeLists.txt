file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/**.c")

add_library(maths SHARED "${SOURCES}")

target_include_directories(maths PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
