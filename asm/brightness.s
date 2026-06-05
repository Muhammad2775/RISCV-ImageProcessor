    .text
    .globl brightness
    .type brightness, @function
brightness:
    # a0 = input
    # a1 = output
    # a2 = size
    # a3 = B
    li t0, 0
1:
    bge t0, a2, 2f
    add t1, a0, t0
    lbu t2, 0(t1)
    add t2, t2, a3
    li t3, 0
    blt t2, t3, 3f
    li t4, 255
    bgt t2, t4, 4f
    j 5f
3:
    li t2, 0
    j 5f
4:
    li t2, 255
5:
    add t5, a1, t0
    sb t2, 0(t5)
    addi t0, t0, 1
    j 1b
2:
    ret
