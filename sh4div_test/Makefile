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

SH4ASM_CORE_DIR = ../external/sh4asm/sh4asm_core

ELFFILE = sh4div_test.elf
BINFILE = sh4div_test.bin
OBJS = sh4div_test.o $(SH4ASM_OBJS)
CFLAGS = -I ../external/sh4asm/sh4asm_core/

all: $(BINFILE)

include $(KOS_BASE)/Makefile.rules

clean:
	rm -f $(ELFFILE) $(BINFILE) $(OBJS)

$(BINFILE): $(ELFFILE)
	sh-elf-objcopy -O binary $(ELFFILE) $(BINFILE)

$(ELFFILE): $(OBJS)
	$(KOS_CC) $(KOS_CFLAGS) $(CFLAGS) $(KOS_LDFLAGS) \
	-o $(ELFFILE) $(KOS_START) $(OBJS) $(OBJEXTRA) $(KOS_LIBS)

include $(KOS_BASE)/Makefile.rules

$(ELFFILE): $(OBJS)
