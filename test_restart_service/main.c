#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Segmentation fault after argv[1] time
int main(int argc, char **argv)
{
    int time_to_sleep = atoi(argv[1]);
    sleep(time_to_sleep);

    int *p = NULL;
    printf("%d\n", *p);
}