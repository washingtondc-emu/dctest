################################################################################
#
# Copyright (c) 2017, snickerbockers <chimerasaurusrex@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of the copyright holder nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
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
