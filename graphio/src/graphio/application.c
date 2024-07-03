#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include <timing/timing.h>

#include "graphio/application.h"

static void *RunApplication(void *vargp);

gioApplication *gio_CreateApplication(Display *display, uint16_t *keys, uint8_t target_fps, LogLevel log_level)
{
    gioApplication *app = calloc(1, sizeof(gioApplication));

    // Configure logger.
    app->logger = logger_Initialize(LOGS_BASE_PATH "graphics.log", log_level);

    app->running = false;

    // Initialize timing.
    app->target_fps = target_fps;
    app->frame_target_frequency = (double)1 / app->target_fps;
    logger_LogDebug(app->logger, "Frame target frequency: %f secs.", app->frame_target_frequency);

    // Create and configure graphics context.
    app->gio_context = gio_CreateGraphioContext(app->logger, display, keys);
    
    return app;
}

void gio_DestroyApplication(gioApplication *app)
{
    if (app->running == false)
    {
        gio_StopApplication(app);
    }

    gio_DestroyGraphioContext(app->gio_context);

    logger_Destroy(app->logger);

    free(app);
}

void gio_StartApplication(gioApplication *app)
{
    app->running = true;
    pthread_create(&(app->thread_id), NULL, RunApplication, (void *)app);
}

void gio_StopApplication(gioApplication *app)
{
    app->running = false;
    pthread_join(app->thread_id, NULL);
    gio_StopGraphioContext(app->gio_context);
}

void gio_SavePixelBufferPNG(const char *filename, uint8_t *pixel_buffer, uint8_t width, uint8_t height, uint8_t channels)
{
    stbi_write_png(filename, width, height, channels, pixel_buffer, width * channels);
}

void *RunApplication(void *vargp)
{
    gioApplication *app = vargp;

    // Each cycle of this loop is one frame.
    while (app->running && !glfwWindowShouldClose(app->gio_context->window))
    {
        // Get start time of frame. TODO: Use GLFW to get time.
        double start_time = gio_GetCurrentTime();

        // Poll events
        glfwPollEvents();

        // TODO: Update texture with data from display buffer.
        gio_UpdateTexture(app->gio_context);

        // TODO: Draw texture to screen.
        gio_DrawFrame(app->gio_context);

        // Get end time of frame, calculate delta and delay.
        double end_time = gio_GetCurrentTime();
        
        double delta_time = end_time - start_time;
        struct timespec delay_time = {
            .tv_nsec = SEC_TO_NS(app->frame_target_frequency - delta_time),
        };

        // Cap at target FPS.
        if (delay_time.tv_nsec > 0)
        {
            nanosleep(&delay_time, NULL);
        }

        // Calculate FPS.
        end_time = gio_GetCurrentTime();
        delta_time = end_time - app->prev_fps_update_time;
        app->frame_count++;
        if (delta_time >= 1.0)
        {
            double fps = (double)app->frame_count / delta_time;
            gio_UpdateFPS(app->gio_context, fps);

            app->frame_count = 0;
            app->prev_fps_update_time = end_time;
        }
    }

    pthread_exit(NULL);
}
