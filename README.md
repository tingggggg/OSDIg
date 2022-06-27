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

## L3 Interrupt Handling

Such asynchronous notifications are called "interrupts" because they interrupt normal execution flow and force the processor to execute an "interrupt handler".

#### Interrupts vs exceptions
* `Synchronous exception`
* `IRQ (Interrupt Request)`
* `FIQ (Fast Interrupt Request)`
* `SError (System Error)`


#### Saving register state

After an exception handler finishes execution, we want all general purpose registers to have the same values they had before the exception was generated.

```
sub	sp, sp, #S_FRAME_SIZE
stp	x0, x1, [sp, #16 * 0]
...
str	x30, [sp, #16 * 15]
```

#### Masking/unmasking interrupts

Sometimes a particular piece of code must never be intercepted by an asynchronous interrupt.
Processor state would be overwritten and lost.
```
.globl enable_irq
enable_irq:
    msr    daifclr, #2
    ret

.globl disable_irq
disable_irq:
    msr    daifset, #2
    ret
```

ARM processor state has 4 bits that are responsible for holding mask status for different types of interrupts. 
* `D` Masks debug exceptions.
* `A` Masks SErrors. It is called A because SErrors sometimes are called asynchronous aborts.
* `I` Masks IRQs
* `F` Masks FIQs


#### Result
```
void handle_irq(void)
{
    unsigned int irq = get32(IRQ_PENDING_1);
    switch (irq) {
        case (SYSTEM_TIMER_IRQ_1):
            handle_timer_irq();
            break;
        ...
    }
}
```

Here we first update compare register so that that next interrupt will be generated after the same time interval. 
Next, we acknowledge the interrupt by writing 1 to the `TIMER_CS` register.
In the documentation `TIMER_CS` is called "Timer Control/Status" register.
```
void handle_timer_irq( void )
{
    curVal += interval;
    put32(TIMER_C1, curVal);
    put32(TIMER_CS, TIMER_CS_M1);
    printf("Timer iterrupt received\n\r");
}
```

![L3 Result](https://github.com/tingggggg/OSDIg/blob/main/images/l3/l3_result.png)