#include <stdlib.h>
#include <signal.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include <timing/timing.h>

#include "graphics/application.h"

static void *RunApplication(void *vargp);

Application *gfx_CreateApplication(uint8_t *src_display_buffer, size_t src_display_buffer_size, pthread_mutex_t *src_display_buffer_lock, LogLevel log_level)
{
    Application *app = calloc(1, sizeof(Application));

    // Configure logger.
    app->logger = logger_Initialize(LOGS_BASE_PATH "graphics.log", log_level);

    app->src_display_buffer = src_display_buffer;
    app->src_display_buffer_size = src_display_buffer_size;
    app->src_display_buffer_lock = src_display_buffer_lock;

    app->running = false;

    // Initialize timing.
    app->fps = 60;
    app->start_time.tv_nsec = 0;
    app->start_time.tv_sec = 0;
    app->end_time.tv_nsec = 0;
    app->end_time.tv_sec = 0;
    app->delay_time.tv_nsec = 0;
    app->delay_time.tv_sec = 0;
    app->delta_time_nsec = 0;
    // (1 / app->fps) * SEC_TO_NS_FACTOR = SEC_TO_NS_FACTOR / app->fps.
    app->frame_frequency_nsec = SEC_TO_NS_FACTOR / app->fps;
    logger_LogDebug(app->logger, "Frame frequency: %lu nsec.", app->frame_frequency_nsec);

    // Create and configure graphics context.
    app->gfx_context = gfx_CreateGraphicsContext(app->logger);

    return app;
}

void gfx_DestroyApplication(Application *app)
{
    if (app->running == false)
    {
        gfx_StopApplication(app);
    }

    gfx_DestroyGraphicsContext(app->gfx_context);

    logger_Destroy(app->logger);

    free(app);
}

void gfx_StartApplication(Application *app)
{
    app->running = true;
    pthread_create(&(app->thread_id), NULL, RunApplication, (void *)app);
}

void gfx_StopApplication(Application *app)
{
    app->running = false;
    pthread_join(app->thread_id, NULL);
}

void gfx_SavePixelBufferPNG(const char *filename, uint8_t *pixel_buffer, uint8_t width, uint8_t height, uint8_t channels)
{
    stbi_write_png(filename, width, height, channels, pixel_buffer, width * channels);
}

void *RunApplication(void *vargp)
{
    Application *app = vargp;

    // Each cycle of this loop is one frame.
    while (app->running && !glfwWindowShouldClose(app->gfx_context->window))
    {
        // Get start time of frame. TODO: Use GLFW to get time.
        timespec_get(&app->start_time, TIME_UTC);

        // Poll events
        glfwPollEvents();

        // TODO: Update texture with data from display buffer.
        
        // TODO: Draw texture to screen.

        // Get end time of frame, calculate delta and delay.
        timespec_get(&app->end_time, TIME_UTC);

        app->delta_time_nsec = SEC_TO_NS(app->end_time.tv_sec - app->start_time.tv_sec) + (app->end_time.tv_nsec - app->start_time.tv_nsec);
        app->delay_time.tv_nsec = app->frame_frequency_nsec - app->delta_time_nsec;

        if (app->delay_time.tv_nsec > 0)
        {
            nanosleep(&app->delay_time, NULL);
        }
    }

    //CALL_VK(vkDeviceWaitIdle(app->gfx_context->device), app->logger, "Failed while waiting for device to go idle.");

    pthread_exit(NULL);
}
