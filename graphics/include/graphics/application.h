#ifndef GRAPHICS_APPLICATION_H
#define GRAPHICS_APPLICATION_H

#include <stdint.h>

#include <logger/logger.h>

typedef struct Application
{
    Logger *logger;
    uint8_t *src_display_buffer;
    size_t src_display_buffer_size;
} Application;

/// @brief Create and initialize an application.
/// @details This sets up all the vulkan configuration, GLFW configuration and custom configurations.
/// @param src_display_buffer Pointer to the memory region that the graphics library will draw from.
/// @param src_display_buffer_size Size of memory region that the graphics library will draw from.
/// @return Returns a handle to the created application.
Application *gfx_CreateApplication(uint8_t *src_display_buffer, size_t src_display_buffer_size);

/// @brief Destroy application and free memory.
/// @param app application to destroy.
void gfx_DestroyApplication(Application *app);

#endif
