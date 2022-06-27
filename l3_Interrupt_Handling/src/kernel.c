#include "mini_uart.h"
#include "printf.h"
#include "utils.h"
#include "irq.h"
#include "timer.h"

void kernel_main(void)
{
    uart_init();
    init_printf(0, putc);

    printf("Hello, Paspberry 3B+\r\n");

    irq_vector_init();
    timer_init();
    enable_interrupt_controller();
    enable_irq();

    while (1) {
        uart_send(uart_recv());
    }
}