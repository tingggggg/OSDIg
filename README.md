# OSDIg

* [L1 Kernel Initialization](https://github.com/tingggggg/OSDIg#l1-kernel-initializationmini-uart--gpio)

Mini UART initialization & Sending data using the Mini UART.

* [L2 Processor Initialization](https://github.com/tingggggg/OSDIg#l2-processor-initialization)

We are going to work more closely with the ARM processor. It has some essential features that can be utilized by the OS. The first such feature is called "Exception levels".

* [L3 Interrupt Handling](https://github.com/tingggggg/OSDIg#l3-interrupt-handling)

We send some command to a device, but it doesn't respond immediately. Instead, it notifies us when the work is completed. Such asynchronous notifications are called "interrupts" because they interrupt normal execution flow and force the processor to execute an "interrupt handler".

* [L4 Processor Scheduler](https://github.com/tingggggg/OSDIg#l4-processor-scheduler)

We still can't call this project an operating system. The reason is that it can't do any of the core tasks that any OS should do. One of such core tasks is called process scheduling. By scheduling mean that an operating system should be able to share CPU time between different processes. The hard part of it is that a process should be unaware of the scheduling happening: it should view itself as the only one occupying the CPU.

* [L5 UserProcesses SystemCalls](https://github.com/tingggggg/OSDIg#l5-userprocesses-systemcalls)

There is still a major drawback in this functionality: there is no process isolation at all.
First of all, we will move all user processes to EL0, which restricts their access to privileged processor operations. 
Then add a set of system calls to the RPi OS.

* [L6 Virtual Memory Management](https://github.com/tingggggg/OSDIg#l6-virtual-memory-management)

The RPi OS now can run and schedule user processes, but the isolation between them is not complete - all processes and the kernel itself share the same memory.
Fix all issues mentioned above by virtual memory.

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

There is still a major drawback in this functionality: there is no process isolation at all.
First of all, we will move all user processes to EL0, which restricts their access to privileged processor operations. 
Then add a set of system calls to the RPi OS.

#### System calls implementation

The main idea behind system calls is very simple: each system call is actually a synchronous exception. If a user program need to execute a syscall, it first has to to prepare all necessary arguments, and then run `svc` instruction. 

Defines 4 simple syscalls:

1. `write` This syscall outputs something on the screen using UART device.
2. `clone` This syscall creates a new user thread. The location of the stack for the newly created thread is passed as the first argument.
3. `malloc` This system call allocates a memory page for a user process.
4. `exit` Each process must call this syscall after it finishes execution.

Use write syscall as an example and take a look at the syscall wrapper function.
The function is very simple: it just stores syscall number in the w8 register and generates a synchronous exception by executing svc instruction. 
```
.globl call_sys_write
call_sys_write:
    mov w8, #SYS_WRITE_NUMBER    
    svc #0
    ret
```
```
void sys_write(char *buf) 
{
    printf(buf);
}
...
void * const sys_call_table[] = {sys_write, sys_malloc, sys_clone, sys_exit};
```

#### Handling  synchronous exceptions

After a synchronous exception is generated, the handler, which is registered in the exception table, is called. The handler starts with the following code.
First of all, as for all exception handlers, `kernel_entry` macro is called. Then `esr_el1` (Exception Syndrome Register) is checked. 
This register contains "exception class" field at offset `ESR_ELx_EC_SHIFT`. If exception class is equal to `ESR_ELx_EC_SVC64` this means that the current exception is caused by the svc instruction and it is a system call. In this case, we jump to `el0_svc` label and show an error message otherwise.

```
el0_sync:
    kernel_entry 0
    mrs    x25, esr_el1                // read the syndrome register
    lsr    x24, x25, #ESR_ELx_EC_SHIFT        // exception class
    cmp    x24, #ESR_ELx_EC_SVC64            // SVC in 64-bit state
    b.eq    el0_svc
    handle_invalid_entry 0, SYNC_ERROR
```

#### Moving a task to user mode

Before any syscall can take place, we obviously need to have a task running in user mode.
The function that actually does the job is called `move_to_user_mode`.

Right now we are in the middle of execution of a kernel thread that was created by forking from the init task.

We've seen that a small area (`pt_regs` area) was reserved at the top of the stack of the newly created task.
This is the first time we are going to use this area: we will save manually prepared processor state there. This state will have exactly the same format as `kernel_exit` macro expects and its structure is described by the `pt_regs` struct.
```
int move_to_user_mode(unsigned long pc)
{
    struct pt_regs *regs = task_pt_regs(current);
    memzero((unsigned long)regs, sizeof(*regs));
    regs->pc = pc;
    regs->pstate = PSR_MODE_EL0t;
    unsigned long stack = get_free_page(); //allocate new user stack
    if (!stack) {
        return -1;
    }
    regs->sp = stack + PAGE_SIZE;
    current->stack = stack;
    return 0;
}
```
The following fields of the `pt_regst` struct are initialized in the `move_to_user_mode` function.

* `pc` It now points to the function that needs to be executed in the user mode. `kernel_exit` will copy pc to the `elr_el1` register, thus making sure that we will return to the pc address after performing exception return.
* `pstate` This field will be copied to `spsr_el1` by the `kernel_exit` and becomes the processor state after exception return is completed. `PSR_MODE_EL0t` constant, which is copied to the `pstate` field, is prepared in such a way that exception return will be made to EL0 level.
* `stack` `move_to_user_mode` allocates a new page for the user stack and sets `sp` field to point to the top of this page.

`task_pt_regs` function is used to calculate the location of the `pt_regs` area.
```
struct pt_regs * task_pt_regs(struct task_struct *tsk)
{
    unsigned long p = (unsigned long)tsk + THREAD_SIZE - sizeof(struct pt_regs);
    return (struct pt_regs *)p;
}
```

#### Forking user processer

`user_process` function will be executed in the user mode. This function calls clone system call 2 times in order to execute user_process1 function in 2 parallel threads.
`clone` syscall wrapping function looks like. 

This function does the following.
* Saves registers `x0` - `x3`, those registers contain parameters of the syscall and later will be overwritten by the syscall handler.
* Calls syscall handler.
* Check return value of the syscall handler: if it is `0` this means that we return here right after the syscall finishes and we are executing inside the original thread - just return to the caller in this case.
* If the return value is non-zero, then it is PID of the new task and we are executing inside of the newly created thread. In this case, execution goes to `thread_start` label.
* The function, originally passed as the first argument, is called in a new thread.
* After the function finishes, `exit` syscall is performed - it never returns.
```
.globl call_sys_clone
call_sys_clone:
    /* Save args for the child.  */
    mov    x10, x0                    /*fn*/
    mov    x11, x1                    /*arg*/
    mov    x12, x2                    /*stack*/

    /* Do the system call.  */
    mov     x0, x2                    /* stack  */
    mov    x8, #SYS_CLONE_NUMBER
    svc    0x0

    cmp    x0, #0
    beq    thread_start
    ret

thread_start:
    mov    x29, 0

    /* Pick the function arg and execute.  */
    mov    x0, x11
    blr    x10

    /* We are done, pass the return value through x0.  */
    mov    x8, #SYS_EXIT_NUMBER
    svc    0x0
```

`copy_cprocess` function supports cloning user threads as well as kernel threads.

In case, when we are creating a new kernel thread, the function behaves exactly the same, as was described in the previous lesson. 
In the other case, when we are cloning a user thread, this part of the code is executed.
```
int copy_process(unsigned long clone_flags, unsigned long fn, unsigned long arg, unsigned long stack)
{
    ...
    if (clone_flags & PF_KTHREAD) {
        ...
    } else {
        struct pt_regs * cur_regs = task_pt_regs(current);
        *cur_regs = *childregs;
        childregs->regs[0] = 0;
        childregs->sp = stack + PAGE_SIZE; 
        p->stack = stack;
    }
    ...
    return pid;
}
```

The current processor state is copied to the new task's state. `x0` in the new state is set to `0`, because `x0` will be interpreted by the caller as a return value of the syscall. We've just seen how clone wrapper function uses this value to determine whether we are still executing as a part of the original thread or a new one.

Next `sp` for the new task is set to point to the top of the new user stack page. We also save the pointer to the stack page in order to do a cleanup after the task finishes.

#### Result
```
void user_process()
{
    ...
    unsigned long stack = call_sys_malloc();
    ...

    // call "call_sys_clone" in sys.S
    int err = call_sys_clone((unsigned long)&user_process1, (unsigned long)"123456", stack);
    ...

    stack = call_sys_malloc();
	...
	err = call_sys_clone((unsigned long)&user_process1, (unsigned long)"abcd", stack);
	...
	call_sys_exit();
}

void kernel_process()
{
    printf("Kernel process started. EL %d\r\n", get_el());
    int err = move_to_user_mode((unsigned long)&user_process);
    ...
}

void kernel_main(void)
{
    ...
    int res = copy_process(PF_KTHREAD, (unsigned long)&kernel_process, 0, 0);
    if (res < 0) {
        printf("Error while starting kernel process\r\n");
        return; 
    }
    ...
}
```

![L5 Result](https://github.com/tingggggg/OSDIg/blob/main/images/l5/l5_result.png)

## L6 Virtual Memory Management

#### Translation process
Virtual memory provides each process with an abstraction that makes it think that it occupies all available memory.
Each time a process needs to access some memory location it uses virtual address, which is translated into a physical address.
```
                           Virtual address                                                                 Physical Memory
+-----------------------------------------------------------------------+                                +-----------------_+
|         | PGD Index | PUD Index | PMD Index | PTE Index | Page offset |                                |                  |
+-----------------------------------------------------------------------+                                |                  |
63        47     |    38      |   29     |    20    |     11      |     0                                |     Page N       |
                 |            |          |          |             +--------------------+           +---->+------------------+
                 |            |          |          +---------------------+            |           |     |                  |
          +------+            |          |                                |            |           |     |                  |
          |                   |          +----------+                     |            |           |     |------------------|
+------+  |        PGD        |                     |                     |            +---------------->| Physical address |
| ttbr |---->+-------------+  |           PUD       |                     |                        |     |------------------|
+------+  |  |             |  | +->+-------------+  |          PMD        |                        |     |                  |
          |  +-------------+  | |  |             |  | +->+-------------+  |          PTE           |     +------------------+
          +->| PUD address |----+  +-------------+  | |  |             |  | +->+--------------+    |     |                  |
             +-------------+  +--->| PMD address |----+  +-------------+  | |  |              |    |     |                  |
             |             |       +-------------+  +--->| PTE address |----+  +-------------_+    |     |                  |
             +-------------+       |             |       +-------------+  +--->| Page address |----+     |                  |
                                   +-------------+       |             |       +--------------+          |                  |
                                                         +-------------+       |              |          |                  |
                                                                               +--------------+          +------------------+
```

#### Allocating user porcesses

The end up implementing is to store the user program in a separate section of the kernel image. 

Define `user_begin` and `user_end` variables, which mark the beginning and end of this region. In this way we can simply copy everything between `user_begin` and `user_end` to the newly allocated process address space, thus simulating loading a user program.

```
    . = ALIGN(0x00001000);
    user_begin = .;
    .text.user : { build/user* (.text) }
    .rodata.user : { build/user* (.rodata) }
    .data.user : { build/user* (.data) }
    .bss.user : { build/user* (.bss) }
    user_end = .;
```

#### Creating first user process
`move_to_user_mode` function is responsible for creating the first user process. We call this function from a kernel thread.

```
    ...
    unsigned long begin = (unsigned long)&user_begin;
    unsigned long end = (unsigned long)&user_end;
    unsigned long process = (unsigned long)&user_process;
    int err = move_to_user_mode(begin, end - begin, process - begin);
    ...
```

* `pc` now points to the offset of the startup function in the user region.
* `allocate_user_page` reserves 1 memory page and maps it to the virtual address, provided as a second argument. In the process of mapping it populates page tables, associated with the current process.
* `memcpy` copy the whole user region to the page(VA) that we have just mapped.
* `set_pgd` 

```
    int move_to_user_mode(unsigned long start, unsigned long size, unsigned long pc)
    {
        struct pt_regs *regs = task_pt_regs(current);
        regs->pstate = PSR_MODE_EL0t;
        regs->pc = pc;
        regs->sp = 2 *  PAGE_SIZE;
        unsigned long code_page = allocate_user_page(current, 0);
        if (code_page == 0)    {
            return -1;
        }
        memcpy(code_page, start, size);
        set_pgd(current->mm.pgd);
        return 0;
    }
```

#### Mapping a virtual page

`allocate_user_page` function allocates a new page, maps it to the provided virtual address and returns a pointer to the page.

```
unsigned long allocate_user_page(struct task_struct *task, unsigned long va) {
    unsigned long page = get_free_page();
    if (page == 0) {
        return 0;
    }
    map_page(task, va, page);
    return page + VA_START;
}
```

`map_page` in some way duplicates what we've been doing in the `__create_page_tables` function: it allocates and populates a page table hierarchy.
`map_page` is responsible for one more important role: it keeps track of the pages that have been allocated during the process of virtuall address mapping.
```
void map_page(struct task_struct *task, unsigned long va, unsigned long page){
    unsigned long pgd;
    if (!task->mm.pgd) {
        task->mm.pgd = get_free_page();
        task->mm.kernel_pages[++task->mm.kernel_pages_count] = task->mm.pgd;
    }
    pgd = task->mm.pgd;
    int new_table;
    unsigned long pud = map_table((unsigned long *)(pgd + VA_START), PGD_SHIFT, va, &new_table);
    if (new_table) {
        task->mm.kernel_pages[++task->mm.kernel_pages_count] = pud;
    }
    unsigned long pmd = map_table((unsigned long *)(pud + VA_START) , PUD_SHIFT, va, &new_table);
    if (new_table) {
        task->mm.kernel_pages[++task->mm.kernel_pages_count] = pmd;
    }
    unsigned long pte = map_table((unsigned long *)(pmd + VA_START), PMD_SHIFT, va, &new_table);
    if (new_table) {
        task->mm.kernel_pages[++task->mm.kernel_pages_count] = pte;
    }
    map_table_entry((unsigned long *)(pte + VA_START), va, page);
    struct user_page p = {page, va};
    task->mm.user_pages[task->mm.user_pages_count++] = p;
}
```

`map_table` function as an analog of the create_table_entry macro. It extracts table index from the virtual address and prepares a descriptor in the parent table that points to the child table.

`map_table_entry` extracts PTE index from the virtual address and then prepares and sets PTE descriptor. It is similar to what we've been doing in the create_block_map macro.
```
unsigned long map_table(unsigned long *table, unsigned long shift, unsigned long va, int* new_table) {
    unsigned long index = va >> shift;
    index = index & (PTRS_PER_TABLE - 1);
    if (!table[index]){
        *new_table = 1;
        unsigned long next_level_table = get_free_page();
        unsigned long entry = next_level_table | MM_TYPE_PAGE_TABLE;
        table[index] = entry;
        return next_level_table;
    } else {
        *new_table = 0;
    }
    return table[index] & PAGE_MASK;
}

void map_table_entry(unsigned long *pte, unsigned long va, unsigned long pa) {
    unsigned long index = va >> PAGE_SHIFT;
    index = index & (PTRS_PER_TABLE - 1);
    unsigned long entry = pa | MMU_PTE_FLAGS;
    pte[index] = entry;
}
```

#### Forking a process

`fork` system call is implemented. copy_process function does most of the job.

```
int copy_process(unsigned long clone_flags, unsigned long fn, unsigned long arg)
{
    preempt_disable();
    struct task_struct *p;

    unsigned long page = allocate_kernel_page();
    p = (struct task_struct *) page;
    struct pt_regs *childregs = task_pt_regs(p);

    if (!p)
        return -1;

    if (clone_flags & PF_KTHREAD) {
        p->cpu_context.x19 = fn;
        p->cpu_context.x20 = arg;
    } else {
        struct pt_regs * cur_regs = task_pt_regs(current);
        *cur_regs = *childregs;
        childregs->regs[0] = 0;
        copy_virt_memory(p);
    }
    p->flags = clone_flags;
    p->priority = current->priority;
    p->state = TASK_RUNNING;
    p->counter = p->priority;
    p->preempt_count = 1; //disable preemtion untill schedule_tail

    p->cpu_context.pc = (unsigned long)ret_from_fork;
    p->cpu_context.sp = (unsigned long)childregs;
    int pid = nr_tasks++;
    task[pid] = p;    

    preempt_enable();
    return pid;
}
```

Copying user processes by function `copy_virt_memory`.
For each page, allocate another empty page and copy the original page content there. We also map the new page using the same virtual address, that is used by the original one.
```
int copy_virt_memory(struct task_struct *dst) {
    struct task_struct* src = current;
    for (int i = 0; i < src->mm.user_pages_count; i++) {
        unsigned long kernel_va = allocate_user_page(dst, src->mm.user_pages[i].virt_addr);
        if( kernel_va == 0) {
            return -1;
        }
        memcpy(src->mm.user_pages[i].virt_addr, kernel_va, PAGE_SIZE);
    }
    return 0;
}
```

#### Result

```
void kernel_process()
{
    printf("Kernel process started. EL %d\r\n", get_el());
    unsigned long begin = (unsigned long)&user_begin;
    unsigned long end = (unsigned long)&user_end;
    unsigned long process = (unsigned long)&user_process;
    int err = move_to_user_mode(begin, end - begin, process - begin);
    if (err < 0) {
        printf("Error while moving process to user mode\r\n");
    }
}

void kernel_main(void)
{
    ...
    int res = copy_process(PF_KTHREAD, (unsigned long)&kernel_process, 0);
    ...
    while (1) {
        schedule();
    }
}
```

![L6 Result](https://github.com/tingggggg/OSDIg/blob/main/images/l6/l6_result.png)