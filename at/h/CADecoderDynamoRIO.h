#pragma once
#include "CADecoder.h"
#include "PatchCommon.h"
#include "dr_defines.h"
#include "ca_defines.h"

using namespace Dyninst::InstructionAPI;

class CADecoderDynamoRIO : public CADecoder{
	public:
		CADecoderDynamoRIO();
		~CADecoderDynamoRIO();
		void decode(uint64_t addr, Instruction::Ptr iptr);
		bool needDepie();
		bool isCall();
		bool isSysCall();
		bool isRet();
		bool isCall_indirect();
		bool isIndirectJmp();
		uint32_t callTarget();
		size_t depie(uint8_t* depie_raw_byte);
		size_t CallToPush(uint8_t *payload);
		size_t CallContextSave(uint8_t *payload);
		size_t callhandler(uint8_t *payload, uint64_t handler_addr);
		size_t JmpToPush(uint8_t *payload);
		size_t JmpContextSave(uint8_t *payload);
		size_t RetContextSave(uint8_t *payload);
		void regUsage(int* flag_map);
		uint64_t get_src_abs_addr();
	private:
		bool isPrefix(uint8_t);
	private:
		instr_t instr;
		uint64_t ins_addr;
		uint8_t raw_byte[MAX_RAW_INSN_SIZE];
		size_t nbytes;
		static void* drcontext;
		static uint8_t prefix[MAX_NUM_PREFIXS];//4 groups of prefix
};
