#include "CADecoderDynamoRIO.h"

#include "Register.h"
#include "dr_api.h"
#include "dr_defines.h"

#include "PatchCommon.h"
#include "ca_defines.h"

using namespace std;
using namespace Dyninst::InstructionAPI;

void* CADecoderDynamoRIO::drcontext = dr_standalone_init();
uint8_t CADecoderDynamoRIO::prefix[MAX_NUM_PREFIXS] = {
	0xF0,//LOCK 
	0xF2,//REPNE/REPNZ
	0xF3,//REPE/REPZ
	0x2E,//CS
	0x36,//SS
	0x3E,//DS
	0x26,//ES
	0x64,//FS
	0x65,//GS
	0x2E,//Branch not taken
	0x3E,//Branch taken
	0x66,//Operand-size override
	0x67/*Address-size override*/};

CADecoderDynamoRIO::CADecoderDynamoRIO()
{
	set_x86_mode(drcontext,false);
}

CADecoderDynamoRIO::~CADecoderDynamoRIO()
{
}

bool CADecoderDynamoRIO::isPrefix(uint8_t val)
{
	for (int i=0;i<MAX_NUM_PREFIXS;i++){
		if (val == prefix[i])
			return false;
	}
	return true;
}

void CADecoderDynamoRIO::decode(uint64_t addr, Instruction::Ptr iptr)
{
	nbytes = iptr->size();
	ins_addr = addr;
	for (size_t i =0; i<nbytes; i++){
		raw_byte[i] = iptr->rawByte(i);
	}
	raw_byte[nbytes] = '\0';
	instr_init(drcontext,&instr);
	decode_from_copy(drcontext, (byte*)raw_byte, (byte*)ins_addr, &instr);
#if 0
	char dis_buf[1024];
	size_t size_of_dis;
	size_of_dis = instr_disassemble_to_buffer(drcontext,&instr,dis_buf,1024);
	dis_buf[size_of_dis] = '\0';
	printf("Dissassemble %x -> %s\n",(unsigned int)ins_addr,dis_buf);
#endif
}

void CADecoderDynamoRIO::regUsage(int* flag_map)
{
	int i;
	for (i=DR_REG_RAX;i<=DR_REG_R15;i++){
		if (instr_writes_to_reg(&instr,i)){
			flag_map[i] |= FLAG_WRITE;
		}
		if (instr_reads_from_reg(&instr,i)){
			flag_map[i] |= FLAG_READ;
		}
	}
}

bool CADecoderDynamoRIO::needDepie()
{
	return instr_has_rel_addr_reference(&instr);
}

bool CADecoderDynamoRIO::isCall()
{
	return instr_is_call(&instr);
}

bool CADecoderDynamoRIO::isSysCall()
{
	return (instr_get_opcode(&instr) == OP_syscall);
}

uint32_t CADecoderDynamoRIO::callTarget()
{
	opnd_t src_opnd;
	if (instr_reads_memory(&instr)||instr_is_call_indirect(&instr))
		return 0;
	src_opnd = instr_get_src(&instr,0);
	return (uint32_t) opnd_get_disp(src_opnd);
}

bool CADecoderDynamoRIO::isCall_indirect()
{
	return instr_is_call_indirect(&instr);
}

bool CADecoderDynamoRIO::isIndirectJmp()
{
	return (instr_get_opcode(&instr) == OP_jmp_ind);
}

bool CADecoderDynamoRIO::isRet()
{
	return (instr_get_opcode(&instr) == OP_ret);
}


/*
 * Convert "call target" into "push target"
 * If target is memory operand, plus 0x20 for the bytes after 0xff
 * If target is register, remove 0xff, and minus 0x80
 * If target is rip-relative, build push instruction
 */
size_t CADecoderDynamoRIO::CallToPush(uint8_t *payload)
{
	opnd_t src_opnd;
	uint32_t abs_addr;
	
	if (instr_reads_memory(&instr))
	{
		memcpy(payload, raw_byte, nbytes);
		if (payload[0] == 0xff)
			payload[1] += 0x20;
		else
			payload[2] += 0x20;
		return nbytes;
	}
	else{
		if (instr_is_call_indirect(&instr))
		{
			if (nbytes == 2)
				payload[0] = raw_byte[1] - 0x80;
			else{
				payload[0] = raw_byte[0];
				payload[1] = raw_byte[2] - 0x80;
			}
			return nbytes-1;
		}
		else{
			src_opnd = instr_get_src(&instr,0);
			abs_addr = (uint32_t) opnd_get_disp(src_opnd);
			payload[0] = 0x68;
			memcpy(payload+1,&abs_addr,sizeof(abs_addr));
			return sizeof(abs_addr)+1;
		}
	}
	return 0;
}

