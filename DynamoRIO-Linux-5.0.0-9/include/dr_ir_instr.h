/* **********************************************************
 * Copyright (c) 2010-2011 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _DR_IR_INSTR_H_
#define _DR_IR_INSTR_H_ 1


/** Returns true iff \p instr can be encoding as a valid IA-32 instruction. */
bool
instr_is_encoding_possible(instr_t *instr);

/**
 * Encodes \p instr into the memory at \p pc.
 * Uses the x86/x64 mode stored in instr, not the mode of the current thread.
 * Returns the pc after the encoded instr, or NULL if the encoding failed.
 * If instr is a cti with an instr_t target, the note fields of instr and
 * of the target must be set with the respective offsets of each instr_t!
 * (instrlist_encode does this automatically, if the target is in the list).
 * x86 instructions can occupy up to 17 bytes, so the caller should ensure
 * the target location has enough room to avoid overflow.
 */
byte *
instr_encode(void *drcontext, instr_t *instr, byte *pc);

/**
 * Encodes \p instr into the memory at \p copy_pc in preparation for copying
 * to \p final_pc.  Any pc-relative component is encoded as though the
 * instruction were located at \p final_pc.  This allows for direct copying
 * of the encoded bytes to \p final_pc without re-relativization.
 *
 * Uses the x86/x64 mode stored in instr, not the mode of the current thread.
 * Returns the pc after the encoded instr, or NULL if the encoding failed.
 * If instr is a cti with an instr_t target, the note fields of instr and
 * of the target must be set with the respective offsets of each instr_t!
 * (instrlist_encode does this automatically, if the target is in the list).
 * x86 instructions can occupy up to 17 bytes, so the caller should ensure
 * the target location has enough room to avoid overflow.
 */
byte *
instr_encode_to_copy(void *drcontext, instr_t *instr, byte *copy_pc,
                     byte *final_pc);
#ifdef DR_FAST_IR
/* flags */
enum {

    INSTR_DO_NOT_MANGLE         = 0x00200000,

};
#endif /* DR_FAST_IR */

/**
 * Data slots available in a label (instr_create_label()) instruction
 * for storing client-controlled data.  Accessible via
 * instr_get_label_data_area().
 */
typedef struct _dr_instr_label_data_t {
    ptr_uint_t data[4]; /**< Generic fields for storing user-controlled data */
} dr_instr_label_data_t;

#ifdef DR_FAST_IR


/**
 * instr_t type exposed for optional "fast IR" access.  Note that DynamoRIO
 * reserves the right to change this structure across releases and does
 * not guarantee binary or source compatibility when this structure's fields
 * are directly accessed.  If the instr_ accessor routines are used, DynamoRIO does
 * guarantee source compatibility, but not binary compatibility.  If binary
 * compatibility is desired, do not use the fast IR feature.
 */
struct _instr_t {
    /* flags contains the constants defined above */
    uint    flags;

    /* raw bits of length length are pointed to by the bytes field */
    byte    *bytes;
    uint    length;

    /* translation target for this instr */
    app_pc  translation;

    uint    opcode;

#ifdef X64
    /* PR 251479: offset into instr's raw bytes of rip-relative 4-byte displacement */
    byte    rip_rel_pos;
#endif

    /* we dynamically allocate dst and src arrays b/c x86 instrs can have
     * up to 8 of each of them, but most have <=2 dsts and <=3 srcs, and we
     * use this struct for un-decoded instrs too
     */
    byte    num_dsts;
    byte    num_srcs;

    union {
        struct {
            /* for efficiency everyone has a 1st src opnd, since we often just
             * decode jumps, which all have a single source (==target)
             * yes this is an extra 10 bytes, but the whole struct is still < 64 bytes!
             */
            opnd_t    src0;
            opnd_t    *srcs; /* this array has 2nd src and beyond */
            opnd_t    *dsts;
        };
        dr_instr_label_data_t label_data;
    };

    uint    prefixes; /* data size, addr size, or lock prefix info */
    uint    eflags;   /* contains EFLAGS_ bits, but amount of info varies
                       * depending on how instr was decoded/built */

    /* this field is for the use of passes as an annotation.
     * it is also used to hold the offset of an instruction when encoding
     * pc-relative instructions. A small range of values is reserved for internal use
     * by DR and cannot be used by clients; see DR_NOTE_FIRST_RESERVED in globals.h.
     */
    void *note;

    /* fields for building instructions into instruction lists */
    instr_t   *prev;
    instr_t   *next;

}; /* instr_t */
#endif /* DR_FAST_IR */

/****************************************************************************
 * INSTR ROUTINES
 */
/**
 * @file dr_ir_instr.h
 * @brief Functions to create and manipulate instructions.
 */



/**
 * Returns an initialized instr_t allocated on the thread-local heap.
 * Sets the x86/x64 mode of the returned instr_t to the mode of dcontext.
 */
/* For -x86_to_x64, sets the mode of the instr to the code cache mode instead of
the app mode. */
instr_t*
instr_create(void *drcontext);

/** Initializes \p instr.
 * Sets the x86/x64 mode of \p instr to the mode of dcontext.
 */
void
instr_init(void *drcontext, instr_t *instr);

/**
 * Deallocates all memory that was allocated by \p instr.  This
 * includes raw bytes allocated by instr_allocate_raw_bits() and
 * operands allocated by instr_set_num_opnds().  Does not deallocate
 * the storage for \p instr itself.
 */
void
instr_free(void *drcontext, instr_t *instr);

/**
 * Performs both instr_free() and instr_init().
 * \p instr must have been initialized.
 */
void
instr_reset(void *drcontext, instr_t *instr);

/**
 * Frees all dynamically allocated storage that was allocated by \p instr,
 * except for allocated bits.
 * Also zeroes out \p instr's fields, except for raw bit fields,
 * whether \p instr is instr_is_meta(), and the x86 mode of \p instr.
 * \p instr must have been initialized.
 */
void
instr_reuse(void *drcontext, instr_t *instr);

/**
 * Performs instr_free() and then deallocates the thread-local heap
 * storage for \p instr.
 */
void
instr_destroy(void *drcontext, instr_t *instr);

INSTR_INLINE
/**
 * Returns the next instr_t in the instrlist_t that contains \p instr.
 * \note The next pointer for an instr_t is inside the instr_t data
 * structure itself, making it impossible to have on instr_t in
 * two different InstrLists (but removing the need for an extra data
 * structure for each element of the instrlist_t).
 */
instr_t*
instr_get_next(instr_t *instr);

INSTR_INLINE
/**
 * Returns the next application (non-meta) instruction in the instruction list
 * that contains \p instr.
 *
 * \note As opposed to instr_get_next(), this routine skips all meta
 * instructions inserted by either DynamoRIO or its clients.
 *
 * \note We do recommend using this routine during the phase of application
 * code analysis, as any meta instructions present are guaranteed to be ok
 * to skip.
 * However, the caution should be exercised if using this routine after any
 * instrumentation insertion has already happened, as instrumentation might
 * affect register usage or other factors being analyzed.
 */
instr_t *
instr_get_next_app(instr_t *instr);

INSTR_INLINE
/** Returns the previous instr_t in the instrlist_t that contains \p instr. */
instr_t*
instr_get_prev(instr_t *instr);

INSTR_INLINE
/** Sets the next field of \p instr to point to \p next. */
void
instr_set_next(instr_t *instr, instr_t *next);

INSTR_INLINE
/** Sets the prev field of \p instr to point to \p prev. */
void
instr_set_prev(instr_t *instr, instr_t *prev);

