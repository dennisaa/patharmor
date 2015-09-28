#define ARMOR_IOC_CALLBACK_ENTER 0x6c0a
#define ARMOR_IOC_CALLBACK_EXIT  0x6c0b
#define SYS_IOCTL                0x10

.section .text

.global armor_cbk_enter
.global armor_cbk_target_call
.global armor_cbk_target_return
.global armor_cbk_return

.global armor_pthread_enter

.type   armor_cbk_enter,     @function
.type   armor_pthread_enter, @function


armor_cbk_enter:
    // parameter1 (rdi): target address of indirect call
    // parameter2 (rsi): size (nr of bytes) of indirect call instruction
    
    mov %rdi, %r11                              // r11 = return address of library function
    mov armor_start_code@GOTPCREL(%rip), %r10   // r10 = start_code
    sub (%r10), %r11                            // r11 = target - start_code
    mov armor_range_code@GOTPCREL(%rip), %r10   // r10 = range_code (end_code - start_code)
    cmp (%r10), %r11                            // if r11 < r10:
    jb return_in_range                          //   continue to return_in_range
    ret                                         // else return
return_in_range:



    mov armor_fd@GOTPCREL(%rip), %r11
    cmp $0x00, (%r11)
    jne armor_fd_set
    ret
armor_fd_set:


    // Indirect call from a library to our target program. We will make the call
    // from here. This means that we need to update the current return address
    // so that it points to the first instruction after the indirect call (in
    // order to not make the indirect call twice).

    mov %rdi, %r10  // r10 = rdi (target of indirect call)
    
    pop %r11        // return addres
    add $18, %r11   // skip over lea, pop, and mov instructions
    add %rsi, %r11  // skip over call instruction

armor_cbk_pthread_hook:
    // make the ARMOR_IOC_CALLBACK_ENTER ioctl. We request the kernel to store
    // the new return address by providing it as the third argument: we have no
    // reliable way of storing it here. The target may destroy any registers and
    // we cannot push it to the stack either (we can't make a new stack frame,
    // as we don't know which parameters we have to provide.
    mov $SYS_IOCTL, %rax                    // rax = syscall number
    mov armor_fd@GOTPCREL(%rip), %rdi
    mov (%rdi), %rdi                        // rdi = 1st argument: fd
    mov $ARMOR_IOC_CALLBACK_ENTER, %rsi     // rsi = 2nd argument: ARMOR_IOC_CALLBACK_ENTER
    mov %r11, %rdx                          // rdx = 3rd argument: new return address
    syscall

    // now, do the stuff that we were actually supposed to do when returning...
    lea 0x8(%rsp), %rsp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %r9
    pop %r8
    pop %rax
    mov (%rsp),%rsp
    // this includes making the indirect call
armor_cbk_target_call:
    call *%r10
armor_cbk_target_return:


    // the indirect call finished. we need to make another ioctl saying the
    // callback exited. First push a dummy value that will act as the new return
    // address and store some registers
    push $0xcafe
    push %rdi
    push %rsi
    push %rdx
    push %rcx
    push %r8
    push %r9
    push %rax

    mov $SYS_IOCTL, %rax
    mov armor_fd@GOTPCREL(%rip), %rdi
    mov (%rdi), %rdi
    mov $ARMOR_IOC_CALLBACK_EXIT, %rsi
    lea 0x38(%rsp), %rdx
    syscall

    // restore
    pop %rax
    pop %r9
    pop %r8
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi

armor_cbk_return:
    ret




armor_pthread_enter:
    /* We want to make the indirect call from armor_cbk_enter so that we always
     * get the same src address for jumps to AT functions. This means that we
     * will jump back to armor_cbk_pthread_hook. We need to set some things up
     * for that to work though.
     * Our arguments: 
     *   1st arg: void *thread_args         RDI
     *   2nd arg: void *thread_function     RSI
     */
    mov  %rsi,  %r10    // make sure we have the target thread function in r10
    mov (%rsp), %r11    // r11 = return address (used by armor_cbk_pthread_hook)

    /* After the CALLBACK_ENTER syscall in armor_cbk_pthread_hook, it will pop
     * some values. Just do the inverse here:
     */
    push %rsp

    push %rax
    push %r8
    push %r9
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    lea -0x8(%rsp),%rsp

    // simply jump. we will return to R11 (our caller)
    jmp armor_cbk_pthread_hook

