# OSDIg

## L1 Kernel Initialization(mini uart & GPIO)

#### GPIO alternative function selection 

![Raspberry Pi GPIO (14 & 15) alternative functions](https://github.com/tingggggg/OSDIg/blob/main/images/l1/alt.png)

The pins 14 and 15 have the TXD1 and RXD1 alternative functions available. This means that if we select alternative function number 5 for pins 14 and 15, they will be used as a Mini UART Transmit Data pin and Mini UART Receive Data pin, respectively. 

![Raspberry Pi GPIO function selector](https://github.com/tingggggg/OSDIg/blob/main/images/l1/gpfsel1.png)

#### Result
```
void kernel_main(void)
{
    uart_init();
    uart_send_string("Hello, Paspberry 3B+\r\n");

    while (1) {
        uart_send(uart_recv());
    }
}
```
![Raspberry Pi GPIO function selector](https://github.com/tingggggg/OSDIg/blob/main/images/l1/l1_result.png)


