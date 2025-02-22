#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <stdbool.h>

#include "core/cpu.h"
#include "core/memory.h"
#include "core/keys.h"
#include "timing/timing.h"
#include "loader/loader.h"

static uint8_t font_data[CH8_FONT_SIZE] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

static void *RunCPU(void *vargp);
static void *RunDelayTimer(void *vargp);
static void *RunSoundTimer(void *vargp);
static void CycleCPU(CPUState *cpu);
// Returns true if any pixels were turned off.
static bool SetPixel(CPUState *cpu, uint8_t x, uint8_t y, uint8_t pixel_value);
static bool SetPixels(CPUState *cpu, uint8_t x, uint8_t y, uint8_t pixels);
static void SetAlpha(CPUState *cpu, uint8_t x, uint8_t y, uint8_t alpha_value);
static bool KeyPressed(CPUState *cpu, uint16_t key_bit);
static uint8_t WaitKeyPressed(CPUState *cpu);
static void PushStack(CPUState *cpu, uint16_t pc);
static uint16_t PopStack(CPUState *cpu);

CPUState *core_CreateCPU(uint8_t clock_target_freq, double (*pfn_get_time)(), LogLevel log_level)
{
    CPUState *cpu = calloc(1, sizeof(CPUState));

    // Internal
    cpu->running = false;
    cpu->clock_target_frequency = (double)1 / clock_target_freq;
    cpu->timer_target_frequency = (double)1 / CH8_TIMER_FREQUENCY;
    cpu->pfn_get_time = pfn_get_time;
    cpu->logger = logger_Initialize(LOGS_BASE_PATH "cpu.log", log_level);

    cpu->font_start_address = CH8_FONT_START_ADDRESS;
    cpu->memory_size = CH8_MEM_SIZE;

    cpu->display.display_buffer_size = CH8_INTERNAL_DISPLAY_BUFFER_SIZE;
    cpu->display.display_buffer_width = CH8_DISPLAY_WIDTH;
    cpu->display.display_buffer_height = CH8_DISPLAY_HEIGHT;
    cpu->display.display_buffer_channels = CH8_INTERNAL_DISPLAY_CHANNELS;
    pthread_mutex_init(&cpu->display.display_buffer_lock, NULL);
    // Initialize alpha-channel to 0xFF.
    for (uint8_t y = 0; y < CH8_DISPLAY_HEIGHT; y++)
    {
        for (uint8_t x = 0; x < CH8_DISPLAY_WIDTH; x++)
        {
            SetAlpha(cpu, x, y, 0xFF);
        }
    }

    cpu->stack_pointer = cpu->stack;

    cpu->delay_timer = 0;

    cpu->sound_timer = 0;
    cpu->audio_context = aud_CreateAudioContext(1);

    if (!aud_CreateSound(cpu->audio_context, SOUNDS_BASE_PATH "sound_timer.wav",
                         SOUND_TIMER_SOUND_SLOT, true))
        logger_LogError(cpu->logger, "Failed to create sound timer sound.");

    cpu->index_register = 0;
    cpu->program_counter = CH8_PROGRAM_START_ADDRESS;

    // Load font into memory.
    logger_LogInfo(cpu->logger, "Loading font starting at address 0x%04x.", CH8_FONT_START_ADDRESS);
    uint16_t end_address = core_LoadBinary16Data(cpu->memory, CH8_FONT_START_ADDRESS, cpu->memory_size, font_data, CH8_FONT_SIZE);
    // Check alignment is correct. Should be 2-byte alignment.
    if (end_address % 2 != 0)
    {
        logger_LogError(cpu->logger, "Font data does not have correct alignment. Alignment should be 2 bytes.");
        raise(SIGABRT);
    }

    return cpu;
}

void core_StartCPU(CPUState *cpu)
{
    cpu->running = true;
    pthread_create(&cpu->thread_id, NULL, RunCPU, (void *)cpu);
    pthread_create(&cpu->delay_timer_thread_id, NULL, RunDelayTimer, (void *)cpu);
    pthread_create(&cpu->sound_timer_thread_id, NULL, RunSoundTimer, (void *)cpu);
    logger_LogInfo(cpu->logger, "Starting CPU on thread 0x%016lx.", cpu->thread_id);
}

