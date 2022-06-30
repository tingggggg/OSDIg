.global get_el
get_el:
    mrs x0, CurrentEL
    lsr x0, x0, #2
    ret

.global put32 // write some data to a 32-bit register
put32:
    str w1, [x0]
    ret

.global get32 // read some data from a 32-bit register
get32:
    ldr w0, [x0]
    ret

.global delay
delay:
    subs x0, x0, #1
    bne delay
    ret
    