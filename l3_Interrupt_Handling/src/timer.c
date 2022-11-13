#include "utils.h"
#include "printf.h"
#include "peripherals/timer.h"

const unsigned int interval = 200000;
unsigned int curVal = 0;

void timer_init(void)
{
    curVal = get32(TIMER_CLO);
    curVal += interval;
    put32(TIMER_C1, curVal); // set first time of interrupt
}

void handle_timer_irq(void)
{
    curVal += interval;
    put32(TIMER_C1, curVal); // update for next interrupt
    put32(TIMER_CS, TIMER_CS_M1);
    printf("Timer interrupt received\r\n");
}

void arm_timer_init(void)
{
    put32(ARM_TIMER_LOAD, interval);
    put32(ARM_TIMER_CTRL, CTRL_32BIT | CTRL_INT_ENABLE | CTRL_ENABLE);
}

void handle_arm_timer_irq(void)
{
    put32(ARM_TIMER_CLR, 1);
    printf("Timer(ARM) interrupt reveived\r\n");
}