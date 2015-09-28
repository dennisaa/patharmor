#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <asm/errno.h>
#include <asm/unistd.h>
#include <linux/mman.h>
#include <asm/proto.h>
#include <asm/delay.h>
#include <linux/init.h>
#include <linux/highmem.h>
#include <linux/sched.h>
#include <linux/linkage.h>
#include <asm/percpu.h>
#include <linux/delay.h>

#include "intercept.h"

void (*syscall_orig)(void) = NULL;
void (*syscall_after_swapgs)(void);
extern void syscall_new(void);

void print_bytestream(char *name, void *pointer, int bytes) {
    int i;

    printk("%s @ %p:\n", name, pointer);
    for (i = 0; i < bytes; i++) { printk("%02x ", (unsigned char) *(unsigned char *)(pointer + i)); }
    printk("\n");
}

void update_lstar(void *syscall_new)
{
    wrmsrl(MSR_LSTAR,syscall_new);
}

uint64_t addr_kernel_stack, addr_old_rsp;

__visible DEFINE_PER_CPU(unsigned long, r11_scratch);

int intercept_syscalls_init(void)
{
    uint64_t value;
   
    addr_old_rsp      = kallsyms_lookup_name("old_rsp");
    addr_kernel_stack = kallsyms_lookup_name("kernel_stack");

    /* Read the original syscall entry points. Assuming it is the same for all CPUs. */
    rdmsrl(MSR_LSTAR, value);

    syscall_orig = (void (*)(void)) value;
    syscall_after_swapgs = (void (*)(void)) kallsyms_lookup_name("system_call_after_swapgs");
   
#if 0
    /* Debug print both the original and new system call handler */
    print_bytestream("syscall_orig", syscall_orig, 256);
    print_bytestream("syscall_new",  syscall_new,  256);
#endif

    /* Overwrite the syscall entry point. All of them. */
    on_each_cpu(update_lstar, syscall_new, 1);

    return 0;
}

void intercept_syscalls_exit(void)
{
    if (syscall_orig == NULL) return;

    /* Restore the syscall entry point. All of them. */
    on_each_cpu(update_lstar, syscall_orig, 1);

    /* Give the system some time to adjust. If the module is being removed too
     * fast, code currently running in our system handler may cause a kernel
     * crash.
     */
    msleep(1000);

    syscall_orig = NULL;
}

noinline void synthesize_reljump(void *from, void *to) {
    struct __arch_relative_insn {
        u8 op;
        s32 raddr;
    } __packed *insn;

    printk("[intercept.c] inserting relative jump %p --> %p\n", from, to);
    insn = (struct __arch_relative_insn *)from;
    insn->raddr = (s32)((long)(to) - ((long)(from) + 5));
    insn->op = 0xe9; /* RELATIVEJUMP_OPCODE */
}

/* Make page containing <address> writable */
int make_rw(unsigned long address) {
   unsigned int level;
   pte_t *pte = lookup_address(address, &level);
   if(pte->pte &~ _PAGE_RW)
      pte->pte |= _PAGE_RW;
   return 0;
}
/* Make page containing <address> write protected */
int make_ro(unsigned long address) {
   unsigned int level;
   pte_t *pte = lookup_address(address, &level);
   pte->pte = pte->pte &~ _PAGE_RW;
   return 0;
}

/* Instrument the kernel so that when <symbol> is called, function at <jmp_dst>
 * is executed first. <jmp_back_src> should point to a 5 byte nopsled at the end
 * of the inserted function - this will be used to patch a jump back to the
 * original function. 
 */
int intercept_symbol(const char *symbol, void *jmp_dst, void *jmp_back_src) {
    void *jmp_src, *jmp_back_dst;
    
    /* Note that the following is highly speculative, we assume that:
     * - We only instrument at kernel function entries.
     * - Each instrumentation point starts with a 5 byte NOP instruction (due to
     *   the function tracer).
     * - 5 bytes is just enough to insert a jump instruction. 
     * We also don't bother about maintaining the orignal instruction.
     * */
    
    /* Find the function that we want to intercept. */
    jmp_src = (void *) kallsyms_lookup_name(symbol);
    if (jmp_src == NULL) {
        printk("Could not find symbol %s\n", symbol);
        return -1;
    }

    /* Where to jump back to exactly. */
    jmp_back_dst = jmp_src + 5;

    /* We now have everything we need:
     * 
     * ORIGINAL FUNCTION
     *
     * +---- <jmp_src>               +---- <jmp_back_dst>
     * |_____________________________|____________________
     * |     |     |     |     |     |        |          |
     * | nop | nop | nop | nop | nop | insn n | insn n+1 |
     * |_____|_____|_____|_____|_____|________|__________|
     * |<--------- 5 bytes --------->|
     *      
     *
     * INTERCEPTOR
     *
     * +---- <jmp_dst>                  +---- <jmp_back_src>
     * |________________________________|________________________
     * |     |     |     |     |        |     |     |     |     |
     * | nop | nop | ... | nop | <code> | nop | nop | ... | nop | 
     * |_____|_____|_____|_____|________|_____|_____|_____|_____|
     * |<------ 5 bytes ------>|        |<------ 5 bytes ------>|
     *
     * Let's instrument.
     */

    /* 1) Disable interrupts and mark pages writable. */
    local_irq_disable();
    make_rw( (unsigned long) jmp_src );
    make_rw( (unsigned long) jmp_dst );
    make_rw( (unsigned long) jmp_back_src );

    /* 2) Overwrite the first 5 bytes of the original basic block with a
     *    relative jump instruction to the interceptor. */
    print_bytestream("jmp_src", jmp_src, 16);
    synthesize_reljump(jmp_src, jmp_dst);
    print_bytestream("jmp_src", jmp_src, 16);

    /* 3) Overwrite the last 5 bytes of the interceptor with a relative jump
     *    back to the original basic block. */
    print_bytestream("jmp_back_src", jmp_back_src, 16);
    synthesize_reljump(jmp_back_src, jmp_back_dst);
    print_bytestream("jmp_back_src", jmp_back_src, 16);

    /* 4) Mark pages not-writeable and enable interrupts again. */
    make_ro( (unsigned long) jmp_src );
    make_ro( (unsigned long) jmp_dst );
    make_ro( (unsigned long) jmp_back_src );
    local_irq_enable();

    return 0;
}

/* Restore the patched function <symbol>. */
int restore_symbol(const char *symbol) {
    void *jmp_src;

    /* Find the function that we want to intercept. */
    jmp_src = (void *) kallsyms_lookup_name(symbol);
    if (jmp_src == NULL) {
        printk("Could not find symbol %s\n", symbol);
        return -1;
    }


    local_irq_disable();
    make_rw( (unsigned long) jmp_src );

    /* Overwrite the first 5 bytes of <symbol> with NOPs. */
    memcpy(jmp_src, "\x90\x90\x90\x90\x90", 5);

    make_ro( (unsigned long) jmp_src );
    local_irq_enable();

    return 0;
}