void core_StopCPU(CPUState *cpu)
{
    logger_LogInfo(cpu->logger, "Stopping CPU on thread 0x%016lx.", cpu->thread_id);
    cpu->running = false;
    pthread_join(cpu->thread_id, NULL);
    pthread_join(cpu->delay_timer_thread_id, NULL);
    pthread_join(cpu->sound_timer_thread_id, NULL);
}

void core_DestroyCPU(CPUState *cpu)
{
    if (cpu->running)
    {
        core_StopCPU(cpu);
    }
    logger_Destroy(cpu->logger);
    aud_DestroyAudioContext(cpu->audio_context);
    free(cpu);
}

void core_DumpMemoryCPU(CPUState *cpu)
{
    for (size_t i = 0; i < cpu->memory_size;)
    {
        if (i % 8 == 0)
            printf("\n%04zu(0x%04zx): ", i, i);

        printf("%04x ", READ_16BIT(cpu->memory, i));
    }

    printf("\n");
}

void *RunCPU(void *vargp)
{
    CPUState *cpu = vargp;
    while (cpu->running)
    {
        // Get start time of cycle.
        double start_time = cpu->pfn_get_time();

        // Do work
        CycleCPU(cpu);

        // Get end time of frame, calculate delta and delay.
        double end_time = cpu->pfn_get_time();

        double delta_time = end_time - start_time;
        struct timespec delay_time = {
            .tv_nsec = SEC_TO_NS(cpu->clock_target_frequency - delta_time),
        };

        // Cap at target clock frequency.
        if (delay_time.tv_nsec > 0)
        {
            nanosleep(&delay_time, NULL);
        }
    }

    pthread_exit(NULL);
}

void *RunDelayTimer(void *vargp)
{
    CPUState *cpu = vargp;
    while (cpu->running)
    {
        // Get start time of cycle.
        double start_time = cpu->pfn_get_time();

        if (cpu->delay_timer > 0)
            cpu->delay_timer--;

        // Get end time of frame, calculate delta and delay.
        double end_time = cpu->pfn_get_time();

        double delta_time = end_time - start_time;
        struct timespec delay_time = {
            .tv_nsec = SEC_TO_NS(cpu->timer_target_frequency - delta_time),
        };

        // Cap at target clock frequency.
        if (delay_time.tv_nsec > 0)
        {
            nanosleep(&delay_time, NULL);
        }
    }

    pthread_exit(NULL);
}

void *RunSoundTimer(void *vargp)
{
    CPUState *cpu = vargp;
    bool sound_playing = false;
    while (cpu->running)
    {
        // Get start time of cycle.
        double start_time = cpu->pfn_get_time();

        if (cpu->sound_timer > 0)
        {
            if(!sound_playing)
            {
                aud_PlaySound(cpu->audio_context, SOUND_TIMER_SOUND_SLOT);
                sound_playing = true;
            }
            cpu->sound_timer--;
        }
        else
        {
            aud_StopSound(cpu->audio_context, SOUND_TIMER_SOUND_SLOT);
            sound_playing = false;
        }

        // Get end time of frame, calculate delta and delay.
        double end_time = cpu->pfn_get_time();

        double delta_time = end_time - start_time;
        struct timespec delay_time = {
            .tv_nsec = SEC_TO_NS(cpu->timer_target_frequency - delta_time),
        };

        // Cap at target clock frequency.
        if (delay_time.tv_nsec > 0)
        {
            nanosleep(&delay_time, NULL);
        }
    }

    pthread_exit(NULL);
}

