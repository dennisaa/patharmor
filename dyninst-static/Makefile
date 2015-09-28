INSTALL_DIR = ../bin

include ../Makefile.inc
DYNINST_LIB  = $(DYNINST_ROOT)/install/lib
DYNINST_INC  = -I$(DYNINST_ROOT) -I$(DYNINST_ROOT)/symlite/h -I$(DYNINST_ROOT)/common/h -I$(DYNINST_ROOT)/dyninstAPI/h -I$(DYNINST_ROOT)/instructionAPI/h -I$(DYNINST_ROOT)/dataflowAPI/h -I$(DYNINST_ROOT)/symtabAPI/h -I$(DYNINST_ROOT)/stackwalk/h -I$(DYNINST_ROOT)/dynC_API/h -I$(DYNINST_ROOT)/dwarf/h -I$(DYNINST_ROOT)/proccontrol/h -I$(DYNINST_ROOT)/elf/h -I$(DYNINST_ROOT)/parseAPI/h -I$(DYNINST_ROOT)/patchAPI/h -I$(DYNINST_ROOT)/dyninstAPI_RT/h  -I$(DYNINST_ROOT)/symlite/src -I$(DYNINST_ROOT)/common/src -I$(DYNINST_ROOT)/dyninstAPI/src -I$(DYNINST_ROOT)/instructionAPI/src -I$(DYNINST_ROOT)/dataflowAPI/src -I$(DYNINST_ROOT)/symtabAPI/src -I$(DYNINST_ROOT)/stackwalk/src -I$(DYNINST_ROOT)/dynC_API/src -I$(DYNINST_ROOT)/dwarf/src -I$(DYNINST_ROOT)/proccontrol/src -I$(DYNINST_ROOT)/elf/src -I$(DYNINST_ROOT)/parseThat/src -I$(DYNINST_ROOT)/parseAPI/src -I$(DYNINST_ROOT)/patchAPI/src -I$(DYNINST_ROOT)/dyninstAPI_RT/src -I$(DYNINST_IROOT)/common/h

PASSNAME := lbr_analysis_pass
SHARED   := arms_bb.o arms_edge.o arms_cfg.o arms_function.o arms_dyninst_cfg.o \
		arms_utils.o lbr_analysis.o address_taken_parser.o
OBJS     := lbr_analysis_pass.o $(SHARED)
CLEANUP  := dump_lbr_states dump_lbr_states.o core test test.o o.test objdump4cfg

V?=0
ifneq ($V,0)
QUIET=
ECHO:=@\#
QMAKE=VERBOSE=1 make
else
QUIET= @
ECHO= echo
QMAKE=make -s
endif

CFLAGS += -DPA_ROOT=$(CURDIR) -Darch_x86_64 -Dx86_64_unknown_linux2_4 -DARCH=x64 -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -std=c++0x -fasynchronous-unwind-tables -fPIC -g -Wno-deprecated -Dos_linux

LDFLAGS += -L$(DYNINST_LIB)

CC=g++
CXX=g++
INCLUDES=$(DYNINST_INC) -I../include -I../include/patharmor

PASSLIBNAME = $(PASSNAME).di

.PHONY: all install clean distclean

all: $(PASSLIBNAME)

$(PASSLIBNAME): $(OBJS)
	$(QUIET) $(ECHO) "  [LINK] $@"
	$(QUIET) $(CXX) $(CFLAGS) -shared -o $@ $(OBJS) $(LDFLAGS)

%.o: %.C
	$(QUIET) $(ECHO)  "  [C++] $<"
	$(QUIET) $(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

%.o: %.c
	$(QUIET) $(ECHO)  "  [C] $<"
	$(QUIET) $(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

install: $(INSTALL_DIR)/$(PASSLIBNAME)

$(INSTALL_DIR)/$(PASSLIBNAME): $(PASSLIBNAME)
	$(QUIET) $(ECHO) "  [INSTALL] $< -> $@"
	$(QUIET) install -c -D -m 744 $? $@

clean:
	$(QUIET) $(ECHO) "  [RM] $(OBJS) $(PASSLIBNAME) $(CLEANUP)"
	$(QUIET) rm -f $(OBJS) $(PASSLIBNAME) $(CLEANUP)

distclean: clean
	$(QUIET) $(ECHO)  "  [RM] $(INSTALL_DIR)/$(PASSLIBNAME)"
	$(QUIET) rm -f $(INSTALL_DIR)/$(PASSLIBNAME)

# Extra dependencies
arms_dyninst_cfg.o: arms_icall_resolver.h env.h

# Special
lbr_analysis_pass.o: lbr_analysis_pass.C env.h
	$(QUIET) $(ECHO)  "  [C++] $<"
	$(QUIET) $(CXX) $(CFLAGS) $(INCLUDES) -c lbr_analysis_pass.C

# Utilities: dump_lbr_states
dump_lbr_states: dump_lbr_states.o
	$(CXX) $(CFLAGS) dump_lbr_states.o -o dump_lbr_states -lcrypto

dump_lbr_states.o: dump_lbr_states.c
	$(CXX) $(CFLAGS) $(INCLUDES) -c dump_lbr_states.c

