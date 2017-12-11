/*******************************************************************************
 *
 * Copyright (c) 2017, snickerbockers <chimerasaurusrex@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>

#include "sh4asm.h"

#define INST_MAX 256

struct jit_ctxt {
    uint16_t inst_list[INST_MAX];
    unsigned inst_count;
};

struct jit_ctxt *cur_jit;

static void flush_jit_ctxt(struct jit_ctxt *ctxt) {
    unsigned idx;
    for (idx = 0; idx < INST_MAX; idx++) {
        uint16_t *ptr = ctxt->inst_list + idx;

        asm volatile (
            "ocbwb @%0\n"
            :
            : "r"(ptr));
    }
}

static void invalidate_icache(void) {
    uint32_t *ccr = (uint32_t*)(0xff00001c);
    uint32_t ccr_tmp = *ccr;
    ccr_tmp |= (1 << 11); // invalidate cache
    *ccr = ccr_tmp;

    /*
     * the official sh4 hardware manual you have to wait at least 8
     * instructions after invalidating the icache before you can branch
     */
    asm volatile (
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n");
}

static void refresh_jit_ctxt(struct jit_ctxt *ctxt) {
    flush_jit_ctxt(ctxt);
    invalidate_icache();
}

static void emit_fn(uint16_t inst) {
    if (cur_jit->inst_count < INST_MAX)
        cur_jit->inst_list[cur_jit->inst_count++] = inst;
    else
        printf("ERROR: instruction overflow!\n");
}

static void clear_jit(struct jit_ctxt *ctxt) {
    ctxt->inst_count = 0;
    sh4asm_reset();
}

static void set_jit(struct jit_ctxt *ctxt) {
    cur_jit = ctxt;
    clear_jit(ctxt);
}

/*
 * emit code to save all registers which should be preserved in the sh4
 * calling convention.
 *
 * source: https://msdn.microsoft.com/en-us/library/ms925519.aspx
 *         (hopefully this is the same in gnu as it is in Microsoft)
 */
static void emit_frame_open(void) {
    sh4asm_input_string(
        // r15 is always a stack pointer
        "mov.l r8, @-r15\n"
        "mov.l r9, @-r15\n"
        "mov.l r10, @-r15\n"
        "mov.l r11, @-r15\n"
        "mov.l r12, @-r15\n"
        "mov.l r13, @-r15\n"
        "mov.l r14, @-r15\n"
        );
}

/*
 * restore the state which was previously saved
 * by emit_frame_open.  This includes the return statement,
 * so be sure to set r0 before calling this.
 *
 * Also make sure you undo any changes to r15 before calling this
 */
static void emit_frame_close(void) {
    sh4asm_input_string(
        "mov.l @r15+, r14\n"
        "mov.l @r15+, r13\n"
        "mov.l @r15+, r12\n"
        "mov.l @r15+, r11\n"
        "mov.l @r15+, r10\n"
        "mov.l @r15+, r9\n"
        "rts\n"
        "mov.l @r15+, r8\n"
        );
}

/*
 * emit a function which will return the sr register.
 * the emitted function will store sr in a random register, then
 * return sr.
 */
static void emit_get_sr(void) {
    // it's modulo 15 because r15 is the stack pointer, so we can't use that.
    unsigned reg_no = rand() % 15;
    printf("reg_no is %u\n", reg_no);
    /* reg_no %= 15; */
    /* printf("post-modulo, reg_no is %u\n", reg_no); */

    emit_frame_open();
    sh4asm_printf("stc sr, r%u\n", reg_no);
    sh4asm_printf("mov r%u, r0\n", reg_no);
    emit_frame_close();
}

// test CLRS and SETS
static int test_clrs_sets(void) {
    static struct jit_ctxt get_sr_ctxt;
    memset(&get_sr_ctxt, 0, sizeof(get_sr_ctxt));

    set_jit(&get_sr_ctxt);
    emit_get_sr();

    refresh_jit_ctxt(&get_sr_ctxt);
    unsigned(*get_sr)(void) = (unsigned(*)(void))get_sr_ctxt.inst_list;

    printf("\tsr is initially 0x%08x\n", get_sr());
    asm volatile("clrs\n");
    printf("\tafter clrs, sr is now 0x%08x\n", get_sr());
    asm volatile("sets\n");
    printf("\tafter sets, sr is now 0x%08x\n", get_sr());
    asm volatile("clrs\n");
    printf("\tafter clrs, sr is now 0x%08x\n", get_sr());

    return 0;
}

// test CLRT and SETT
static int test_clrt_sett(void) {
    static struct jit_ctxt get_sr_ctxt;
    memset(&get_sr_ctxt, 0, sizeof(get_sr_ctxt));

    set_jit(&get_sr_ctxt);
    emit_get_sr();

    refresh_jit_ctxt(&get_sr_ctxt);
    unsigned(*get_sr)(void) = (unsigned(*)(void))get_sr_ctxt.inst_list;

    printf("\tsr is initially 0x%08x\n", get_sr());
    asm volatile("clrt\n");
    printf("\tafter clrt, sr is now 0x%08x\n", get_sr());
    asm volatile("sett\n");
    printf("\tafter sett, sr is now 0x%08x\n", get_sr());
    asm volatile("clrt\n");
    printf("\tafter clrt, sr is now 0x%08x\n", get_sr());

    return 0;
}

