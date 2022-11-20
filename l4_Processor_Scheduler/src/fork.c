#include "mm.h"
#include "sched.h"
#include "entry.h"
#include "printf.h"

int copy_process(unsigned long fn, unsigned long arg, unsigned int priority)
{
    preempt_disable();
    struct task_struct *p;

    p = (struct task_struct *) get_free_page();
    if (!p)
        return 1;
    p->priority = priority;
    p->state = TASK_RUNNING;
    p->counter = p->priority;
    p->preempt_count = 1; // disable preemtion until schedule_tail

    p->cpu_context.x19 = fn;
    p->cpu_context.x20 = arg;
    p->cpu_context.pc = (unsigned long)ret_from_fork;
    p->cpu_context.sp = (unsigned long)p + THREAD_SIZE;

    p->next_task = 0;
    
    // int pid = nr_tasks++;
    // task[pid] = p;

    // Find the last task
    struct task_struct *tail_task = head_task;
    while (tail_task->next_task) 
        tail_task = tail_task->next_task;

    // Add new task node two the tasks list(tail)
    tail_task->next_task = p; 
    
    printf("\n\r----------- Task created -----------\r\n");
	printf("\n\rStruct task allocated at 0x%08x.\r\n", p);
    printf("(Origin tail task allocated at 0x%08x.)\r\n", tail_task);
	printf("p->cpu_context.x19 = 0x%08x. (fn)\r\n", p->cpu_context.x19);
	printf("p->cpu_context.x20 = 0x%08x. (arg)\r\n", p->cpu_context.x20);
	printf("p->cpu_context.pc  = 0x%08x. (ret_from_fork)\r\n", p->cpu_context.pc);
	printf("p->cpu_context.sp  = 0x%08x. (sp)\r\n", p->cpu_context.sp);

    preempt_enable();
    return 0;
}