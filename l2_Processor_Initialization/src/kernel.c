#include "mini_uart.h"
#include "printf.h"
#include "utils.h"

#include "ff.h"
#include "systimer.h"
#include "power.h"

void kernel_main(void)
{
    uart_init();
    init_printf(0, putc);

    printf("Hello, raspberry 3B+\r\n");

    /* ******** */
    // fs test
    FATFS fs;
    FATFS* pfs = &fs;
    FRESULT res;

    res = f_mount(pfs, "0", 0);
    if (res != FR_OK)
    {
        printf("f_mount failed\r\n");
        return;
    }

    uint32_t nclst = 0;
    res = f_getfree("0", &nclst, &pfs);
    if (res != FR_OK)
    {
        printf("f_getfree failed\r\n");
        return;
    }
    
    systimer_sleep(4);
    for (int i = 0; i < 24; i++) {
        char c;
        systimer_sleep(1);

        if(uart_dataready()) {
            uint32_t size = 0;
            for(int i=0; i<4; ++i)
            {
                c = uart_getc();
                uart_send(c);
                size = size << 8;
                size = size + c;
            }


            char* data = (char *)0x90000;
            char* bp = data;
            for(int s=0; s<size; ++s) {
                *bp = uart_getc();
                uart_send(*bp);
                bp += 1;
            }
            systimer_sleep(1);
            uart_send('#');

            char finish;
            finish = uart_recv();

            FIL fdst;
            FRESULT res = f_open(&fdst, "0:/kernel8.img", FA_CREATE_ALWAYS | FA_WRITE);
            if (res != FR_OK)
            {
                // printf("f_open failed\r\n");
                systimer_sleep(4);
                reset();
                return;
            }

            
            uint32_t sizewrite = 0;
            res = f_write(&fdst, (void *)data, size, (unsigned int*)&sizewrite);
            f_close(&fdst);
        }
    }

    /* ******** */

    int el = get_el();
    printf("Exception level: %d\r\n", el);

    while (1) {
        uart_send(uart_recv());
    }
}
