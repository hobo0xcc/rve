addi x2, x2, 20
addi x3, x3, 20
beq x2, x3, label2
label1:
addi x1, x1, 24
j label3
label2:
addi x1, x1, 42
label3:
nop
