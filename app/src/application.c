#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include <timing/timing.h>

#include "application.h"

Application *CreateApplication(Display *display, uint16_t *keys, uint8_t target_fps, LogLevel log_level)
{
    Application *app = calloc(1, sizeof(Application));

    // Configure logger.
    app->logger = logger_Initialize(LOGS_BASE_PATH "application.log", log_level);

    // Initialize timing.
    app->target_fps = target_fps;
    app->frame_target_frequency = (double)1 / app->target_fps;
    logger_LogDebug(app->logger, "Frame target frequency: %f secs.", app->frame_target_frequency);

    // Create and configure graphics context.
    app->gio_context = gio_CreateGraphioContext(app->logger, display, keys);

    return app;
}

void RunApplication(Application *app)
{
    // Each cycle of this loop is one frame.
    while (!glfwWindowShouldClose(app->gio_context->window))
    {
        // Get start time of frame.
        double start_time = gio_GetCurrentTime();

        // Poll events
        glfwPollEvents();

        // TODO: Draw display to screen.
        gio_Draw(app->gio_context);

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

    gio_StopGraphioContext(app->gio_context);
}

void DestroyApplication(Application *app)
{
    gio_DestroyGraphioContext(app->gio_context);

    logger_Destroy(app->logger);

    free(app);
}