#define TEST_CASE(fn) { .name = #fn, .test_fn = fn }

static struct test_case {
    char const *name;
    int(*test_fn)(void);
} const tests[] = {
    TEST_CASE(test_clrs_sets),
    TEST_CASE(test_clrt_sett),
    { NULL }
};

/*
 * The following instructions do not have tests of their own, but are
 * implicitly tested by other tests:
 *
 * RTS (every test)
 * STC SR, Rn (test_clrs_sets, test_clrt_sett)
 *
 * TODO: the following instructions do not yet have tests implemented
 *
 * CLRMAC
 * LDTLB
 * NOP
 * RTE
 * SLEEP
 * FRCHG
 * FSCHG
 * MOVT Rn
 * CMP/PZ
 * CMP/PL
 * DT
 * ROTL Rn
 * ROTR Rn
 * ROTCL Rn
 * ROTCR Rn
 * SHAL Rn
 * SHAR Rn
 * SHLL Rn
 * SHLR Rn
 * SHLL2 Rn
 * SHLR2 Rn
 * SHLL8 Rn
 * SHLR8 Rn
 * SHLL16 Rn
 * SHLR16 Rn
 * BRAF Rn
 * BSRF Rn
 * CMP/EQ #imm, R0
 * AND.B #imm, @(R0, GBR)
 * AND #imm, R0
 * OR.B #imm, @(R0, GBR)
 * OR #imm, R0
 * TST #imm, R0
 * TST.B #imm, @(R0, GBR)
 * XOR #imm, R0
 * XOR.B #imm, @(R0, GBR)
 * BF label
 * BF/S label
 * BT label
 * BT/S label
 * BRA label
 * BSR label
 * TRAPA #immed
 * TAS.B @Rn
 * OCBI @Rn
 * OCBP @Rn
 * OCBWB @Rn
 * PREF @Rn
 * JMP @Rn
 * JSR @Rn
 * LDC Rm, SR
 * LDC Rm, GBR
 * LDC Rm, VBR
 * LDC Rm, SSR
 * LDC Rm, SPC
 * LDC Rm, DBR
 * STC GBR, Rn
 * STC VBR, Rn
 * STC SSR, Rn
 * STC SPC, Rn
 * STC SGR, Rn
 * STC DBR, Rn
 * LDC.L @Rm+, SR
 * LDC.L @Rm+, GBR
 * LDC.L @Rm+, VBR
 * LDC.L @Rm+, SSR
 * LDC.L @Rm+, SPC
 * LDC.L @Rm+, DBR
 * STC.L SR, @-Rn
 * STC.L GBR, @-Rn
 * STC.L VBR, @-Rn
 * STC.L SSR, @-Rn
 * STC.L SPC, @-Rn
 * STC.L SGR, @-Rn
 * STC.L DBR, @-Rn
 * MOV #imm, Rn
 * ADD #imm, Rn
 * MOV.W @(disp, PC), Rn
 * MOV.L @(disp, PC), Rn
 * MOV Rm, Rn
 * SWAP.B Rm, Rn
 * SWAP.W Rm, Rn
 * XTRCT Rm, Rn
 * ADD Rm, Rn
 * ADDC Rm, Rn
 * ADDV Rm, Rn
 * CMP/EQ Rm, Rn
 * CMP/HS Rm, Rn
 * CMP/GE Rm, Rn
 * CMP/HI Rm, Rn
 * CMP/GT Rm, Rn
 * CMP/STR Rm, Rn
 * DIV1 Rm, Rn
 * DIV0S Rm, Rn
 * DIV0U
 * DMULS.L Rm, Rn
 * DMULU.L Rm, Rn
 * EXTS.B Rm, Rn
 * EXTS.W Rm, Rn
 * EXTU.B Rm, Rn
 * EXTU.W Rm, Rn
 * MUL.L Rm, Rn
 * MULS.W Rm, Rn
 * MULU.W Rm, Rn
 * NEG Rm, Rn
 * NEGC Rm, Rn
 * SUB Rm, Rn
 * SUBC Rm, Rn
 * SUBV Rm, Rn
 * AND Rm, Rn
 * NOT Rm, Rn
 * OR Rm, Rn
 * TST Rm, Rn
 * XOR Rm, Rn
 * SHAD Rm, Rn
 * SHLD Rm, Rn
 * LDC Rm, Rn_BANK
 * LDC.L @Rm+, Rn_BANK
 * STC Rm_BANK, Rn
 * STC.L Rm_BANK, @-Rn
 * LDS Rm, MACH
 * LDS Rm, MACL
 * STS MACH, Rn
 * STS MACL, Rn
 * LDS Rm, PR
 * STS PR, Rn
 * LDS.L @Rm+, MACH
 * LDS.L @Rm+, MACL
 * STS.L MACH, @-Rn
 * STS.L MACL, @-Rn
 * LDS.L @Rm+, PR
 * STS.L PR, @-Rn
 * MOV.B Rm, @Rn
 * MOV.W Rm, @Rn
 * MOV.L Rm, @Rn
 * MOV.B @Rm, Rn
 * MOV.W @Rm, Rn
 * MOV.L @Rm, Rn
 * MOV.B Rm, @-Rn
 * MOV.W Rm, @-Rn
 * MOV.L Rm, @-Rn
 * MOV.B @Rm+, Rn
 * MOV.W @Rm+, Rn
 * MOV.L @Rm+, Rn
 * MAC.L @Rm+, @Rn+
 * MAC.W @Rm+, @Rn+
 * MOV.B R0, @(disp, Rn)
 * MOV.W R0, @(disp, Rn)
 * MOV.L Rm, @(disp, Rn)
 * MOV.B @(disp, Rm), R0
 * MOV.W @(disp, Rm), R0
 * MOV.L @(disp, Rm), Rn
 * MOV.B Rm, @(R0, Rn)
 * MOV.W Rm, @(R0, Rn)
 * MOV.L Rm, @(R0, Rn)
 * MOV.B @(R0, Rm), Rn
 * MOV.W @(R0, Rm), Rn
 * MOV.L @(R0, Rm), Rn
 * MOV.B R0, @(disp, GBR)
 * MOV.W R0, @(disp, GBR)
 * MOV.L R0, @(disp, GBR)
 * MOV.B @(disp, GBR), R0
 * MOV.W @(disp, GBR), R0
 * MOV.L @(disp, GBR), R0
 * MOVA @(disp, PC), R0
 * MOVCA.L R0, @Rn
 * FLDI0 FRn
 * FLDI1 Frn
 * FMOV FRm, FRn
 * FMOV DRm, DRn
 * FMOV XDm, DRn
 * FMOV DRm, XDn
 * FMOV XDm, XDn
 * FMOV.S @Rm, FRn
 * FMOV @Rm, DRn
 * FMOV @Rm, XDn
 * FMOV.S @(R0, Rm), FRn
 * FMOV @(R0, Rm), DRn
 * FMOV @(R0, Rm), XDn
 * FMOV.S @Rm+, FRn
 * FMOV @Rm+, DRn
 * FMOV @Rm+, XDn
 * FMOV.S FRm, @Rn
 * FMOV DRm, @Rn
 * FMOV XDm, @Rn
 * FMOV.S FRm, @-Rn
 * FMOV DRm, @-Rn
 * FMOV XDm, @-Rn
 * FMOV.S FRm, @(R0, Rn)
 * FMOV DRm, @(R0, Rn)
 * FMOV XDm, @(R0, Rn)
 * FLDS FRm, FPUL
 * FSTS FPUL, FRn
 * FABS FRn
 * FABS DRn
 * FADD FRm, FRn
 * FADD DRm, DRn
 * FCMP/EQ FRm, FRn
 * FCMP/EQ DRm, DRn
 * FCMP/GT FRm, FRn
 * FCMP/GT DRm, DRn
 * FDIV FRm, FRn
 * FDIV DRm, DRn
 * FLOAT FPUL, FRn
 * FLOAT FPUL, DRn
 * FMAC FR0, FRm, FRn
 * FMUL FRm, FRn
 * FMUL DRm, DRn
 * FNEG FRn
 * FNEG DRn
 * FSQRT FRn
 * FSQRT DRn
 * FSUB FRm, FRn
 * FSUB DRm, DRn
 * FTRC FRm, FPUL
 * FTRC DRm, FPUL
 * FCNVDS DRm, FPUL
 * FCNVSD FPUL, DRn
 * LDS Rm, FPSCR
 * LDS Rm, FPUL
 * LDS.L @Rm+, FPSCR
 * LDS.L @Rm+, FPUL
 * STS FPSCR, Rn
 * STS FPUL, Rn
 * STS.L FPSCR, @-Rn
 * STS.L FPUL, @-Rn
 * FIPR FVm, FVn
 * FTRV XMTRX, FVn
 * FSCA FPUL, DRn
 * FSRRA FRn
 */

#define N_TESTS 8

static jmp_buf error_point;

__attribute__((__noreturn__)) static void
error_handler(char const *fmt, va_list args) {
    vprintf(fmt, args);
    va_end(args);
    longjmp(error_point, 1);
}

int main(int argc, char **argv) {
    struct test_case const *curs = tests;

    if (setjmp(error_point) != 0) {
        printf("ERROR: the test failed due to some critical error\n");
        return 1;
    }

    sh4asm_set_error_handler(error_handler);
    sh4asm_set_emitter(emit_fn);

    while (curs->name) {
        int test_no;
        for (test_no = 0; test_no < N_TESTS; test_no++) {
            printf("==== %s iteration %u ====\n", curs->name, test_no);
            unsigned res = curs->test_fn();
            printf("%s iteration %u: %s!\n",
                   curs->name, test_no, res == 0 ? "SUCCESS" : "FAILURE");
        }
        curs++;
    }

    return 0;
}
