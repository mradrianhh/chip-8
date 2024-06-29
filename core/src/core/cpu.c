#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "core/cpu.h"
#include "core/memory.h"
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

CPUState *core_InitializeCPU()
{
    CPUState *cpu = calloc(1, sizeof(CPUState));
    cpu->memory_size = CH8_MEM_SIZE;
    cpu->stack_pointer = cpu->stack;

    cpu->delay_timer = 0;
    cpu->sound_timer = 0;

    cpu->index_register = 0;
    cpu->program_counter = CH8_PROGRAM_START_ADDRESS;

    cpu->logger = logger_Initialize(LOGS_BASE_PATH "cpu.log");
    logger_SetLogLevel(cpu->logger, LOG_LEVEL_FULL);

    // Load font into memory at 0x0050-0x009F.
    logger_LogInfo(cpu->logger, "Loading font starting at address 0x50.");
    uint16_t end_address = core_LoadBinary16Data(cpu->memory, 0x50, cpu->memory_size, font_data, CH8_FONT_SIZE);
    // Check alignment is correct. Should be 2-byte alignment.
    if (end_address % 2 != 0)
    {
        logger_LogError(cpu->logger, "Font data does not have correct alignment. Alignment should be 2 bytes.");
        raise(SIGABRT);
    }

    return cpu;
}

void core_DestroyCPU(CPUState *cpu)
{
    logger_Destroy(cpu->logger);
    free(cpu);
}

void core_CycleCPU(CPUState *cpu)
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
        case 0x00E0:
        {
            // 0x00E0 - Clear screen.
            logger_LogDebug(cpu->logger, "(0x%04X) - Clear screen(NOT IMPLEMENTED).\n", instruction);
            break;
        }
        case 0x00EE:
        {
            // 0x00EE - Return.
            logger_LogDebug(cpu->logger, "(0x%04X) - Return(NOT IMPLEMENTED).\n", instruction);
            break;
        }
        // 0x0NNN - Ignored as we're not running on a machine with actual chip-8 support.
        default:
            logger_LogDebug(cpu->logger, "(0x%04X) - Call machine code routine(NOT IMPLEMENTED).\n", instruction);
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
        logger_LogDebug(cpu->logger, "(0x%04X) - Set program counter to 0x%04X.\n", instruction, immediate_addr);
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
        logger_LogDebug(cpu->logger, "(0x%04X) - Set V%X to 0x%02X.\n", instruction, register_index, immediate_value);
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
        logger_LogDebug(cpu->logger, "(0x%04X) - Add 0x%02X to V%X.\n", instruction, register_index, immediate_value);
        break;
    }
    // 0xANNN - Set index register(I) to NNN.
    case 0xA000:
    {
        // Extract 12-bit immediate address(NNN).
        uint16_t immediate_addr = instruction & 0x0FFF;
        // Set I.
        cpu->index_register = immediate_addr;
        logger_LogDebug(cpu->logger, "(0x%04X) - Set I to 0x%04X.\n", instruction, immediate_addr);
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
        logger_LogDebug(cpu->logger, "(0x%04X) - Draw sprite at (V%X, V%X). Width: 8 pixels. Height: %d pixels(NOT IMPLEMENTED).\n",
                        instruction, register_index_x, register_index_y, height);
        break;
    }
    default:
        logger_LogDebug(cpu->logger, "(0x%04X) - (NOT IMPLEMENTED).\n");
        break;
    }
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
