# OSDIg

* [L1 Kernel Initialization](https://github.com/tingggggg/OSDIg#l1-kernel-initializationmini-uart--gpio)
* [L2 Processor Initialization](https://github.com/tingggggg/OSDIg#l2-processor-initialization)
* [L3 Interrupt Handling](https://github.com/tingggggg/OSDIg#l3-interrupt-handling)
* [L4 Processor Scheduler](https://github.com/tingggggg/OSDIg#l4-processor-scheduler)

*****

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

## L4 Processor Scheduler


#### Task struct
If we want to manage processes, the first thing we should do is to create a struct that describes a process.

* `cpu_context` This is an embedded structure that contains values of all registers that might be different between the tasks, that are being switched. Accordingly to ARM calling conventions registers x0 - x18 can be overwritten by the called function, so the caller must not assume that the values of those registers will survive after a function call. That's why it doesn't make sense to save x0 - x18 registers.
* `state` This is the state of the currently running task. For tasks that are just doing some work on the CPU the state will always be `TASK_RUNNING`.
* `counter` This field is used to determine how long the current task has been running. counter decreases by 1 each timer tick and when it reaches 0 another task is scheduled.
* `prioritt` When a new task is scheduled its priority is copied to counter. By setting tasks priority, we can regulate the amount of processor time that the task gets relative to other tasks.
* `preempt_count` If this field has a non-zero value it is an indicator that right now the current task is executing some critical function that must not be interrupted (for example, it runs the scheduling function.).

```
struct cpu_context {
    unsigned long x19;
    ...
    unsigned long x28;
    unsigned long fp;
    unsigned long sp;
    unsigned long pc;
};

struct task_struct {
    struct cpu_context cpu_context;
    long state;
    long counter;
    long priority;
    long preempt_count;
};
```

#### Memory Allocation
Each task in the system should have its dedicated stack. That's why when creating a new task we must have a way to allocate memory.

The allocator can work only with memory pages (each page is 4 KB in size). There is an array called `mem_map` that for each page in the system holds its status: whether it is allocated or free. Whenever we need to allocate a new page, we just loop through this array and return the first free page. This implementation is based on 2 assumptions:

1. We know the total amount of memory in the system. It is `1 GB - 1 MB` (the last megabyte of memory is reserved for device registers.). This value is stored in the `HIGH_MEMORY` constant.
2. First 4 MB of memory are reserved for the kernel image and init task stack. This value is stored in `LOW_MEMORY` constant. All memory allocations start right after this point.

```
static unsigned short mem_map [ PAGING_PAGES ] = {0,};

unsigned long get_free_page()
{
    for (int i = 0; i < PAGING_PAGES; i++){
        if (mem_map[i] == 0){
            mem_map[i] = 1;
            return LOW_MEMORY + i*PAGE_SIZE;
        }
    }
    return 0;
}

void free_page(unsigned long p)
{
    mem_map[(p - LOW_MEMORY) / PAGE_SIZE] = 0;
}
```