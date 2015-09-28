#include <stdio.h>
#include <stdint.h> 
#include <limits.h>

#include "BPatch.h"
#include "BPatch_addressSpace.h" 
#include "BPatch_process.h" 
#include "BPatch_object.h"
#include "BPatch_binaryEdit.h" 
#include "BPatch_function.h"
#include "BPatch_point.h"
#include "BPatch_flowGraph.h" 
#include "BPatch_memoryAccessAdapter.h"

#include "PatchCommon.h"
#include "PatchMgr.h"
#include "PatchModifier.h"

#include "Instruction.h"
#include "Operand.h"
#include "Expression.h"
#include "Visitor.h"
#include "Register.h"
#include "BinaryFunction.h"
#include "Immediate.h"
#include "Dereference.h"
#include "Parsing.h" 
#include "Edge.h"
#include "Symtab.h" 

#include <string>
#include <set>
#include <vector>  

using namespace std;
using namespace Dyninst;
using namespace Dyninst::PatchAPI;
using namespace Dyninst::ParseAPI;
using namespace Dyninst::SymtabAPI;
using namespace Dyninst::InstructionAPI;

#include "function.h"
#include "instPoint.h"

#include "env.h"
#include "defs.h"
#include "arms_utils.h"
#include "arms_instruction.h"
#include "arms_bb.h"
#include "arms_edge.h"
#include "arms_function.h"
#include "arms_cfg.h"
#include "arms_icall_resolver.h"

#include "arms_dyninst_cfg.h"

void 
DICFG::insert_functions_and_bbs(const CodeObject::funclist& funcs)
{
	CodeObject::funclist::iterator 	funcs_iter;
	PARSE_API_RET_BLOCKLIST::iterator blocks_iter;
	ParseAPI::Function *fun; 
	ParseAPI::Block *block; 
	ArmsFunction *arms_fun, *cf;
	ArmsBasicBlock *arms_block; 
	
	/* Create objects representing individual functions */  
	for(funcs_iter = funcs.begin(); funcs_iter != funcs.end(); funcs_iter++) {
		fun = *funcs_iter;
		if (find_function((address_t)fun->addr())) continue; 
		arms_fun = new ArmsFunction((address_t)fun->addr(), fun->name(), this);
		store_function(arms_fun); 
	} 

	/* Insert their basic blocks; mark their start and end address, plus set 
	 * all containing functions. */
	for(funcs_iter = funcs.begin(); funcs_iter != funcs.end(); funcs_iter++) {
		fun = *funcs_iter;
		arms_fun = find_function((address_t)fun->addr());

		ParseAPI::Function::blocklist blocks = fun->blocks();
		for(blocks_iter = blocks.begin(); blocks_iter != blocks.end(); blocks_iter++) {
			block = *blocks_iter; 

//			fprintf(stderr, "handling bb 0x%lx\n", block->start());

			/* don't handle shared blocks multiple times */
			if (find_bb((address_t) block->start())) continue; 

			fprintf(stderr, "adding new bb 0x%lx\n", block->start());

			arms_block = new ArmsBasicBlock((address_t) block->start(), (address_t) block->end(),
				(address_t) block->last(), this); 
			store_bb(arms_block); 

			/* add the containing functions */
			std::vector<ParseAPI::Function *> containing_funcs; 
			block->getFuncs(containing_funcs); 
			std::vector<ParseAPI::Function *>::iterator cf_iter = containing_funcs.begin();
			for ( ; cf_iter != containing_funcs.end(); cf_iter++) {
				cf = find_function((address_t)((*cf_iter)->addr()));
				arms_block->add_containing_function(cf);
				cf->add_bb(arms_block);
			} 
		} 
	} 

	/* Mark entry bbs and exit bbs */  
	for(funcs_iter = funcs.begin(); funcs_iter != funcs.end(); funcs_iter++) {
		fun = *funcs_iter;
		arms_fun = find_function((address_t)fun->addr());
		
		Block *entry_block = fun->entry(); 
		assert(entry_block); 
		ArmsBasicBlock *arms_entry_block = find_bb((address_t)entry_block->start());
		arms_entry_block->set_is_entry_block(); 
		arms_fun->add_entry_block(arms_entry_block);

		PARSE_API_RET_BLOCKLIST return_blocks = fun->returnBlocks();
		for(blocks_iter = return_blocks.begin(); blocks_iter != return_blocks.end(); blocks_iter++) {
			ArmsBasicBlock *arms_exit_block = find_bb((address_t)(*blocks_iter)->start());
			arms_exit_block->set_is_exit_block(); 
			arms_fun->add_exit_block(arms_exit_block);
		}   
	}
}


