/*
MIT License

Copyright (c) 2018 Ben Lancaster

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifdef _MSC_VER
#include "getopt.h"
#else
#include <getopt.h>
#endif


#include <libprco/dbug.h>
#include <libprco/parser.h>
#include <libprco/module.h>
#include <libprco/arch/prco_isa.h>
#include <libprco/arch/prco_impl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct prco_emu_core {
        unsigned short pc;
        unsigned short r_regs[8];
        unsigned short r_sr;

        unsigned short lmem[0xff];

        struct prco_op_struct current_op;

        int should_branch;
};

struct prco_emu_core core = {0};

#define PRINT_SPACE(d) \
        dprintf((d), "\t\t\t\t\t\t\t\t");


void dbug_print_regs(void)
{
        int i;

        PRINT_SPACE(D_EMU);
        for(i = 0; i < 8; i++) {
                dprintf(D_EMU, "%02x ", core.r_regs[i]);
        }
        dprintf(D_EMU, "\r\n");
}

void dbug_print_mem(void)
{
        int i;
        int width = 8;
        int i2;

        for (i = 0; i <= width; i++) {
                printf("%02x\t", i);
        }

        printf("\r\n"
               "============================="
               "============================="
               "============================="
                "\r\n");

        for (i = 0; i < 0xff; i++) {
                dprintf(D_INFO, "%02x\t", core.lmem[i]);
                if(i2 == width) {
                        dprintf(D_INFO, "\r\n");
                        i2 = 0;
                } else {
                        i2++;
                }
        }
        dprintf(D_INFO, "\r\n");
}

unsigned short
alu_cmp(struct prco_emu_core *core,
        unsigned short a, unsigned short b)
{
        unsigned short tmp = 0;
        unsigned short ret = 0;

        tmp = a - b;

        // Zero flag
        if(tmp == 0) ret |= 1;

        // Signed flag
        ret |= ((tmp & 0x8000) >> 15);

        // Overflow flag
        switch((tmp & 0xC000) >> 14) {
        case 0b01:
        case 0b10:
                ret |= (1 >> 2);
                break;
        default:
                ret |= (0 >> 2);
        }

        PRINT_SPACE(D_EMU2);
        dprintf(D_EMU2, "ALU_CMP: %02x %02x = %02x\r\n", a, b, ret);

        return ret;
}

unsigned short
alu_should_jmp(unsigned char imm8)
{
        unsigned short ret = 0;

        switch(imm8) {
        case 0:
                ret = 1;
                break;
        case JMP_JE:
                ret = (core.r_sr & 0b1) == 1;
                break;
        case JMP_JNE:
                ret = (core.r_sr & 0b1) == 0;
                break;
        case JMP_JS:
                ret = ((core.r_sr & 0b10) >> 1) ==  0;
                break;
        case JMP_JG:
                ret = (
                        ((core.r_sr & 0b1) == 0) &
                        (
                                ((core.r_sr & 0b10) >> 1) == ((core.r_sr & 0b100) >> 2)
                        )
                );
                break;
        case JMP_JL:
                ret = (
                        ((core.r_sr & 0b10) >> 1) != ((core.r_sr & 0b100) >> 2)
                );
                break;
        default:
                ret = 0;
        }


        return ret;
}

unsigned short
alu_should_set(unsigned char imm8)
{
        switch(imm8) {
        case JMP_JE:
                return (core.r_sr & 0b1) == 1;
        case JMP_JL:
                return ((core.r_sr & 0b10) >> 1) != ((core.r_sr & 0b100) >> 2);
        case JMP_JG:
                return ((core.r_sr & 0b1) == 0) &
                       (
                               ((core.r_sr & 0b10) >> 1) == ((core.r_sr & 0b100) >> 2)
                       );
        default:
                return 0;
        }
}

void emu_load_mem(char *fpath)
{
        int i = 0;
        FILE *f;
        char buf[32];
        int buf_i = 0;
        char c = 0;

        f = fopen(fpath, "r");
        if(!f) return;

        while((c = fgetc(f)) != EOF) {
                if(isalnum(c)) {
                        buf[buf_i++] = c;
                }

                if (c == '\n') {
                        buf[buf_i] = 0;
                        int num = strtol(buf, NULL, 16);
                        //dprintf(D_EMU, "Read Word: '%s' %x\r\n", buf, num);
                        buf_i = 0;
                        core.lmem[i++] = num;
                }
        }
}

void emu_exec(struct prco_op_struct *op)
{
        switch(op->op) {
        case NOP:
                break;
        case ADD:
                core.r_regs[op->regD] += core.r_regs[op->regA];
                break;
        case ADDI:
                core.r_regs[op->regD] += (signed char)op->imm8;
                dbug_print_regs();
                break;
        case SUBI:
                core.r_regs[op->regD] -= (signed char)op->imm8;
                dbug_print_regs();
                break;
        case MOV:
                core.r_regs[op->regD] = core.r_regs[op->regA];
                dbug_print_regs();
                break;
        case MOVI:
                core.r_regs[op->regD] = op->imm8;
                dbug_print_regs();
                break;
        case SW:
                core.lmem[core.r_regs[op->regA + op->simm5]] = core.r_regs[op->regD];
                PRINT_SPACE(D_EMU2);
                dprintf(D_EMU2, "SW $%02x, mem[%02x]\r\n",
                        core.r_regs[op->regD],
                        core.r_regs[op->regA + op->simm5]);
                break;
        case LW:
                core.r_regs[op->regD] = core.lmem[core.r_regs[op->regA + op->simm5]];
                PRINT_SPACE(D_EMU2);
                dprintf(D_EMU2, "LW mem[%02x], $%02x\r\n",
                        core.r_regs[op->regA + op->simm5],
                        core.r_regs[op->regD]);
                dbug_print_regs();
                break;

        case CMP:
                core.r_sr = alu_cmp(
                        &core,
                        core.r_regs[op->regD],
                        core.r_regs[op->regA]);
                break;
        case JMP:
                core.should_branch = alu_should_jmp(op->imm8);
                break;

        case SET:
                core.r_regs[op->regD] = alu_should_set(op->imm8);
                break;
        }
}

struct prco_op_struct emu_decode(unsigned short mc)
{
        struct prco_op_struct dec = {0};
        dec.op = mc >> 11;
        dec.regD = (mc >> 8) & 0b111;
        dec.regA = (mc >> 5) & 0b111;
        dec.imm8 = (mc >> 0) & 0xff;
        dec.simm5 = (mc & 0b11111);
        dec.opcode = mc;

        assert_opcode(&dec, 0);

        return dec;
}

int emu_init(struct prco_emu_core *core)
{
        memset(core, 0, sizeof(*core));
        core->r_regs[7] = 0xff;

        // Load memory with instruction data
        emu_load_mem("verilog_memh.txt");
}

int emu_run(struct prco_emu_core *core)
{
        while(1) {
                dprintf(D_EMU, "0x%02x ", core->pc);

                // Decode the instruction
                core->current_op = emu_decode(core->lmem[core->pc]);
                if(core->current_op.op == HALT) break;

                // Execute the instruction
                emu_exec(&core->current_op);

                // Update pc
                if(core->should_branch) {
                        PRINT_SPACE(D_EMU2);
                        core->pc = core->r_regs[core->current_op.regD];
                        dprintf(D_EMU2, "Branching to %02x\r\n", core->pc);
                        core->should_branch = 0;
                } else {
                        core->pc++;
                }
        }
}

int main(int argc, char **argv)
{
        // Turn on all debug prints
        g_dbug_level = 0xff;

        // Initialise the core
        emu_init(&core);

        // Print default registers and memory
        dbug_print_mem();
        dbug_print_regs();

        // Run the emulator
        emu_run(&core);

        // After, print registers and memory
        dbug_print_mem();
        dbug_print_regs();

        return 0;
}