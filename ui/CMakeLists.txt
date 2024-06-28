file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/**.c")

add_library(ui SHARED "${SOURCES}")

target_include_directories(ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