void 
DICFG::insert_edges(const CodeObject::funclist& funcs)
{
	CodeObject::funclist::iterator 	funcs_iter;
	ParseAPI::Function::blocklist::iterator	blocks_iter;
	ParseAPI::Block::edgelist::const_iterator	edges_iter;
	ParseAPI::Function *fun; 
	ParseAPI::Block *source_block, *target_block; 
	ParseAPI::Edge *edge; 
	ArmsFunction *arms_fun;  
	ArmsBasicBlock *arms_source_block, *arms_target_block; 
	ArmsEdge *arms_edge; 

	std::set<Address> seen; 

	for(funcs_iter = funcs.begin(); funcs_iter != funcs.end(); funcs_iter++) {
		fun = *funcs_iter;
		arms_fun = find_function((address_t)fun->addr());
		ParseAPI::Function::blocklist blocks = fun->blocks();
		
		for(blocks_iter = blocks.begin(); blocks_iter != blocks.end(); blocks_iter++) {
			source_block = *blocks_iter; 
			arms_source_block = find_bb((address_t)(source_block->start()));

			/* don't handle shared blocks multiple times */
			if (seen.find(source_block->start()) != seen.end()) continue; 
			seen.insert(source_block->start());

			const ParseAPI::Block::edgelist& edges = source_block->targets(); 	
			for(edges_iter = edges.begin(); edges_iter != edges.end(); edges_iter++) {
				edge = *edges_iter;
				if (edge->type() == CALL_FT) continue; 

				target_block = edge->trg(); 
		
				/* If it's an indirect call, we'll add the return edge separately */
				if ((edge->type() == RET) && (target_block->start() == (Address) -1)) continue; 

				arms_target_block = (target_block->start() == (Address) -1) ? 
					0 : find_bb((address_t)target_block->start());  

				/* an indirect call - see if we have data from LLVM to help */
				if ((! arms_target_block) && (edge->type() == CALL)) {
					std::vector<void*> targets;
					int ret = arms_icall_resolver((void*)(source_block->last()), targets);
					for (unsigned i = 0; i < targets.size(); i++) {
						handle_interprocedural(arms_source_block, (address_t)targets[i], arms_indirect_call);
					}
					if (targets.size() > 0) {
						continue; 
					}
				}

				if (!arms_target_block) { 
					arms_target_block = ArmsBasicBlock::create_dummy_basic_block(arms_fun, this);
				}

				arms_edge = new ArmsEdge(arms_source_block, arms_target_block, this); 
				copy_edge_type(arms_edge, edge, (target_block->start() == (Address) -1)); 
				arms_source_block->add_outgoing_edge(arms_edge);
				arms_target_block->add_incoming_edge(arms_edge);
//				fprintf(stderr, "new edge %s\n", arms_edge->to_string().c_str());
			}
		} 
	}
}

void
DICFG::copy_edge_type(ArmsEdge *arms_edge, ParseAPI::Edge *edge, bool indirect)
{
	arms_edge_type_t type; 

	switch(edge->type()) {
		case CALL:
			type = (indirect) ? arms_indirect_call : arms_direct_call;   
			arms_edge->set_type(type);  
			break; 
		
		case COND_TAKEN:
			arms_edge->set_type(arms_cond_taken); 
			break; 

		case COND_NOT_TAKEN:
			arms_edge->set_type(arms_cond_not_taken); 
			break; 

		case INDIRECT:
			type = (edge->interproc()) ?  arms_inter_indirect_jmp : arms_indirect_jmp;   
			arms_edge->set_type(type); 
			break; 

		case DIRECT:
			type = (edge->interproc()) ?  arms_inter_direct_jmp : arms_direct_jmp;   
			arms_edge->set_type(type); 
			break; 

		case FALLTHROUGH:
			arms_edge->set_type(arms_fallthrough); 
			break; 

		case CATCH:
			arms_edge->set_type(arms_catch); 
			break; 

		case CALL_FT:
			arms_edge->set_type(arms_call_ft); 
			break; 

		case RET: 
			arms_edge->set_type(arms_ret); 
			break; 

		default:
			assert(0);
	} 
}

