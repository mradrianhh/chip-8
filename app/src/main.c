#include <stdio.h>
#include <unistd.h>

#include <core/cpu.h>
#include <core/memory.h>
#include <core/keys.h>

#include <loader/loader.h>

#include <application.h>

#ifdef CH8_EXAMPLE_ROMS_DIR
#define ROMS_BASE_PATH CH8_EXAMPLE_ROMS_DIR
#else
#define ROMS_BASE_PATH "../../assets/roms/"
#endif

int main(int argc, char **argv)
{
    core_InitializeLoader(LOG_LEVEL_FULL);

    CPUState *cpu = core_CreateCPU(60, gio_GetCurrentTime, LOG_LEVEL_FULL);
    core_LoadBinary16File(ROMS_BASE_PATH "ibm_logo.bin", cpu->memory, CH8_PROGRAM_START_ADDRESS, cpu->memory_size);
    
    Application *app = CreateApplication(&cpu->display, &cpu->keys, 60, LOG_LEVEL_FULL);

    core_StartCPU(cpu);

    RunApplication(app);
    
    core_StopCPU(cpu);

    DestroyApplication(app);

    core_DestroyCPU(cpu);

    core_DestroyLoader();
    return 0;
}