void CycleCPU(CPUState *cpu)
{
    // Fetch instruction.
    // Side-effect: increases program_counter by 2.
    uint16_t instruction = READ_16BIT(cpu->memory, cpu->program_counter);

    // Decode and execute instruction.
    // Test on most significant nibble.
    switch (instruction & 0xF000)
    {
    case 0x0000:
    {
        switch (instruction & 0x00FF)
        {
        // 0x00E0 - Clear screen.
        case 0x00E0:
        {
            // Set every pixel to 0.
            for (uint8_t y = 0; y < CH8_DISPLAY_HEIGHT; y++)
            {
                for (uint8_t x = 0; x < CH8_DISPLAY_WIDTH; x++)
                {
                    SetPixel(cpu, x, y, 0);
                }
            }
            logger_LogDebug(cpu->logger, "(0x%04X) - Clear screen.", instruction);
            break;
        }
        // 0x00EE - Return.
        case 0x00EE:
        {
            // Pop previous PC of stack and set current PC.
            cpu->program_counter = PopStack(cpu);
            logger_LogDebug(cpu->logger, "(0x%04X) - Return.", instruction);
            break;
        }
        // 0x0NNN - Ignored as we're not running on a machine with actual chip-8 support.
        default:
            logger_LogDebug(cpu->logger, "(0x%04X) - Call machine code routine(NOT IMPLEMENTED).", instruction);
            break;
        }
        break;
    }
    // 0x1NNN - Jump to NNN.
    case 0x1000:
    {
        // Extract 12-bit immediate address(NNN).
        uint16_t immediate_addr = instruction & 0x0FFF;
        // Set PC.
        cpu->program_counter = immediate_addr;
        logger_LogDebug(cpu->logger, "(0x%04X) - Jump to 0x%04X.", instruction, immediate_addr);
        break;
    }
    // 0x2NNN - Call subroutine at address NNN.
    case 0x2000:
    {
        // Extract 12-bit immediate address(NNN).
        uint16_t immediate_addr = instruction & 0x0FFF;
        // Push current PC on stack.
        PushStack(cpu, cpu->program_counter);
        // Set PC to NNN.
        cpu->program_counter = immediate_addr;
        logger_LogDebug(cpu->logger, "(0x%04X) - Call subroutine at address %04X.", instruction, immediate_addr);
        break;
    }
    // 0x3XNN - Skips next instruction if Vx == NN.
    case 0x3000:
    {
        // Extract 4-bit register index(X).
        uint8_t register_index = (instruction & 0x0F00) >> 8;
        // Extract 8-bit immediate value(NN).
        uint8_t immediate_value = instruction & 0x00FF;
        // Skip next instruction if VX == NN.
        if (cpu->variable_registers[register_index] == immediate_value)
        {
            // Note: we only increase by two here because PC is automatically incremented
            //       when we read the next instruction.
            cpu->program_counter += 2;
        }
        logger_LogDebug(cpu->logger, "(0x%04X) - Skip next instruction if (V%X(%02X) == %02X)(%s).",
                        instruction, register_index, cpu->variable_registers[register_index], immediate_value,
                        cpu->variable_registers[register_index] == immediate_value ? "true" : "false");
        break;
    }
    // 0x4XNN - Skips next instruction if VX != NN.
    case 0x4000:
    {
        // Extract 4-bit register index(X).
        uint8_t register_index = (instruction & 0x0F00) >> 8;
        // Extract 8-bit immediate value(NN).
        uint8_t immediate_value = instruction & 0x00FF;
        // Skip next instruction if VX == NN.
        if (cpu->variable_registers[register_index] != immediate_value)
        {
            // Note: we only increase by two here because PC is automatically incremented
            //       when we read the next instruction.
            cpu->program_counter += 2;
        }
        logger_LogDebug(cpu->logger, "(0x%04X) - Skip next instruction if (V%X(%02X) != %02X)(%s).",
                        instruction, register_index, cpu->variable_registers[register_index], immediate_value,
                        cpu->variable_registers[register_index] != immediate_value ? "true" : "false");
        break;
    }
    // 0x5XY0 - Skips next instruction if VX == VY.
    case 0x5000:
    {
        // Extract 4-bit register index(X).
        uint8_t register_index_x = (instruction & 0x0F00) >> 8;
        // Extract 4-bit register_index(Y).
        uint8_t register_index_y = (instruction & 0x00F0) >> 4;
        // Skip next instruction if VX == NN.
        if (cpu->variable_registers[register_index_x] == cpu->variable_registers[register_index_y])
        {
            // Note: we only increase by two here because PC is automatically incremented
            //       when we read the next instruction.
            cpu->program_counter += 2;
        }
        logger_LogDebug(cpu->logger, "(0x%04X) - Skip next instruction if (V%X(%02X) == V%X(%02X))(%s).",
                        instruction, register_index_x, cpu->variable_registers[register_index_x],
                        register_index_y, cpu->variable_registers[register_index_y],
                        cpu->variable_registers[register_index_x] == cpu->variable_registers[register_index_y] ? "true" : "false");
        break;
    }
    // 0x6XNN - Set Vx to NN.
    case 0x6000:
    {
        // Extract 4-bit register index(X).
        uint8_t register_index = (instruction & 0x0F00) >> 8;
        // Extract 8-bit immediate value(NN).
        uint8_t immediate_value = instruction & 0x00FF;
        // Set Vx.
        cpu->variable_registers[register_index] = immediate_value;
        logger_LogDebug(cpu->logger, "(0x%04X) - Set V%X to 0x%02X.", instruction, register_index, immediate_value);
        break;
    }
    // 0x7XNN - Add NN to Vx.
    case 0x7000:
    {
        // Extract 4-bit register index(X).
        uint8_t register_index = (instruction & 0x0F00) >> 8;
        // Extract 8-bit immediate value(NN).
        uint8_t immediate_value = instruction & 0x00FF;
        // Add to Vx.
        cpu->variable_registers[register_index] += immediate_value;
        logger_LogDebug(cpu->logger, "(0x%04X) - Add 0x%02X to V%X.", instruction, register_index, immediate_value);
        break;
    }
    // 0x8XY_ - All 0x8000 instructions have X/Y register index.
    case 0x8000:
    {
        // Extract 4-bit register index(X).
        uint8_t register_index_x = (instruction & 0x0F00) >> 8;
        // Extract 4-bit register index(Y).
        uint8_t register_index_y = (instruction & 0x00F0) >> 4;

        switch (instruction & 0x000F)
        {
        // 0x8XY0 - Set VX to VY.
        case 0x0000:
        {
            // Set VX to VY.
            cpu->variable_registers[register_index_x] = cpu->variable_registers[register_index_y];
            logger_LogDebug(cpu->logger, "(0x%04X) - Set V%X to V%X(%02X).",
                            instruction, register_index_x, register_index_y, cpu->variable_registers[register_index_y]);
            break;
        }
        // 0x8XY1 - Set VX to VX | VY.
        case 0x0001:
        {
            cpu->variable_registers[register_index_x] |= cpu->variable_registers[register_index_y];
            logger_LogDebug(cpu->logger, "(0x%04X) - Set V%X to V%X(%02X) | V%X(%02X).",
                            instruction, register_index_x,
                            register_index_x, cpu->variable_registers[register_index_x],
                            register_index_y, cpu->variable_registers[register_index_y]);
            break;
        }
        // 0x8XY2 - Set VX to VX & VY.
        case 0x0002:
        {
            cpu->variable_registers[register_index_x] &= cpu->variable_registers[register_index_y];
            logger_LogDebug(cpu->logger, "(0x%04X) - Set V%X to V%X(%02X) & V%X(%02X).",
                            instruction, register_index_x,
                            register_index_x, cpu->variable_registers[register_index_x],
                            register_index_y, cpu->variable_registers[register_index_y]);
            break;
        }
        // 0x8XY3 - Set VX to VX ^ VY.
        case 0x0003:
        {
            cpu->variable_registers[register_index_x] ^= cpu->variable_registers[register_index_y];
            logger_LogDebug(cpu->logger, "(0x%04X) - Set V%X to V%X(%02X) ^ V%X(%02X).",
                            instruction, register_index_x,
                            register_index_x, cpu->variable_registers[register_index_x],
                            register_index_y, cpu->variable_registers[register_index_y]);
            break;
        }
        // 0x8XY4 - Add VY to VX.
        // Set VF if overflow.
        case 0x0004:
        {
            // We need to use a uint16_t here to be able to check for overflow.
            uint16_t result = cpu->variable_registers[register_index_x] + cpu->variable_registers[register_index_y];
            // Initially, we set overflow to 0.
            cpu->variable_registers[0xF] = 0;
            // If there is overflow, we set it to 1.
            if (result > 0xFF)
                cpu->variable_registers[0xF] = 1;
            // Either way, we will store the first byte of result in VX.
            cpu->variable_registers[register_index_x] = result & 0xFF;

            logger_LogDebug(cpu->logger, "(0x%04X) - Add V%X(%02X) to V%X - VF(%02X).",
                            instruction, register_index_y, cpu->variable_registers[register_index_y],
                            register_index_x, cpu->variable_registers[0xF]);
            break;
        }
        // 0x8XY5 - Subtract VY from VX.
        // Set VF if not underflow.
        case 0x0005:
        {
            // Check underflow.
            uint8_t underflow = cpu->variable_registers[register_index_x] < cpu->variable_registers[register_index_y] ? 1 : 0;
            // Set VX.
            cpu->variable_registers[register_index_x] -= cpu->variable_registers[register_index_y];
            // Set underflow.
            cpu->variable_registers[0xF] = !underflow;

            logger_LogDebug(cpu->logger, "(0x%04X) - Sub V%X(%02X) from V%X - VF(%02X).",
                            instruction, register_index_y, cpu->variable_registers[register_index_y],
                            register_index_x, cpu->variable_registers[0xF]);
            break;
        }
        // 0x8XY6 - Right-shift VX by 1.
        // Stores least significant bit in VF prior to shift.
        case 0x0006:
        {
            // Store least significant bit in VF prior to shift.
            cpu->variable_registers[0xF] = cpu->variable_registers[register_index_x] & 0x01;
            // Right-shift VX by 1.
            cpu->variable_registers[register_index_x] >>= 1;

            logger_LogDebug(cpu->logger, "(0x%04X) -  V%X(%02X) >> 1 - VF(%02X).",
                            instruction, register_index_x, cpu->variable_registers[register_index_x],
                            cpu->variable_registers[0xF]);
            break;
        }
        // 0x8XY7 - Set VX to VY - VX.
        // Set VF if not underflow.
        case 0x0007:
        {
            // Check underflow.
            uint8_t underflow = cpu->variable_registers[register_index_y] < cpu->variable_registers[register_index_x] ? 1 : 0;
            // Set VX.
            cpu->variable_registers[register_index_x] = cpu->variable_registers[register_index_y] - cpu->variable_registers[register_index_x];
            // Set underflow.
            cpu->variable_registers[0xF] = !underflow;

            logger_LogDebug(cpu->logger, "(0x%04X) - Set V%X to V%X(%02X) - V%X(%02X) - VF(%02X).",
                            instruction, register_index_x,
                            register_index_y, cpu->variable_registers[register_index_y],
                            register_index_x, cpu->variable_registers[register_index_x],
                            cpu->variable_registers[0xF]);
            break;
        }
        // 0x8XYE - Left-shift VX by 1.
        // Stores most significant bit in VF prior to shift.
        case 0x000E:
        {
            // Store most significant bit in VF prior to shift.
            cpu->variable_registers[0xF] = cpu->variable_registers[register_index_x] & (0x01 << 7);
            // Left-shift VX by 1.
            cpu->variable_registers[register_index_x] <<= 1;

            logger_LogDebug(cpu->logger, "(0x%04X) -  V%X(%02X) << 1 - VF(%02X).",
                            instruction, register_index_x, cpu->variable_registers[register_index_x],
                            cpu->variable_registers[0xF]);
            break;
        }
        default:
            logger_LogDebug(cpu->logger, "(0x%04X) - (NOT IMPLEMENTED).", instruction);
            break;
        }
    }
    // 0x9XY0 - Skips next instruction if VX != VY.
    case 0x9000:
    {
        // Extract 4-bit register index(X).
        uint8_t register_index_x = (instruction & 0x0F00) >> 8;
        // Extract 4-bit register_index(Y).
        uint8_t register_index_y = (instruction & 0x00F0) >> 4;
        // Skip next instruction if VX == NN.
        if (cpu->variable_registers[register_index_x] != cpu->variable_registers[register_index_y])
        {
            // Note: we only increase by two here because PC is automatically incremented
            //       when we read the next instruction.
            cpu->program_counter += 2;
        }
        logger_LogDebug(cpu->logger, "(0x%04X) - Skip next instruction if (V%X(%02X) != V%X(%02X))(%s).",
                        instruction, register_index_x, cpu->variable_registers[register_index_x],
                        register_index_y, cpu->variable_registers[register_index_y],
                        cpu->variable_registers[register_index_x] != cpu->variable_registers[register_index_y] ? "true" : "false");
        break;
    }
    // 0xANNN - Set index register(I) to NNN.
    case 0xA000:
    {
        // Extract 12-bit immediate address(NNN).
        uint16_t immediate_addr = instruction & 0x0FFF;
        // Set I.
        cpu->index_register = immediate_addr;
        logger_LogDebug(cpu->logger, "(0x%04X) - Set I to 0x%04X.", instruction, immediate_addr);
        break;
    }
    // 0xBNNN - Jump to address V0 + NNN.
    case 0xB000:
    {
        // Extract 12-bit immediate address(NNN).
        uint16_t immediate_addr = instruction & 0x0FFF;
        // Set PC to V0 + NNN.
        cpu->program_counter = cpu->variable_registers[0x0] + immediate_addr;

        logger_LogDebug(cpu->logger, "(0x%04X) - Jump to V0(%02X) + 0x%04X.",
                        instruction, cpu->variable_registers[0x0], immediate_addr);
        break;
    }
    // 0xCXNN - Set VX to bitwise-and between random number and NN.
    case 0xC000:
    {
        // Extract 4-bit register index(X).
        uint8_t register_index = (instruction & 0x0F00) >> 8;
        // Extract 8-bit immediate value(NN).
        uint8_t immediate_value = instruction & 0x00FF;
        // Set VX to bitwise-and between random number and NN.
        uint8_t random_number = (uint8_t)rand();
        cpu->variable_registers[register_index] = random_number & immediate_value;

        logger_LogDebug(cpu->logger, "(0x%04X) - Set V%X to rand(%02X) & %02X.",
                        instruction, register_index,
                        random_number, immediate_value);
        break;
    }
    // 0xDXYN - Draw a sprite at (Vx, Vy) with 8 pixels width and N pixels height.
    // Set Vf if any pixels are turned off(set to 0) when drawing.
    case 0xD000:
    {
        // Extract 8-bit register index(X).
        uint8_t register_index_x = (instruction & 0x0F00) >> 8;
        // Extract 8-bit register index(Y).
        uint8_t register_index_y = (instruction & 0x00F0) >> 4;
        // Extract height N.
        uint8_t height = (instruction & 0x000F);

        // We modulo by width/height so coordinate will wrap if it's past the width/height
        // of the screen.
        // Get X coordinate modulo 64.
        uint8_t x_coord = cpu->variable_registers[register_index_x] % 64;
        // Get Y coordinate module 32.
        uint8_t y_coord = cpu->variable_registers[register_index_y] % 32;
        // Set VF to 0 initially.
        cpu->variable_registers[0xF] = 0;

        // There are N rows of 8 bits in a sprite.
        // The fonts, for example, are all 5 rows tall, which each row containing 8 bits/1 byte.

        // If we turn off any pixels, we set this flag so we can set VF correctly.
        bool turned_off = false;
        // The index register points at the first row in the sprite.
        // We should loop through all N rows without incrementing I, and draw it to the screen.
        // We stop if we reach the bottom of the screen.
        for (uint16_t i = 0; i < height && y_coord < cpu->display.display_buffer_height; i++)
        {
            // Get row.
            uint8_t row = cpu->memory[cpu->index_register + i];
            // Set pixels in display buffer to bits in row.
            if (SetPixels(cpu, x_coord, y_coord, row))
                turned_off = true;
            y_coord++;
        }

        cpu->variable_registers[0xF] = turned_off;

        logger_LogDebug(cpu->logger, "(0x%04X) - Draw sprite at (V%X(0x%02X), V%X(0x%02X)). Width: 8 pixels. Height: %d pixels. VF(0x%02X).",
                        instruction, register_index_x, cpu->variable_registers[register_index_x],
                        register_index_y, cpu->variable_registers[register_index_y],
                        height, cpu->variable_registers[0xF]);
        break;
    }
    // 0xEX__ - Both instructions here have register index X.
    case 0xE000:
    {
        // Extract 4-bit register index(X).
        uint8_t register_index = (instruction & 0x0F00) >> 8;

        switch (instruction & 0x00FF)
        {
        // 0xEX9E - Skips next instruction if key stored in VX is pressed.
        case 0x009E:
        {
            uint16_t key_bit = (0x1 << cpu->variable_registers[register_index]);
            // If key is pressed, increment PC by 2.
            if (KeyPressed(cpu, key_bit))
            {
                cpu->program_counter += 2;
            }
            logger_LogDebug(cpu->logger, "(0x%04X) - Skip next instruction if key V%X(%02X) is pressed.",
                            instruction, register_index, cpu->variable_registers[register_index]);
            break;
        }
        // 0xEXA1 - Skips next instruction if key stored in VX is not pressed.
        case 0x00A1:
        {
            uint16_t key_bit = (0x1 << cpu->variable_registers[register_index]);
            // If key is not pressed, increment PC by 2.
            if (!KeyPressed(cpu, key_bit))
            {
                cpu->program_counter += 2;
            }
            logger_LogDebug(cpu->logger, "(0x%04X) - Skip next instruction if key V%X(%02X) is not pressed.",
                            instruction, register_index, cpu->variable_registers[register_index]);
            break;
        }
        default:
            logger_LogDebug(cpu->logger, "(0x%04X) - (NOT IMPLEMENTED).", instruction);
            break;
        }
    }
    // 0xFX__ - All instructions here have register index X.
    case 0xF000:
    {
        // Extract 4-bit register index(X).
        uint8_t register_index = (instruction & 0x0F00) >> 8;

        switch (instruction & 0x00FF)
        {
        // 0xFX07 - Set VX to value in delay timer.
        case 0x0007:
        {
            cpu->variable_registers[register_index] = cpu->delay_timer;
            logger_LogDebug(cpu->logger, "(0x%04X) - Set V%X to delay timer(%02X).",
                            instruction, register_index, cpu->delay_timer);
            break;
        }
        // 0xFX0A - Wait for keypress and assign it to VX.
        case 0x000A:
        {
            uint8_t key_pressed = WaitKeyPressed(cpu);
            cpu->variable_registers[register_index] = key_pressed;
            logger_LogDebug(cpu->logger, "(0x%04X) - Waited for keypress. Key %02X pressed and stored in V%X.",
                            instruction, key_pressed, register_index);
            break;
        }
        // 0xFX15 - Sets delay timer to VX.
        case 0x0015:
        {
            // Set delay timer.
            cpu->delay_timer = cpu->variable_registers[register_index];
            logger_LogDebug(cpu->logger, "(0x%04X) - Set delay timer to V%X(%02X).",
                            instruction, register_index, cpu->variable_registers[register_index]);
            break;
        }
        // 0xFX18 - Sets sound timer to VX.
        case 0x0018:
        {
            // Set sound timer.
            cpu->sound_timer = cpu->variable_registers[register_index];

            logger_LogDebug(cpu->logger, "(0x%04X) - Set sound timer to V%X(%02X).",
                            instruction, register_index, cpu->variable_registers[register_index]);
            break;
        }
        // 0xFX1E - Add VX to I.
        // VF is not affected.
        case 0x001E:
        {
            // Add VX to I.
            cpu->index_register += cpu->variable_registers[register_index];

            logger_LogDebug(cpu->logger, "(0x%04X) - Add V%X(%02X) to I.",
                            instruction, register_index, cpu->variable_registers[register_index]);
            break;
        }
        // 0xFX29 - Set I to location of sprite indexed by VX.
        case 0x0029:
        {
            uint16_t sprite_addr = cpu->memory[cpu->font_start_address + (5 * cpu->variable_registers[register_index])];
            cpu->index_register = sprite_addr;
            logger_LogDebug(cpu->logger, "(0x%04X) - Set I to address(%04X) of sprite V%X(%02X).",
                            instruction, sprite_addr, register_index, cpu->variable_registers[register_index]);
            break;
        }
        // 0xFX33 - Store the binary-coded decimal of VX with the
        //          100-place at I, 10-place at I+1 and 1-place at I+2.
        case 0x0033:
        {
            // 100-place
            uint8_t binary = cpu->variable_registers[register_index];
            uint8_t modulo = binary % 100;
            uint8_t result = (binary - modulo) / 100;
            cpu->memory[cpu->index_register] = result;
            // 10-place
            binary = modulo;
            modulo = binary % 10;
            result = (binary - modulo) / 10;
            cpu->memory[cpu->index_register + 1] = result;
            // 1-place
            cpu->memory[cpu->index_register + 2] = modulo;
            logger_LogDebug(cpu->logger, "(0x%04X) - Store BCD of V%X(%02X) starting at address I(%04X).",
                            instruction, register_index, cpu->variable_registers[register_index], cpu->index_register);
            break;
        }
        // 0xFX55 - Store V0-VX in memory starting at address I.
        case 0x0055:
        {
            for (uint8_t i = 0; i <= register_index; i++)
            {
                cpu->memory[cpu->index_register + i] = cpu->variable_registers[i];
            }
            logger_LogDebug(cpu->logger, "(0x%04X) - Storing registers V0-V%X in memory starting at address I(%04X).",
                            instruction, register_index, cpu->index_register);
            break;
        }
        // 0xFX65 - Loads V0-VX from memory starting at address I.
        case 0x0065:
        {
            for (uint8_t i = 0; i <= register_index; i++)
            {
                cpu->variable_registers[i] = cpu->memory[cpu->index_register + i];
            }
            logger_LogDebug(cpu->logger, "(0x%04X) - Loading registers V0-V%X from memory starting at address I(%04X).",
                            instruction, register_index, cpu->index_register);
            break;
        }
        default:
            logger_LogDebug(cpu->logger, "(0x%04X) - (NOT IMPLEMENTED).\n", instruction);
            break;
        }
    }
    default:
        logger_LogDebug(cpu->logger, "(0x%04X) - (NOT IMPLEMENTED).\n", instruction);
        break;
    }
}