Expression::Ptr DICFG::the_pc = Expression::Ptr(new RegisterAST(MachRegister::getPC(Arch_x86_64)));

void
DICFG::insert_functions(std::vector<BPatch_function *> *funcs)
{
	std::vector<BPatch_function *>::iterator funcs_iter; 

	/* Create ArmsFunction objects of individual functions */
	BPatch_function *bp_fun;
	ArmsFunction *arms_fun;  
	for(funcs_iter = funcs->begin(); funcs_iter != funcs->end(); funcs_iter++) {
		bp_fun = *funcs_iter; 
		if (find_function((address_t)(bp_fun->getBaseAddr()))) continue;  
		arms_fun = new ArmsFunction((address_t)(bp_fun->getBaseAddr()), bp_fun->getName(), this); 
		store_function(arms_fun); 
	}

	/* Insert CFGs of individual functions. */
	for(funcs_iter = funcs->begin(); funcs_iter != funcs->end(); funcs_iter++) {
		insert_intraprocedural_function_flow_graph(*funcs_iter); 	
		set_entry_and_exit_points_of_function(*funcs_iter); 
	}
}

/* Insert a new function object, and create its flow graph. 
 * At this stage, we don't connect it to other functions/modules yet. */
void 
DICFG::insert_intraprocedural_function_flow_graph(BPatch_function *bp_fun) 
{
	PatchFunction 		*pfun = PatchAPI::convert(bp_fun);
	BPatch_flowGraph 	*bp_fg;

	std::set<BPatch_basicBlock*> bp_bbs; 
	std::set<BPatch_basicBlock*>::iterator bp_bbs_iter; 
	BPatch_basicBlock *bp_block; 

	ArmsFunction	*arms_fun; 
	ArmsBasicBlock	*arms_block, *arms_block_source, *arms_block_target; 
	ArmsEdge		*arms_edge; 
	
	arms_fun = find_function((address_t)bp_fun->getBaseAddr());
 	
	if (!(bp_fg = bp_fun->getCFG())) return;  
	bp_fg->getAllBasicBlocks(bp_bbs);

	/* 1. Insert all basic blocks. Mark all entry and exit ones. */
	for (bp_bbs_iter = bp_bbs.begin(); bp_bbs_iter != bp_bbs.end(); bp_bbs_iter++) {
		bp_block = *bp_bbs_iter; 
		fprintf(stderr, "insert_intraprocedural_function_flow_graph %s: %lx-%lx\n", bp_fun->getName().c_str(), bp_block->getStartAddress(), bp_block->getLastInsnAddress());
		if (find_bb((address_t) bp_block->getStartAddress())) continue; 

		arms_block = new ArmsBasicBlock(
			(address_t) bp_block->getStartAddress(), (address_t) bp_block->getEndAddress(),
			(address_t) bp_block->getLastInsnAddress(), arms_fun, this); 
		arms_block->set_if_entry_block(bp_block->isEntryBlock());
		arms_block->set_if_exit_block(bp_block->isExitBlock());
		store_bb(arms_block); 
	}


	/* 2. Connect basic blocks within this function. */
	for (bp_bbs_iter = bp_bbs.begin(); bp_bbs_iter != bp_bbs.end(); bp_bbs_iter++) {
		bp_block = *bp_bbs_iter; 	

		arms_block_source = find_bb((address_t)(bp_block->getStartAddress())); 	
		
		std::vector<BPatch_edge*> bp_edges; 
		std::vector<BPatch_edge*>::iterator bp_edges_iter; 
		BPatch_edge *bp_edge; 

		bp_block->getOutgoingEdges(bp_edges);

		for(bp_edges_iter = bp_edges.begin(); bp_edges_iter != bp_edges.end(); bp_edges_iter++) {
			bp_edge = *bp_edges_iter; 	
			arms_block_target = find_bb((address_t)(bp_edge->getTarget()->getStartAddress())); 

			if (!arms_block_source || !arms_block_target) {
				continue; 
			} 
			if (arms_block_source->forward_connected_with(arms_block_target)) 
				continue;

			bool intra = (arms_block_source->get_function() == arms_block_target->get_function());

			arms_edge = new ArmsEdge(arms_block_source, arms_block_target, this); 
			copy_edge_type(arms_edge, bp_edge, intra); 
			
			arms_block_source->add_outgoing_edge(arms_edge);
			arms_block_target->add_incoming_edge(arms_edge);
		}
	}

}

