#include <armor.h>

#define SYS_IOCTL           0x10

.section .text

.global armor_lib_enter
.global armor_lib_return
.type   armor_lib_enter, @function

/* This snippet will be inserted before specific library function entries. In
 * DIY mode, we are responsible for saving and restoring registers. Remember
 * that we are invoked via a call instruction. The x64 System V ABI calling
 * convention tells us:
 * - RDI, RSI, RDX, RCX, R8, R9 are used to hold the first six integer or
 *   pointer arguments (for syscalls, R10 is used instead of RCX).
 * - RBP, RBX, R12, R13, R14, R15 are callee-save. This means that the caller
 *   can assume these registers are unmodified after a call instruction. Other
 *   registers must be saved by the caller.
 * This means that we can freely use the following registers:
 * - RAX (used for return values)
 * - RSP (stack pointer, let's not use this one)
 * - R10
 * - R11
 * If we want to use anything else, we should save and restore these.
 *
 * KNOWN BUG (October 30, 2014)
 * The assumption that we can use R10 and R11 freely is wrong. Compiler
 * optimizations may exploit these registers to pass extra or hidden arguments to
 * specific functions. I found this to be the case for at least one function in
 * LIBC: hack_digit. This function assumes that the value in R10 is valid and when
 * it makes a call to __mpn_mul_1(), which tries to write to a memory address
 * based on the original R10 value, we segfault.
 * We could solve this by pusing r10 and r11 (which would require some magic
 * tricks to make the indirect call possible), but since we are not supposed to
 * wrap local, static functions anyway, I think we can get away with just
 * ignoring this issue.
 */
armor_lib_enter:
    /* We first need to check whether we are absolutely ready to enter our
     * wrapper code, or that we should forward control to the real
     * implementation directly.
     *
     * We do not want to execute our wrapper code if we are being called from
     * anything other than the target's code. We thus have to check whether
     *      start_code < return_address < end_code
     */ 
    mov 0x8(%rsp), %r11                         // r11 = return address of library function
    mov armor_start_code@GOTPCREL(%rip), %r10   // r10 = start_code
    sub (%r10), %r11                            // r11 = target - start_code
    mov armor_range_code@GOTPCREL(%rip), %r10   // r10 = range_code (end_code - start_code)
    cmp (%r10), %r11                            // if r11 < r10:
    jb return_in_range                          //   continue to return_in_range
    ret                                         // else return
return_in_range:

    /* We also cannot enter our wrapper code if <armor_fd> (the file descriptor
     * for /dev/armor) is not set.
     */
    mov armor_fd@GOTPCREL(%rip), %r11           // r11 = armor_fd
    cmp $0x00, (%r11)                           // if armor_fd != 0
    jne armor_fd_set                            //   continue to armor_fd_set
    ret                                         // else return
armor_fd_set:

    /* We have to do some actual work:
     * - Invoke an ioctl, telling our module that we are entering a library function;
     * - Let the library code do its work; and
     * - Invoke another ioctl, indicating that the library function finished.
     *
     * The second ioctl invocation complicates things a bit: as with library
     * entry, we only want to invoke it if we originate from the target's
     * address space. It does not make sense to adds above checks another time
     * though. Instead, we will do a bit of pop and call magic here:
     * - pop (the 1st return address, armor_lib_enter) into reg x
     * - pop (the 2nd return address, library_function)    into reg y
     * Now, the stack will be setup as if we are about to invoke the library
     * function. By doing a:
     * - call reg x
     * we invoke the original library function, but ensure that we return into
     * this (armor_lib_enter) wrapper code. When back here, we can safely
     * perform the second ioctl, and jump back to the original return address by
     * doing:
     * - push reg y
     * - ret
     * An additional advantage of this approach is that we can again predict
     * what the LBR will look like for library calls. However, for this to work,
     * we need to save the return address so that we can restore it when
     * leaving the wrapper, but where?
     * - We cannot use a global variable in the GOT: multi-threaded applications
     *   banging on the same library function would corrupt each others return
     *   addresses.
     * - We cannot store this value on the stack as this would corrupt the stack
     *   frame for the real library function.
     * - We would like to use a callee-save register (rbx, rbp, r12, r13, r14 or
     *   r15), but then we need to save such register first on the stack,
     *   resulting in a corrupted stack frame.
     * We will pass the return address to the kernel module and let them handle
     * it. We expect it back upon the second syscall (...LIB_EXIT).
     */



    /* Store some registers as we will use them for our syscall. First the ones
     * that we are definitly modifying:
     */
    push %rdi   // will be our 1st argument
    push %rsi   // will be our 2nd argument
    push %rdx   // will be our 3rd argument
    /* Now, since RBP, RBX, R12, R13, R14 and R15 are callee-save (and we did
     * not use them), I guess that we can assume that the kernel will save these
     * for us. We are also free to use RAX, RSP, R10 and R11, so we don't have
     * to save these as well. What's left is the function arguments.  
     */
    push %rcx
    push %r8
    push %r9

    // Setup the ARMOR_IOC_LIB_ENTER ioctl. 
    mov $SYS_IOCTL, %rax                    // rax = syscall number
    mov armor_fd@GOTPCREL(%rip), %rdi
    mov (%rdi), %rdi                        // rdi = 1st argument: armor_fd
    mov $ARMOR_IOC_LIB_ENTER, %rsi          // rsi = 2nd argument: ARMOR_IOC_LIB_ENTER
    mov 0x38(%rsp), %rdx                   // rdx = 3rd argument: return address of library function
    
    // Make the syscall
    syscall

    /* We are now ready to invoke the original library function. First restore
     * the registers... */
    pop %r9
    pop %r8
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi
    // ...and setup the stack frame correctly... 
    pop %r10 // return address of armor_lib_enter, the target of our call
    pop %r11 // return address of library function, saved by the kernel already

    call *%r10

armor_lib_function_returned:
    
    /* The library call finished. We need to make another ioctl to our module so
     * that a) the module can enable the LBR feature again, and b) it can tell
     * us where we need to return to.
     * We pass the kernel a pointer to where we expect the return address so
     * that our module can write the address to the correct location on the
     * stack directly. This not only saves us some instructions, we also don't
     * have to rely on the signed long return value of the ioctl() syscall:
     * experiments showed that the kernel basically converts this into an 32 bit
     * value, which is sometimes not sufficient to hold a return address.
     */
    push $0xdead  // dummy value on the stack to hold the return address
    push %rdi   // store the registers
    push %rsi
    push %rdx
    push %rcx
    push %r8
    push %r9
    push %rax   // we also need to save the return value of the library call

    // Setup the ARMOR_IOC_LIB_EXIT ioctl
    mov $SYS_IOCTL, %rax            // rax = syscal number
    mov armor_fd@GOTPCREL(%rip), %rdi
    mov (%rdi), %rdi                // rdi = 1st argument: armor_fd  
    mov $ARMOR_IOC_LIB_EXIT, %rsi   // rsi = 2nd argument: ARMOR_IOC_LIB_EXIT
    lea 0x38(%rsp), %rdx            // rdx = 3rd argument: &return_address

    // Make the syscall
    syscall

    // Restore the registers
    pop %rax
    pop %r9
    pop %r8
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi

armor_lib_return:
    // And return!
    ret;
