
LDFLAGS += -ldl -lrt -rdynamic

SRCS = di-opt.cpp

all: di-opt

ECHO=echo
CXX=g++
CFLAGS += -I../include -fasynchronous-unwind-tables -fPIC -g -D__STDC_LIMIT_MACROS -Wno-deprecated -Dos_linux  -Darch_x86_64  -Dx86_64_unknown_linux2_4  -DARCH=x64
INSTALLDIR=../bin

# Dyninst headers
include ../Makefile.inc
INCLUDES += -I$(DYNINST_ROOT)/symlite/h -I$(DYNINST_ROOT)/common/h -I$(DYNINST_ROOT)/dyninstAPI/h -I$(DYNINST_ROOT)/instructionAPI/h -I$(DYNINST_ROOT)/dataflowAPI/h -I$(DYNINST_ROOT)/symtabAPI/h -I$(DYNINST_ROOT)/stackwalk/h -I$(DYNINST_ROOT)/dynC_API/h -I$(DYNINST_ROOT)/dwarf/h -I$(DYNINST_ROOT)/proccontrol/h -I$(DYNINST_ROOT)/elf/h -I$(DYNINST_ROOT)/parseAPI/h -I$(DYNINST_ROOT)/patchAPI/h -I$(DYNINST_ROOT)/dyninstAPI_RT/h  -I$(DYNINST_ROOT)/symlite/src -I$(DYNINST_ROOT)/common/src -I$(DYNINST_ROOT)/dyninstAPI/src -I$(DYNINST_ROOT)/instructionAPI/src -I$(DYNINST_ROOT)/dataflowAPI/src -I$(DYNINST_ROOT)/symtabAPI/src -I$(DYNINST_ROOT)/stackwalk/src -I$(DYNINST_ROOT)/dynC_API/src -I$(DYNINST_ROOT)/dwarf/src -I$(DYNINST_ROOT)/proccontrol/src -I$(DYNINST_ROOT)/elf/src -I$(DYNINST_ROOT)/parseThat/src -I$(DYNINST_ROOT)/parseAPI/src -I$(DYNINST_ROOT)/patchAPI/src -I$(DYNINST_ROOT)/dyninstAPI_RT/src -I$(DYNINST_IROOT)/common/h

LDFLAGS=-L$(DYNINST_IROOT)/lib -ldyninstAPI -linstructionAPI -lparseAPI -lpatchAPI -lsymtabAPI -ldl -lrt -rdynamic  #  -lcommon

INSTALL_DIR = ../bin

all: di-opt

di-opt: di-opt.o
	$(QUIET) $(ECHO) "  [LINK] $@"
	$(QUIET) $(CXX) $(CFLAGS) -o $@ $@.o $(LDFLAGS) $(LIBS)

%.o: %.cpp $(HEADERS)
	$(QUIET) $(ECHO)  "  [C++] $<"
	$(QUIET) $(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

install: $(INSTALL_DIR)/di-opt

$(INSTALL_DIR)/di-opt: di-opt
	install -c -D -m 744 $? $@

clean:
	rm -f *.o di-opt
