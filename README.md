# OSDIg

* [L1 Kernel Initialization](https://github.com/tingggggg/OSDIg#l1-kernel-initializationmini-uart--gpio)
* [L2 Processor Initialization](https://github.com/tingggggg/OSDIg#l2-processor-initialization)
* [L3 Interrupt Handling](https://github.com/tingggggg/OSDIg#l3-interrupt-handling)
* [L4 Processor Scheduler](https://github.com/tingggggg/OSDIg#l4-processor-scheduler)
# [L5 UserProcesses SystemCalls](https://github.com/tingggggg/OSDIg#l5-userprocesses-syscalls)

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

#### Creating a new task

The stack pointer is set to the top of the newly allocated memory page. `pc` is set to the ret_from_fork function.
```
struct task_struct *p;
p = (struct task_struct *) get_free_page();
if (!p)
    return 1;
...

p->cpu_context.x19 = fn;
p->cpu_context.x20 = arg;
p->cpu_context.pc = (unsigned long)ret_from_fork;
p->cpu_context.sp = (unsigned long)p + THREAD_SIZE;
```

It calls the function stored in `x19` register with the argument stored in `x20`.
```
.globl ret_from_fork
ret_from_fork:
    bl    schedule_tail
    mov    x0, x20
    blr    x19         //should never return
```

The function only prepares new task_struct and adds it to the task array. 
This task will be executed only after schedule function is called.

#### Scheduling algorithm

* The first inner `for` loop iterates over all tasks and tries to find a task in `TASK_RUNNING` state with the maximum counter. If such task is found and its counter is greater then 0, we immediately break from the outer `while` loop and switch to this task. If no such task is found this means that no tasks in `TASK_RUNNING` state currently exist or all such tasks have 0 counters. 
```
void _schedule(void)
{
    preempt_disable();
    int next,c;
    struct task_struct * p;
    while (1) {
        c = -1;
        next = 0;
        for (int i = 0; i < NR_TASKS; i++){
            p = task[i];
            if (p && p->state == TASK_RUNNING && p->counter > c) {
                c = p->counter;
                next = i;
            }
        }
        if (c) {
            break;
        }
        for (int i = 0; i < NR_TASKS; i++) {
            p = task[i];
            if (p) {
                p->counter = (p->counter >> 1) + p->priority;
            }
        }
    }
    switch_to(task[next]);
    preempt_enable();
}
```

#### Switching tasks
After the task in TASK_RUNNING state with non-zero counter is found, switch_to function is called. It looks like this.
```
void switch_to(struct task_struct * next)
{
    if (current == next)
        return;
    struct task_struct * prev = current;
    current = next;
    cpu_switch_to(prev, next);
}
```

This is the place where the real context switch happens.
* All calle-saved registers are stored in the order, in which they are defined in cpu_context structure. x30, which is the link register and contains function return address, is stored as pc, current stack pointer is saved as sp and x29 is saved as fp (frame pointer).
* Callee saved registers are restored from the next `cpu_context`.
* `ret` Function returns to the location pointed to by the link register (x30) If we are switching to some task for the first time, this will be ret_from_fork function.
```
.globl cpu_switch_to
cpu_switch_to:
    mov    x10, #THREAD_CPU_CONTEXT
    add    x8, x0, x10
    mov    x9, sp
    stp    x19, x20, [x8], #16        // store callee-saved registers
    stp    x21, x22, [x8], #16
    stp    x23, x24, [x8], #16
    stp    x25, x26, [x8], #16
    stp    x27, x28, [x8], #16
    stp    x29, x9, [x8], #16
    str    x30, [x8]
    add    x8, x1, x10
    ldp    x19, x20, [x8], #16        // restore callee-saved registers
    ldp    x21, x22, [x8], #16
    ldp    x23, x24, [x8], #16
    ldp    x25, x26, [x8], #16
    ldp    x27, x28, [x8], #16
    ldp    x29, x9, [x8], #16
    ldr    x30, [x8]
    mov    sp, x9
    ret
```

#### Result

```
void process(char *array)
{
    while (1) {
        for (int i = 0; i < 5; i++) {
            uart_send(array[i]);
            delay(100000);
        }
    }
}

void kernel_main(void)
{
    uart_init();
    init_printf(0, putc);
    ...
    int res = copy_process((unsigned long)&process, (unsigned long)"12345");
    ...
    res = copy_process((unsigned long)&process, (unsigned long)"abcde");
    ...
    while (1) {
        schedule();
    }
}
```

```
         0 +------------------+
           | kernel image     |
           |------------------|
           |                  |
           |------------------|
           | init task stack  |
0x00400000 +------------------+
           | task_struct 1    |
           |------------------|
           |                  |
           |------------------|
           | task 1 stack     |
0x00401000 +------------------+
           | task_struct 2    |
           |------------------|
           |                  |
0x00402000 +------------------+
           |                  |
           |                  |
0x3F000000 +------------------+
           | device registers |
0x40000000 +------------------+
```

![L4 Result](https://github.com/tingggggg/OSDIg/blob/main/images/l4/l4_result.png)


## L5 UserProcesses SystemCalls

WIP