INSTR_INLINE
/**
 * Gets the value of the user-controlled note field in \p instr.
 * \note Important: is also used when emitting for targets that are other
 * instructions.  Thus it will be overwritten when calling instrlist_encode()
 * or instrlist_encode_to_copy() with \p has_instr_jmp_targets set to true.
 * \note The note field is copied (shallowly) by instr_clone().
 */
void *
instr_get_note(instr_t *instr);

INSTR_INLINE
/** Sets the user-controlled note field in \p instr to \p value. */
void
instr_set_note(instr_t *instr, void *value);

/** Return the taken target pc of the (direct branch) instruction. */
app_pc
instr_get_branch_target_pc(instr_t *cti_instr);

/** Set the taken target pc of the (direct branch) instruction. */
void
instr_set_branch_target_pc(instr_t *cti_instr, app_pc pc);

/**
 * Returns true iff \p instr is a conditional branch, unconditional branch,
 * or indirect branch with a program address target (NOT an instr_t address target)
 * and \p instr is ok to mangle.
 */
bool
instr_is_exit_cti(instr_t *instr);

/** Return true iff \p instr's opcode is OP_int, OP_into, or OP_int3. */
bool
instr_is_interrupt(instr_t *instr);

INSTR_INLINE
/**
 * Return true iff \p instr is an application (non-meta) instruction
 * (see instr_set_app() for more information).
 */
bool
instr_is_app(instr_t *instr);

/**
 * Sets \p instr as an application (non-meta) instruction.
 * An application instruction might be mangled by DR if necessary,
 * e.g., to create an exit stub for a branch instruction.
 * All application instructions that are added to basic blocks or
 * traces should have their translation fields set (via
 * #instr_set_translation()).
 */
void
instr_set_app(instr_t *instr);

INSTR_INLINE
/**
 * Return true iff \p instr is a meta instruction
 * (see instr_set_meta() for more information).
 */
bool
instr_is_meta(instr_t *instr);

/**
 * Sets \p instr as a meta instruction.
 * A meta instruction will not be mangled by DR in any way, which is necessary
 * to have DR not create an exit stub for a branch instruction.
 * Meta instructions should not fault (unless such faults are handled
 * by the client) and are not considered
 * application instructions but rather added instrumentation code (see
 * #dr_register_bb_event() for further information).
 */
void
instr_set_meta(instr_t *instr);

INSTR_INLINE
/**
 * Return true iff \p instr is not a meta-instruction
 * (see instr_set_app() for more information).
 *
 * \deprecated instr_is_app()/instr_is_meta() should be used instead.
 */
bool
instr_ok_to_mangle(instr_t *instr);

/**
 * Sets \p instr to "ok to mangle" if \p val is true and "not ok to
 * mangle" if \p val is false.
 *
 * \deprecated instr_set_app()/instr_set_meta() should be used instead.
 */
void
instr_set_ok_to_mangle(instr_t *instr, bool val);

/**
 * A convenience routine that calls both
 * #instr_set_meta (instr) and
 * #instr_set_translation (instr, NULL).
 */
void
instr_set_meta_no_translation(instr_t *instr);

/** Return true iff \p instr is to be emitted into the cache. */
bool
instr_ok_to_emit(instr_t *instr);

/**
 * Set \p instr to "ok to emit" if \p val is true and "not ok to emit"
 * if \p val is false.  An instruction that should not be emitted is
 * treated normally by DR for purposes of exits but is not placed into
 * the cache.  It is used for final jumps that are to be elided.
 */
void
instr_set_ok_to_emit(instr_t *instr, bool val);

/**
 * Returns the length of \p instr.
 * As a side effect, if instr_is_app(instr) and \p instr's raw bits
 * are invalid, encodes \p instr into bytes allocated with
 * instr_allocate_raw_bits(), after which instr is marked as having
 * valid raw bits.
 */
int
instr_length(void *drcontext, instr_t *instr);

/** Returns number of bytes of heap used by \p instr. */
int
instr_mem_usage(instr_t *instr);

/**
 * Returns a copy of \p orig with separately allocated memory for
 * operands and raw bytes if they were present in \p orig.
 * Only a shallow copy of the \p note field is made.
 */
instr_t *
instr_clone(void *drcontext, instr_t *orig);

/**
 * Convenience routine: calls
 * - instr_create(dcontext)
 * - instr_set_opcode(opcode)
 * - instr_set_num_opnds(dcontext, instr, num_dsts, num_srcs)
 *
 * and returns the resulting instr_t.
 */
instr_t *
instr_build(void *drcontext, int opcode, int num_dsts, int num_srcs);

/**
 * Convenience routine: calls
 * - instr_create(dcontext)
 * - instr_set_opcode(instr, opcode)
 * - instr_allocate_raw_bits(dcontext, instr, num_bytes)
 *
 * and returns the resulting instr_t.
 */
instr_t *
instr_build_bits(void *drcontext, int opcode, uint num_bytes);

/**
 * Returns true iff \p instr's opcode is NOT OP_INVALID.
 * Not to be confused with an invalid opcode, which can be OP_INVALID or
 * OP_UNDECODED.  OP_INVALID means an instruction with no valid fields:
 * raw bits (may exist but do not correspond to a valid instr), opcode,
 * eflags, or operands.  It could be an uninitialized
 * instruction or the result of decoding an invalid sequence of bytes.
 */
bool
instr_valid(instr_t *instr);

/** Get the original application PC of \p instr if it exists. */
app_pc
instr_get_app_pc(instr_t *instr);

/** Returns \p instr's opcode (an OP_ constant). */
int
instr_get_opcode(instr_t *instr);

/** Assumes \p opcode is an OP_ constant and sets it to be instr's opcode. */
void
instr_set_opcode(instr_t *instr, int opcode);

INSTR_INLINE
/**
 * Returns the number of source operands of \p instr.
 *
 * \note Addressing registers used in destination memory references
 * (i.e., base, index, or segment registers) are not separately listed
 * as source operands.
 */
int
instr_num_srcs(instr_t *instr);

INSTR_INLINE
/**
 * Returns the number of destination operands of \p instr.
 */
int
instr_num_dsts(instr_t *instr);

/**
 * Assumes that \p instr has been initialized but does not have any
 * operands yet.  Allocates storage for \p num_srcs source operands
 * and \p num_dsts destination operands.
 */
void
instr_set_num_opnds(void *drcontext, instr_t *instr, int num_dsts, int num_srcs);

/**
 * Returns \p instr's source operand at position \p pos (0-based).
 */
opnd_t
instr_get_src(instr_t *instr, uint pos);

/**
 * Returns \p instr's destination operand at position \p pos (0-based).
 */
opnd_t
instr_get_dst(instr_t *instr, uint pos);

/**
 * Sets \p instr's source operand at position \p pos to be \p opnd.
 * Also calls instr_set_raw_bits_valid(\p instr, false) and
 * instr_set_operands_valid(\p instr, true).
 */
void
instr_set_src(instr_t *instr, uint pos, opnd_t opnd);

/**
 * Sets \p instr's destination operand at position \p pos to be \p opnd.
 * Also calls instr_set_raw_bits_valid(\p instr, false) and
 * instr_set_operands_valid(\p instr, true).
 */
void
instr_set_dst(instr_t *instr, uint pos, opnd_t opnd);

/**
 * Assumes that \p cti_instr is a control transfer instruction
 * Returns the first source operand of \p cti_instr (its target).
 */
opnd_t
instr_get_target(instr_t *cti_instr);

