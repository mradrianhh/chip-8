#ifndef CORE_CONVERT_H
#define CORE_CONVERT_H

#include <elf.h>

void loader_ConvertElf32EhdrBeLe(Elf32_Ehdr *ehdr);
void loader_ConvertElf32ShdrBeLe(Elf32_Shdr *shdr);
void loader_ConvertElf32PhdrBeLe(Elf32_Phdr *phdr);

uint16_t loader_Convert16BitBeLe(uint16_t bytes);

#endif
