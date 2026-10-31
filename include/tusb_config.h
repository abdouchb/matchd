#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------
// Common Configuration
//--------------------------------------------------------------------

#define CFG_TUSB_MCU          OPT_MCU_NONE
#define CFG_TUSB_OS           OPT_OS_NONE
#define CFG_TUSB_DEBUG        1

#include <stddef.h>
#include <stdio.h>
extern void kprintf(const char *format, ...);

//--------------------------------------------------------------------
// Host Configuration
//--------------------------------------------------------------------

// Enable Host stack
#define CFG_TUH_ENABLED       1
#define CFG_TUH_MAX_SPEED     OPT_MODE_DEFAULT_SPEED
#define CFG_TUH_DEVICE_MAX    4

// Number of hub devices
#define CFG_TUH_HUB           4

#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_HOST | OPT_MODE_HIGH_SPEED)

// Enable RNDIS, CDC, MSC, and HID Host
#define CFG_TUH_RNDIS         1
#define CFG_TUH_CDC           1
#define CFG_TUH_MSC           1
#define CFG_TUH_HID           4

//--------------------------------------------------------------------
// Memory Configuration
//--------------------------------------------------------------------

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN    __attribute__ ((aligned(64)))

#ifdef __cplusplus
 }
#endif

#endif // TUSB_CONFIG_H_