/**
 * Assumes that \p cti_instr is a control transfer instruction.
 * Sets the first source operand of \p cti_instr to be \p target.
 * Also calls instr_set_raw_bits_valid(\p instr, false) and
 * instr_set_operands_valid(\p instr, true).
 */
void
instr_set_target(instr_t *cti_instr, opnd_t target);

/** Returns true iff \p instr's operands are up to date. */
bool
instr_operands_valid(instr_t *instr);

/** Sets \p instr's operands to be valid if \p valid is true, invalid otherwise. */
void
instr_set_operands_valid(instr_t *instr, bool valid);

/**
 * Returns true iff \p instr's opcode is valid.
 * If the opcode is ever set to other than OP_INVALID or OP_UNDECODED it is assumed
 * to be valid.  However, calling instr_get_opcode() will attempt to
 * decode a valid opcode, hence the purpose of this routine.
 */
bool
instr_opcode_valid(instr_t *instr);

/** Returns \p instr's eflags use as EFLAGS_ constants or'ed together. */
uint
instr_get_eflags(instr_t *instr);

/** Returns the eflags usage of instructions with opcode \p opcode,
 * as EFLAGS_ constants or'ed together.
 */
uint
instr_get_opcode_eflags(int opcode);

/**
 * Returns \p instr's arithmetic flags (bottom 6 eflags) use
 * as EFLAGS_ constants or'ed together.
 * If \p instr's eflags behavior has not been calculated yet or is
 * invalid, the entire eflags use is calculated and returned (not
 * just the arithmetic flags).
 */
uint
instr_get_arith_flags(instr_t *instr);

/**
 * Assumes that \p instr does not currently have any raw bits allocated.
 * Sets \p instr's raw bits to be \p length bytes starting at \p addr.
 * Does not set the operands invalid.
 */
void
instr_set_raw_bits(instr_t *instr, byte * addr, uint length);

/** Sets \p instr's raw bits to be valid if \p valid is true, invalid otherwise. */
void
instr_set_raw_bits_valid(instr_t *instr, bool valid);

/** Returns true iff \p instr's raw bits are a valid encoding of instr. */
bool
instr_raw_bits_valid(instr_t *instr);

/** Returns true iff \p instr has its own allocated memory for raw bits. */
bool
instr_has_allocated_bits(instr_t *instr);

/** Returns true iff \p instr's raw bits are not a valid encoding of \p instr. */
bool
instr_needs_encoding(instr_t *instr);

/**
 * Return true iff \p instr is not a meta-instruction that can fault
 * (see instr_set_meta_may_fault() for more information).
 *
 * \deprecated Any meta instruction can fault if it has a non-NULL
 * translation field and the client fully handles all of its faults,
 * so this routine is no longer needed.
 */
bool
instr_is_meta_may_fault(instr_t *instr);

/**
 * \deprecated Any meta instruction can fault if it has a non-NULL
 * translation field and the client fully handles all of its faults,
 * so this routine is no longer needed.
 */
void
instr_set_meta_may_fault(instr_t *instr, bool val);

/**
 * Allocates \p num_bytes of memory for \p instr's raw bits.
 * If \p instr currently points to raw bits, the allocated memory is
 * initialized with the bytes pointed to.
 * \p instr is then set to point to the allocated memory.
 */
void
instr_allocate_raw_bits(void *drcontext, instr_t *instr, uint num_bytes);

/**
 * Sets the translation pointer for \p instr, used to recreate the
 * application address corresponding to this instruction.  When adding
 * or modifying instructions that are to be considered application
 * instructions (i.e., non meta-instructions: see #instr_is_app),
 * the translation should always be set.  Pick
 * the application address that if executed will be equivalent to
 * restarting \p instr.  Currently the translation address must lie
 * within the existing bounds of the containing code block.
 * Returns the supplied \p instr (for easy chaining).  Use
 * #instr_get_app_pc to see the current value of the translation.
 */
instr_t *
instr_set_translation(instr_t *instr, app_pc addr);

/**
 * Calling this function with \p instr makes it safe to keep the
 * instruction around indefinitely when its raw bits point into the
 * cache.  The function allocates memory local to \p instr to hold a
 * copy of the raw bits. If this was not done, the original raw bits
 * could be deleted at some point.  Making an instruction persistent
 * is necessary if you want to keep it beyond returning from the call
 * that produced the instruction.
 */
void
instr_make_persistent(void *drcontext, instr_t *instr);

/**
 * Assumes that \p instr's raw bits are valid.
 * Returns a pointer to \p instr's raw bits.
 * \note A freshly-decoded instruction has valid raw bits that point to the
 * address from which it was decoded.  However, for instructions presented
 * in the basic block or trace events, use instr_get_app_pc() to retrieve
 * the corresponding application address, as the raw bits will not be set
 * for instructions added after decoding, and may point to a different location
 * for insructions that have been modified.
 */
byte *
instr_get_raw_bits(instr_t *instr);

/** If \p instr has raw bits allocated, frees them. */
void
instr_free_raw_bits(void *drcontext, instr_t *instr);

/**
 * Assumes that \p instr's raw bits are valid and have > \p pos bytes.
 * Returns a pointer to \p instr's raw byte at position \p pos (beginning with 0).
 */
byte
instr_get_raw_byte(instr_t *instr, uint pos);

/**
 * Assumes that \p instr's raw bits are valid and allocated by \p instr
 * and have > \p pos bytes.
 * Sets instr's raw byte at position \p pos (beginning with 0) to the value \p byte.
 */
void
instr_set_raw_byte(instr_t *instr, uint pos, byte byte);

/**
 * Assumes that \p instr's raw bits are valid and allocated by \p instr
 * and have >= num_bytes bytes.
 * Copies the \p num_bytes beginning at start to \p instr's raw bits.
 */
void
instr_set_raw_bytes(instr_t *instr, byte *start, uint num_bytes);

/**
 * Assumes that \p instr's raw bits are valid and allocated by \p instr
 * and have > pos+3 bytes.
 * Sets the 4 bytes beginning at position \p pos (0-based) to the value word.
 */
void
instr_set_raw_word(instr_t *instr, uint pos, uint word);

/**
 * Assumes that \p instr's raw bits are valid and have > \p pos + 3 bytes.
 * Returns the 4 bytes beginning at position \p pos (0-based).
 */
uint
instr_get_raw_word(instr_t *instr, uint pos);

/**
 * Assumes that \p prefix is a PREFIX_ constant.
 * Ors \p instr's prefixes with \p prefix.
 * Returns the supplied instr (for easy chaining).
 */
instr_t *
instr_set_prefix_flag(instr_t *instr, uint prefix);

/**
 * Assumes that \p prefix is a PREFIX_ constant.
 * Returns true if \p instr's prefixes contain the flag \p prefix.
 */
bool
instr_get_prefix_flag(instr_t *instr, uint prefix);
#ifdef X64


/**
 * Each instruction stores whether it should be interpreted in 32-bit
 * (x86) or 64-bit (x64) mode.  This routine sets the mode for \p instr.
 *
 * \note For 64-bit DR builds only.
 */
void
instr_set_x86_mode(instr_t *instr, bool x86);

/**
 * Returns true if \p instr is an x86 instruction (32-bit) and false
 * if \p instr is an x64 instruction (64-bit).
 *
 * \note For 64-bit DR builds only.
 */
bool
instr_get_x86_mode(instr_t *instr);
#endif


/**
 * Shrinks all registers not used as addresses, and all immed integer and
 * address sizes, to 16 bits.
 * Does not shrink DR_REG_ESI or DR_REG_EDI used in string instructions.
 */
