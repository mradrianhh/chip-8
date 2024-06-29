#include <stdio.h>

#include <core/cpu.h>
#include <core/memory.h>

#include <loader/loader.h>

#ifdef CH8_EXAMPLE_ROMS_DIR
#define ROMS_BASE_PATH CH8_EXAMPLE_ROMS_DIR
#else
#define ROMS_BASE_PATH "../../assets/roms/"
#endif

int main(int argc, char **argv)
{
    CPUState *cpu = core_InitializeCPU();
    core_InitializeLoader();

    // Test binary.
    // Should load program data starting at address 0x0200.
    core_LoadBinary16File(ROMS_BASE_PATH "ibm_logo.bin", cpu->memory, 0x200, cpu->memory_size);

    core_DumpMemoryCPU(cpu);

    for(int i = 0; i < 100; i++)
    {
        core_CycleCPU(cpu);
    }

    core_DestroyLoader();
    core_DestroyCPU(cpu);
    return 0;
}
