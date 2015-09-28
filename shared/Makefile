

LLVM_VERSION=3.4

LDFLAGS += -Wl,--whole-archive -lcrypto -Wl,--no-whole-archive
LIBNAME = wrappers
OBJS = wrappers.o callback.o libenter.o 
INCLUDES = -I../include/patharmor -I../include
LIBSONAME = lib$(LIBNAME).so
CFLAGS   += $(CXXFLAGS) -std=gnu99  -fPIC -Wall -Wno-long-long -D_GNU_SOURCE
INCLUDES += -I$(LLVMINC) -Iinclude -I../common/libtaskctl/include
LDFLAGS += -Wl,-soname -Wl,"${LIBSONAME}" -pthread -lrt

HEADERS += $(wildcard include/*.h) $(wildcard $(LLVMINC)/*.h) $(wildcard $(LLVMINC)/common/util/*.h) $(wildcard $(LLVMINC)/$(LIBNAME)/*.h) $(wildcard $(LLVMINC)/common/ut/*.h)

all: $(LIBSONAME)

QUIET=
ECHO=echo
CC=gcc

$(LIBSONAME): $(OBJS) $(OTHER_OBJS)
	$(QUIET) $(ECHO) "  [LINK] $@"
	$(QUIET) $(CC) $(CFLAGS) $(LLVMGOLD_LDFLAGS) -shared -o $@ $(OBJS) $(OTHER_OBJS) $(LDFLAGS) 

%.o: %.c $(HEADERS)
	$(QUIET) $(ECHO) "  [CC] $<"
	$(QUIET) $(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

%.o: %.S $(HEADERS)
	$(QUIET) $(ECHO) "  [AS] $<"
	$(QUIET) $(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

INSTALL_DIR=../bin

install: $(INSTALL_DIR)/$(LIBSONAME)

$(INSTALL_DIR)/$(LIBSONAME): $(LIBSONAME)
	$(QUIET) $(ECHO) "  [INSTALL] $< -> $@"
	$(QUIET) install -c -D -m 744 $? $@

clean_overrides:
	$(QUIET) $(ECHO) "  [RM] $(LIBOVERRIDE_FILES)"
	$(QUIET) cd override && rm -f $(LIBOVERRIDE_FILES)

clean: $(CLEAN_OVERRIDES)
	$(QUIET) $(ECHO) "  [RM] $(OBJS) $(LIBSONAME)"
	$(QUIET) rm -f $(OBJS) $(LIBSONAME) $(OTHER_OBJS)


distclean: clean
	$(QUIET) $(ECHO) "  [RM] $(INSTALL_DIR)/$(LIBSONAME)"
	$(QUIET) rm -f $(INSTALL_DIR)/$(LIBSONAME) 

PHONY: clean_overrides
