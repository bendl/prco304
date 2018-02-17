// prco instruction defines

`define PRCO_OP_NOP         5'b00000
`define PRCO_OP_LW          5'b00001
`define PRCO_OP_SW          5'b00010
`define PRCO_OP_MOV         5'b00011
`define PRCO_OP_MOVI        5'b00100
`define PRCO_OP_PUSH        5'b00101
`define PRCO_OP_POP         5'b00110
`define PRCO_OP_BEQ         5'b00111
`define PRCO_OP_ADD         5'b01000
`define PRCO_OP_ADDI        5'b01001
`define PRCO_OP_SUB         5'b01010
`define PRCO_OP_SUBI        5'b01011
`define PRCO_OP_JMP         5'b01100
`define PRCO_OP_CMP         5'b01101
`define PRCO_OP_NEG         5'b01110
`define PRCO_OP_CALL        5'b01111
`define PRCO_OP_RET         5'b10000
`define PRCO_OP_SPC         5'b10001
`define PRCO_OP_HALT        5'b10010
`define PRCO_OP_INT         5'b10011
`define PRCO_OP_OR          5'b10100
`define PRCO_OP_XOR         5'b10101
`define PRCO_OP_AND         5'b10110

`define PRCO_OP_BITS        15:11
`define PRCO_SELD_BITS      10:8
`define PRCO_SELA_BITS      10:8
`define PRCO_IMM8_BITS      7:0

`define PRCO_OP_JMP_J        0
`define PRCO_OP_JMP_JE       1
`define PRCO_OP_JMP_JNE      2
`define PRCO_OP_JMP_JG       3
`define PRCO_OP_JMP_JGE      4
`define PRCO_OP_JMP_JL       5
`define PRCO_OP_JMP_JLE      6
`define PRCO_OP_JMP_JS       7
`define PRCO_OP_JMP_JNS      8