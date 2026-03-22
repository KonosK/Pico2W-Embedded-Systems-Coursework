#include <stdio.h>
#include "pico/stdlib.h"
#include <tusb.h>

#define L 50


unsigned char line[L+1]; //must be global so it's not automatcally deallocated when the function returns

unsigned char *readLineAndEcho(void){
    
    unsigned char u, *p = line;
    for (u=getchar(); u != '\r' && p-line < L; u=getchar()){
        putchar(*p++ = u);
    }

    *p = 0; //null terminate the string

    return line;
}

int main(){
    stdio_init_all();
    while(!tud_cdc_connected()){
        sleep_ms(100);
    }
    printf("TTY serial terminal connected\n");

    printf("What is your name?\n");
    printf("\nHello %s !\n", readLineAndEcho());

    return 0;
}