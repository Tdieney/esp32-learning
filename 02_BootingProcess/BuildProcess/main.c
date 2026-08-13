#include <stdio.h>
#include "math.h"

/* Preprocessor Macro */
#define APP_TITLE "GCC 4-Stage Compilation Pipeline Demo"

/* Global Initialized Variable (stored in .data section) */
int global_counter = 42;

/* Global Uninitialized Variable (stored in .bss section) */
int global_buffer[16];

int main(void) {
    printf("=========================================\n");
    printf("  %s\n", APP_TITLE);
    printf("=========================================\n\n");

    int a = 12;
    int b = 5;

    int sum = add(a, b);
    int prod = multiply(a, b);
    int sq = SQUARE(a);

    printf("Inputs: a = %d, b = %d\n", a, b);
    printf("Result of add(a, b)      : %d\n", sum);
    printf("Result of multiply(a, b) : %d\n", prod);
    printf("Result of SQUARE(a)      : %d\n", sq);
    printf("Global Counter           : %d\n", global_counter);

    return 0;
}