void 
DICFG::copy_edge_type(ArmsEdge *arms_edge, BPatch_edge *bp_edge, bool intraprocedural) 
{
	Dyninst::ParseAPI::Edge* edge;
	edge = ParseAPI::convert(bp_edge);

	switch (edge->type()) {
		case ParseAPI::CALL:
			arms_edge->set_type(arms_direct_call); 
			break; 
		case ParseAPI::COND_TAKEN:
			arms_edge->set_type(arms_cond_taken); 
			break; 
		case ParseAPI::COND_NOT_TAKEN:
			arms_edge->set_type(arms_cond_not_taken); 
			break; 
		case ParseAPI::INDIRECT:
			arms_edge->set_type(arms_indirect_jmp); 
			break; 
		case ParseAPI::DIRECT:
			arms_edge->set_type((intraprocedural) ? arms_direct_jmp : arms_inter_direct_jmp); 
			break; 
		case ParseAPI::FALLTHROUGH:
			arms_edge->set_type(arms_fallthrough); 
			break; 
		case ParseAPI::CATCH:
			arms_edge->set_type(arms_catch); 
			break; 
		case ParseAPI::CALL_FT:        // fallthrough after call instruction
			arms_edge->set_type(arms_call_ft); 
			break; 
		case ParseAPI::RET:
			arms_edge->set_type(arms_ret); 
			break; 
		case ParseAPI::NOEDGE:
			arms_edge->set_type(arms_no_edge); 
			break; 
		case ParseAPI::_edgetype_end_:
			arms_edge->set_type(arms_unknown); 
			break; 
		default: 
			arms_edge->set_type(arms_unknown); 
			break; 
	}
} 


void 
DICFG::set_entry_and_exit_points_of_function(BPatch_function *bp_fun)
{
	ArmsFunction *arms_fun = find_function((address_t)bp_fun->getBaseAddr()); 
	assert(arms_fun); 

	/* Entry points */
	BPatch_Vector<BPatch_point *> entry_points;
	BPatch_Vector<BPatch_point *>::iterator entry_points_iter;
	address_t entry_address;

	bp_fun->getEntryPoints(entry_points);
	for(entry_points_iter = entry_points.begin(); entry_points_iter != entry_points.end(); entry_points_iter++) {
		entry_address = (address_t)(*entry_points_iter)->getAddress(); 
		arms_fun->add_entry_block(entry_address); 
	}

	/* Exit points */
	BPatch_Vector<BPatch_point *> exit_points;
	BPatch_Vector<BPatch_point *>::iterator exit_points_iter;
	address_t exit_address;

	bp_fun->getExitPoints(exit_points);
	for(exit_points_iter = exit_points.begin(); exit_points_iter != exit_points.end(); exit_points_iter++) {
		exit_address = (address_t)(*exit_points_iter)->getAddress(); 
		arms_fun->add_exit_block(exit_address); 
	}
} 


void 
DICFG::insert_plt_entries(Symtab *symtab)
{
	vector<SymtabAPI::relocationEntry> fbt;
	vector<SymtabAPI::relocationEntry>::iterator fbt_iter;

	bool result = symtab->getFuncBindingTable(fbt);
	if (!result)
		return;	

	for (fbt_iter = fbt.begin(); fbt_iter != fbt.end(); fbt_iter++) {
		create_plt_function((*fbt_iter).name(), (address_t)((*fbt_iter).target_addr()));
	}

}

