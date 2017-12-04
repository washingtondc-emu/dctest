/*******************************************************************************
 *
 *
 *    dctest: Dreamcast test program collection
 *    Copyright (C) 2017 snickerbockers <chimerasaurusrex@gmail.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 ******************************************************************************/

#include <kos.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "sh4asm.h"

typedef unsigned(*test_fn)(int, int);
typedef unsigned(*test_fn3)(int, int, int);

#define INST_MAX 256
uint16_t inst_list[INST_MAX];
unsigned inst_count;

static void flush_inst_list(void) {
    unsigned idx;
    for (idx = 0; idx < INST_MAX; idx++) {
        uint16_t *ptr = inst_list + idx;

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

static void refresh_inst_list(void) {
    flush_inst_list();
    invalidate_icache();
}

static void emit_fn(uint16_t inst) {
    if (inst_count < INST_MAX)
        inst_list[inst_count++] = inst;
    else
        printf("ERROR: instruction overflow!\n");
}

static void clear_jit(void) {
    inst_count = 0;
    sh4asm_reset();
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

static int pick_reg(bool const whitelist[16]) {
    unsigned reg_count = 0;
    unsigned reg_idx;
    for (reg_idx = 0; reg_idx < 16; reg_idx++)
        if (whitelist[reg_idx])
            reg_count++;
    unsigned pick = rand() % reg_count;
    for (reg_idx = 0; reg_idx < 16; reg_idx++)
        if (whitelist[reg_idx]) {
            if (!pick)
                return reg_idx;
            pick--;
        }
    printf("WARNING: REACHED END OF %s\n", __func__);
    return 0;
}

static int unsigned_div_test_32_16(void) {
    /*
     * pick a random 32-bit dividend and a random 16-bit divisor,
     * being careful to ensure that there is no overflow
     */
    uint32_t dividend, divisor, quotient;
    do {
        dividend = rand();
        divisor = rand() & 0xffff;
    } while ((!divisor) || (dividend >= (divisor << 16)));

    printf("%u / %u\n", (unsigned)dividend, (unsigned)divisor);

    /*
     * XXX: you can't actually rely on the expected quotient here to validate
     * results since that may very well turn out to use the exact same opcodes
     * as the generated asm
     */
    quotient = dividend / divisor;
    printf("the expected result is %u\n", (unsigned)quotient);

    clear_jit();

    unsigned idx;
    bool whitelist[16] = {
        false, true, true, true, true, false, true, true,
        true,  true, true, true, true, true,  true, false };
    unsigned dividend_reg = pick_reg(whitelist);
    whitelist[dividend_reg] = false;
    unsigned divisor_reg = pick_reg(whitelist);

    emit_frame_open();
    sh4asm_printf("mov r4, r%u\n", dividend_reg);
    sh4asm_printf("mov r5, r%u\n", divisor_reg);
    sh4asm_printf("shll16 r%u\n", divisor_reg);
    sh4asm_printf("mov #16, r0\n");
    sh4asm_printf("div0u\n");
    for (idx = 0; idx < 16; idx++)
        sh4asm_printf("div1 r%u, r%u\n", divisor_reg, dividend_reg);
    sh4asm_printf("rotcl r%u\n", dividend_reg);
    sh4asm_printf("extu.w r%u, r0\n", dividend_reg);
    emit_frame_close();

    refresh_inst_list();

    test_fn func_ptr = (test_fn)inst_list;
    unsigned actual_quotient = func_ptr(dividend, divisor);
    printf("the actual result is %u\n", actual_quotient);

    return actual_quotient == quotient;
}

static int signed_div_test_16_16(void) {
    /*
     * pick random 16-bit signed integers.
     * this is less complicated than it looks.
     */
    uint32_t dividend, divisor, quotient;

    do {
        dividend = rand();
        divisor = rand();

        uint32_t dividend_sign = dividend & 0x8000;
        if (dividend_sign)
            dividend |= ~0xffff;
        else
            dividend &= 0xffff;
        uint32_t divisor_sign = divisor & 0x8000;
        if (divisor_sign)
            divisor |= ~0xffff;
        else
            divisor &= 0xffff;
    } while (!divisor);

    bool whitelist[16] = {
        false, true, true, false, true, false, true, true,
        true,  true, true, true,  true, true,  true, false };
    unsigned dividend_reg = pick_reg(whitelist);
    whitelist[dividend_reg] = false;
    unsigned divisor_reg = pick_reg(whitelist);

    clear_jit();

    emit_frame_open();
    sh4asm_printf("mov r4, r%u\n", dividend_reg);
    sh4asm_printf("mov r5, r%u\n", divisor_reg);

    sh4asm_printf("shll16 r%u\n", divisor_reg);
    sh4asm_printf("exts.w r%u, r%u\n", dividend_reg, dividend_reg);
    sh4asm_printf("xor r0, r0\n");
    sh4asm_printf("mov r%u, r3\n", dividend_reg);
    sh4asm_printf("rotcl r3\n");
    sh4asm_printf("div0s r%u, r%u\n", divisor_reg, dividend_reg);
    unsigned idx;
    for (idx = 0; idx < 16; idx++)
        sh4asm_printf("div1 r%u, r%u\n", divisor_reg, dividend_reg);
    sh4asm_printf("exts.w r%u, r%u\n", dividend_reg, dividend_reg);
    sh4asm_printf("rotcl r%u\n", dividend_reg);
    sh4asm_printf("addc r0, r%u\n", dividend_reg);
    sh4asm_printf("exts.w r%u, r0\n", dividend_reg);
    emit_frame_close();

    refresh_inst_list();

    /*
     * XXX: you can't actually rely on the expected quotient here to validate
     * results since that may very well turn out to use the exact same opcodes
     * as the generated asm
     */
    quotient = (int32_t)dividend / (int32_t)divisor;
    printf("%d / %d\n", (int)dividend, (int)divisor);
    printf("the expected result is %d\n", (int)quotient);

    test_fn func_ptr = (test_fn)inst_list;
    int32_t actual_quotient = func_ptr(dividend, divisor);
    printf("the actual result is %d\n", (int)actual_quotient);

    return actual_quotient == quotient;
}

static int signed_div_test_32_32(void) {
    int32_t dividend, divisor, quotient;

    bool whitelist[16] = {
        false, true, true, false, true, false, true, true,
        true,  true, true, true,  true, true,  true, false };
    unsigned dividend_reg = pick_reg(whitelist);
    whitelist[dividend_reg] = false;
    unsigned divisor_reg = pick_reg(whitelist);

    clear_jit();
    emit_frame_open();

    sh4asm_printf("mov r4, r%u\n", dividend_reg);
    sh4asm_printf("mov r5, r%u\n", divisor_reg);

    sh4asm_printf("mov r%u, r3\n", dividend_reg);
    sh4asm_printf("rotcl r3\n");
    sh4asm_printf("subc r0, r0\n");
    sh4asm_printf("xor r3, r3\n");
    sh4asm_printf("subc r3, r%u\n", dividend_reg);

    // at this point the dividend is in one's-complement
    sh4asm_printf("div0s r%u, r0\n", divisor_reg);
    unsigned idx;
    for (idx = 0; idx < 32; idx++) {
        sh4asm_printf("rotcl r%u\n", dividend_reg);
        sh4asm_printf("div1 r%u, r0\n", divisor_reg);
    }
    sh4asm_printf("rotcl r%u\n", dividend_reg);
    sh4asm_printf("addc r3, r%u\n", dividend_reg);
    sh4asm_printf("mov r%u, r0\n", dividend_reg);
    emit_frame_close();

    refresh_inst_list();

    do {
        dividend = rand();
        divisor = rand();
    } while (!divisor);

    quotient = dividend / divisor;
    printf("%d / %d\n", (int)dividend, (int)divisor);
    printf("the expected result is %d\n", (int)quotient);

    test_fn func_ptr = (test_fn)inst_list;
    int32_t actual_quotient = func_ptr(dividend, divisor);
    printf("the actual result is %d\n", (int)actual_quotient);

    return actual_quotient == quotient;
}

static int unsigned_div_test_64_32(void) {
    int32_t quotient, quotient_actual;
    uint32_t dividend_high, dividend_low, divisor;
    int64_t dividend64;

    bool whitelist[16] = {
        false, true, true, true, true, false, false, true,
        true,  true, true, true,  true, true,  true, false };
    unsigned dividend_hi_reg = pick_reg(whitelist);
    whitelist[dividend_hi_reg] = false;
    unsigned dividend_low_reg = pick_reg(whitelist);
    whitelist[dividend_low_reg] = false;
    unsigned divisor_reg = pick_reg(whitelist);

    clear_jit();
    emit_frame_open();

    sh4asm_printf("mov r4, r%u\n", dividend_hi_reg);
    sh4asm_printf("mov r5, r%u\n", dividend_low_reg);
    sh4asm_printf("mov r6, r%u\n", divisor_reg);
    sh4asm_printf("div0u\n");
    unsigned idx;
    for (idx = 0; idx < 32; idx++) {
        sh4asm_printf("rotcl r%u\n", dividend_low_reg);
        sh4asm_printf("div1 r%u, r%u\n", divisor_reg, dividend_hi_reg);
    }
    sh4asm_printf("rotcl r%u\n", dividend_low_reg);
    sh4asm_printf("mov r%u, r0\n", dividend_low_reg);

    emit_frame_close();
    refresh_inst_list();

    do {
        dividend_high = rand();
        dividend_low = rand();
        divisor = rand();
    } while ((!divisor) || (dividend_high >= divisor));
    memcpy(&dividend64, &dividend_low, sizeof(dividend_low));
    memcpy(((uint32_t*)&dividend64) + 1, &dividend_high, sizeof(dividend_high));
    quotient = dividend64 / divisor;

    test_fn3 func_ptr = (test_fn3)inst_list;
    quotient_actual = func_ptr(dividend_high, dividend_low, divisor);
    printf("%lld / %d\n", (long long)dividend64, (int)divisor);
    printf("the expected result is %d\n", (int)quotient);
    printf("the actual result is %d\n", (int)quotient_actual);

    return quotient == quotient_actual;
}

#define N_TRIALS 8

int main(int argc, char **argv) {
    sh4asm_set_emitter(emit_fn);
    unsigned trial_no;
    unsigned n_success = 0;
    int res;
    printf("attempting %d trials\n", N_TRIALS);
    printf("==== unsigned_div_test_32_16 ====\n");
    for (trial_no = 0; trial_no < N_TRIALS; trial_no++) {
        res = unsigned_div_test_32_16();
        n_success += res;
        printf(res ? "SUCCESS\n" : "FAILURE\n");
    }
    printf("==== signed_div_test_16_16 ====\n");
    for (trial_no = 0; trial_no < N_TRIALS; trial_no++) {
        res = signed_div_test_16_16();
        n_success += res;
        printf(res ? "SUCCESS\n" : "FAILURE\n");
    }
    printf("==== signed_div_test_32_32 ====\n");
    for (trial_no = 0; trial_no < N_TRIALS; trial_no++) {
        res = signed_div_test_32_32();
        n_success += res;
        printf(res ? "SUCCESS\n" : "FAILURE\n");
    }
    printf("==== unsigned_div_test_64_32 ====\n");
    for (trial_no = 0; trial_no < N_TRIALS; trial_no++) {
        res = unsigned_div_test_64_32();
        n_success += res;
        printf(res ? "SUCCESS\n" : "FAILURE\n");
    }
    printf("%d successes out of %d total trials\n", n_success, N_TRIALS * 4);

    return 0;
}
