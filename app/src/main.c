#include <stdio.h>

#include "testcore.h"
#include "testui.h"
#include "testgraphics.h"
#include "testio.h"
#include "testaudio.h"
#include "logger.h"
#include "events.h"

#ifdef CH8_LOGS_DIR
#define LOGS_BASE_PATH CH8_LOGS_DIR
#else 
#define LOGS_BASE_PATH "../../logs/"
#endif

int main(int argc, char **argv)
{
    test_core();
    test_ui();
    test_graphics();
    test_io();
    test_audio();
    
    set_log_level(LOG_LEVEL_FULL);
    Logger *logger = logger_init(LOGS_BASE_PATH "app.log");
    log_info(logger, "Testing app");
    logger_destroy(logger);

    events_init();
    events_shutdown();

    return 0;
}
