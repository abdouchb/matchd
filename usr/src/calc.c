#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "zarqaa.h"

void _start() {
    printf("\n--- ZarqaaOS Math Benchmark & Calculator ---\n");
    
    printf("Computing Fibonacci numbers...\n");
    int fib[10];
    fib[0] = 0;
    fib[1] = 1;
    for (int i = 2; i < 10; i++) {
        fib[i] = fib[i-1] + fib[i-2];
    }
    
    printf("Fibonacci sequence: ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", fib[i]);
    }
    printf("\n");

    printf("Allocating 1000 integers on heap...\n");
    int *big_array = (int*)malloc(1000 * sizeof(int));
    if (big_array) {
        int sum = 0;
        for (int i = 0; i < 1000; i++) {
            big_array[i] = i + 1;
            sum += big_array[i];
        }
        printf("Sum of 1 to 1000 = %d (Expected: 500500)\n", sum);
        free(big_array);
        printf("Memory successfully freed!\n");
    } else {
        printf("Heap allocation failed!\n");
    }

    printf("--- Benchmark Complete ---\n\n");
    exit();
}
