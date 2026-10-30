/*
 * Zarqaa OS 0.1 - Interrupt Descriptor Table
 * Creator: Abdelhak Chabbi
 */

#include "kernel/idt.h"
#include "kernel/pic.h"

struct __attribute__((packed)) IDTEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
};

struct __attribute__((packed)) IDTR {
    uint16_t limit;
    uint64_t base;
};

static struct IDTEntry idt[256];
static struct IDTR idtr;

void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t ist, uint8_t type_attr) {
    idt[vector].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[vector].selector    = 0x08; /* KERNEL_CODE_SEG */
    idt[vector].ist         = ist & 0x07;
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].reserved    = 0;
}

void idt_init(void) {
    /* Set up Exception Handlers */
    idt_set_gate(0, (uint64_t)isr_stub_0, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(1, (uint64_t)isr_stub_1, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(2, (uint64_t)isr_stub_2, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(3, (uint64_t)isr_stub_3, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(4, (uint64_t)isr_stub_4, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(5, (uint64_t)isr_stub_5, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(6, (uint64_t)isr_stub_6, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(7, (uint64_t)isr_stub_7, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(8, (uint64_t)isr_stub_8, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(9, (uint64_t)isr_stub_9, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(10, (uint64_t)isr_stub_10, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(11, (uint64_t)isr_stub_11, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(12, (uint64_t)isr_stub_12, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(13, (uint64_t)isr_stub_13, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(14, (uint64_t)isr_stub_14, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(15, (uint64_t)isr_stub_15, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(16, (uint64_t)isr_stub_16, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(17, (uint64_t)isr_stub_17, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(18, (uint64_t)isr_stub_18, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(19, (uint64_t)isr_stub_19, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(20, (uint64_t)isr_stub_20, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(21, (uint64_t)isr_stub_21, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(22, (uint64_t)isr_stub_22, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(23, (uint64_t)isr_stub_23, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(24, (uint64_t)isr_stub_24, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(25, (uint64_t)isr_stub_25, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(26, (uint64_t)isr_stub_26, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(27, (uint64_t)isr_stub_27, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(28, (uint64_t)isr_stub_28, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(29, (uint64_t)isr_stub_29, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(30, (uint64_t)isr_stub_30, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(31, (uint64_t)isr_stub_31, 0, IDT_INTERRUPT_GATE);

    /* Set up IRQ Handlers */
    idt_set_gate(32, (uint64_t)isr_stub_32, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(33, (uint64_t)isr_stub_33, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(34, (uint64_t)isr_stub_34, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(35, (uint64_t)isr_stub_35, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(36, (uint64_t)isr_stub_36, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(37, (uint64_t)isr_stub_37, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(38, (uint64_t)isr_stub_38, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(39, (uint64_t)isr_stub_39, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(40, (uint64_t)isr_stub_40, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(41, (uint64_t)isr_stub_41, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(42, (uint64_t)isr_stub_42, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(43, (uint64_t)isr_stub_43, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(44, (uint64_t)isr_stub_44, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(45, (uint64_t)isr_stub_45, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(46, (uint64_t)isr_stub_46, 0, IDT_INTERRUPT_GATE);
    idt_set_gate(47, (uint64_t)isr_stub_47, 0, IDT_INTERRUPT_GATE);

    /* System Call Gate */
    idt_set_gate(128, (uint64_t)isr_stub_128, 0, IDT_USER_INTERRUPT);

    /* TLB Shootdown Interrupt */
    idt_set_gate(253, (uint64_t)isr_stub_253, 0, IDT_INTERRUPT_GATE);

    /* Spurious Interrupt Gate */
    idt_set_gate(255, (uint64_t)isr_stub_255, 0, IDT_INTERRUPT_GATE);

    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt;

    __asm__ volatile("lidt %0" : : "m"(idtr));
}

void idt_reload(void) {
    __asm__ volatile("lidt %0" : : "m"(idtr));
}

#include "kernel/irq.h"
#include "kernel/setjmp.h"
#include "kernel/early_boot_log.h"
#include "zarqaa/string.h"

extern uint64_t scheduler_switch_task(uint64_t current_rsp);

extern volatile bool g_driver_loading;
extern const char *g_current_driver_name;
extern jmp_buf driver_recovery_buf;
extern void longjmp(jmp_buf env, int val);

uint64_t isr_handler(struct interrupt_frame *frame) {
    uint64_t rsp = (uint64_t)frame;
    if (frame->int_no < 32) {
        if (g_driver_loading) {
            char fail_msg[128];
            sprintf(fail_msg, "Driver '%s' crashed (Exception %d)", g_current_driver_name ? g_current_driver_name : "Unknown", frame->int_no);
            early_blog(BLOG_FAIL, "EXCEPT", fail_msg);
            longjmp(driver_recovery_buf, 1);
        }
        uint64_t cr2 = 0;
        if (frame->int_no == 14) {
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        }
        if ((frame->cs & 3) == 3) {
            extern const char *scheduler_get_current_name(void);
            extern void task_exit(void);
            kprintf("[USER-CRASH] Task '%s' crashed (Exception %d, RIP: %p, CR2: %p, ERR: 0x%x). Terminating task...\n",
                    scheduler_get_current_name(), frame->int_no, (void*)frame->rip, (void*)cr2, frame->err_code);
            task_exit();
            return rsp;
        }
        kprintf("EXCEPTION: %d, ERR CODE: %x, RIP: %p, CR2: %p\n", frame->int_no, frame->err_code, (void*)frame->rip, (void*)cr2);
        panic("CPU Exception");
    } else if (frame->int_no >= 32 && frame->int_no < 48) {
        uint8_t irq = frame->int_no - 32;
        
        irq_handler_t handler = get_irq_handler(irq);
        if (handler) {
            uint64_t new_rsp = handler(frame);
            if (new_rsp != 0) {
                rsp = new_rsp;
            }
        } else {
            kprintf("Unhandled IRQ: %d\n", irq);
        }

        extern void lapic_eoi(void);
        lapic_eoi();
    } else if (frame->int_no == 128) {
        extern void syscall_dispatcher(struct interrupt_frame *frame);
        syscall_dispatcher(frame);
    } else if (frame->int_no == 253) {
        /* TLB Shootdown IPI */
        uint64_t current_cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
        __asm__ volatile("mov %0, %%cr3" : : "r"(current_cr3));
        extern void lapic_eoi(void);
        lapic_eoi();
    } else if (frame->int_no == 255) {
        /* Spurious interrupt: Intel SDM specifies that no EOI should be generated */
    }
    
    return rsp;
}
