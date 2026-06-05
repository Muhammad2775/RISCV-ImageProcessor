    .text
    .globl threshold

threshold:
    # a0 = input
    # a1 = output
    # a2 = size
    # a3 = threshold T

    li t0, 0              # i = 0

loop:
    bge t0, a2, done      # if i >= size → exit

    add t1, a0, t0        # input + i
    lbu t2, 0(t1)         # load pixel

    blt t2, a3, below     # if pixel < T

    li t3, 255            # else → 255
    j store

below:
    li t3, 0

store:
    add t4, a1, t0        # output + i
    sb t3, 0(t4)

    addi t0, t0, 1
    j loop

done:
    ret
    