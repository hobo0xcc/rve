.section .text.init,"ax"

.global _start
_start:
    la sp, 0x1000
    addi sp, sp, -8
    sd ra, 0(sp)
    jal ra, main
    ld ra, 0(sp)
    addi sp, sp, 8
    ret