void
instr_shrink_to_16_bits(instr_t *instr);
#ifdef X64


/**
 * Shrinks all registers, including addresses, and all immed integer and
 * address sizes, to 32 bits.
 *
 * \note For 64-bit DR builds only.
 */
void
instr_shrink_to_32_bits(instr_t *instr);
#endif


/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's operands references a
 * register that overlaps \p reg.
 *
 * Returns false for multi-byte nops with an operand using reg.
 */
bool
instr_uses_reg(instr_t *instr, reg_id_t reg);

/**
 * Returns true iff at least one of \p instr's operands references a floating
 * point register.
 */
bool
instr_uses_fp_reg(instr_t *instr);

/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's source operands references \p reg.
 *
 * Returns false for multi-byte nops with a source operand using reg.
 *
 * \note Use instr_reads_from_reg() to also consider addressing
 * registers in destination operands.
 */
bool
instr_reg_in_src(instr_t *instr, reg_id_t reg);

/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's destination operands references \p reg.
 */
bool
instr_reg_in_dst(instr_t *instr, reg_id_t reg);

/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's destination operands is
 * a register operand for a register that overlaps \p reg.
 */
bool
instr_writes_to_reg(instr_t *instr, reg_id_t reg);

/**
 * Assumes that reg is a DR_REG_ constant.
 * Returns true iff at least one of instr's operands reads
 * from a register that overlaps reg (checks both source operands
 * and addressing registers used in destination operands).
 *
 * Returns false for multi-byte nops with an operand using reg.
 */
bool
instr_reads_from_reg(instr_t *instr, reg_id_t reg);

/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's destination operands is
 * the same register (not enough to just overlap) as \p reg.
 */
bool
instr_writes_to_exact_reg(instr_t *instr, reg_id_t reg);

/**
 * Replaces all instances of \p old_opnd in \p instr's source operands with
 * \p new_opnd (uses opnd_same() to detect sameness).
 */
bool
instr_replace_src_opnd(instr_t *instr, opnd_t old_opnd, opnd_t new_opnd);

/**
 * Returns true iff \p instr1 and \p instr2 have the same opcode, prefixes,
 * and source and destination operands (uses opnd_same() to compare the operands).
 */
bool
instr_same(instr_t *instr1, instr_t *instr2);

/**
 * Returns true iff any of \p instr's source operands is a memory reference.
 *
 * Unlike opnd_is_memory_reference(), this routine conisders the
 * semantics of the instruction and returns false for both multi-byte
 * nops with a memory operand and for the #OP_lea instruction, as they
 * do not really reference the memory.  It does return true for
 * prefetch instructions.
 */
bool
instr_reads_memory(instr_t *instr);

/** Returns true iff any of \p instr's destination operands is a memory reference. */
bool
instr_writes_memory(instr_t *instr);

/**
 * Returns true iff \p instr writes to an xmm register and zeroes the top half
 * of the corresponding ymm register as a result (some instructions preserve
 * the top half while others zero it when writing to the bottom half).
 */
bool
instr_zeroes_ymmh(instr_t *instr);
#ifdef X64


/**
 * Returns true iff any of \p instr's operands is a rip-relative memory reference.
 *
 * \note For 64-bit DR builds only.
 */
bool
instr_has_rel_addr_reference(instr_t *instr);

/**
 * If any of \p instr's operands is a rip-relative memory reference, returns the
 * address that reference targets.  Else returns false.
 *
 * \note For 64-bit DR builds only.
 */
bool
instr_get_rel_addr_target(instr_t *instr, /*OUT*/ app_pc *target);

/**
 * If any of \p instr's destination operands is a rip-relative memory
 * reference, returns the operand position.  If there is no such
 * destination operand, returns -1.
 *
 * \note For 64-bit DR builds only.
 */
int
instr_get_rel_addr_dst_idx(instr_t *instr);

/**
 * If any of \p instr's source operands is a rip-relative memory
 * reference, returns the operand position.  If there is no such
 * source operand, returns -1.
 *
 * \note For 64-bit DR builds only.
 */
int
instr_get_rel_addr_src_idx(instr_t *instr);
#endif /* X64 */


/**
 * Returns NULL if none of \p instr's operands is a memory reference.
 * Otherwise, returns the effective address of the first memory operand
 * when the operands are considered in this order: destinations and then
 * sources.  The address is computed using the passed-in registers.
 * \p mc->flags must include DR_MC_CONTROL and DR_MC_INTEGER.
 * For instructions that use vector addressing (VSIB, introduced in AVX2),
 * mc->flags must additionally include DR_MC_MULTIMEDIA.
 *
 * Like instr_reads_memory(), this routine does not consider
 * multi-byte nops that use addressing operands, or the #OP_lea
 * instruction's source operand, to be memory references.
 */
app_pc
instr_compute_address(instr_t *instr, dr_mcontext_t *mc);

/**
 * Performs address calculation in the same manner as
 * instr_compute_address() but handles multiple memory operands.  The
 * \p index parameter should be initially set to 0 and then
 * incremented with each successive call until this routine returns
 * false, which indicates that there are no more memory operands.  The
 * address of each is computed in the same manner as
 * instr_compute_address() and returned in \p addr; whether it is a
 * write is returned in \p is_write.  Either or both OUT variables can
 * be NULL.
 * \p mc->flags must include DR_MC_CONTROL and DR_MC_INTEGER.
 * For instructions that use vector addressing (VSIB, introduced in AVX2),
 * mc->flags must additionally include DR_MC_MULTIMEDIA.
 *
 * Like instr_reads_memory(), this routine does not consider
 * multi-byte nops that use addressing operands, or the #OP_lea
 * instruction's source operand, to be memory references.
 */
bool
instr_compute_address_ex(instr_t *instr, dr_mcontext_t *mc, uint index,
                         OUT app_pc *addr, OUT bool *write);

/**
 * Performs address calculation in the same manner as
 * instr_compute_address_ex() with additional information
 * of which opnd is used for address computation returned
 * in \p pos. If \p pos is NULL, it is the same as
 * instr_compute_address_ex().
 *
 * Like instr_reads_memory(), this routine does not consider
 * multi-byte nops that use addressing operands, or the #OP_lea
 * instruction's source operand, to be memory references.
 */
bool
instr_compute_address_ex_pos(instr_t *instr, dr_mcontext_t *mc, uint index,
                             OUT app_pc *addr, OUT bool *is_write,
                             OUT uint *pos);

/**
 * Calculates the size, in bytes, of the memory read or write of \p instr.
 * If \p instr does not reference memory, or is invalid, returns 0.
 * If \p instr is a repeated string instruction, considers only one iteration.
 * If \p instr uses vector addressing (VSIB, introduced in AVX2), considers
 * only the size of each separate memory access.
 */
uint
instr_memory_reference_size(instr_t *instr);

/**
 * \return a pointer to user-controlled data fields in a label instruction.
 * These fields are available for use by clients for their own purposes.
 * Returns NULL if \p instr is not a label instruction.
 * \note These data fields are copied (shallowly) across instr_clone().
 */
dr_instr_label_data_t *
instr_get_label_data_area(instr_t *instr);

/**
 * Returns true iff \p instr is an IA-32/AMD64 "mov" instruction: either OP_mov_st,
 * OP_mov_ld, OP_mov_imm, OP_mov_seg, or OP_mov_priv.
 */
bool
instr_is_mov(instr_t *instr);

