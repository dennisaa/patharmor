#include <asm/percpu.h>
.section .text

.global syscall_new 

.macro SAVE
    swapgs
    mov %r11, PER_CPU_VAR(r11_scratch)
    mov (addr_old_rsp), %r11
    mov %rsp,%gs:(%r11)
    mov (addr_kernel_stack), %r11
    mov %gs:(%r11),%rsp
    mov PER_CPU_VAR(r11_scratch), %r11
    sti
    
    sub $0x50,%rsp
    mov %rdi,0x40(%rsp)
    mov %rsi,0x38(%rsp)
    mov %rdx,0x30(%rsp)
    mov %rax,0x20(%rsp)
    mov %r8,0x18(%rsp)
    mov %r9,0x10(%rsp)
    mov %r10,0x8(%rsp)
    mov %r11,(%rsp)
    mov %rax,0x48(%rsp)
    mov %rcx,0x50(%rsp)
.endm

.macro CONTINUE
    cli
    mov 0x50(%rsp),%rcx
    mov (%rsp),%r11
    mov 0x8(%rsp),%r10
    mov 0x10(%rsp),%r9
    mov 0x18(%rsp),%r8
    mov 0x20(%rsp),%rax
    mov 0x30(%rsp),%rdx
    mov 0x38(%rsp),%rsi
    mov 0x40(%rsp),%rdi
    mov (addr_old_rsp), %r11
    mov %gs:(%r11),%rsp
    jmp *syscall_after_swapgs
.endm

syscall_new:
    // syscall nr is in RAX.
    cmp $0x09, %rax
    je do_mmap

    cmp $0x0a, %rax
    je do_mprotect

    cmp $0x0d, %rax
    je do_sigaction

    cmp $0x0f, %rax
    je do_sigreturn

    cmp $0x3b, %rax
    je do_execve

#if 0
    cmp $0x00, %rax
    je do_read

    cmp $0x01, %rax
    je do_write
#endif

    // fallthrough:
    jmp *syscall_orig

do_mmap:
    SAVE
    call new_sys_mmap_pgoff
    CONTINUE

do_mprotect:
    SAVE
    call new_sys_mprotect
    CONTINUE

do_sigaction:
    SAVE
    call new_sys_rt_sigaction
    CONTINUE

do_sigreturn:
    SAVE
    call new_sys_rt_sigreturn
    CONTINUE

do_execve:
    SAVE
    call new_sys_execve
    CONTINUE 

do_read:
    SAVE
    call new_sys_read
    CONTINUE 

do_write:
    SAVE
    call new_sys_write
    CONTINUE 
