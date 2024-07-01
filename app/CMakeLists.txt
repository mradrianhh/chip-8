file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/**.c")

add_executable(chip-8 "${SOURCES}")

target_include_directories(chip-8 PUBLIC 
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_SOURCE_DIR}/third-party/includes"
)

# Must link 'm' to use STB.
target_link_libraries(chip-8 PRIVATE core ui graphics events logger io audio m)