/**
 * Returns true iff \p instr's opcode is OP_call, OP_call_far, OP_call_ind,
 * or OP_call_far_ind.
 */
bool
instr_is_call(instr_t *instr);

/** Returns true iff \p instr's opcode is OP_call or OP_call_far. */
bool
instr_is_call_direct(instr_t *instr);

/** Returns true iff \p instr's opcode is OP_call. */
bool
instr_is_near_call_direct(instr_t *instr);

/** Returns true iff \p instr's opcode is OP_call_ind or OP_call_far_ind. */
bool
instr_is_call_indirect(instr_t *instr);

/** Returns true iff \p instr's opcode is OP_ret, OP_ret_far, or OP_iret. */
bool
instr_is_return(instr_t *instr);

/**
 * Returns true iff \p instr is a control transfer instruction of any kind
 * This includes OP_jcc, OP_jcc_short, OP_loop*, OP_jecxz, OP_call*, and OP_jmp*.
 */
bool
instr_is_cti(instr_t *instr);

/**
 * Returns true iff \p instr is a control transfer instruction that takes an
 * 8-bit offset: OP_loop*, OP_jecxz, OP_jmp_short, or OP_jcc_short
 */
bool
instr_is_cti_short(instr_t *instr);

/** Returns true iff \p instr is one of OP_loop* or OP_jecxz. */
bool
instr_is_cti_loop(instr_t *instr);

/**
 * Returns true iff \p instr's opcode is OP_loop* or OP_jecxz and instr has
 * been transformed to a sequence of instruction that will allow a 32-bit
 * offset.
 * If \p pc != NULL, \p pc is expected to point the the beginning of the encoding of
 * \p instr, and the following instructions are assumed to be encoded in sequence
 * after \p instr.
 * Otherwise, the encoding is expected to be found in \p instr's allocated bits.
 */
bool
instr_is_cti_short_rewrite(instr_t *instr, byte *pc);

/**
 * Returns true iff \p instr is a conditional branch: OP_jcc, OP_jcc_short,
 * OP_loop*, or OP_jecxz.
 */
bool
instr_is_cbr(instr_t *instr);

/**
 * Returns true iff \p instr is a multi-way (indirect) branch: OP_jmp_ind,
 * OP_call_ind, OP_ret, OP_jmp_far_ind, OP_call_far_ind, OP_ret_far, or
 * OP_iret.
 */
bool
instr_is_mbr(instr_t *instr);

/**
 * Returns true iff \p instr is an unconditional direct branch: OP_jmp,
 * OP_jmp_short, or OP_jmp_far.
 */
bool
instr_is_ubr(instr_t *instr);

/**
 * Returns true iff \p instr is a near unconditional direct branch: OP_jmp,
 * or OP_jmp_short.
 */
bool
instr_is_near_ubr(instr_t *instr);

/**
 * Returns true iff \p instr is a far control transfer instruction: OP_jmp_far,
 * OP_call_far, OP_jmp_far_ind, OP_call_far_ind, OP_ret_far, or OP_iret.
 */
bool
instr_is_far_cti(instr_t *instr);

/** Returns true if \p instr is an absolute call or jmp that is far. */
bool
instr_is_far_abs_cti(instr_t *instr);

/**
 * Returns true iff \p instr is used to implement system calls: OP_int with a
 * source operand of 0x80 on linux or 0x2e on windows, or OP_sysenter,
 * or OP_syscall, or #instr_is_wow64_syscall() for WOW64.
 */
bool
instr_is_syscall(instr_t *instr);
#ifdef WINDOWS


/**
 * Returns true iff \p instr is the indirect transfer from the 32-bit
 * ntdll.dll to the wow64 system call emulation layer.  This
 * instruction will also return true for instr_is_syscall, as well as
 * appear as an indirect call, so clients modifying indirect calls may
 * want to avoid modifying this type.
 *
 * \note Windows-only
 */
bool
instr_is_wow64_syscall(instr_t *instr);
#endif


/**
 * Returns true iff \p instr is a prefetch instruction: OP_prefetchnta,
 * OP_prefetchnt0, OP_prefetchnt1, OP_prefetchnt2, OP_prefetch, or
 * OP_prefetchw.
 */
bool
instr_is_prefetch(instr_t *instr);

/**
 * Tries to identify common cases of moving a constant into either a
 * register or a memory address.
 * Returns true and sets \p *value to the constant being moved for the following
 * cases: mov_imm, mov_st, and xor where the source equals the destination.
 */
bool
instr_is_mov_constant(instr_t *instr, ptr_int_t *value);

/** Returns true iff \p instr is a floating point instruction. */
bool
instr_is_floating(instr_t *instr);
/**
 * Indicates which type of floating-point operation and instruction performs.
 */
typedef enum {
    DR_FP_STATE,   /**< Loads, stores, or queries general floating point state. */
    DR_FP_MOVE,    /**< Moves floating point values from one location to another. */
    DR_FP_CONVERT, /**< Converts to or from floating point values. */
    DR_FP_MATH,    /**< Performs arithmetic or conditional operations. */
} dr_fp_type_t;


/**
 * Returns true iff \p instr is a floating point instruction.
 * @param[in] instr  The instruction to query
 * @param[out] type  If the return value is true and \p type is
 *   non-NULL, the type of the floating point operation is written to \p type.
 */
bool
instr_is_floating_ex(instr_t *instr, dr_fp_type_t *type);

/** Returns true iff \p instr is part of Intel's MMX instructions. */
bool
instr_is_mmx(instr_t *instr);

/** Returns true iff \p instr is part of Intel's SSE or SSE2 instructions. */
bool
instr_is_sse_or_sse2(instr_t *instr);

/** Returns true iff \p instr is a "mov $imm -> (%esp)". */
bool
instr_is_mov_imm_to_tos(instr_t *instr);

/** Returns true iff \p instr is a label meta-instruction. */
bool
instr_is_label(instr_t *instr);

/** Returns true iff \p instr is an "undefined" instruction (ud2) */
bool
instr_is_undefined(instr_t *instr);

/**
 * Assumes that \p instr's opcode is OP_int and that either \p instr's
 * operands or its raw bits are valid.
 * Returns the first source operand if \p instr's operands are valid,
 * else if \p instr's raw bits are valid returns the first raw byte.
 */
int
instr_get_interrupt_number(instr_t *instr);

/**
 * Assumes that \p instr is a conditional branch instruction
 * Reverses the logic of \p instr's conditional
 * e.g., changes OP_jb to OP_jnb.
 * Works on cti_short_rewrite as well.
 */
void
instr_invert_cbr(instr_t *instr);

/**
 * Assumes that instr is a meta instruction (instr_is_meta())
 * and an instr_is_cti_short() (8-bit reach). Converts instr's opcode
 * to a long form (32-bit reach).  If instr's opcode is OP_loop* or
 * OP_jecxz, converts it to a sequence of multiple instructions (which
 * is different from instr_is_cti_short_rewrite()).  Each added instruction
 * is marked instr_is_meta().
 * Returns the long form of the instruction, which is identical to \p instr
 * unless \p instr is OP_loop* or OP_jecxz, in which case the return value
 * is the final instruction in the sequence, the one that has long reach.
 * \note DR automatically converts app short ctis to long form.
 */
instr_t *
instr_convert_short_meta_jmp_to_long(void *drcontext, instrlist_t *ilist,
                                     instr_t *instr);

/**
 * Given \p eflags, returns whether or not the conditional branch, \p
 * instr, would be taken.
 */
bool
instr_jcc_taken(instr_t *instr, reg_t eflags);

