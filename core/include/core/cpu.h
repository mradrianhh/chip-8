#ifndef CORE_CPU_H
#define CORE_CPU_H

#include <stdint.h>

#include "logger/logger.h"

#define CH8_MEM_SIZE                (4096)
#define CH8_VREG_COUNT              (16)
#define CH8_DISPLAY_SIZE            (64 * 32)
#define CH8_STACK_DEPTH             (16)
#define CH8_FONT_SIZE               (16 * 5)
#define CH8_PROGRAM_START_ADDRESS   (0x200)

typedef struct CPUState
{
    // Memory
    uint8_t memory[CH8_MEM_SIZE];
    size_t memory_size;
    uint32_t display_buffer[CH8_DISPLAY_SIZE];
    uint16_t stack[CH8_STACK_DEPTH];
    uint16_t *stack_pointer;
    // Registers
    uint8_t variable_registers[CH8_VREG_COUNT];
    uint16_t program_counter;
    uint16_t index_register;
    // Timers
    uint8_t delay_timer;
    uint8_t sound_timer;
    // Internal
    Logger *logger;
} CPUState;

CPUState *core_InitializeCPU();
void core_DestroyCPU(CPUState *cpu);
void core_CycleCPU(CPUState *cpu);
void core_DumpMemoryCPU(CPUState *cpu);

#endif
