file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/**.c")

add_library(core SHARED "${SOURCES}")

target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(core PRIVATE logger)
