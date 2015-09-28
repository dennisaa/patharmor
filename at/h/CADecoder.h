#pragma once

#include "ca_defines.h"
#include "PatchCommon.h"

using namespace Dyninst::InstructionAPI;
class CADecoder
{
	public:
		virtual ~CADecoder() {};
		virtual void decode(uint64_t, Instruction::Ptr) = 0;
		virtual bool needDepie() = 0;
		virtual bool isCall() = 0;
		virtual bool isSysCall() = 0;
		virtual bool isRet() = 0;
		virtual bool isCall_indirect() = 0;
		virtual bool isIndirectJmp() = 0;
		virtual uint32_t callTarget() = 0;
		virtual size_t depie(uint8_t*) = 0;
		virtual size_t JmpToPush(uint8_t *) = 0;
		virtual size_t JmpContextSave(uint8_t *) = 0;
		virtual size_t CallToPush(uint8_t *) = 0;
		virtual size_t CallContextSave(uint8_t *) = 0;
		virtual size_t callhandler(uint8_t *, uint64_t ) = 0;
		virtual size_t RetContextSave(uint8_t *) = 0;
		virtual void regUsage(int*) = 0;
		virtual uint64_t get_src_abs_addr() = 0;
};
