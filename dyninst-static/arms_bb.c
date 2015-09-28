#include <stdio.h>
#include <stdint.h> 
#include <limits.h>

using namespace std; 

#include <string> 
#include <vector> 
#include <set> 
#include <algorithm> 
#include <map>

#include "defs.h" 
#include "arms_utils.h" 
#include "arms_edge.h" 
#include "arms_function.h"
#include "arms_cfg.h"

#include "arms_bb.h"

uint64_t ArmsBasicBlock::global_id        = 0; 
uint64_t ArmsBasicBlock::num_dummy_blocks = 0;

void 
ArmsBasicBlock::set_is_entry_block(void) 
{
	is_entry_block_ = true; 
}

void 
ArmsBasicBlock::set_if_entry_block(bool entry)
{
	if (entry) set_is_entry_block(); 
}

void 
ArmsBasicBlock::set_is_exit_block(void) 
{
	is_exit_block_ = true; 
}

void 
ArmsBasicBlock::set_if_exit_block(bool exit)
{
	if (exit) set_is_exit_block(); 
}

string 
ArmsBasicBlock::to_string(void)
{
	return string_format("%sBB(%p)", is_dummy() ? "d" : "", (void*)start_address);
}

void 
ArmsBasicBlock::add_incoming_edge(ArmsEdge *edge)
{
	incoming_edges.insert(edge); 
}

int
ArmsBasicBlock::foreach_incoming_edge(int (*callback)(ArmsEdge*,void*), void *arg)
{
    typedef std::set<ArmsEdge*>::iterator edge_iter;

    int ret;
    edge_iter iter;
    ArmsEdge *edge;

    if(incoming_edges.size() < 1) {
        return 0;
    }

    for(iter = incoming_edges.begin(); iter != incoming_edges.end(); iter++) {
        edge = *iter;
        if((ret = callback(edge, arg))) {
            return ret;
        }
    }

    return 0;
}

size_t
ArmsBasicBlock::incoming_edge_count()
{
    return incoming_edges.size();
}

ArmsEdge*
ArmsBasicBlock::get_incoming_edge(size_t i)
{
    typedef std::set<ArmsEdge*>::iterator edge_iter;

    edge_iter iter;
    
    if(i >= incoming_edges.size()) {
        return NULL;
    }

    iter = incoming_edges.begin();
    std::advance(iter, i);

    return *iter;
}

void
ArmsBasicBlock::delete_incoming_edge(ArmsEdge *e)
{
    incoming_edges.erase(e);
}

void
ArmsBasicBlock::add_outgoing_edge(ArmsEdge *edge)
{
	outgoing_edges.insert(edge); 
}

size_t
ArmsBasicBlock::outgoing_edge_count(void)
{
    return outgoing_edges.size();
}

ArmsEdge*
ArmsBasicBlock::get_outgoing_edge(size_t i)
{
    typedef std::set<ArmsEdge*>::iterator edge_iter;

    edge_iter iter;
    
    if(i >= outgoing_edges.size()) {
        return NULL;
    }

    iter = outgoing_edges.begin();
    std::advance(iter, i);

    return *iter;
}

void
ArmsBasicBlock::delete_outgoing_edge(ArmsEdge *e)
{
    outgoing_edges.erase(e);
}

bool
ArmsBasicBlock::has_outbound_fastpath(ArmsBasicBlock *bb)
{
    size_t i;

    for(i = 0; i < outgoing_edge_count(); i++) {
        if(get_outgoing_edge(i)->target() == bb &&
           get_outgoing_edge(i)->is_fastpath()) {
            return true;
        }
    }

    return false;
}

/* returns true if the bb has (among the others) a non call fallthrough edge; 
 * in principle, it means that a call site was resolved already
 */
bool
ArmsBasicBlock::has_no_call_ft_outgoing_edge(void)
{
    set<ArmsEdge*>::iterator iter;
    ArmsEdge *edge;

    for(iter = outgoing_edges.begin(); iter != outgoing_edges.end(); iter++) {
        edge = *iter;
		if (edge->is_no_call_ft()) return true; 
    }

	return false; 
}


/* Drop an outgoing call_ft edge (there should be just one!). Also, make the target of the edge drop it as well. */
void 
ArmsBasicBlock::drop_call_ft_edge(void)
{
    std::set<ArmsEdge*>::iterator iter;
	std::set<ArmsEdge*>::iterator iter_to_drop = outgoing_edges.end(); 

	for(iter = outgoing_edges.begin(); iter != outgoing_edges.end(); iter++) {
		ArmsEdge *edge = *iter;
		if (edge->get_type() == arms_call_ft) {
			iter_to_drop = iter; 
			break;
		}
	}

	if (iter_to_drop == outgoing_edges.end()) { 
		return; 
	}

	(*iter_to_drop)->get_target()->drop_incoming_call_ft_edge(); 
	outgoing_edges.erase(iter_to_drop);
}

