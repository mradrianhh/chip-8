file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/**.c")

add_library(common SHARED "${SOURCES}")

target_include_directories(common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
