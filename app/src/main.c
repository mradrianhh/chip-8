#include <stdio.h>
#include <unistd.h>

#include <core/cpu.h>
#include <core/memory.h>
#include <core/keys.h>

#include <loader/loader.h>

#include <graphio/application.h>

#ifdef CH8_EXAMPLE_ROMS_DIR
#define ROMS_BASE_PATH CH8_EXAMPLE_ROMS_DIR
#else
#define ROMS_BASE_PATH "../../assets/roms/"
#endif

int main(int argc, char **argv)
{
    core_InitializeLoader(LOG_LEVEL_FULL);

    CPUState *cpu = core_InitializeCPU(LOG_LEVEL_FULL);
    core_LoadBinary16File(ROMS_BASE_PATH "delay_timer_test.bin", cpu->memory, CH8_PROGRAM_START_ADDRESS, cpu->memory_size);

    // Run CPU, create PNG.
    for (int i = 0; i < 100; i++)
    {
        core_CycleCPU(cpu);
    }

    gioApplication *app = gio_CreateApplication(&cpu->display, &cpu->keys, 1, LOG_LEVEL_FULL);
    gio_StartApplication(app);
    while(true){}
    gio_StopApplication(app);
    gio_DestroyApplication(app);

    core_DestroyCPU(cpu);

    core_DestroyLoader();
    return 0;
}