void 
DICFG::insert_corner_case_functions(void)
{
	const char bash_cmd[512] = "cat " PATHARMOR_ROOT "/objdump4cfg | grep \"<register_tm_clones>:\" | awk '{ print $1; }'  ";
	FILE *pipe;

	pipe = popen(bash_cmd, "r");
	if (pipe == NULL) {
		perror("pipe");
		return;
	} 

	uint64_t reg_addr; 
	fscanf(pipe, "%p", (void**)&reg_addr); 

	if (reg_addr) {
		create_dummy_function(string("register_tm_clones"), (address_t)reg_addr);
	}

	pclose(pipe); 
}

void 
DICFG::insert_interprocedural_edges(std::vector<BPatch_function *> *funcs)
{
	std::vector<BPatch_function *>::iterator funcs_iter; 

	for(funcs_iter = funcs->begin(); funcs_iter != funcs->end(); funcs_iter++) {
		BPatch_function *bp_fun = *funcs_iter; 
		ArmsFunction *arms_fun = find_function((address_t)bp_fun->getBaseAddr()); 
		assert(arms_fun); 

		BPatch_Vector<BPatch_point *> call_points;
		BPatch_Vector<BPatch_point *>::iterator call_points_iter;
		bp_fun->getCallPoints(call_points);	
		for(call_points_iter = call_points.begin(); call_points_iter != call_points.end(); call_points_iter++) {
			insert_interprocedural_edge(arms_fun, *call_points_iter);
		}
	}
}

void
DICFG::insert_interprocedural_edge(ArmsFunction *arms_fun, BPatch_point *call_point)
{
	/* Check if it's a call or a jmp */
	BPatch_basicBlock* call_bb = call_point->getBlock();
	if (!call_bb) {
		return; 
	}

	std::vector<std::pair<Instruction::Ptr, Dyninst::Address> > insns;
	std::vector<std::pair<Instruction::Ptr, Dyninst::Address> >::reverse_iterator insns_iter;
	Instruction::Ptr call_insn;
	call_bb->getInstructions(insns);
	for(insns_iter = insns.rbegin(); insns_iter != insns.rend(); insns_iter++) {
		if (insns_iter->second == ((Address)call_point->getAddress())) {
			call_insn = insns_iter->first; 
			break; 
		}
	}
	if (!call_insn) {
		return; 
	}
	bool call = (call_insn->getCategory() == c_CallInsn);
	
	/* direct */
	BPatch_function *bp_called_fun = call_point->getCalledFunction(); 
	if (bp_called_fun) {
		arms_edge_type_t type = (call) ? arms_direct_call : arms_inter_direct_jmp; 
		handle_interprocedural(arms_fun, (address_t)call_point->getAddress(), 
			(address_t)(bp_called_fun->getBaseAddr()), type); 
		return;
	} 

	/* indirect or rip-relative */
	Expression::Ptr target_expr; 
	address_t target; 
       	std::vector<void*> targets;
       	int ret;

	target_expr = call_insn->getControlFlowTarget(); 
	target_expr->bind(the_pc.get(), Result(u64, (Address)call_point->getAddress())); 
	Result res = target_expr->eval(); 
	if (!res.defined) {
		arms_edge_type_t type = (call) ? arms_indirect_call : arms_inter_indirect_jmp; 
		ret = arms_icall_resolver((void*)call_point->getAddress(), targets);
		if (ret < 0) {
			targets.push_back((void*)arms_fun->get_base_addr());
		}
		for (unsigned i=0;i<targets.size();i++) {
			target = (address_t) targets[i];
                       	handle_interprocedural(arms_fun, (address_t)call_point->getAddress(), target, type);
               	}
	} else {
		target = (address_t)(res.convert<Address>());
		arms_edge_type_t type = (call) ? arms_direct_call : arms_inter_direct_jmp; 
		handle_interprocedural(arms_fun, (address_t)call_point->getAddress(), target, type); 
	}

}