/* Drop an outgoing call_ft edge (there should be just one!). Also, make the target of the edge drop it as well. */
void 
ArmsBasicBlock::drop_incoming_call_ft_edge(void)
{
    std::set<ArmsEdge*>::iterator iter;
	std::set<ArmsEdge*>::iterator iter_to_drop = incoming_edges.end(); 

	for(iter = incoming_edges.begin(); iter != incoming_edges.end(); iter++) {
		ArmsEdge *edge = *iter;
		if (edge->get_type() == arms_call_ft) {
			iter_to_drop = iter; 
			break; 
		}
	}

	if (iter_to_drop == incoming_edges.end()) { 
		return; 
	}

	incoming_edges.erase(iter_to_drop);
}


bool
ArmsBasicBlock::is_ft(void) 
{
	if (incoming_edges.size() != 1) 
		return false;

    std::set<ArmsEdge*>::iterator iter;
	for(iter = incoming_edges.begin(); iter != incoming_edges.end(); iter++) {
		ArmsEdge *edge = *iter;
		if (edge->get_type() == arms_fallthrough) {
			return true; 
		}
	}

	return false;
}


bool
ArmsBasicBlock::outgoing_is_ft(void) 
{
	if (outgoing_edges.size() != 1) 
		return false;

    std::set<ArmsEdge*>::iterator iter;
	for(iter = outgoing_edges.begin(); iter != outgoing_edges.end(); iter++) {
		ArmsEdge *edge = *iter;
		if (edge->get_type() == arms_fallthrough) {
			return true; 
		}
	}

	return false;
}


int 
ArmsBasicBlock::foreach_function(void (*callback)(ArmsFunction*,ArmsBasicBlock*), ArmsBasicBlock*arg)
{
    std::vector<ArmsFunction*>::iterator fun_iter;

    for(fun_iter = funcs.begin(); fun_iter != funcs.end(); fun_iter++) {
        callback(*fun_iter, arg); 
    }

    return 0;
}


/* provided there is exactly one */
ArmsBasicBlock*
ArmsBasicBlock::get_preceding_bb(void)
{
	if (incoming_edges.size() != 1) 
		return 0;

    std::set<ArmsEdge*>::iterator iter;
	for(iter = incoming_edges.begin(); iter != incoming_edges.end(); iter++) {
		ArmsEdge *edge = *iter;
		return edge->get_source(); 
	}

	return 0;
}

/* provided there is exactly one */
ArmsBasicBlock*
ArmsBasicBlock::get_following_bb(void)
{
	if (outgoing_edges.size() != 1) 
		return 0;

    std::set<ArmsEdge*>::iterator iter;
	for(iter = outgoing_edges.begin(); iter != outgoing_edges.end(); iter++) {
		ArmsEdge *edge = *iter;
		return edge->get_target(); 
	}

	return 0;
}

ArmsBasicBlock*
ArmsBasicBlock::get_fallthrough_bb(void)
{
	/* XXX: Assume the fallthrough bb is the closest one preceding the last instruction
	 * of this bb. This WILL be incorrect for blocks that don't actually have a fallthrough. */

	size_t i;
	ArmsBasicBlock *bb;

	for(i = 1; i < 16 /* an x86 instruction cannot be larger than this */; i++) {
		bb = cfg_->find_bb(last_insn_address + i);
		if(bb) return bb;
	}

	return NULL;
}

ArmsBasicBlock*
ArmsBasicBlock::create_dummy_basic_block(ArmsFunction *fun, CFG *cfg)
{
	uint64_t dummy_addr = DUMMY_ADDR + (++ArmsBasicBlock::num_dummy_blocks); 
	ArmsBasicBlock *bb = new ArmsBasicBlock(dummy_addr, dummy_addr, dummy_addr, fun, cfg); 	
	bb->set_is_dummy();
	return bb; 
}

ArmsBasicBlock*
ArmsBasicBlock::create_dummy_basic_block(address_t dummy_addr, ArmsFunction *fun, CFG *cfg)
{
	num_dummy_blocks++;
	ArmsBasicBlock *bb = new ArmsBasicBlock(dummy_addr, dummy_addr+1, dummy_addr, fun, cfg); 	
	bb->set_is_dummy();
	return bb; 
}

ArmsBasicBlock*
ArmsBasicBlock::create_dummy_basic_block(address_t dummy_start, address_t dummy_end, ArmsFunction *fun, CFG *cfg)
{
	num_dummy_blocks++;
	ArmsBasicBlock *bb = new ArmsBasicBlock(dummy_start, dummy_end, dummy_start, fun, cfg); 	
	bb->set_is_dummy();
	return bb; 
}

