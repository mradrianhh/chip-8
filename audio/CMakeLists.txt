file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/**.c")

add_library(audio SHARED "${SOURCES}")

target_include_directories(audio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