void 
DICFG::analyze_unresolved_control_transfers(std::vector<BPatch_function *> *funcs) 
{
	std::vector<BPatch_function *>::iterator funcs_iter; 

	/* Look at the unresolved control transfers */
	for(funcs_iter = funcs->begin(); funcs_iter != funcs->end(); funcs_iter++) {
		BPatch_function *bp_fun = *funcs_iter; 

		BPatch_Vector<BPatch_point *> unresolvedCFs;
		BPatch_Vector<BPatch_point *>::iterator unresolved_iter; 
		BPatch_point *unresolved_point;
		
		address_t unresolved_instr_addr;

		bp_fun->getUnresolvedControlTransfers(unresolvedCFs);
		for (unresolved_iter = unresolvedCFs.begin(); unresolved_iter != unresolvedCFs.end(); unresolved_iter++) {
			unresolved_point = *unresolved_iter;
			unresolved_instr_addr = (address_t) (unresolved_point->getAddress());

			debug_check_if_cs_remains_unresolved(unresolved_instr_addr); 
		}
	}
}

void DICFG::parse_all(void)
{
	BPatch_addressSpace *handle = cfg_handle;
	SymtabCodeSource *sts = cfg_sts;
	CodeObject *co = cfg_co;

	// Parse the binary 
	co->parse(); 

	/* Parse the functions found by the BPatch API */
	BPatch_image *image = handle->getImage();
	std::vector<BPatch_module *> *mods = image->getModules();
	std::vector<BPatch_module *>::iterator mods_iter; 
	for (mods_iter = mods->begin(); mods_iter != mods->end(); mods_iter++) {
		address_t mod_start = (address_t)(*mods_iter)->getBaseAddr();
		address_t mod_end   = (address_t)(*mods_iter)->getBaseAddr() + (*mods_iter)->getSize();
		if((get_start_addr() == 0) || (mod_start < get_start_addr())) {
			set_start_addr(mod_start);
		}
		if((get_end_addr() == 0) || (mod_end > get_end_addr())) {
			set_end_addr(mod_end);
		}

		std::vector<BPatch_function *> *funcs = (*mods_iter)->getProcedures(false); 
		std::vector<BPatch_function *>::iterator funcs_iter = funcs->begin();
		for(; funcs_iter != funcs->end(); funcs_iter++) {
			co->parse((Address)(*funcs_iter)->getBaseAddr(), true);
		} 
	}

	/* Parse PLT entries */
	Symtab *symtab	= Symtab::findOpenSymtab(string((char *) this->get_module_name().c_str()));
	vector<SymtabAPI::relocationEntry> fbt;
	vector<SymtabAPI::relocationEntry>::iterator fbt_iter;
	symtab->getFuncBindingTable(fbt);

	for (fbt_iter = fbt.begin(); fbt_iter != fbt.end(); fbt_iter++) {
		co->parse((Address)((*fbt_iter).target_addr()), true);
	}

	const CodeObject::funclist& funcs = co->funcs();
	
	insert_functions_and_bbs(funcs);		
	for (fbt_iter = fbt.begin(); fbt_iter != fbt.end(); fbt_iter++) {
		address_t plt_fun_addr = (address_t)(*fbt_iter).target_addr();

		if((get_start_addr() == 0) || (plt_fun_addr < get_start_addr())) {
			set_start_addr(plt_fun_addr);
		}
		if((get_end_addr() == 0) || (plt_fun_addr > get_end_addr())) {
			set_end_addr(plt_fun_addr);
		}

		mark_function_as_plt(plt_fun_addr);
	}
}

void DICFG::retouch(Address entrypoint)
{
	parse_all();

	cfg_co->parse(entrypoint, true);
	
	const CodeObject::funclist& funcs = cfg_co->funcs();

	printf("re-inserting bbs\n");
	
	insert_functions_and_bbs(funcs);		

	printf("re-inserting edges\n");

	insert_edges(funcs);		

	printf("re-inserting done\n");
}

static DICFG* 
try_ParseAPI(BPatch_addressSpace *handle) 
{
	SymtabCodeSource *sts;
	CodeObject *co;

	std::vector<BPatch_object*> objs;
	handle->getImage()->getObjects(objs);
	assert(objs.size() > 0);
	const char *bin = objs[0]->pathName().c_str();

	// Create a new binary object 
	sts 	= new SymtabCodeSource((char*)bin);
	co 	= new CodeObject(sts);

	DICFG *parse_cfg = new DICFG(bin, sts, co, handle); 

	parse_cfg->parse_all();

	const CodeObject::funclist& funcs = co->funcs();

	parse_cfg->insert_edges(funcs);		

	return parse_cfg; 
}


