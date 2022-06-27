#ifndef _MINI_UART_H_
#define _MINI_UART_H_

void uart_init(void);
char uart_recv(void);
void uart_send(char c);
void putc(void *p, char c);

#endif /* _MINI_UART_H_ */