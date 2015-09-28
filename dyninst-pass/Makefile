
INSTALL_DIR = ../bin

PASSNAME := padyn
OBJS     := padyn.o

# Dyninst headers
include ../Makefile.inc
INCLUDES += -I$(DYNINST_ROOT)/symlite/h -I$(DYNINST_ROOT)/common/h -I$(DYNINST_ROOT)/dyninstAPI/h -I$(DYNINST_ROOT)/instructionAPI/h -I$(DYNINST_ROOT)/dataflowAPI/h -I$(DYNINST_ROOT)/symtabAPI/h -I$(DYNINST_ROOT)/stackwalk/h -I$(DYNINST_ROOT)/dynC_API/h -I$(DYNINST_ROOT)/dwarf/h -I$(DYNINST_ROOT)/proccontrol/h -I$(DYNINST_ROOT)/elf/h -I$(DYNINST_ROOT)/parseAPI/h -I$(DYNINST_ROOT)/patchAPI/h -I$(DYNINST_ROOT)/dyninstAPI_RT/h  -I$(DYNINST_ROOT)/symlite/src -I$(DYNINST_ROOT)/common/src -I$(DYNINST_ROOT)/dyninstAPI/src -I$(DYNINST_ROOT)/instructionAPI/src -I$(DYNINST_ROOT)/dataflowAPI/src -I$(DYNINST_ROOT)/symtabAPI/src -I$(DYNINST_ROOT)/stackwalk/src -I$(DYNINST_ROOT)/dynC_API/src -I$(DYNINST_ROOT)/dwarf/src -I$(DYNINST_ROOT)/proccontrol/src -I$(DYNINST_ROOT)/elf/src -I$(DYNINST_ROOT)/parseThat/src -I$(DYNINST_ROOT)/parseAPI/src -I$(DYNINST_ROOT)/patchAPI/src -I$(DYNINST_ROOT)/dyninstAPI_RT/src -I$(DYNINST_IROOT)/common/h

# Code definitions
CFLAGS += -fasynchronous-unwind-tables -fPIC -g -I../include -D__STDC_LIMIT_MACROS -Wno-deprecated -Dos_linux -I$(DYNINST_ROOT) -Darch_x86_64 -Dx86_64_unknown_linux2_4 -DARCH=x64 --std=c++11  -DARMOR_WRAP_LIBRARIES -DARMOR_WRAP_INDIRECTS -I ../include/patharmor

CFLAGS   += --std=c++11 
FFLAGS   += -DARMOR_WRAP_LIBRARIES
FFLAGS   += -DARMOR_WRAP_INDIRECTS

CC=gcc
CXX=g++
ECHO=echo

PASSLIBNAME =$(PASSNAME).di

all: $(PASSLIBNAME) $(EXTRA_TARGETS)

$(PASSLIBNAME): $(OBJS)
	$(QUIET) $(ECHO) "  [LINK] $@"
	$(QUIET) $(CXX) $(CFLAGS) $(FFLAGS) -shared -o $@ $(CPPS) $(OBJS) $(LDFLAGS) $(LIBS)

%.o: %.cpp
	$(QUIET) $(ECHO)  "  [C++] $<"
	$(QUIET) $(CXX) $(CFLAGS) $(FFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	$(QUIET) $(ECHO) "  [RM] $(OBJS) $(PASSLIBNAME) $(CLEANUP)"
	$(QUIET) rm -f $(OBJS) $(PASSLIBNAME) $(CLEANUP)

install:
	install -c -D -m 744 $(PASSNAME).di $(INSTALL_DIR)
