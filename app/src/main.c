#include <stdio.h>
#include <unistd.h>

#include <core/cpu.h>
#include <core/memory.h>

#include <loader/loader.h>

#include <graphics/application.h>

#ifdef CH8_EXAMPLE_ROMS_DIR
#define ROMS_BASE_PATH CH8_EXAMPLE_ROMS_DIR
#else
#define ROMS_BASE_PATH "../../assets/roms/"
#endif

#ifdef CH8_PNGS_DIR
#define PNGS_BASE_PATH CH8_PNGS_DIR
#else
#define PNGS_BASE_PATH "../../assets/pngs/"
#endif

int main(int argc, char **argv)
{
    core_InitializeLoader(LOG_LEVEL_FULL);

    CPUState *cpu = core_InitializeCPU(LOG_LEVEL_FULL);
    core_LoadBinary16File(ROMS_BASE_PATH "ibm_logo.bin", cpu->memory, CH8_PROGRAM_START_ADDRESS, cpu->memory_size);

    gfxApplication *app = gfx_CreateApplication(cpu->display_buffer, cpu->display_buffer_size, &cpu->display_buffer_lock, LOG_LEVEL_FULL);
    gfx_StartApplication(app);

    for (int i = 0; i < 100; i++)
    {
        core_CycleCPU(cpu);
    }


    gfx_StopApplication(app);
    gfx_DestroyApplication(app);

    gfx_SavePixelBufferPNG(PNGS_BASE_PATH "display_buffer.png", cpu->display_buffer,
                           CH8_DISPLAY_WIDTH, CH8_DISPLAY_HEIGHT, CH8_INTERNAL_DISPLAY_CHANNELS);
    core_DestroyCPU(cpu);

    core_DestroyLoader();
    return 0;
}
