#include "zarqaa.h"
#include "stdio.h"
#include "stdlib.h"

void _start() {
    printf("Hello from User Space ZarqaaOS with libc!\n");
    printf("Testing dynamic memory allocation...\n");
    
    int* array = (int*)malloc(5 * sizeof(int));
    if (array) {
        for (int i = 0; i < 5; i++) {
            array[i] = i * 10;
        }
        
        printf("Array values: ");
        for (int i = 0; i < 5; i++) {
            printf("%d ", array[i]);
        }
        printf("\nMemory test successful!\n");
    } else {
        printf("Memory allocation failed!\n");
    }
    
    // Exit
    exit();
}
