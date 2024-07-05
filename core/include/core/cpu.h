#ifndef CORE_CPU_H
#define CORE_CPU_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#include "logger/logger.h"
#include "display.h"

#define CH8_MEM_SIZE (4096)
#define CH8_VREG_COUNT (16)

#define CH8_STACK_DEPTH (16)

#define CH8_FONT_START_ADDRESS (0x50)
#define CH8_FONT_SIZE (16 * 5)
#define CH8_PROGRAM_START_ADDRESS (0x200)

#define CH8_TIMER_FREQUENCY (60)

typedef struct CPUState
{
    // Memory
    uint8_t memory[CH8_MEM_SIZE];
    size_t memory_size;
    size_t font_start_address;
    // Peripherals
    Display display;
    uint16_t keys;
    // Stack
    uint16_t stack[CH8_STACK_DEPTH];
    uint16_t *stack_pointer;
    // Registers
    uint8_t variable_registers[CH8_VREG_COUNT];
    uint16_t program_counter;
    uint16_t index_register;
    // Timers
    uint8_t delay_timer;
    pthread_t delay_timer_thread_id;
    uint8_t sound_timer;
    pthread_t sound_timer_thread_id;
    // Clock
    double timer_target_frequency;
    double clock_target_frequency;
    double (*pfn_get_time)();
    // Internal
    Logger *logger;
    pthread_t thread_id;
    bool running;
} CPUState;

CPUState *core_CreateCPU(uint8_t clock_target_freq, double (*pfn_get_time)(), LogLevel log_level);

void core_StartCPU(CPUState *cpu);

void core_StopCPU(CPUState *cpu);

void core_DestroyCPU(CPUState *cpu);

void core_DumpMemoryCPU(CPUState *cpu);

#endif
