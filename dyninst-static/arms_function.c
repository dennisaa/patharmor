#include <stdio.h>
#include <stdlib.h>
#include <assert.h> 
#include <stdint.h> 

using namespace std; 

#include <set>
#include <map> 
#include <string> 
#include <vector> 

#include "defs.h"
#include "arms_utils.h"
#include "arms_bb.h"
#include "arms_edge.h"
#include "arms_cfg.h" 

#include "arms_function.h" 

string ArmsFunction::fun_dummy_name = string("NA"); 

void
ArmsFunction::add_bb(ArmsBasicBlock *bb)
{
	basic_blocks.insert(bb);
}

void 
ArmsFunction::add_entry_block(address_t bb_addr) 
{
	ArmsBasicBlock *bb = cfg->find_bb(bb_addr);
	if (!bb) return; 
	entry_points.insert(bb);	
}

void 
ArmsFunction::add_entry_block(ArmsBasicBlock *bb) 
{
	entry_points.insert(bb);	
}

ArmsBasicBlock*
ArmsFunction::get_entry_point(size_t i)
{
  typedef std::set<ArmsBasicBlock*>::iterator block_iter;

  block_iter iter;

  iter = entry_points.begin();
  std::advance(iter, i);

  return (*iter);
}

ArmsBasicBlock*
ArmsFunction::get_exit_point(size_t i)
{
  typedef std::set<ArmsBasicBlock*>::iterator block_iter;

  block_iter iter;

  iter = exit_points.begin();
  std::advance(iter, i);

  return (*iter);
}

int
ArmsFunction::foreach_entry_block(int (*callback)(ArmsBasicBlock*,void*), void *arg)
{
    typedef std::set<ArmsBasicBlock*>::iterator block_iter;

    int ret;
    block_iter iter;
    ArmsBasicBlock *entry;

    if(entry_points.size() < 1) {
        return 0;
    }

    for(iter = entry_points.begin(); iter != entry_points.end(); iter++) {
        entry = *iter;
        if((ret = callback(entry, arg))) {
            return ret;
        }
    }

    return 0;
}

int
ArmsFunction::foreach_inbound_edge(int (*callback)(ArmsEdge*,void*), void *arg)
{
    typedef std::set<ArmsBasicBlock*>::iterator block_iter;

    int ret;
    size_t i;
    block_iter iter;
    ArmsBasicBlock *entry;

    if(entry_points.size() < 1) {
        return 0;
    }

    for(iter = entry_points.begin(); iter != entry_points.end(); iter++) {
        entry = *iter;
        for(i = 0; i < entry->incoming_edge_count(); i++) {
            if((ret = callback(entry->get_incoming_edge(i), arg))) {
                return ret;
            }
        }
    }

    return 0;
}

void 
ArmsFunction::add_exit_block(address_t bb_last_insn_addr) 
{
	ArmsBasicBlock *bb = cfg->find_bb_by_last_insn_address(bb_last_insn_addr);
	if (!bb) return; 
	exit_points.insert(bb);	
}

void 
ArmsFunction::add_exit_block(ArmsBasicBlock *bb) 
{
	exit_points.insert(bb);	
}

string 
ArmsFunction::to_string(void)
{
	return string_format("Fun(%p)", (void*)base_addr);
}
