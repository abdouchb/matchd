#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "zarqaa.h"

void _start() {
    printf("\n========================================\n");
    printf("   Welcome to ZarqaaOS User-Space Shell!\n");
    printf("========================================\n");
    printf("Type 'help' for available commands.\n\n");

    char buffer[128];
    while (1) {
        printf("ZarqaaOS> ");
        gets(buffer, sizeof(buffer));

        if (strlen(buffer) == 0) {
            continue;
        }

        if (strcmp(buffer, "help") == 0) {
            printf("Available built-in commands:\n");
            printf("  help     - Show this help message\n");
            printf("  clear    - Clear screen\n");
            printf("  echo     - Echo text back\n");
            printf("  exit     - Exit shell\n");
            printf("Or type an executable name to run it (e.g. /hd0/HELLO.ELF)\n");
        } else if (strcmp(buffer, "clear") == 0) {
            printf("\033[2J\033[H");
        } else if (strncmp(buffer, "echo ", 5) == 0) {
            printf("%s\n", buffer + 5);
        } else if (strcmp(buffer, "exit") == 0) {
            printf("Exiting shell...\n");
            break;
        } else {
            // Try to execute it
            printf("Executing: %s\n", buffer);
            int ret = (int)syscall2(SYS_EXEC, (uint64_t)buffer, (uint64_t)"user_app");
            if (ret < 0) {
                printf("Unknown command or file not found: %s\n", buffer);
            }
        }
    }

    exit();
}