/**
 * Converts a cmovcc opcode \p cmovcc_opcode to the OP_jcc opcode that
 * tests the same bits in eflags.
 */
int
instr_cmovcc_to_jcc(int cmovcc_opcode);

/**
 * Given \p eflags, returns whether or not the conditional move
 * instruction \p instr would execute the move.  The conditional move
 * can be an OP_cmovcc or an OP_fcmovcc instruction.
 */
bool
instr_cmovcc_triggered(instr_t *instr, reg_t eflags);

/**
 * Returns true if \p instr is one of a class of common nops.
 * currently checks:
 * - nop
 * - nop reg/mem
 * - xchg reg, reg
 * - mov reg, reg
 * - lea reg, (reg)
 */
bool
instr_is_nop(instr_t *instr);

/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode and no sources or destinations.
 */
instr_t *
instr_create_0dst_0src(void *drcontext, int opcode);

/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode and a single source (\p src).
 */
instr_t *
instr_create_0dst_1src(void *drcontext, int opcode,
                       opnd_t src);

/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode and two sources (\p src1, \p src2).
 */
instr_t *
instr_create_0dst_2src(void *drcontext, int opcode,
                       opnd_t src1, opnd_t src2);

/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode and three sources
 * (\p src1, \p src2, \p src3).
 */
instr_t *
instr_create_0dst_3src(void *drcontext, int opcode,
                       opnd_t src1, opnd_t src2, opnd_t src3);

/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode and one destination (\p dst).
 */
instr_t *
instr_create_1dst_0src(void *drcontext, int opcode,
                       opnd_t dst);

/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, one destination(\p dst),
 * and one source (\p src).
 */
instr_t *
instr_create_1dst_1src(void *drcontext, int opcode,
                       opnd_t dst, opnd_t src);

/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, one destination (\p dst),
 * and two sources (\p src1, \p src2).
 */
instr_t *
instr_create_1dst_2src(void *drcontext, int opcode,
                       opnd_t dst, opnd_t src1, opnd_t src2);

/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, one destination (\p dst),
 * and three sources (\p src1, \p src2, \p src3).
 */
instr_t *
instr_create_1dst_3src(void *drcontext, int opcode,
                       opnd_t dst, opnd_t src1, opnd_t src2, opnd_t src3);

/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, one destination (\p dst),
 * and five sources (\p src1, \p src2, \p src3, \p src4, \p src5).
 */
instr_t *
instr_create_1dst_5src(void *drcontext, int opcode,
                       opnd_t dst, opnd_t src1, opnd_t src2, opnd_t src3,
                       opnd_t src4, opnd_t src5);

/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and no sources.
 */
instr_t *
instr_create_2dst_0src(void *drcontext, int opcode,
                       opnd_t dst1, opnd_t dst2);

/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and one source (\p src).
 */
instr_t *
instr_create_2dst_1src(void *drcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t src);

/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and two sources (\p src1, \p src2).
 */
instr_t *
instr_create_2dst_2src(void *drcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t src1, opnd_t src2);

/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and three sources (\p src1, \p src2, \p src3).
 */
instr_t *
instr_create_2dst_3src(void *drcontext, int opcode,
                       opnd_t dst1, opnd_t dst2,
                       opnd_t src1, opnd_t src2, opnd_t src3);

/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and four sources (\p src1, \p src2, \p src3, \p src4).
 */
instr_t *
instr_create_2dst_4src(void *drcontext, int opcode,
                       opnd_t dst1, opnd_t dst2,
                       opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4);

/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, three destinations
 * (\p dst1, \p dst2, \p dst3) and no sources.
 */
instr_t *
instr_create_3dst_0src(void *drcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t dst3);

/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, three destinations
 * (\p dst1, \p dst2, \p dst3) and three sources
 * (\p src1, \p src2, \p src3).
 */
instr_t *
instr_create_3dst_3src(void *drcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t src1, opnd_t src2, opnd_t src3);

/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, three destinations
 * (\p dst1, \p dst2, \p dst3) and four sources
 * (\p src1, \p src2, \p src3, \p src4).
 */
instr_t *
instr_create_3dst_4src(void *drcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4);

/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, three destinations
 * (\p dst1, \p dst2, \p dst3) and five sources
 * (\p src1, \p src2, \p src3, \p src4, \p src5).
 */
instr_t *
instr_create_3dst_5src(void *drcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t src1, opnd_t src2, opnd_t src3,
                       opnd_t src4, opnd_t src5);

/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, four destinations
 * (\p dst1, \p dst2, \p dst3, \p dst4) and 1 source (\p src).
 */
instr_t *
instr_create_4dst_1src(void *drcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t dst3, opnd_t dst4,
                       opnd_t src);

/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, four destinations
 * (\p dst1, \p dst2, \p dst3, \p dst4) and four sources
 * (\p src1, \p src2, \p src3, \p src4).
 */
instr_t *
instr_create_4dst_4src(void *drcontext, int opcode,
                       opnd_t dst1, opnd_t dst2, opnd_t dst3, opnd_t dst4,
                       opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4);

/** Convenience routine that returns an initialized instr_t for OP_popa. */
instr_t *
instr_create_popa(void *drcontext);

/** Convenience routine that returns an initialized instr_t for OP_pusha. */
instr_t *
instr_create_pusha(void *drcontext);


/****************************************************************************
 * EFLAGS
 */
/* we only care about these 11 flags, and mostly only about the first 6
 * we consider an undefined effect on a flag to be a write
 */
#define EFLAGS_READ_CF   0x00000001 /**< Reads CF (Carry Flag). */
#define EFLAGS_READ_PF   0x00000002 /**< Reads PF (Parity Flag). */
#define EFLAGS_READ_AF   0x00000004 /**< Reads AF (Auxiliary Carry Flag). */
#define EFLAGS_READ_ZF   0x00000008 /**< Reads ZF (Zero Flag). */
#define EFLAGS_READ_SF   0x00000010 /**< Reads SF (Sign Flag). */
#define EFLAGS_READ_TF   0x00000020 /**< Reads TF (Trap Flag). */
#define EFLAGS_READ_IF   0x00000040 /**< Reads IF (Interrupt Enable Flag). */
#define EFLAGS_READ_DF   0x00000080 /**< Reads DF (Direction Flag). */
#define EFLAGS_READ_OF   0x00000100 /**< Reads OF (Overflow Flag). */
#define EFLAGS_READ_NT   0x00000200 /**< Reads NT (Nested Task). */
#define EFLAGS_READ_RF   0x00000400 /**< Reads RF (Resume Flag). */
#define EFLAGS_WRITE_CF  0x00000800 /**< Writes CF (Carry Flag). */
#define EFLAGS_WRITE_PF  0x00001000 /**< Writes PF (Parity Flag). */
#define EFLAGS_WRITE_AF  0x00002000 /**< Writes AF (Auxiliary Carry Flag). */
#define EFLAGS_WRITE_ZF  0x00004000 /**< Writes ZF (Zero Flag). */
#define EFLAGS_WRITE_SF  0x00008000 /**< Writes SF (Sign Flag). */
#define EFLAGS_WRITE_TF  0x00010000 /**< Writes TF (Trap Flag). */
#define EFLAGS_WRITE_IF  0x00020000 /**< Writes IF (Interrupt Enable Flag). */
#define EFLAGS_WRITE_DF  0x00040000 /**< Writes DF (Direction Flag). */
#define EFLAGS_WRITE_OF  0x00080000 /**< Writes OF (Overflow Flag). */
#define EFLAGS_WRITE_NT  0x00100000 /**< Writes NT (Nested Task). */
#define EFLAGS_WRITE_RF  0x00200000 /**< Writes RF (Resume Flag). */