bool
ArmsBasicBlock::forward_connected_with(ArmsBasicBlock *bb)
{
    set<ArmsEdge*>::iterator iter;
    ArmsEdge *edge;

    for(iter = outgoing_edges.begin(); iter != outgoing_edges.end(); iter++) {
        edge = *iter;
		if (edge->get_target() == bb) return true; 
    }

	return false; 
}

void
ArmsBasicBlock::get_forward_connected_bbs(vector<ArmsBasicBlock*>& forward_connected, bool& all_indirect) 
{
    std::set<ArmsEdge*>::iterator iter;
    ArmsEdge *edge;

	all_indirect = true; 

    for(iter = outgoing_edges.begin(); iter != outgoing_edges.end(); iter++) {
        edge = *iter;
		forward_connected.push_back(edge->get_target()); 
		if (!edge_type_is_indirect(edge->type())) all_indirect = false;
    }

	std::sort(forward_connected.begin(), forward_connected.end());
}

void
ArmsBasicBlock::get_forward_connected_bbs(vector<address_t>& forward_connected, bool& all_indirect) 
{
    std::set<ArmsEdge*>::iterator iter;
    ArmsEdge *edge;

	all_indirect = true; 

    for(iter = outgoing_edges.begin(); iter != outgoing_edges.end(); iter++) {
        edge = *iter;
		forward_connected.push_back(edge->get_target()->get_start_address()); 
		if (!edge_type_is_indirect(edge->type())) all_indirect = false;
    }

	std::sort(forward_connected.begin(), forward_connected.end());
}

void 
ArmsBasicBlock::print_forward_connected_bbs(vector<address_t>& forward_connected)
{
	if (forward_connected.size() <= 0) return;
	
	for(unsigned int i = 0; i < forward_connected.size(); i++) { 
		if (i > 8) break; 
	}
}

void
ArmsBasicBlock::print_subgraph(int level)
{
	int i;
        ArmsEdge *edge;
	ArmsFunction *fun = get_function();
        std::set<ArmsEdge*>::iterator iter;

	fprintf(stderr, "%*sBB %s, %lu outgoing edges, function %s\n", level*5+5, "", to_string().c_str(), outgoing_edges.size(), fun->get_name().c_str());

        for(iter = outgoing_edges.begin(); iter != outgoing_edges.end(); iter++) {
            edge = *iter;
	    fprintf(stderr, "%*sedge %s\n", level*5+5, "", edge->to_string().c_str());
	    edge->get_target()->print_subgraph(level+1);
        }
}

bool
ArmsBasicBlock::intra_procedural_indirect_jump_targets_only(void)
{
	int i;
        ArmsEdge *edge;
	ArmsFunction *fun = get_function();
        std::set<ArmsEdge*>::iterator iter;

        if(outgoing_edges.size() == 0) { return false; }

        for(iter = outgoing_edges.begin(); iter != outgoing_edges.end(); iter++) {
            edge = *iter;
	    ArmsFunction *fun_target = edge->target()->get_function();
	    if(!edge->is_intraprocedural() || !edge->is_indirect_jump()) {
		return false;
	    }
        }

        return true;
}

void 
ArmsBasicBlock::compare_edges(ArmsBasicBlock *other_bb)
{
	std::vector<address_t> forward_connected; 
	std::vector<address_t> other_forward_connected; 
	
	bool all_indirect, other_all_indirect;

	get_forward_connected_bbs(forward_connected, all_indirect);
	other_bb->get_forward_connected_bbs(other_forward_connected, other_all_indirect);

	std::vector<address_t> diff(forward_connected.size() + other_forward_connected.size());
	std::vector<address_t>::iterator it;

	it = std::set_symmetric_difference(
		forward_connected.begin(), forward_connected.end(),
		other_forward_connected.begin(), other_forward_connected.end(), 
		diff.begin());
	diff.resize(it - diff.begin());

	// recognize and filter our plts: we have a dummy jump target 
	if ((diff.size() == 1) && 
		(forward_connected.size() == 1) && (other_forward_connected.size() == 0)) {
		ArmsFunction *fun = get_function();
		if (fun && fun->is_plt()) return; 
	}

	if ((diff.size() > 0) && (forward_connected.size() == 1) && 
		(outgoing_is_ft()) && (!other_bb->outgoing_is_ft())) {
		ArmsBasicBlock *following_bb = get_following_bb();
		return following_bb->compare_edges(other_bb);  
	}

	/* indirects not recognized by ida */
	if ((diff.size() > 0) && (all_indirect) && (other_forward_connected.size() == 0)) {
		return; 
	} 

	if (diff.size() > 0) { 
		print_forward_connected_bbs(forward_connected);
		other_bb->print_forward_connected_bbs(other_forward_connected);
	}
}
