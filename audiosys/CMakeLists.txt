file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/**.c")

# Find OpenAL library.
find_library(openal_LIBRARY NAMES openal PATHS "${CMAKE_SOURCE_DIR}/third-party/libs/openal" REQUIRED NO_DEFAULT_PATH)
IF (openal_LIBRARY)
	set(openal_FOUND ON)
	MESSAGE("Using bundled OpenAL library version")
ENDIF()

IF (NOT openal_LIBRARY)
	message(FATAL_ERROR "Could not find openal library!")
ELSE()
	message(STATUS ${openal_LIBRARY})
ENDIF()

add_library(audiosys SHARED "${SOURCES}")

target_include_directories(audiosys PUBLIC 
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_SOURCE_DIR}/third-party/includes"
)

target_link_libraries(audiosys PRIVATE logger ${openal_LIBRARY} audio)