#define EFLAGS_READ_ALL  0x000007ff /**< Reads all flags. */
#define EFLAGS_WRITE_ALL 0x003ff800 /**< Writes all flags. */
/* 6 most common flags ("arithmetic flags"): CF, PF, AF, ZF, SF, OF */
/** Reads all 6 arithmetic flags (CF, PF, AF, ZF, SF, OF). */
#define EFLAGS_READ_6    0x0000011f
/** Writes all 6 arithmetic flags (CF, PF, AF, ZF, SF, OF). */
#define EFLAGS_WRITE_6   0x0008f800

/** Converts an EFLAGS_WRITE_* value to the corresponding EFLAGS_READ_* value. */
#define EFLAGS_WRITE_TO_READ(x) ((x) >> 11)
/** Converts an EFLAGS_READ_* value to the corresponding EFLAGS_WRITE_* value. */
#define EFLAGS_READ_TO_WRITE(x) ((x) << 11)

/**
 * The actual bits in the eflags register that we care about:\n<pre>
 *   11 10  9  8  7  6  5  4  3  2  1  0
 *   OF DF       SF ZF    AF    PF    CF  </pre>
 */
enum {
    EFLAGS_CF = 0x00000001, /**< The bit in the eflags register of CF (Carry Flag). */
    EFLAGS_PF = 0x00000004, /**< The bit in the eflags register of PF (Parity Flag). */
    EFLAGS_AF = 0x00000010, /**< The bit in the eflags register of AF (Aux Carry Flag). */
    EFLAGS_ZF = 0x00000040, /**< The bit in the eflags register of ZF (Zero Flag). */
    EFLAGS_SF = 0x00000080, /**< The bit in the eflags register of SF (Sign Flag). */
    EFLAGS_DF = 0x00000400, /**< The bit in the eflags register of DF (Direction Flag). */
    EFLAGS_OF = 0x00000800, /**< The bit in the eflags register of OF (Overflow Flag). */
};



#ifdef DR_FAST_IR


/* CLIENT_ASSERT with a trailing comma in a debug build, otherwise nothing. */
#define CLIENT_ASSERT_(cond, msg) DR_IF_DEBUG_(CLIENT_ASSERT(cond, msg))

/* Internally DR has multiple levels of IR, but once it gets to a client, we
 * assume it's already level 3 or higher, and we don't need to do any checks.
 * Furthermore, instr_decode() and get_thread_private_dcontext() are not
 * exported.
 */
#define MAKE_OPNDS_VALID(instr) ((void)0)
/* Turn off checks if a client includes us with DR_FAST_IR.  We can't call the
 * internal routines we'd use for these checks anyway.
 */
#define CLIENT_ASSERT(cond, msg)
#define DR_IF_DEBUG(stmt)
#define DR_IF_DEBUG_(stmt)

/* Any function that takes or returns an opnd_t by value should be a macro,
 * *not* an inline function.  Most widely available versions of gcc have trouble
 * optimizing structs that have been passed by value, even after inlining.
 */

/* opnd_t predicates */

/* Simple predicates */
#define OPND_IS_NULL(op)        ((op).kind == NULL_kind)
#define OPND_IS_IMMED_INT(op)   ((op).kind == IMMED_INTEGER_kind)
#define OPND_IS_IMMED_FLOAT(op) ((op).kind == IMMED_FLOAT_kind)
#define OPND_IS_NEAR_PC(op)     ((op).kind == PC_kind)
#define OPND_IS_NEAR_INSTR(op)  ((op).kind == INSTR_kind)
#define OPND_IS_REG(op)         ((op).kind == REG_kind)
#define OPND_IS_BASE_DISP(op)   ((op).kind == BASE_DISP_kind)
#define OPND_IS_FAR_PC(op)      ((op).kind == FAR_PC_kind)
#define OPND_IS_FAR_INSTR(op)   ((op).kind == FAR_INSTR_kind)
#define OPND_IS_MEM_INSTR(op)   ((op).kind == MEM_INSTR_kind)
#define OPND_IS_VALID(op)       ((op).kind < LAST_kind)

#define opnd_is_null            OPND_IS_NULL
#define opnd_is_immed_int       OPND_IS_IMMED_INT
#define opnd_is_immed_float     OPND_IS_IMMED_FLOAT
#define opnd_is_near_pc         OPND_IS_NEAR_PC
#define opnd_is_near_instr      OPND_IS_NEAR_INSTR
#define opnd_is_reg             OPND_IS_REG
#define opnd_is_base_disp       OPND_IS_BASE_DISP
#define opnd_is_far_pc          OPND_IS_FAR_PC
#define opnd_is_far_instr       OPND_IS_FAR_INSTR
#define opnd_is_mem_instr       OPND_IS_MEM_INSTR
#define opnd_is_valid           OPND_IS_VALID

/* Compound predicates */
INSTR_INLINE
bool
opnd_is_immed(opnd_t op)
{
    return op.kind == IMMED_INTEGER_kind || op.kind == IMMED_FLOAT_kind;
}

INSTR_INLINE
bool
opnd_is_pc(opnd_t op) {
    return op.kind == PC_kind || op.kind == FAR_PC_kind;
}

INSTR_INLINE
bool
opnd_is_instr(opnd_t op)
{
    return op.kind == INSTR_kind || op.kind == FAR_INSTR_kind;
}

INSTR_INLINE
bool
opnd_is_near_base_disp(opnd_t op)
{
    return op.kind == BASE_DISP_kind && op.seg.segment == DR_REG_NULL;
}

INSTR_INLINE
bool
opnd_is_far_base_disp(opnd_t op)
{
    return op.kind == BASE_DISP_kind && op.seg.segment != DR_REG_NULL;
}


#ifdef X64
# define OPND_IS_REL_ADDR(op)   ((op).kind == REL_ADDR_kind)
# define opnd_is_rel_addr       OPND_IS_REL_ADDR

INSTR_INLINE
bool
opnd_is_near_rel_addr(opnd_t opnd)
{
    return opnd.kind == REL_ADDR_kind && opnd.seg.segment == DR_REG_NULL;
}

INSTR_INLINE
bool
opnd_is_far_rel_addr(opnd_t opnd)
{
    return opnd.kind == REL_ADDR_kind && opnd.seg.segment != DR_REG_NULL;
}
#endif /* X64 */

/* opnd_t constructors */

/* XXX: How can we macro-ify these?  We can use C99 initializers or a copy from
 * a constant, but that implies a full initialization, when we could otherwise
 * partially intialize.  Do we care?
 */
INSTR_INLINE
opnd_t
opnd_create_null(void)
{
    opnd_t opnd;
    opnd.kind = NULL_kind;
    return opnd;
}

INSTR_INLINE
opnd_t
opnd_create_reg(reg_id_t r)
{
    opnd_t opnd DR_IF_DEBUG(= {0});  
    CLIENT_ASSERT(r <= DR_REG_LAST_ENUM && r != DR_REG_INVALID,
                  "opnd_create_reg: invalid register");
    opnd.kind = REG_kind;
    opnd.value.reg = r;
    opnd.size = 0; /* indicates full size of reg */
    return opnd;
}