/* 
 * generate assemble code for "movq lib_addr,rax; call *rax"
 */
size_t CADecoderDynamoRIO::callhandler(uint8_t *payload, uint64_t handler_addr)
{
	const uint8_t general_payload[12] = {0x48,0xb8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xd0};
	memcpy(payload,general_payload,12);
	memcpy(payload+2,&handler_addr,sizeof(handler_addr));
	return 12;
}

/*
 * push rbp,bx,cx,dx,si,di,r8,r9,r12,r13,r14,r15, 
 * calling conversion: cx,dx,di,si,r8,r9 parameter, bx,bp,r12-r15 callee saved.
 * TODO optimize the save, maybe later drop it and put the save into library
 */
size_t CADecoderDynamoRIO::CallContextSave(uint8_t *payload)
{
	const uint8_t general_payload[18] = {0x55,0x57,0x56,0x52,0x51,0x41,0x50,0x41,0x51,
		0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x53};
	memcpy(payload,general_payload,18);
	return 18;
}

/*
 * push ax,bx,bp,r12-r15 base on calling conversion
 * TODO optimize the save, maybe later drop it and put the save into library
 */
size_t CADecoderDynamoRIO::RetContextSave(uint8_t *payload)
{
	const uint8_t general_payload[18] = {0x55,0x57,0x56,0x52,0x51,0x41,0x50,0x41,0x51,
		0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x53};
	memcpy(payload,general_payload,18);
	return 18;
}

size_t CADecoderDynamoRIO::JmpToPush(uint8_t *payload)
{
	const uint8_t sub_rsp[7] = {0x48,0x81,0xec,0x00,0x10,0x00,0x00};
	/*
	 * Since the indirect jmp can happen in a red-zone function, so need to reserve the stack
	 * First generate a sub rsp,0x1000 instruction then the push
	 */
	memcpy(payload, sub_rsp,7);
	
	if (instr_reads_memory(&instr))
	{
		memcpy(payload+7, raw_byte, nbytes);
		if (payload[7] == 0xff)
			payload[8] += 0x10;
		else
			payload[9] += 0x10;
		return nbytes+7;
	}
	else{
		if (nbytes == 2)
			payload[7] = raw_byte[1] - 0x90;
		else{
			payload[7] = raw_byte[0];
			payload[8] = raw_byte[2] - 0x90;
		}
		return nbytes-1+7;
	}
	return 0;
}

size_t CADecoderDynamoRIO::JmpContextSave(uint8_t *payload)
{
	return 0;
}

/* 
 *  x64 instruction format: [PREFIX][OPCODE][MODR/M][SIB][DISPLACEMENT][IMM]
 *  which maxium 17bytes long.
 *  PREFIX(4byte max): has four groups, each group can contribute 1byte. 
 *                     FS register have prefix 0x64
 *  OPCODE(1-3bytes): Consist of REX prefix and the real opcode.
 *                    REX prefix only used in long mode(64bit), for
 *                    detail check intel developer ABI book.
 *                    A usefull fact is REX always start with "0100"(binary, 0x4 hex) pattern
 *  MODR/M(1bytes)  : Define the operands and memory addressing mode. 
 *  SIB(1bytes)     : Define the Scale,index and Base
 *
 *  For rip-relative instruction in x64
 *  The MODR/M always be "00 reg 101" and SIB always not used.
 *  REX.B in REX prefix have no effect. and the displacement always 4bytes.
 */

/*
 * For convert IP-relative into absolute addressing:
 * 1. Find the displacement offset
 * 2. Copy the orig raw byte until the disp off, 
 *    and modify the last bit to zero. MODR/M byte :"00 reg 101" --> "00 reg 100"
 * 3. Insert 0x25 as SIB byte "00 100 101" means scale=1, no index, no base
 * 4. Modify the displacement and copy this 4byte new displacement.
 * 5. Copy the rest (Imm if there are any)
 * Note : If want to use FS, add 0x64 in prefix.
 */

