#ifndef ZARQAA_ACPI_H
#define ZARQAA_ACPI_H

#include <stdint.h>
#include <stdbool.h>

#pragma pack(push, 1)

// Standard ACPI Table Header
struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};

// ACPI FADT (Fixed ACPI Description Table)
struct acpi_fadt {
    struct acpi_sdt_header h;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t  reserved;
    uint8_t  preferred_pm_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4bios_req;
    uint8_t  pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t  pm1_event_len;
    uint8_t  pm1_ctrl_len;
    uint8_t  pm2_ctrl_len;
    uint8_t  pm_timer_len;
    uint8_t  gpe0_len;
    uint8_t  gpe1_len;
    uint8_t  gpe1_base;
    uint8_t  cstate_ctrl;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alrm;
    uint8_t  mon_alrm;
    uint8_t  century;
    uint16_t boot_arch_flags;
    uint8_t  reserved2;
    uint32_t flags;
    uint8_t  reset_reg[12];
    uint8_t  reset_value;
    uint8_t  reserved3[3];
    uint64_t x_firmware_ctrl;
    uint64_t x_dsdt;
};

#pragma pack(pop)

void acpi_init(void);
void acpi_power_off(void);

#endif // ZARQAA_ACPI_H
