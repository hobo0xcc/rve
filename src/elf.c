#include "elf.h"
#include "rve.h"

uint64_t LoadElf(State *state, size_t size, uint8_t *bin) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)bin;

    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
        Error("Not an ELF file");
    } else if (ehdr->e_type != ET_EXEC) {
        Error("Not an executable ELF file");
    }

    uint64_t sh_size = sizeof(Elf64_Shdr);
    Elf64_Shdr *shstrtab =
        (Elf64_Shdr *)(bin + ehdr->e_shoff + (sh_size * ehdr->e_shstrndx));

    char *strtab = (char *)(bin + shstrtab->sh_offset);

    uint64_t text_addr;
    for (uint64_t i = 0; i < sh_size * ehdr->e_shnum; i += sh_size) {
        Elf64_Shdr *shdr = (Elf64_Shdr *)(bin + ehdr->e_shoff + i);
        if (shdr->sh_type == SHT_PROGBITS) {
            uint64_t virtual_addr = shdr->sh_addr;
            if (virtual_addr == 0)
                continue;

            uint8_t *text = (uint8_t *)(bin + shdr->sh_offset);
            size_t text_size = shdr->sh_size;
            char *name = (strtab + shdr->sh_name);
            printf("%s: 0x%llx\n", name, virtual_addr);
            if (!strcmp(name, ".text"))
                text_addr = virtual_addr;

            LoadBinaryIntoMemory(state, text, text_size, virtual_addr - DRAM_BASE);
        }
    }

    return ehdr->e_entry;
}
