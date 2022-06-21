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
![L1 Result](https://github.com/tingggggg/OSDIg/blob/main/images/l1/l1_result.png)


## L2 Processor Initialization

It has some essential features that can be utilized by the OS. The first such feature is called "Exception levels".

#### Finding current Exception level
```
.globl get_el
get_el:
    mrs x0, CurrentEL
    lsr x0, x0, #2
    ret
```

#### Changing current exception level
1. Address of the current instruction is saved in the `ELR_ELn`  register. (It is called `Exception link register`)
1. Current processor state is stored in `SPSR_ELn` register (`Saved Program Status Register`)

```
#define SPSR_MASK_ALL 		(7 << 6) // change EL to EL1
#define SPSR_EL1h		(5 << 0) // EL1h mode means that we are using EL1 dedicated stack pointer
#define SPSR_VALUE		(SPSR_MASK_ALL | SPSR_EL1h)
```

```
// SPSR_EL3, Saved Program Status Register (EL3)
ldr    x0, =SPSR_VALUE
msr    spsr_el3, x0

// ELR_EL3, Exception Link Register (EL3)
adr    x0, el1_entry        
msr    elr_el3, x0
eret 
```

#### Result
```
void kernel_main(void)
{
    ...
    printf("Hello, Paspberry 3B+\r\n");

    int el = get_el();
    printf("Exception level: %d\r\n", el);
    ...
}
```

![L2 Result](https://github.com/tingggggg/OSDIg/blob/main/images/l2/l2_result.png)