#include "mini_uart.h"
#include "printf.h"
#include "utils.h"

void kernel_main(void)
{
    uart_init();
    init_printf(0, putc);

    printf("Hello, Paspberry 3B+\r\n");

    int el = get_el();
    printf("Exception level: %d\r\n", el);

    while (1) {
        uart_send(uart_recv());
    }
}