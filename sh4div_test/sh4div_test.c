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
    asm("nop\n");
    asm("nop\n");
    asm("nop\n");
    asm("nop\n");
    asm("nop\n");
    asm("nop\n");
    asm("nop\n");
    asm("nop\n");
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

    sh4asm_input_string(prog_asm);
    refresh_inst_list();

    test_fn func_ptr = (test_fn)inst_list;
    unsigned actual_quotient = func_ptr(dividend, divisor);
    printf("the actual result is %u\n", actual_quotient);

    return 0;
}

int main(int argc, char **argv) {
    sh4asm_set_emitter(emit_fn);
    unsigned_div_test_32_16();

    return 0;
}
