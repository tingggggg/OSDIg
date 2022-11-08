#include "pl011_uart.h"
#include "utils.h"

static unsigned int semaphore = 0;

void kernel_main(unsigned int core_id)
{
    const unsigned int core_id_from_func = proc_id();
    
    // Do the UART initialization only for CORE #0
    if (core_id_from_func == 0) {
        uart_init();
    }

    // Wait for previous CORE to finish displaying
    while (core_id_from_func != semaphore) {}

    uart_send_string("Hello, from processor core_id ");
    uart_send(core_id + '0');
    uart_send_string("\r\n");

    uart_send_string("Hello, from processor core_id_from_func ");
    uart_send(core_id_from_func + '0');
    uart_send_string("\r\n");

    // Counts for next CORE to go
    ++semaphore;

    // Do the echo only for CORE #0
    if (core_id_from_func == 0) {
        // Wait for all cores to finish
        while (semaphore != 4) { }

        while (1) {
            uart_send(uart_recv());
        }
    }
}