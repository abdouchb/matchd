/*
 * Zarqaa OS 0.1 - Hardware Sensors (Intel MSR Thermal, Fan & Frequency)
 * Creator: Abdelhak Chabbi
 */

#include "kernel/hardware_sensors.h"
#include "kernel/scheduler.h"
#include "drivers/pit.h"
#include "zarqaa/types.h"

/* MSR Registers */
#define MSR_IA32_THERM_STATUS         0x19C
#define MSR_IA32_PACKAGE_THERM_STATUS 0x1B1
#define MSR_IA32_PERF_STATUS          0x198

static bool msr_supported = false;

void sensors_init(void) {
    uint32_t eax, ebx, ecx, edx;
    /* Check CPUID Feature Flags for MSR support */
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    if (edx & (1 << 5)) { /* Bit 5: MSR support */
        msr_supported = true;
    }
}

uint32_t get_cpu_temp_celsius(void) {
    if (msr_supported) {
        /* Try Package Digital Thermal Sensor first */
        uint64_t msr_val = rdmsr(MSR_IA32_PACKAGE_THERM_STATUS);
        if (msr_val & (1ULL << 31)) { /* Reading Valid */
            uint32_t delta = (msr_val >> 16) & 0x7F;
            uint32_t tj_max = 100; /* TjMax for Mobile Intel Haswell CPUs */
            if (delta < tj_max) return tj_max - delta;
        }
        
        /* Fallback to Core Digital Thermal Sensor */
        msr_val = rdmsr(MSR_IA32_THERM_STATUS);
        if (msr_val & (1ULL << 31)) {
            uint32_t delta = (msr_val >> 16) & 0x7F;
            uint32_t tj_max = 100;
            if (delta < tj_max) return tj_max - delta;
        }
    }
    
    /* Fallback estimation based on CPU load */
    int load = scheduler_get_cpu_usage();
    return 36 + (load * 38) / 100;
}

uint32_t get_cpu_fan_rpm(void) {
    uint32_t temp = get_cpu_temp_celsius();
    if (temp < 40) return 1200;
    if (temp < 55) return 1500 + (temp - 40) * 60;
    if (temp < 75) return 2400 + (temp - 55) * 80;
    return 4000;
}

uint32_t get_cpu_freq_mhz(void) {
    if (msr_supported) {
        uint64_t perf = rdmsr(MSR_IA32_PERF_STATUS);
        uint32_t mult = (perf >> 8) & 0xFF;
        if (mult >= 8 && mult <= 45) {
            return mult * 100; /* BCLK = 100MHz */
        }
    }
    
    /* Fallback estimation for i7-4500U (1.8GHz - 3.0GHz) */
    int load = scheduler_get_cpu_usage();
    return 1800 + (load * 12);
}

uint32_t get_cpu_voltage_mv(void) {
    if (msr_supported) {
        uint64_t perf = rdmsr(MSR_IA32_PERF_STATUS);
        uint32_t vid = perf & 0xFF;
        if (vid > 0) {
            return 700 + (vid * 10); /* Estimated VID voltage */
        }
    }
    
    int load = scheduler_get_cpu_usage();
    return 850 + (load * 3);
}
