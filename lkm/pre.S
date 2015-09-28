
.section .text

.global pre_signal_delivered
.global pre_signal_delivered_exit

pre_signal_delivered:
    /* Reserve 5 bytes to patch the original instructions into. Note that this
     * is highly speculative: 5 bytes should be enough due to the presence of
     * the function tracer:
     * 
     * One of the most powerful tracers of Ftrace is the function tracer. It
     * uses the -pg option of gcc to have every function in the kernel call a
     * special function "mcount()". That function must be implemented in
     * assembly because the call does not follow the normal C ABI.
     * - http://lwn.net/Articles/365835/
     *
     * I assume that every kernel function starts with such call to mcount() and
     * that this occupies at most 5 bytes. More NOPs are required if this is not
     * true or if you want to instrument at a different location than function
     * entry.
     */
    nop
    nop
    nop
    nop
    nop

    /* Save some registers. Have a look at the original function prototype:
     *
     * < 3.14: void signal_delivered(int sig, siginfo_t *info, struct k_sigaction *ka, 
     *                                 struct pt_regs *regs, int stepping)
     *
     * > 3.14: static void signal_delivered(struct ksignal *ksig, int stepping) 
     *
     * This *should* mean that saving the first five argument registers is
     * enough. Since we will make a call here, I don't think that we have to
     * worry about callee-save registers. Called function should do that for us.
     */
    push %rdi // arg1
    push %rsi // arg2
    push %rdx // arg3
    push %rcx // arg4
    push %r8  // arg5

    /* Functionality goes here. */
    call new_signal_delivered 

    /* Restore registers. */
    pop %r8
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi

pre_signal_delivered_exit:
    /* Reserve 5 bytes to patch a relative jump back to the original function. */
    nop
    nop
    nop
    nop
    nop