INSTR_INLINE
opnd_t
opnd_create_reg_partial(reg_id_t r, opnd_size_t subsize)
{
    opnd_t opnd DR_IF_DEBUG(= {0});  
    CLIENT_ASSERT((r >= DR_REG_MM0 && r <= DR_REG_XMM15) ||
                  (r >= DR_REG_YMM0 && r <= DR_REG_YMM15),
                  "opnd_create_reg_partial: non-multimedia register");
    opnd.kind = REG_kind;
    opnd.value.reg = r;
    opnd.size = subsize;
    return opnd;
}

INSTR_INLINE
opnd_t
opnd_create_pc(app_pc pc)
{
    opnd_t opnd;
    opnd.kind = PC_kind;
    opnd.value.pc = pc;
    return opnd;
}

/* opnd_t accessors */

#define OPND_GET_REG(opnd) \
    (CLIENT_ASSERT_(opnd_is_reg(opnd), "opnd_get_reg called on non-reg opnd") \
     (opnd).value.reg)
#define opnd_get_reg OPND_GET_REG

#define GET_BASE_DISP(opnd) \
    (CLIENT_ASSERT_(opnd_is_base_disp(opnd), \
                    "opnd_get_base_disp called on invalid opnd type") \
     (opnd).value.base_disp)

#define OPND_GET_BASE(opnd)  (GET_BASE_DISP(opnd).base_reg)
#define OPND_GET_DISP(opnd)  (GET_BASE_DISP(opnd).disp)
#define OPND_GET_INDEX(opnd) (GET_BASE_DISP(opnd).index_reg)
#define OPND_GET_SCALE(opnd) (GET_BASE_DISP(opnd).scale)

#define opnd_get_base OPND_GET_BASE
#define opnd_get_disp OPND_GET_DISP
#define opnd_get_index OPND_GET_INDEX
#define opnd_get_scale OPND_GET_SCALE

#define OPND_GET_SEGMENT(opnd) \
    (CLIENT_ASSERT_(opnd_is_base_disp(opnd) \
                    IF_X64(|| opnd_is_abs_addr(opnd) || \
                           opnd_is_rel_addr(opnd)), \
                    "opnd_get_segment called on invalid opnd type") \
     (opnd).seg.segment)
#define opnd_get_segment OPND_GET_SEGMENT

/* instr_t accessors */

INSTR_INLINE
bool
instr_is_app(instr_t *instr)
{
    CLIENT_ASSERT(instr != NULL, "instr_is_app: passed NULL");
    return ((instr->flags & INSTR_DO_NOT_MANGLE) == 0);
}

INSTR_INLINE
bool
instr_ok_to_mangle(instr_t *instr)
{
    return instr_is_app(instr);
}

INSTR_INLINE
bool
instr_is_meta(instr_t *instr)
{
    CLIENT_ASSERT(instr != NULL, "instr_is_meta: passed NULL");
    return ((instr->flags & INSTR_DO_NOT_MANGLE) != 0);
}


INSTR_INLINE
int
instr_num_srcs(instr_t *instr)
{
    MAKE_OPNDS_VALID(instr);
    return instr->num_srcs;
}

INSTR_INLINE
int
instr_num_dsts(instr_t *instr)
{
    MAKE_OPNDS_VALID(instr);
    return instr->num_dsts;
}

/* src0 is static, rest are dynamic. */
#define INSTR_GET_SRC(instr, pos)                                   \
    (MAKE_OPNDS_VALID(instr),                                       \
     CLIENT_ASSERT_(pos >= 0 && pos < (instr)->num_srcs,            \
                    "instr_get_src: ordinal invalid")               \
     ((pos) == 0 ? (instr)->src0 : (instr)->srcs[(pos) - 1]))

#define INSTR_GET_DST(instr, pos)                                   \
    (MAKE_OPNDS_VALID(instr),                                       \
     CLIENT_ASSERT_(pos >= 0 && pos < (instr)->num_dsts,            \
                    "instr_get_dst: ordinal invalid")               \
     (instr)->dsts[pos])

/* Assumes that if an instr has a jump target, it's stored in the 0th src
 * location.
 */
#define INSTR_GET_TARGET(instr)                                         \
    (MAKE_OPNDS_VALID(instr),                                           \
     CLIENT_ASSERT_(instr_is_cti(instr),                                \
                    "instr_get_target: called on non-cti")              \
     CLIENT_ASSERT_((instr)->num_srcs >= 1,                             \
                    "instr_get_target: instr has no sources")           \
     (instr)->src0)

#define instr_get_src INSTR_GET_SRC
#define instr_get_dst INSTR_GET_DST
#define instr_get_target INSTR_GET_TARGET

INSTR_INLINE
void
instr_set_note(instr_t *instr, void *value)
{
    instr->note = value;
}

INSTR_INLINE
void *
instr_get_note(instr_t *instr)
{
    return instr->note;
}

INSTR_INLINE
instr_t*
instr_get_next(instr_t *instr)
{
    return instr->next;
}

INSTR_INLINE
instr_t *
instr_get_next_app(instr_t *instr)
{
    CLIENT_ASSERT(instr != NULL, "instr_get_next_app: passed NULL");
    for (instr = instr->next; instr != NULL; instr = instr->next) {
        if (instr_is_app(instr))
            return instr;
    }
    return NULL;
}

INSTR_INLINE
instr_t*
instr_get_prev(instr_t *instr)
{
    return instr->prev;
}

INSTR_INLINE
void
instr_set_next(instr_t *instr, instr_t *next)
{
    instr->next = next;
}

INSTR_INLINE
void
instr_set_prev(instr_t *instr, instr_t *prev)
{
    instr->prev = prev;
}

#endif /* DR_FAST_IR */




/****************************************************************************
 * instr_t prefixes
 *
 * Note that prefixes that change the data or address size, or that
 * specify a different base segment, are not specified on a
 * whole-instruction level, but rather on individual operands (of
 * course with multiple operands they must all match).
 * The rep and repne prefixes are encoded directly into the opcodes.
 *
 */
#define PREFIX_LOCK          0x01 /**< Makes the instruction's memory accesses atomic. */
#define PREFIX_JCC_NOT_TAKEN 0x02 /**< Branch hint: conditional branch is taken. */
#define PREFIX_JCC_TAKEN     0x04 /**< Branch hint: conditional branch is not taken. */
#define PREFIX_XACQUIRE      0x08 /**< Transaction hint: start lock elision. */
#define PREFIX_XRELEASE      0x10 /**< Transaction hint: end lock elision. */



/**
 * Prints the instruction \p instr to file \p outfile.
 * Does not print address-size or data-size prefixes for other than
 * just-decoded instrs, and does not check that the instruction has a
 * valid encoding.  Prints each operand with leading zeros indicating
 * the size.
 * The default is to use AT&T-style syntax, unless the \ref op_syntax_intel
 * "-syntax_intel" runtime option is specified.
 */
void
instr_disassemble(void *drcontext, instr_t *instr, file_t outfile);

/**
 * Prints the instruction \p instr to the buffer \p buf.
 * Always null-terminates, and will not print more than \p bufsz characters,
 * which includes the final null character.
 * Returns the number of characters printed, not including the final null.
 *
 * Does not print address-size or data-size prefixes for other than
 * just-decoded instrs, and does not check that the instruction has a
 * valid encoding.  Prints each operand with leading zeros indicating
 * the size.
 * The default is to use AT&T-style syntax, unless the \ref op_syntax_intel
 * "-syntax_intel" runtime option is specified.
 */
size_t
instr_disassemble_to_buffer(void *drcontext, instr_t *instr,
                            char *buf, size_t bufsz);

#endif /* _DR_IR_INSTR_H_ */
