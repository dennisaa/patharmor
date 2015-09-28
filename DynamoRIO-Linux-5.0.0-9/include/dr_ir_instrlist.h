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

#ifndef _DR_IR_INSTRLIST_H_
#define _DR_IR_INSTRLIST_H_ 1

/****************************************************************************
 * INSTRLIST ROUTINES
 */
/**
 * @file dr_ir_instrlist.h
 * @brief Functions to create and manipulate lists of instructions.
 */


/** Returns an initialized instrlist_t allocated on the thread-local heap. */
instrlist_t*
instrlist_create(void *drcontext);

/** Initializes \p ilist. */
void
instrlist_init(instrlist_t *ilist);

/** Deallocates the thread-local heap storage for \p ilist. */
void
instrlist_destroy(void *drcontext, instrlist_t *ilist);

/** Frees the instructions in \p ilist. */
void
instrlist_clear(void *drcontext, instrlist_t *ilist);

/** Destroys the instructions in \p ilist and destroys the instrlist_t object itself. */
void
instrlist_clear_and_destroy(void *drcontext, instrlist_t *ilist);

/**
 * All future instructions inserted into \p ilist that do not have raw bits
 * will have instr_set_translation() called with \p pc as the target.
 * This is a convenience routine to make it easy to have the same
 * code generate non-translation and translation instructions, and it does
 * not try to enforce that all instructions have translations (e.g.,
 * some could be inserted via instr_set_next()).
 */
void
instrlist_set_translation_target(instrlist_t *ilist, app_pc pc);

/** Returns the translation target, or NULL if none is set. */
app_pc
instrlist_get_translation_target(instrlist_t *ilist);

/** Returns the first instr_t in \p ilist. */
instr_t*
instrlist_first(instrlist_t *ilist);

/**
 * Returns the first application (non-meta) instruction in the instruction list
 * \p ilist.
 *
 * \note All preceding meta instructions will be skipped.
 *
 * \note We do recommend using this routine during the phase of application
 * code analysis, as any non-app instructions present are guaranteed to be ok
 * to skip.
 * However, the caution should be exercised if using this routine after any
 * instrumentation insertion has already happened, as instrumentation might
 * affect register usage or other factors being analyzed.
 */
instr_t *
instrlist_first_app(instrlist_t *ilist);

/** Returns the last instr_t in \p ilist. */
instr_t*
instrlist_last(instrlist_t *ilist);

/** Adds \p instr to the end of \p ilist. */
void
instrlist_append(instrlist_t *ilist, instr_t *instr);

/** Adds \p instr to the front of \p ilist. */
void
instrlist_prepend(instrlist_t *ilist, instr_t *instr);

/**
 * Allocates a new instrlist_t and for each instr_t in \p old allocates
 * a new instr_t using instr_clone to produce a complete copy of \p old.
 * Each operand that is opnd_is_instr() has its target updated
 * to point to the corresponding instr_t in the new instrlist_t
 * (this routine assumes that all such targets are contained within \p old,
 * and may fault otherwise).
 */
instrlist_t*
instrlist_clone(void *drcontext, instrlist_t *old);

/** Inserts \p instr into \p ilist prior to \p where. */
void
instrlist_preinsert(instrlist_t *ilist, instr_t *where, instr_t *instr);

/** Inserts \p instr into \p ilist after \p where. */
void
instrlist_postinsert(instrlist_t *ilist, instr_t *where, instr_t *instr);

/** Replaces \p oldinst with \p newinst in \p ilist (does not destroy \p oldinst). */
instr_t*
instrlist_replace(instrlist_t *ilist, instr_t *oldinst, instr_t *newinst);

/** Removes (does not destroy) \p instr from \p ilist. */
void
instrlist_remove(instrlist_t *ilist, instr_t *instr);

/**
 * Specifies the fall-through target of a basic block if its last
 * instruction is a conditional branch instruction.
 * It can only be called in basic block building event callbacks
 * when the \p for_trace parameter is false,
 * and has NO EFFECT in other cases.
 */
bool
instrlist_set_fall_through_target(instrlist_t *bb, app_pc tgt);

/**
 * Specifies the return target of a basic block if its last
 * instruction is a call instruction.
 * It can only be called in basic block building event callbacks
 * when the \p for_trace parameter is false,
 * and has NO EFFECT in other cases.
 */
bool
instrlist_set_return_target(instrlist_t *bb, app_pc tgt);


/**
 * Encodes each instruction in \p ilist in turn in contiguous memory starting
 * at \p pc.  Returns the pc after all of the encodings, or NULL if any one
 * of the encodings failed.
 * Uses the x86/x64 mode stored in each instr, not the mode of the current thread.
 * In order for instr_t operands to be encoded properly,
 * \p has_instr_jmp_targets must be true.  If \p has_instr_jmp_targets is true,
 * the note field of each instr_t in ilist will be overwritten, and if any
 * instr_t targets are not in \p ilist, they must have their note fields set with
 * their offsets relative to pc.
 * x86 instructions can occupy up to 17 bytes each, so the caller should ensure
 * the target location has enough room to avoid overflow.
 */
byte *
instrlist_encode(void *drcontext, instrlist_t *ilist, byte *pc,
                 bool has_instr_jmp_targets);

/**
 * Encodes each instruction in \p ilist in turn in contiguous memory
 * starting \p copy_pc in preparation for copying to \p final_pc.  Any
 * pc-relative instruction is encoded as though the instruction list were
 * located at \p final_pc.  This allows for direct copying of the
 * encoded bytes to \p final_pc without re-relativization.
 *
 * Returns the pc after all of the encodings, or NULL if any one
 * of the encodings failed.
 *
 * Uses the x86/x64 mode stored in each instr, not the mode of the current thread.
 *
 * In order for instr_t operands to be encoded properly,
 * \p has_instr_jmp_targets must be true.  If \p has_instr_jmp_targets is true,
 * the note field of each instr_t in ilist will be overwritten, and if any
 * instr_t targets are not in \p ilist, they must have their note fields set with
 * their offsets relative to pc.
 *
 * If \p max_pc is non-NULL, computes the total size required to encode the
 * instruction list before performing any encoding.  If the whole list will not
 * fit starting at \p copy_pc without exceeding \p max_pc, returns NULL without
 * encoding anything.  Otherwise encodes as normal.  Note that x86 instructions
 * can occupy up to 17 bytes each, so if \p max_pc is NULL, the caller should
 * ensure the target location has enough room to avoid overflow.
 */
byte *
instrlist_encode_to_copy(void *drcontext, instrlist_t *ilist, byte *copy_pc,
                         byte *final_pc, byte *max_pc, bool has_instr_jmp_targets);


/**
 * Prints each instruction in \p ilist in sequence to \p outfile.
 * The default is to use AT&T-style syntax, unless the \ref op_syntax_intel
 * "-syntax_intel" runtime option is specified.
 */
void
instrlist_disassemble(void *drcontext, app_pc tag,
                      instrlist_t *ilist, file_t outfile);


#endif /* _DR_IR_INSTRLIST_H_ */
