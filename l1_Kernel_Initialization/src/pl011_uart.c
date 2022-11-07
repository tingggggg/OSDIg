#include "peripherals/gpio.h"
#include "peripherals/pl011_uart.h"
#include "utils.h"
#include "pl011_uart.h"

void uart_send(char c)
{
    while (get32(PL011_FR) & (1 << 5)) {}
    put32(PL011_DR, c);
}

char uart_recv(void)
{
    while (get32(PL011_FR) & (1 << 4)) {}
    return (get32(PL011_DR) & 0xFF);
}

void uart_send_string(char *str)
{
    for (int i = 0; str[i] != '\0'; i++) {
        uart_send((char)str[i]);
    }
}

void uart_init(void)
{
    unsigned int selector;

    selector = get32(GPFSEL1);
    selector &= ~(7 << 12);
    selector |= 4 << 12;
    selector &= ~(7 << 15);
    selector |= 4 << 15;
    put32(GPFSEL1, selector);

    put32(GPPUD, 0);
    delay(150);
    put32(GPPUDCLK0, (1 << 14) | (1 << 15));
    delay(150);
    put32(GPPUDCLK0, 0);

    // disable uart first
    put32(PL011_CR, 0);
    put32(PL011_IMSC, 0);

    /* reference 
    https://github.com/s-matyukevich/raspberry-pi-os/blob/31fc1481f529ba1a72a8a6bc62dc488b84fc2cdb/exercises/lesson01/2/adkaster/src/uart.c#L50
    
    Assume 48MHz UART Reference Clock (Standard)
    Calculate UART clock divider per datasheet
    BAUDDIV = (FUARTCLK/(16 Baud rate))
     Note: We get 6 bits of fraction for the baud div
    48000000/(16 * 115200) = 3000000/115200 = 26.0416666...
    Integer part = 26 :)
    From http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0183g/I54603.html
    we want floor(0.04166666.. * 64 + 0.5) = 3
    */
    put32(PL011_IBRD, 26);
    put32(PL011_FBRD, 3);

    // set to 8 bits(words length) and enable FIFOs
    put32(PL011_LCRH, (3 << 5) | (1 << 4));

    // Enable RX, TX, UART
    put32(PL011_CR, (1 << 9) | (1 << 8) | (1 << 0));
}
