/*
 * Zarqaa OS - ELF64 Definitions and Loader
 */

#ifndef KERNEL_ELF_H
#define KERNEL_ELF_H

#include <stdint.h>

#define ELF_MAGIC 0x464C457FU // '\x7F', 'E', 'L', 'F' in little endian

typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;

#define EI_NIDENT 16

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half    e_type;
    Elf64_Half    e_machine;
    Elf64_Word    e_version;
    Elf64_Addr    e_entry;
    Elf64_Off     e_phoff;
    Elf64_Off     e_shoff;
    Elf64_Word    e_flags;
    Elf64_Half    e_ehsize;
    Elf64_Half    e_phentsize;
    Elf64_Half    e_phnum;
    Elf64_Half    e_shentsize;
    Elf64_Half    e_shnum;
    Elf64_Half    e_shstrndx;
} Elf64_Ehdr;

#define PT_LOAD    1

typedef struct {
    Elf64_Word    p_type;
    Elf64_Word    p_flags;
    Elf64_Off     p_offset;
    Elf64_Addr    p_vaddr;
    Elf64_Addr    p_paddr;
    Elf64_Xword   p_filesz;
    Elf64_Xword   p_memsz;
    Elf64_Xword   p_align;
} Elf64_Phdr;

/* 
 * elf_load: Loads an ELF file from the VFS.
 * Returns 0 on success, -1 on failure.
 * `out_entry_point` receives the virtual address to start executing.
 * `cr3` is the PML4 physical address for the new process's address space.
 */
int elf_load(const char *path, uint64_t *out_entry_point, uint64_t cr3);

#endif /* KERNEL_ELF_H */
