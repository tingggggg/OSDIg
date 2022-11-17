#include "sched.h"
#include "irq.h"
#include "printf.h"

static struct task_struct init_task = INIT_TASK;
struct task_struct *current = &(init_task);
struct task_struct *task[NR_TASKS] = {&(init_task), };
int nr_tasks = 1;

void preempt_disable(void)
{
    current->preempt_count++;
}

void preempt_enable(void)
{
    current->preempt_count--;
}

void print_debug_tasks(void) 
{
    struct task_struct *p;
    printf("\n\r----------- Task switch -----------\r\n");
    for (int i = 0; i < NR_TASKS; i++) {
        p = task[i];
        if (!p)
            return;

        printf("\n\r\ttask[%d] counter = %d\n\r", i, p->counter);
		printf("\ttask[%d] priority = %d\n\r", i, p->priority);
		printf("\ttask[%d] preempt_count = %d\n\r", i, p->preempt_count);
		printf("\ttask[%d] sp = 0x%08x\n\r", i, p->cpu_context.sp);
        printf("\n\r------------------------------\r\n");
    }
}

void _schedule(char *call_from)
{
    preempt_disable();
    int next, c;
    struct task_struct *p;
    while (1) {
        c = -1;
        next = 0;
        for (int i = 0; i < NR_TASKS; i++) {
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
    if (task[next] != current) {
        printf("_schedule(%s) switch_to next task (pid=%x)\r\n", call_from, next);
        print_debug_tasks();
    }
    switch_to(task[next]);
    preempt_enable();
}

void schedule(char *call_from)
{
    current->counter = 0;
    _schedule(call_from);
}

void switch_to(struct task_struct *next)
{
    if (current == next)
        return;
    struct task_struct *prev = current;
    current = next;
    cpu_switch_to(prev, next);
}

void schedule_tail(void) {
	preempt_enable();
}

void timer_tick()
{
    --current->counter;
    if (current->counter > 0 || current->preempt_count > 0) {
        return;
    }
    current->counter = 0;
    enable_irq();
    _schedule("irq_handler");
    disable_irq();
}