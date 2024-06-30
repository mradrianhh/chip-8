file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/**.c")

# Vulkan SDK
find_library(Vulkan_LIBRARY NAMES vulkan PATHS "${CMAKE_SOURCE_DIR}/third-party/libs/vulkan" REQUIRED NO_DEFAULT_PATH)
IF (Vulkan_LIBRARY)
	set(Vulkan_FOUND ON)
	MESSAGE("Using bundled Vulkan library version")
ENDIF()

IF (NOT Vulkan_FOUND)
	message(FATAL_ERROR "Could not find Vulkan library!")
ELSE()
	message(STATUS ${Vulkan_LIBRARY})
ENDIF()

# Assimp
find_library(Assimp_LIBRARY NAMES assimp PATHS "${CMAKE_SOURCE_DIR}/third-party/libs/assimp" REQUIRED NO_DEFAULT_PATH)
if(Assimp_LIBRARY)
	set(Assimp_FOUND ON)
	message("Using bundled Assimp library")
endif()

if(NOT Assimp_FOUND)
	message(FATAL_ERROR "Could not find Assimp library!")
else()
	message(STATUS ${Assimp_LIBRARY})
endif()

add_library(graphics SHARED "${SOURCES}")

target_include_directories(graphics PUBLIC 
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_SOURCE_DIR}/third-party/includes"
	"${CMAKE_SOURCE_DIR}/external/cglm/include"
)

target_link_libraries(graphics PRIVATE logger core ${Vulkan_LIBRARY} ${Assimp_LIBRARY} glfw cglm m)
