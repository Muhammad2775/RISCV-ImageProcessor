.text
.globl blur

blur:
    # a0 = input buffer
    # a1 = output buffer
    # a2 = width
    # a3 = height

    # save callee-saved registers we use
    # s0 = input base
    # s1 = output base
    # s2 = width
    # s3 = height
    # s4 = last_col = width - 1
    # s5 = last_row = height - 1
    addi sp, sp, -48
    sd   s0, 0(sp)
    sd   s1, 8(sp)
    sd   s2, 16(sp)
    sd   s3, 24(sp)
    sd   s4, 32(sp)
    sd   s5, 40(sp)

    mv   s0, a0
    mv   s1, a1
    mv   s2, a2
    mv   s3, a3

    addi s4, s2, -1          # last_col
    addi s5, s3, -1          # last_row

    # Magic reciprocal for division by 9:
    # quotient = (sum * 0x1C71C71C71C71C72) >> 64
    li   a7, 0x1C71C71C71C71C72
    li   t0, 0               # y = 0

y_loop:
    bge  t0, s3, done

    # row offsets
    mul  t1, t0, s2
    add  t2, s0, t1          # in_row  = input + y * width
    add  t3, s1, t1          # out_row = output + y * width

    # Border rows: copy whole row unchanged
    beqz t0, copy_row
    beq  t0, s5, copy_row

    li   t1, 0               # x = 0

x_loop:
    bge  t1, s2, next_row

    # Border columns: copy pixel unchanged
    beqz t1, copy_pixel
    beq  t1, s4, copy_pixel

    # Interior pixel:
    # Explicit 3x3 box blur with 9 direct samples
    # Row pointers:
    #   top    = in_row - width
    #   middle = in_row
    #   bottom = in_row + width
    sub  t5, t2, s2          # top row base
    add  t6, t2, s2          # bottom row base

    add  t4, t2, t1          # center ptr

    add  t5, t5, t1          # top ptr for x
    add  t6, t6, t1          # bottom ptr for x

    li   a4, 0               # sum = 0

    # top row
    lbu  a0, -1(t5)
    add  a4, a4, a0
    lbu  a0, 0(t5)
    add  a4, a4, a0
    lbu  a0, 1(t5)
    add  a4, a4, a0

    # middle row
    lbu  a0, -1(t4)
    add  a4, a4, a0
    lbu  a0, 0(t4)
    add  a4, a4, a0
    lbu  a0, 1(t4)
    add  a4, a4, a0

    # bottom row
    lbu  a0, -1(t6)
    add  a4, a4, a0
    lbu  a0, 0(t6)
    add  a4, a4, a0
    lbu  a0, 1(t6)
    add  a4, a4, a0

    # average = sum / 9 using reciprocal multiply
    mulhu a4, a4, a7         # a4 = sum / 9

    # store output pixel
    add  a1, t3, t1
    sb   a4, 0(a1)

    addi t1, t1, 1
    j    x_loop

copy_pixel:
    # copy border pixel unchanged
    add  t4, t2, t1
    lbu  a0, 0(t4)
    add  a1, t3, t1
    sb   a0, 0(a1)

    addi t1, t1, 1
    j    x_loop

copy_row:
    # copy entire border row unchanged
    li   t1, 0

copy_row_loop:
    bge  t1, s2, next_row
    add  t4, t2, t1
    lbu  a0, 0(t4)
    add  a1, t3, t1
    sb   a0, 0(a1)

    addi t1, t1, 1
    j    copy_row_loop

next_row:
    addi t0, t0, 1
    j    y_loop

done:
    # restore callee-saved registers
    ld   s0, 0(sp)
    ld   s1, 8(sp)
    ld   s2, 16(sp)
    ld   s3, 24(sp)
    ld   s4, 32(sp)
    ld   s5, 40(sp)
    addi sp, sp, 48
    ret
