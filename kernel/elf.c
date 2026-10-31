#include "kernel/elf.h"
#include "filesystem/vfs.h"
#include "kernel/vmm.h"
#include "kernel/pmm.h"
#include "zarqaa/string.h"

extern uint64_t hhdm_offset;
extern void kprintf(const char *fmt, ...);

int elf_load(const char *path, uint64_t *out_entry_point, uint64_t cr3) {
    if (!path) return -1;
    
    struct vfs_node *node = vfs_open(path);
    if (!node) {
        kprintf("ELF: Could not open %s\n", path);
        return -1;
    }

    Elf64_Ehdr ehdr;
    if (vfs_read(node, &ehdr, sizeof(Elf64_Ehdr), 0) != sizeof(Elf64_Ehdr)) {
        kprintf("ELF: Failed to read ELF header\n");
        return -1;
    }

    if (*(uint32_t *)ehdr.e_ident != ELF_MAGIC) {
        kprintf("ELF: Invalid magic number: 0x%x\n", *(uint32_t *)ehdr.e_ident);
        return -1;
    }

    if (ehdr.e_machine != 0x3E) { // EM_X86_64
        kprintf("ELF: Not an x86_64 executable (machine=0x%x)\n", ehdr.e_machine);
        return -1;
    }

    uint64_t *pml4 = (uint64_t *)(cr3 + hhdm_offset);

    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr phdr;
        if (vfs_read(node, &phdr, sizeof(Elf64_Phdr), ehdr.e_phoff + (i * ehdr.e_phentsize)) != sizeof(Elf64_Phdr)) {
            kprintf("ELF: Failed to read program header %d\n", i);
            return -1;
        }

        if (phdr.p_type == PT_LOAD) {
            uint64_t mem_size = phdr.p_memsz;
            uint64_t file_size = phdr.p_filesz;
            uint64_t vaddr = phdr.p_vaddr;
            uint64_t offset = phdr.p_offset;

            if (mem_size == 0) continue;

            uint64_t page_start = vaddr & ~0xFFFULL;
            uint64_t page_end = (vaddr + mem_size + 0xFFFULL) & ~0xFFFULL;

            /* Map the required pages in the new PML4 and copy data via HHDM */
            for (uint64_t virt = page_start; virt < page_end; virt += 4096) {
                uint64_t phys = pmm_alloc();
                if (!phys) {
                    kprintf("ELF: OOM during PT_LOAD\n");
                    return -1; 
                }
                vmm_map_page(pml4, virt, phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);

                void *kpage = (void *)(phys + hhdm_offset);
                memset(kpage, 0, 4096);

                if (file_size > 0) {
                    uint64_t seg_file_start = vaddr;
                    uint64_t seg_file_end = vaddr + file_size;
                    uint64_t page_vstart = virt;
                    uint64_t page_vend = virt + 4096;

                    uint64_t copy_start = (page_vstart > seg_file_start) ? page_vstart : seg_file_start;
                    uint64_t copy_end = (page_vend < seg_file_end) ? page_vend : seg_file_end;

                    if (copy_end > copy_start) {
                        uint64_t copy_len = copy_end - copy_start;
                        uint64_t dest_offset_in_page = copy_start - page_vstart;
                        uint64_t src_offset_in_file = offset + (copy_start - seg_file_start);

                        int rd = vfs_read(node, (uint8_t *)kpage + dest_offset_in_page, copy_len, src_offset_in_file);
                        if (rd != (int)copy_len) {
                            kprintf("ELF: Failed to read PT_LOAD slice (got %d, expected %lu)\n", rd, copy_len);
                            return -1;
                        }
                    }
                }
            }
        }
    }

    if (out_entry_point) *out_entry_point = ehdr.e_entry;

    return 0;
}
