#include <byteswap.h>

#include "loader/convert.h"

void loader_ConvertElf32EhdrBeLe(Elf32_Ehdr *ehdr)
{
    ehdr->e_type = __bswap_16(ehdr->e_type);
    ehdr->e_machine = __bswap_16(ehdr->e_machine);
    ehdr->e_version = __bswap_32(ehdr->e_version);
    ehdr->e_entry = __bswap_32(ehdr->e_entry);
    ehdr->e_phoff = __bswap_32(ehdr->e_phoff);
    ehdr->e_shoff = __bswap_32(ehdr->e_shoff);
    ehdr->e_flags = __bswap_32(ehdr->e_flags);
    ehdr->e_ehsize = __bswap_16(ehdr->e_ehsize);
    ehdr->e_phentsize = __bswap_16(ehdr->e_phentsize);
    ehdr->e_phnum = __bswap_16(ehdr->e_phnum);
    ehdr->e_shentsize = __bswap_16(ehdr->e_shentsize);
    ehdr->e_shnum = __bswap_16(ehdr->e_shnum);
    ehdr->e_shstrndx = __bswap_16(ehdr->e_shstrndx);
}

void loader_ConvertElf32ShdrBeLe(Elf32_Shdr *shdr)
{
    shdr->sh_name = __bswap_32(shdr->sh_name);
    shdr->sh_type = __bswap_32(shdr->sh_type);
    shdr->sh_flags = __bswap_32(shdr->sh_flags);
    shdr->sh_addr = __bswap_32(shdr->sh_addr);
    shdr->sh_offset = __bswap_32(shdr->sh_offset);
    shdr->sh_size = __bswap_32(shdr->sh_size);
    shdr->sh_link = __bswap_32(shdr->sh_link);
    shdr->sh_info = __bswap_32(shdr->sh_info);
    shdr->sh_addralign = __bswap_32(shdr->sh_addralign);
    shdr->sh_entsize = __bswap_32(shdr->sh_entsize);
}

void loader_ConvertElf32PhdrBeLe(Elf32_Phdr *phdr)
{
    phdr->p_type = __bswap_32(phdr->p_type);
    phdr->p_offset = __bswap_32(phdr->p_offset);
    phdr->p_vaddr = __bswap_32(phdr->p_vaddr);
    phdr->p_paddr = __bswap_32(phdr->p_paddr);
    phdr->p_filesz = __bswap_32(phdr->p_filesz);
    phdr->p_memsz = __bswap_32(phdr->p_memsz);
    phdr->p_flags = __bswap_32(phdr->p_flags);
    phdr->p_align = __bswap_32(phdr->p_align);
}

uint16_t loader_Convert16BitBeLe(uint16_t bytes)
{
    return __bswap_16(bytes);
}
