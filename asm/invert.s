.global invert

invert:
    # a0 = input pointer
    # a1 = output pointer
    # a2 = size

    li t0, 0              # i = 0

loop:
    beq t0, a2, done      # if i == size → exit

    lbu t1, 0(a0)         # load input[i]
    li t2, 255
    sub t1, t2, t1        # t1 = 255 - pixel
    sb t1, 0(a1)          # store to output[i]

    addi a0, a0, 1        # input++
    addi a1, a1, 1        # output++
    addi t0, t0, 1        # i++

    j loop

done:
    ret
