
# Run me with GNU make

MCFLAGS += -DARMOR_DEBUG
MCFLAGS += -DARMOR_DEBUG_JIT
MCFLAGS += -DARMOR_STATS
MCFLAGS += -DARMOR_VALIDATE
MCFLAGS += -DARMOR_JIT
MODULE = armor-module
OBJS = armor.o \
	   lbr.o \
       intercept.o 

S_OBJS = pre.o syscall-handler.o

HEADERS = $(wildcard include/*.h) $(wildcard $(LLVMINC)/patharmor/*.h)

MODULE_NAME=$(MODULE).ko

KDIR ?= /lib/modules/$(shell uname -r)/build

MCFLAGS += -I$(shell pwd)/../include -I$(shell pwd)/../include/patharmor

obj-m += $(MODULE).o
$(MODULE)-objs := $(OBJS) $(S_OBJS)

$(MODULE_NAME): $(HEADERS) $(patsubst %.o,%.c,$(OBJS)) $(patsubst %.o,%.S,$(S_OBJS))
	$(QUIET) make -C $(KDIR) M=$(CURDIR) modules EXTRA_CFLAGS="$(MCFLAGS)"

all: $(MODULE_NAME)

install:
	true

clean:
	$(RM) -rf *.o $(MODULE_NAME) .tmp_versions Module.symvers modules.order armor-module.mod.c .*.o.cmd .*.ko.cmd