bool SetPixel(CPUState *cpu, uint8_t x, uint8_t y, uint8_t pixel_value)
{
    if (x >= cpu->display.display_buffer_width || y >= cpu->display.display_buffer_height)
        return false;

    // This is set to true if we turn of any pixels and returned in the end.
    bool turned_off = false;

    // index * 4 because we index as if it's one byte per pixel, but actually it's 4(RGBA).
    size_t actual_index = ((y * cpu->display.display_buffer_width) + x) * cpu->display.display_buffer_channels;
    // Set RGB. A is always set to 0xFF.
    // Since we always set all channels at the same time, we only need to check the first channel
    // to see if we turn off the pixel.
    size_t actual_value = pixel_value == 1 ? 0xFF : 0x00;
    if (cpu->display.display_buffer[actual_index] == actual_value && actual_value == 0xFF)
    {
        actual_value = 0x00;
        turned_off = true;
    }
    cpu->display.display_buffer[actual_index] = actual_value;
    cpu->display.display_buffer[actual_index + 1] = actual_value;
    cpu->display.display_buffer[actual_index + 2] = actual_value;

    return turned_off;
}

bool SetPixels(CPUState *cpu, uint8_t x, uint8_t y, uint8_t pixels)
{
    assert(x < cpu->display.display_buffer_width);
    assert(y < cpu->display.display_buffer_height);

    // This is set to true if we turn of any pixels and returned in the end.
    bool turned_off = false;

    // Loop through each bit in 'pixels'.
    for (uint8_t i = 0; i < 8; i++)
    {
        uint8_t mask = 1 << (7 - i);
        uint8_t pixel_value = (pixels & mask) >> (7 - i);
        // If the pixel is turned off, we must return it.
        if (SetPixel(cpu, x + i, y, pixel_value))
            turned_off = true;
    }

    return turned_off;
}

void SetAlpha(CPUState *cpu, uint8_t x, uint8_t y, uint8_t alpha_value)
{
    assert(x < cpu->display.display_buffer_width);
    assert(y < cpu->display.display_buffer_height);

    // index * 4 because we index as if it's one byte per pixel, but actually it's 4(RGBA).
    size_t actual_index = ((y * cpu->display.display_buffer_width) + x) * cpu->display.display_buffer_channels;
    // Set RGB. A is always set to 0xFF.
    cpu->display.display_buffer[actual_index + 3] = alpha_value;
}

bool KeyPressed(CPUState *cpu, uint16_t key_bit)
{
    return cpu->keys & key_bit;
}

uint8_t WaitKeyPressed(CPUState *cpu)
{
    // Loop while keys == 0(no key pressed).
    uint16_t keys = cpu->keys;
    while (keys == 0)
    {
        keys = cpu->keys;
    }

    return MapBitKey(keys);
}

void PushStack(CPUState *cpu, uint16_t pc)
{
    *(cpu->stack_pointer) = pc;
    cpu->stack_pointer++;
}

uint16_t PopStack(CPUState *cpu)
{
    cpu->stack_pointer--;
    return *(cpu->stack_pointer);
}
