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

static int unsigned_div_test_32_16(void) {
    static char const *prog_asm =
        /*
         * old test: divisor goes into r1, dividend goes into r2
         * new test: divisor goes into r5, dividend goes into r4
         */
        "shll16 r5\n"
        "mov #16, r0\n"
        "div0u\n"

        /*
         * looping is untenable here because we don't want to touch the T flag
         * it *is* possible to save/restore the T flag on every iteration, but
         * it's easier to just copy/paste the same instruction 16 times.
         */
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"

        "rotcl r4\n"
        "rts\n"
        "extu.w r4, r0\n";

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
    sh4asm_input_string(prog_asm);
    refresh_inst_list();

    test_fn func_ptr = (test_fn)inst_list;
    unsigned actual_quotient = func_ptr(dividend, divisor);
    printf("the actual result is %u\n", actual_quotient);

    return actual_quotient == quotient;
}

static int signed_div_test_16_16(void) {
    static char const *prog_asm =
        /*
         * old test: divisor goes into r1, dividend goes into r2
         * new test: divisor goes into r5, dividend goes into r4
         */
        "shll16 r5\n"
        "exts.w r4, r4\n"
        "xor r0, r0\n"
        "mov r4, r3\n"
        "rotcl r3\n"
        "subc r0, r4\n"

        "div0s r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"
        "div1 r5, r4\n"

        "exts.w r4, r4\n"
        "rotcl r4\n"
        "addc r0, r4\n"
        "rts\n"
        "exts.w r4, r0\n";

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

    /*
     * XXX: you can't actually rely on the expected quotient here to validate
     * results since that may very well turn out to use the exact same opcodes
     * as the generated asm
     */
    quotient = (int32_t)dividend / (int32_t)divisor;
    printf("%d / %d\n", (int)dividend, (int)divisor);
    printf("the expected result is %d\n", (int)quotient);

    clear_jit();
    sh4asm_input_string(prog_asm);
    refresh_inst_list();

    test_fn func_ptr = (test_fn)inst_list;
    int32_t actual_quotient = func_ptr(dividend, divisor);
    printf("the actual result is %d\n", (int)actual_quotient);

    return actual_quotient == quotient;
}

static int signed_div_test_32_32(void) {
    int32_t dividend, divisor, quotient;

    static char const *prog_asm =
        "mov r4, r3\n"
        "rotcl r3\n"
        "subc r0, r0\n"
        "xor r3, r3\n"
        "subc r3, r4\n"

        // at this point the dividend is in one's-complement
        "div0s r5, r0\n"

        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"
        "rotcl r4\n"
        "div1 r5, r0\n"

        "rotcl r4\n"
        "addc r3, r4\n"
        "rts\n"
        "mov r4, r0\n";

    clear_jit();
    sh4asm_input_string(prog_asm);
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

    /*
     * This test doesn't follow the same format as the other three.
     *
     * It expects the dividend to be a 64-bit int with the upper 4 bytes in R4,
     * and the lower 4 bytes in R5.  The divisor goes in R6.  The quotient will be
     * left in R5.
     */
    static char const *prog_asm =
        "div0u\n"

        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"
        "rotcl r5\n"
        "div1 r6, r4\n"

        "rotcl r5\n"
        "rts\n"
        "mov r5, r0\n";


    clear_jit();
    sh4asm_input_string(prog_asm);
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
