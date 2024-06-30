#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <elf.h>

#include "loader/loader.h"
#include "loader/convert.h"

static Logger *logger;

void core_InitializeLoader(LogLevel log_level)
{
    logger = logger_Initialize(LOGS_BASE_PATH "loader.log");
    logger_SetLogLevel(logger, log_level);
}

void core_DestroyLoader()
{
    logger_Destroy(logger);
}

uint32_t core_LoadElf32File(const char *filename, uint8_t *region, size_t region_size)
{
    FILE *fp;
    Elf32_Ehdr eh;
    Elf32_Phdr ph[16];

    if ((fp = fopen(filename, "rb")) == NULL)
    {
        logger_LogError(logger, "Can't open ELF-file %s.", filename);
        raise(SIGABRT);
    }

    fread(&eh, sizeof(eh), 1, fp);

    if (eh.e_ident[EI_DATA] == ELFDATA2MSB)
    {
        loader_ConvertElf32EhdrBeLe(&eh);
    }

    // Check header validity.
    if (eh.e_ident[EI_MAG0] != ELFMAG0 ||
        eh.e_ident[EI_MAG1] != ELFMAG1 ||
        eh.e_ident[EI_MAG2] != ELFMAG2 ||
        eh.e_ident[EI_MAG3] != ELFMAG3 ||
        eh.e_ident[EI_CLASS] != ELFCLASS32 ||
        eh.e_ident[EI_VERSION] != EV_CURRENT ||
        eh.e_machine != EM_MIPS)
    {
        logger_LogError(logger, "Invalid ELF header.");
        raise(SIGABRT);
    }

    // Check program headers size
    if (eh.e_phoff == 0 || eh.e_phnum == 0 || eh.e_phnum > 16 || eh.e_phentsize != sizeof(Elf32_Phdr))
    {
        logger_LogError(logger, "Invalid program headers.");
        raise(SIGABRT);
    }

    // Read program header
    fseek(fp, eh.e_phoff, SEEK_SET);
    fread(ph, eh.e_phentsize, eh.e_phnum, fp);

    // Load each segment
    for (int i = 0; i < eh.e_phnum; i++)
    {
        if (eh.e_ident[EI_DATA] == ELFDATA2MSB)
        {
            loader_ConvertElf32PhdrBeLe(&ph[i]);
        }
        if (ph[i].p_type == PT_LOAD)
        {
            if (ph[i].p_filesz)
            {
                // Check if the filesize plus the offset into the memory region exceeds the region size.
                if (ph[i].p_vaddr + ph[i].p_filesz > region_size)
                {
                    logger_LogError(logger, "Data loaded from program header %d in file %s exceeds range of memory region.",
                                    i, filename);
                    raise(SIGABRT);
                }
                fseek(fp, ph[i].p_offset, SEEK_SET);
                fread(&region[ph[i].p_vaddr], ph[i].p_filesz, 1, fp);
            }
            if (ph[i].p_filesz < ph[i].p_memsz)
            {
                // Check if the memory range we attempt to set is within the bounds of our region_size.
                if ((ph[i].p_vaddr + ph[i].p_filesz) + (ph[i].p_memsz - ph[i].p_filesz) > region_size)
                {
                    logger_LogError(logger, "Attempting to set data from program header %d in file %s that outside the range of our memory region.",
                                    i, filename);
                    raise(SIGABRT);
                }
                memset(&region[ph[i].p_vaddr + ph[i].p_filesz], 0, ph[i].p_memsz - ph[i].p_filesz);
            }
        }
    }

    fclose(fp);

    return eh.e_entry;
}

uint16_t core_LoadBinary16File(const char *filename, uint8_t *region, uint16_t offset, size_t region_size)
{
    FILE *fp;

    if ((fp = fopen(filename, "rb")) == NULL)
    {
        logger_LogError(logger, "Can't open binary file %s.", filename);
        raise(SIGABRT);
    }

    // find size of file.
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Check if size of file plus the offset exceeds the size of the region.
    if (size + offset > region_size)
    {
        logger_LogError(logger, "Attempting to load data from file %s starting at offset %04x will exceed the memory size.",
                        filename, offset);
        raise(SIGABRT);
    }

    uint8_t file_buffer[size];
    fread(&region[offset], size, 1, fp);

    fclose(fp);

    return offset + (uint16_t)size;
}

uint16_t core_LoadBinary16Data(uint8_t *region, uint16_t offset, size_t region_size, uint8_t *data, size_t data_size)
{
    if (offset + data_size > region_size)
    {
        logger_LogError(logger, "Attempting to load data which exceeds size of destination region. Offset: %04x. Region size: %zu. Data size: %zu.",
                        offset, region_size, data_size);
        raise(SIGABRT);
    }

    memcpy(&region[offset], data, data_size);
    logger_LogInfo(logger, "Loaded 16-bit binary data of size %zx starting at offset %04X.", data_size, offset);

    return offset + (uint16_t)data_size;
}
