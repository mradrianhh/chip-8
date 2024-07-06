#ifndef APP_APPLICATION_H
#define APP_APPLICATION_H

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#include <logger/logger.h>
#include <core/keys.h>
#include <graphio/graphio.h>

typedef struct Application
{
    Logger *logger;
    // Internal
    pthread_t thread_id;
    // Internal timing
    uint8_t target_fps;
    double frame_target_frequency;
    uint64_t frame_count;
    double prev_fps_update_time;
    // Context and data
    GraphioContext *gio_context;
    uint16_t keys;
} Application;

/// @brief Create and initialize an application.
/// @details This sets up all the vulkan configuration, GLFW configuration and custom configurations.
/// @param src_display_buffer Pointer to the memory region that the graphics library will draw from.
/// @param src_display_buffer_size Size of memory region that the graphics library will draw from.
/// @return Returns a handle to the created application.
Application *CreateApplication(Display *display, uint16_t *keys, uint8_t target_fps, LogLevel log_level);

/// @brief Starts the application in a new thread with a render loop at 60 hz.
/// @param app Handle to application.
void RunApplication(Application *app);

/// @brief Destroy application and free memory.
/// @param app application to destroy.
void DestroyApplication(Application *app);

#endif
