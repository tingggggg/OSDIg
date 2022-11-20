#ifndef _SCHED_H
#define _SCHED_H

#define THREAD_CPU_CONTEXT      0 // offset of cpu_context in task_struct
#define THREAD_FPSIME_CONTEXT	(14 * 64 / 8) // 14 = 13 registers of cpu_context + 1 to point to the next free position

#ifndef __ASSEMBLER__

#define THREAD_SIZE             4096

#define NR_TASKS                64

#define FIRST_TASK              task[0]
#define LAST_TASK               task[NR_TASKS - 1]

#define TASK_RUNNING            0

extern struct task_struct *current;
extern struct task_struct * task[NR_TASKS];
extern int nr_tasks;

struct cpu_context {
	unsigned long x19;
	unsigned long x20;
	unsigned long x21;
	unsigned long x22;
	unsigned long x23;
	unsigned long x24;
	unsigned long x25;
	unsigned long x26;
	unsigned long x27;
	unsigned long x28;
	unsigned long fp;
	unsigned long sp;
	unsigned long pc;
};

struct fpsimd_context {
	__uint128_t v[32];
	unsigned int fpsr;
	unsigned int fpcr;
};

struct task_struct {
	struct cpu_context cpu_context;
	struct fpsimd_context fpsimd_context;
	long state;	
	long counter;
	long priority;
	long preempt_count;
};

extern void sched_init(void);
extern void schedule(char *call_from);
extern void timer_tick(void);
extern void preempt_disable(void);
extern void preempt_enable(void);
extern void switch_to(struct task_struct* next);
extern void cpu_switch_to(struct task_struct* prev, struct task_struct* next);

#define INIT_TASK \
/*cpu_context*/	{ {0,0,0,0,0, 0,0,0,0,0, 0,0,0}, \
/*fpsimd_context*/ {{0}, 0, 0}, \
/* state etc */	0,0,1,0 \
}

#endif /* __ASSEMBLER__ */

#endif /* _SCHED_H */