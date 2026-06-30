.global _start

_start:
    MOV r0, #10        @ operando A
    MOV r1, #3         @ operando B
    MOV r2, #0         @ opcode: 0 soma, 1 sub, 2 mul, 3 div, 4 fat
    MOV r10, #0        @ flag de erro: 0 ok, 1 erro

    CMP r2, #0
    BEQ soma

    CMP r2, #1
    BEQ subtracao

    CMP r2, #2
    BEQ multiplicacao

    CMP r2, #3
    BEQ divisao

    CMP r2, #4
    BEQ fatorial

    B erro

soma:
    ADD r3, r0, r1
    B fim

subtracao:
    SUB r3, r0, r1
    B fim

multiplicacao:
    MOV r3, #0
    MOV r4, r0
    MOV r5, r1

mul_loop:
    CMP r5, #0
    BEQ fim

    TST r5, #1
    ADDNE r3, r3, r4

    LSL r4, r4, #1
    LSR r5, r5, #1
    B mul_loop

divisao:
    CMP r1, #0
    BEQ erro_div_zero

    MOV r3, #0        @ quociente
    MOV r4, r0        @ resto parcial

div_loop:
    CMP r4, r1
    BLT fim

    SUB r4, r4, r1
    ADD r3, r3, #1
    B div_loop

fatorial:
    MOV r3, #1
    MOV r4, r0

fat_loop:
    CMP r4, #1
    BLE fim

    MUL r3, r3, r4
    SUB r4, r4, #1
    B fat_loop

erro_div_zero:
    MOV r10, #1
    MOV r3, #0
    B fim

erro:
    MOV r10, #1
    MOV r3, #0
    B fim

fim:
    B fim
