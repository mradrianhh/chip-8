#ifndef CORE_CPU_H
#define CORE_CPU_H

#include <stdint.h>
#include <pthread.h>

#include "logger/logger.h"

#define CH8_MEM_SIZE (4096)
#define CH8_VREG_COUNT (16)
// 64 pixels width, 32 pixels height. 4 bytes per pixel.
#define CH8_DISPLAY_WIDTH (64)
#define CH8_DISPLAY_HEIGHT (32)
#define CH8_INTERNAL_DISPLAY_CHANNELS (4)
#define CH8_INTERNAL_DISPLAY_BUFFER_SIZE (CH8_DISPLAY_WIDTH * CH8_DISPLAY_HEIGHT * CH8_INTERNAL_DISPLAY_CHANNELS)
#define CH8_DISPLAY_BUFFER_SIZE (CH8_DISPLAY_WIDTH * CH8_DISPLAY_HEIGHT)

#define CH8_STACK_DEPTH (16)

#define CH8_FONT_START_ADDRESS (0x50)
#define CH8_FONT_SIZE (16 * 5)
#define CH8_PROGRAM_START_ADDRESS (0x200)

typedef struct CPUState
{
    // Memory
    uint8_t memory[CH8_MEM_SIZE];
    size_t memory_size;
    // We need a lock on the display buffer since it will be read from the graphics thread.
    // This is to prevent the CPU from updating it while we're drawing.
    pthread_mutex_t display_buffer_lock;
    uint8_t display_buffer[CH8_INTERNAL_DISPLAY_BUFFER_SIZE];
    size_t display_buffer_size;
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

CPUState *core_InitializeCPU(LogLevel log_level);
void core_DestroyCPU(CPUState *cpu);
void core_CycleCPU(CPUState *cpu);
void core_DumpMemoryCPU(CPUState *cpu);

#endif
