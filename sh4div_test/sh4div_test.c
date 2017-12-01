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

#include "sh4asm.h"

static void emit_fn(uint16_t inst) {
    printf("instruction received - 0x%04x\n", (unsigned)inst);
}

int main(int argc, char **argv) {
    sh4asm_set_emitter(emit_fn);
    sh4asm_input_string("mov r3, r4\n");

    return 0;
}
