#include "mini_uart.h"

void kernel_main(void)
{
    uart_init(115200);
    uart_send_string("Hello, Paspberry 3B+\r\n");

    while (1) {
        uart_send(uart_recv());
    }
}