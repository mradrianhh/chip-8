file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/**.c")

add_library(events SHARED "${SOURCES}")

target_include_directories(events PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(events PRIVATE logger)