/*
 * Limitation : 1.Although it is possible to use 64-bit operand, however, it involve the opcode 
 *              changing instead of just REX.W changing. So we only support change the ip-relative
 *              into 32bit operand, i.e., the first rewrite version code space must be in the lower 32-bit address
 *			    2.For 64-bit, Intel support VEX mode, which between Prefixs and opcode.
 *                It started with either 0XC5(2bytes vex) or 0xC4(3bytes vex), currently I do not support it
 */  

size_t CADecoderDynamoRIO::depie(uint8_t* depie_raw_byte)
{
//	instr_t depie_instr;
	size_t depie_nbytes;
	opnd_t src_opnd,dst_opnd;
	int src_idx,dst_idx;
	uint32_t abs_addr;
	unsigned int rip_rel_pos;
	
	if (!needDepie()) return 0;
	
	src_idx = instr_get_rel_addr_src_idx(&instr);
	dst_idx = instr_get_rel_addr_dst_idx(&instr);
	if (src_idx!=-1){
		src_opnd = instr_get_src(&instr,(uint)src_idx);
		abs_addr = (uint32_t) opnd_get_disp(src_opnd);
#if 0		
		printf("\t src displacement is %x\n",abs_addr);
#endif
	}
	if (dst_idx!=-1){
		dst_opnd = instr_get_dst(&instr,(uint)dst_idx);
		abs_addr = (uint32_t)opnd_get_disp(dst_opnd);
#if 0
		printf("\t dst displacement is %x\n",abs_addr);
#endif
	}
	depie_nbytes = nbytes+1;
	
	/* find displacement offset*/
	decode_sizeof(drcontext,(byte*)raw_byte,NULL,&rip_rel_pos);
	assert(rip_rel_pos > 0);

	for (size_t i=0; i<rip_rel_pos; i++)
		depie_raw_byte[i] = raw_byte[i];

	/*cleanup the last bit in MODR/M byte, add SIB byte*/
	depie_raw_byte[rip_rel_pos-1] &= 0xFE;
	depie_raw_byte[rip_rel_pos] = 0x25;

	/* copy new displacement */
	*(uint32_t*)(&depie_raw_byte[rip_rel_pos+1]) = abs_addr;

	/* copy the rest */
	for (size_t i=rip_rel_pos+4; i<nbytes;i++)
		depie_raw_byte[i+1] = raw_byte[i];

#if 0
	instr_init(drcontext,&depie_instr);
	decode_from_copy(drcontext, (byte*)depie_raw_byte, (byte*)ins_addr, &depie_instr);
	char dis_buf[1024];
	size_t size_of_dis;
	size_of_dis = instr_disassemble_to_buffer(drcontext,&depie_instr,dis_buf,1024);
	dis_buf[size_of_dis] = '\0';
	printf("Dissassemble %x -> %s\n",(unsigned int)ins_addr,dis_buf);
	printf("Raw byte : \n\t");
	for (size_t i=0; i<depie_nbytes; i++){
		printf("%x ",depie_raw_byte[i]);
	}
	printf("\n");
#endif


	return depie_nbytes;
}

uint64_t CADecoderDynamoRIO::get_src_abs_addr()
{
	opnd_t src_opnd;
	int src_idx;
	uint64_t abs_addr;
	int nr_src,i;
	
	if (instr_is_cti(&instr))
		return 0;

	if (instr_is_mov_constant(&instr,(ptr_int_t*)(&abs_addr)))
		return abs_addr;

	if (instr_get_opcode(&instr) == OP_lea){
		nr_src = instr_num_srcs(&instr);
		for (i=0;i<nr_src;i++){
			src_opnd = instr_get_src(&instr,i);
			if (opnd_is_abs_addr(src_opnd)){
				return (uint64_t)opnd_get_addr(src_opnd);
			}
		}
	}
	
// Now src should PC relative if there is address 
	abs_addr = 0;
	if (needDepie()){
		src_idx = instr_get_rel_addr_src_idx(&instr);
		if (src_idx!=-1){
			src_opnd = instr_get_src(&instr,(uint)src_idx);
			abs_addr = (uint64_t) opnd_get_disp(src_opnd);
		}
	}
	
	return abs_addr;
}

