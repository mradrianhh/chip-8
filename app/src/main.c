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

int main(int argc, char **argv)
{
    // Loader is used by CPU so must be initialized first.
    core_InitializeLoader(LOG_LEVEL_FULL);
    CPUState *cpu = core_InitializeCPU(LOG_LEVEL_FULL);
    Application *app = gfx_CreateApplication(cpu->display_buffer, cpu->display_buffer_size, &cpu->display_buffer_lock, LOG_LEVEL_FULL);

    // Test binary.
    // Should load program data starting at address 0x0200.
    core_LoadBinary16File(ROMS_BASE_PATH "ibm_logo.bin", cpu->memory, 0x200, cpu->memory_size);

    gfx_StartApplication(app);

    sleep(10);

    for (int i = 0; i < 100; i++)
    {
        core_CycleCPU(cpu);
    }

    gfx_StopApplication(app);

    gfx_DestroyApplication(app);
    core_DestroyCPU(cpu);
    core_DestroyLoader();
    return 0;
}
