#include <stdlib.h>

#include "graphics/application.h"

Application *gfx_CreateApplication(uint8_t *src_display_buffer, size_t src_display_buffer_size)
{
    Application *app = calloc(1, sizeof(Application));

    // Configure logger.
    app->logger = logger_Initialize(LOGS_BASE_PATH "graphics.log");

    app->src_display_buffer = src_display_buffer;
    app->src_display_buffer_size = src_display_buffer_size;

    return app;
}

void gfx_DestroyApplication(Application *app)
{
    logger_Destroy(app->logger);
    
    free(app);
}
