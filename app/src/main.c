#include <stdio.h>
#include <unistd.h>

#include <core/cpu.h>
#include <core/memory.h>
#include <core/keys.h>

#include <loader/loader.h>

#include <application.h>

#ifdef CH8_PNGS_DIR
#define PNGS_BASE_PATH CH8_PNGS_DIR
#else
#define PNGS_BASE_PATH "../../assets/pngs/"
#endif

#ifdef CH8_EXAMPLE_ROMS_DIR
#define ROMS_BASE_PATH CH8_EXAMPLE_ROMS_DIR
#else
#define ROMS_BASE_PATH "../../assets/roms/"
#endif

#define TEST_SUITE_ROMS ROMS_BASE_PATH "test_suite/"

int main(int argc, char **argv)
{
    core_InitializeLoader(LOG_LEVEL_FULL);

    CPUState *cpu = core_CreateCPU(60, gio_GetCurrentTime, LOG_LEVEL_FULL);

    if (argc == 1)
    {
        core_LoadBinary16File(TEST_SUITE_ROMS "1-chip8-logo.ch8", cpu->memory,
                              CH8_PROGRAM_START_ADDRESS, cpu->memory_size);
    }
    else if (argc == 2)
    {
        core_LoadBinary16File(argv[1], cpu->memory,
                              CH8_PROGRAM_START_ADDRESS, cpu->memory_size);
    }

    Application *app = CreateApplication(&cpu->display, &cpu->keys, 60, LOG_LEVEL_FULL);

    core_StartCPU(cpu);

    RunApplication(app);

    core_StopCPU(cpu);

    DestroyApplication(app);

    gio_SavePixelBufferPNG(PNGS_BASE_PATH "display_buffer.png", cpu->display.display_buffer,
                           cpu->display.display_buffer_width,
                           cpu->display.display_buffer_height,
                           cpu->display.display_buffer_channels);

    core_DestroyCPU(cpu);

    core_DestroyLoader();
    return 0;
}
