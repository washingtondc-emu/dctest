################################################################################
#
#
#    dctest: Dreamcast test program collection
#    Copyright (C) 2017 snickerbockers
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#
################################################################################

all:
	make -C sh4div_test
SUBDIRS =

# XXX this path a ../ at the beginning because it's relative to sh4div_test's
# subdirectory
SH4ASM_CORE_DIR = ../external/sh4asm/sh4asm_core

SH4ASM_OBJS = $(SH4ASM_CORE_DIR)/sh4asm.o  $(SH4ASM_CORE_DIR)/parser.o      \
	$(SH4ASM_CORE_DIR)/sh4_bin_emit.o $(SH4ASM_CORE_DIR)/sh4_asm_emit.o \
	$(SH4ASM_CORE_DIR)/lexer.o   $(SH4ASM_CORE_DIR)/disas.o
export SH4ASM_OBJS

all: sh4div_test

sh4div_test:
	make -C sh4div_test

clean:
	make -C sh4div_test clean
	-rm -f $(SH4ASM_LIB) $(SH4ASM_OBJS)

include $(KOS_BASE)/Makefile.rules
