file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/**.c")

add_library(core SHARED "${SOURCES}")

target_include_directories(core PUBLIC 
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_SOURCE_DIR}/third-party/includes"
)

target_link_libraries(core PUBLIC logger common audiosys)
