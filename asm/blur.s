.text
.globl blur

blur:
    # a0 = input
    # a1 = output
    # a2 = width
    # a3 = height

    li t0, 0              # y = 0

y_loop:
    bge t0, a3, done

    li t1, 0              # x = 0

x_loop:
    bge t1, a2, next_row

    # ---- BORDER CHECK ----
    beqz t0, copy_pixel
    beqz t1, copy_pixel

    addi t2, a3, -1
    beq t0, t2, copy_pixel

    addi t3, a2, -1
    beq t1, t3, copy_pixel

    # ---- SUM = 0 ----
    li t4, 0              # sum

    li t5, -1             # dy = -1

dy_loop:
    li t6, -1             # dx = -1

dx_loop:
    # ny = y + dy
    add t2, t0, t5

    # nx = x + dx
    add t3, t1, t6

    # index = ny * width + nx
    mul t2, t2, a2
    add t2, t2, t3

    # address = input + index
    add t2, a0, t2

    # load pixel
    lbu t3, 0(t2)

    # sum += pixel
    add t4, t4, t3

    # dx++
    addi t6, t6, 1
    li t2, 1
    ble t6, t2, dx_loop

    # dy++
    addi t5, t5, 1
    li t2, 1
    ble t5, t2, dy_loop

    # avg = sum / 9
    li t2, 9
    div t4, t4, t2

    j store_pixel

copy_pixel:
    # index = y * width + x
    mul t2, t0, a2
    add t2, t2, t1

    add t2, a0, t2
    lbu t4, 0(t2)

store_pixel:
    # output[index]
    mul t2, t0, a2
    add t2, t2, t1
    add t2, a1, t2

    sb t4, 0(t2)

    addi t1, t1, 1
    j x_loop

next_row:
    addi t0, t0, 1
    j y_loop

done:
    ret
    