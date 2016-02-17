#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int global_int = 0;

int main() {
    int i;
    int stack_int;

    int * malloc_int;

    stack_int  = 0;
    malloc_int = malloc(sizeof(int));

    *malloc_int = 0;

    {
        char * pmi_rank = getenv("PMI_RANK");
        if (pmi_rank != NULL) {
            int pmi = atoi(pmi_rank);

            global_int  = (pmi * 10);
            stack_int   = 1 + (pmi * 10);
            *malloc_int = 2 + (pmi * 10);
        }

        printf("HELLO WORLD from PMI_RANK: %s\n", pmi_rank);
    }


    printf("mem mappings:\n"
        "global_int: %p\n"
        "stack_int : %p\n"
        "malloc_int: %p\n",
        &global_int,
        &stack_int,
        malloc_int);

    for (i = 0; i < 10; i++) {
        printf("%d\n", 10 - i);
        sleep(1);
    }

    free(malloc_int);

    return 0;
} 
