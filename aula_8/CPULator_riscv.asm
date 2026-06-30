.global _start

_start:
    li x10, 10       # operando A
    li x11, 3        # operando B
    li x12, 0        # opcode: 0 soma, 1 sub, 2 mul, 3 div, 4 fat
    li x20, 0        # flag de erro: 0 ok, 1 erro

    li x5, 0
    beq x12, x5, soma

    li x5, 1
    beq x12, x5, subtracao

    li x5, 2
    beq x12, x5, multiplicacao

    li x5, 3
    beq x12, x5, divisao

    li x5, 4
    beq x12, x5, fatorial

    j erro

soma:
    add x13, x10, x11
    j fim

subtracao:
    sub x13, x10, x11
    j fim

multiplicacao:
    li x13, 0
    mv x6, x10
    mv x7, x11

mul_loop:
    beq x7, x0, fim

    andi x8, x7, 1
    beq x8, x0, pula_soma

    add x13, x13, x6

pula_soma:
    slli x6, x6, 1
    srli x7, x7, 1
    j mul_loop

divisao:
    beq x11, x0, erro_div_zero

    li x13, 0       # quociente
    mv x6, x10      # resto parcial

div_loop:
    blt x6, x11, fim

    sub x6, x6, x11
    addi x13, x13, 1
    j div_loop

fatorial:
    li x13, 1
    mv x6, x10

fat_loop:
    li x5, 1
    ble x6, x5, fim

    mul x13, x13, x6
    addi x6, x6, -1
    j fat_loop

erro_div_zero:
    li x20, 1
    li x13, 0
    j fim

erro:
    li x20, 1
    li x13, 0
    j fim

fim:
    j fim
