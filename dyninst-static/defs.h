#ifndef __CFG_DEFS__
#define __CFG_DEFS__

#include "BPatch.h"

typedef uint64_t address_t; 
const address_t DUMMY_ADDR = 0x2910; 

#ifdef DYNINST_8_2
#define PARSE_API_RET_BLOCKLIST ParseAPI::Function::const_blocklist
#else
#define PARSE_API_RET_BLOCKLIST ParseAPI::Function::blocklist
#endif

#endif 
