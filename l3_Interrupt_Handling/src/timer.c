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