#ifndef GRAPHICS_APPLICATION_H
#define GRAPHICS_APPLICATION_H

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

#include <logger/logger.h>

#include "internal.h"

typedef struct Application
{
    Logger *logger;
    uint8_t *src_display_buffer;
    size_t src_display_buffer_size;
    pthread_mutex_t *src_display_buffer_lock;
    uint8_t fps;
    // Internal
    pthread_t thread_id;
    bool running;
    // Internal timing
    struct timespec start_time;
    struct timespec end_time;
    struct timespec delay_time;
    long delta_time_nsec;
    long frame_frequency_nsec;
    // Internal graphics context.
    GraphicsContext *gfx_context;
} Application;

/// @brief Create and initialize an application.
/// @details This sets up all the vulkan configuration, GLFW configuration and custom configurations.
/// @param src_display_buffer Pointer to the memory region that the graphics library will draw from.
/// @param src_display_buffer_size Size of memory region that the graphics library will draw from.
/// @return Returns a handle to the created application.
Application *gfx_CreateApplication(uint8_t *src_display_buffer, size_t src_display_buffer_size, pthread_mutex_t *src_display_buffer_lock, LogLevel log_level);

/// @brief Destroy application and free memory.
/// @param app application to destroy.
void gfx_DestroyApplication(Application *app);

/// @brief Starts the application in a new thread with a render loop at 60 hz.
/// @param app Handle to application.
void gfx_StartApplication(Application *app);

/// @brief Stops the application by signaling the render loop to stop.
/// @remark This will end the thread but not destroy the application.
/// @param app Handle to application.
void gfx_StopApplication(Application *app);

#endif