void
dyninst_analyze_address_taken(BPatch_addressSpace *handle, DICFG *cfg)
{
	/* XXX: this is the most naive address-taken analysis that can be used by the
         * lbr_analysis_pass. More sophisticated ones can be (and are) plugged in in the pass.
         * This naive solution is provided only for comparison with more sophisticated ones.
	 * 
         * This analysis looks for instruction operands that correspond to known function addresses,
         * and then marks these functions as having their address taken. In particular, we
         * do /not/ look for function pointers stored in (static) memory, or for function
         * pointers that are computed at runtime. 
         */

	SymtabCodeSource *sts;
	CodeObject *co;

	std::vector<BPatch_object*> objs;
	handle->getImage()->getObjects(objs);
	assert(objs.size() > 0);
	const char *bin = objs[0]->pathName().c_str();

	// Create a new binary object 
	sts 	= new SymtabCodeSource((char*)bin);
	co 	= new CodeObject(sts);

	// Parse the binary 
	co->parse(); 

	BPatch_image *image = handle->getImage();
	std::vector<BPatch_module *> *mods = image->getModules();
	std::vector<BPatch_module *>::iterator mods_iter; 
	for (mods_iter = mods->begin(); mods_iter != mods->end(); mods_iter++) {
		std::vector<BPatch_function *> *funcs = (*mods_iter)->getProcedures(false); 
		std::vector<BPatch_function *>::iterator funcs_iter = funcs->begin();
		for(; funcs_iter != funcs->end(); funcs_iter++) {
			co->parse((Address)(*funcs_iter)->getBaseAddr(), true);
			BPatch_flowGraph *fg = (*funcs_iter)->getCFG();
			std::set<BPatch_basicBlock*> blocks;
			fg->getAllBasicBlocks(blocks);
			std::set<BPatch_basicBlock*>::iterator block_iter;
			for (block_iter = blocks.begin(); block_iter != blocks.end(); ++block_iter) {
				BPatch_basicBlock *block = (*block_iter);
				std::vector<Instruction::Ptr> insns;
				block->getInstructions(insns);
				std::vector<Instruction::Ptr>::iterator insn_iter;
				for (insn_iter = insns.begin(); insn_iter != insns.end(); ++insn_iter) {
					Instruction::Ptr ins = *insn_iter;
					std::vector<Operand> ops;
					ins->getOperands(ops);
					std::vector<Operand>::iterator op_iter;
					for (op_iter = ops.begin(); op_iter != ops.end(); ++op_iter) {
						Expression::Ptr expr = (*op_iter).getValue();

						struct OperandAnalyzer : public Dyninst::InstructionAPI::Visitor {
							virtual void visit(BinaryFunction* op) {};
							virtual void visit(Dereference* op) {}
							virtual void visit(Immediate* op) {
								address_t addr;
								ArmsFunction *func;
								switch(op->eval().type) {
								case s32:
									addr = op->eval().val.s32val;
									break;
								case u32:
									addr = op->eval().val.u32val;
									break;
								case s64:
									addr = op->eval().val.s64val;
									break;
								case u64:
									addr = op->eval().val.u64val;
									break;
								default:
									return;
								}
								func = cfg_->find_function(addr);
								if(func) {
									printf("Instruction [%s] references function 0x%jx\n", ins_->format().c_str(), addr);
									func->set_addr_taken();
								}
							}
							virtual void visit(RegisterAST* op) {}
							OperandAnalyzer(DICFG *cfg, Instruction::Ptr ins) {
								cfg_ = cfg;
								ins_ = ins;
							};
							DICFG *cfg_;
							Instruction::Ptr ins_;
						};

						OperandAnalyzer oa(cfg, ins);
						expr->apply(&oa);
					}
				}
			}
		} 
	}
}


DICFG*
dyninst_build_cfg(BPatch_addressSpace *handle)
{
	DICFG *cfg = try_ParseAPI(handle);
	return cfg;
}

