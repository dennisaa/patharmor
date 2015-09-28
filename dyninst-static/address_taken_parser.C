#include <set>
#include <stdlib.h>
#include <stdint.h>

#include <forward_list>
#include <fstream>
#include <istream>

#include "BPatch.h"
#include "BPatch_addressSpace.h" 
#include "BPatch_binaryEdit.h" 
#include "BPatch_basicBlock.h"
#include "BPatch_flowGraph.h" 
#include "BPatch_function.h"
#include "BPatch_memoryAccessAdapter.h"
#include "BPatch_object.h"
#include "BPatch_point.h"

#include "binaryEdit.h"

#include <dynProcess.h>

#include <Immediate.h>
#include <Instruction.h>
#include <Operand.h>
#include <Symbol.h>
#include <Symtab.h>

#include "PatchCommon.h"

#include "address_taken_parser.h"

#define PRINT_DEBUG 1

#if PRINT_DEBUG > 0
#define DEBUG(...) do { fprintf(stderr, __VA_ARGS__);fflush(stderr); } while(0)
#define PRINT(...) do { fprintf(stdout, __VA_ARGS__); } while(0)
#else
#define DEBUG(...) do { /* nothing */ } while(0)
#define PRINT(...) do { /* nothing */ } while(0)
#endif

#if PRINT_DEBUG > 1
#define PRINT_PLACE() do {fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __func__); } while(0)
#else
#define PRINT_PLACE() do { /* nothing */ } while(0)
#endif

/******************************************************************************
 *                         AddressTakenParserBase	                      *
 ******************************************************************************/
/* This policy implements shared functionality, but is not used as-is.
 ******************************************************************************/
class AddressTakenParserBase : public AddressTakenParser {
protected:
	std::set< uint64_t > _callSites;
	std::set< uint64_t > _addressesTaken;
	BPatch_addressSpace *_AddressSpace;
	CFG *_cfg;

	virtual void reset(void) = 0;

public:
	AddressTakenParserBase() : _callSites(), _addressesTaken() {
		PRINT_PLACE();

		_AddressSpace = NULL;
		_cfg = NULL;
	}

	/* ************************************************************************/
	void set(BPatch_addressSpace *as, CFG *cfg) {
		PRINT_PLACE();

		_AddressSpace = as;
		_cfg = cfg;
		reset();
	}

	/* ************************************************************************/
	std::set<uint64_t> *getCallSites(void) {
		PRINT_PLACE();

		std::set<uint64_t> * tmp = new std::set<uint64_t>();
		for(auto it = _callSites.begin(); it != _callSites.end(); ++it) {
			tmp->insert(*it);
		}
		return tmp;
	}

	/* ************************************************************************/
	std::set<uint64_t> *getAddressesTaken(void) {
		PRINT_PLACE();

		std::set<uint64_t> * tmp = new std::set<uint64_t>();
		for(auto it = _addressesTaken.begin(); it != _addressesTaken.end(); ++it) {
			tmp->insert(*it);
		}
		return tmp;
	}

	/* ************************************************************************/
	void show(void) {
		PRINT_PLACE();

		std::cout << "Using " << name() << " as address taken policy\n";
	}

	/* ************************************************************************/
	virtual const std::string name(void) {
		return "base";
	}
};

/******************************************************************************
 *                         AddressTakenParserBinCFI	                      *
 ******************************************************************************/
/* This policy implements the policy as found in:
 * "Control Flow Integrety for COTS Binaries" - Mingwei Zhang and R. Sekar
 ******************************************************************************/
class AddressTakenParserBinCFI : public AddressTakenParserBase {
private:
	uint64_t _textBase, _textSize;
	uint64_t _dataBase, _dataSize;

	class ref_at {
		public:
			typedef enum ref_type { ABS, OFFSET, BASE_OFFSET, REL_OFFSET, PLT_OFFSET, GOT_OFFSET } ref_type_t;

			uint64_t *location;
			uint64_t value;
			ref_type_t type;

			ref_at(uint64_t *_loc, uint64_t _val, ref_type_t _type)
				: location(_loc), value(_val), type(_type) {
				/* Nothing for now. */
			};
	};
	
	/* The following sets contain possible addresses which might be
	 * taken, some need to be filtered out to make sure they actually do
	 * point to the start of a function.
	 * 
	 * All need to be checked that they are actually taken, before
	 * moving them to _addressesTaken, which is the set which is
	 * returned to the clients of these classes. */
	std::list< struct ref_at * > _raw_CK; // raw constant, actual pointer to a function
	std::list< struct ref_at * > _raw_CC; // Any kind of offset which might be used as a relative pointer

	std::vector< SymtabAPI::relocationEntry > _plt;
	std::set< uint64_t > _addressesTakenMaybe;

	void parseRaw(BPatch_image *image, uint64_t base, size_t size, BPatch_object::Region::type_t type) {
		char *p;
		char *buf = (char *)malloc(size);
		uint64_t target;

		PRINT_PLACE();

		/* We want to parse the raw data of the file, so we have to do
		 * the following:
		 * 1. Retrieve the memory section
		 * 2. Read it into a buffer
		 * 3. Parse the raw data.
		 */

		assert(_textBase != 0);
		assert(_textSize != 0);
		
		/* Get a low level representation so we can read values from the
		 * address space. */
		BPatch_binaryEdit *proc = dynamic_cast<BPatch_binaryEdit*>(_AddressSpace);
		BinaryEdit *lle = proc->lowlevel_edit();
	
		assert(lle != NULL);
		assert(buf != NULL);
		assert(image != NULL);
		
		if (BPatch_object::Region::CODE == type) {
			lle->readTextSpace((void *)base, size, buf);
		} else {
			lle->readDataSpace((void *)base, size, buf, true);
		}

		// Substract the size of the pointer type of the binary
		for(p = buf; p < buf + size - 8; p++) {
			uint64_t *val = (uint64_t *)p;
			uint64_t *pos = val - (uint64_t *)buf + (uint64_t *)base;
			
			// Check it is within Text section range, and points to a
			// function entry.
			if ((*val >= _textBase) &&
				(*val < _textBase + _textSize) &&
				(isFunctionStart(*val))) {
				DEBUG("## ABS : %p:%jx\n", pos, *val);
				_raw_CK.push_front(new ref_at(pos, *val, ref_at::ABS));
			}

			if ((*val < _textSize)) {
				//~ if (0 != *val)
					//~ DEBUG("## REL : %p:%jx\n", pos, *val);
				_raw_CC.push_front(new ref_at(pos, *val, ref_at::OFFSET));
			}
		}

#if PRINT_DEBUG > 2
		for(auto it = _raw_CK.begin(); it != _raw_CK.end(); ++it) 
			DEBUG("### AT Possible pointer CK: %p: %#lx\n", (*it)->location, (*it)->value);

		for(auto it = _raw_CC.begin(); it != _raw_CC.end(); ++it) 
			DEBUG("### AT Possible pointer CK: %p: %#lx\n", (*it)->location, (*it)->value);
#endif

		free(buf);
	}

	void findCKs(std::vector< BPatch_object * > *objs, BPatch_image *image) {
		PRINT_PLACE();

		for(auto et = objs->begin(); et != objs->end(); ++et) {
			std::vector< BPatch_object::Region > regions;
			(*et)->regions(regions);

			for(auto it = regions.begin(); it != regions.end(); ++it) {
				switch(it->type) {
				case BPatch_object::Region::CODE:
				case BPatch_object::Region::DATA:
					parseRaw(image, it->base, it->size, it->type);
					break;
				default:
					DEBUG("AT - findCK(): Unknown region type\n");
					break;
				}
			}
		}
	}

	bool isPLTEntryOffset(const uint64_t val, uint64_t *target) {
		for(auto it = _plt.begin(); it != _plt.end(); ++it) {
			if (val == it->rel_addr()) {
				*target = it->target_addr();
				return true;
			}
		}
		return false;
	}

	bool isFunctionStart(const uint64_t val) {
		std::vector<BPatch_function *> functions;
		_AddressSpace->getImage()->findFunction(val, functions);

		for(auto it = functions.begin(); it != functions.end(); ++it) {
			Dyninst::Address start, end;
			if ((*it)->getAddressRange(start, end) && (start == val)) {
				return true;
			}
		}

		return false;
	}

	bool validateRawCK(uint64_t *value, uint64_t base) {
		auto it = _raw_CK.begin();
		while(it != _raw_CK.end()) {
			if ((*it)->location == value + base) {
				/* raw_CK contains only valid pointer to functions. */
				DEBUG("** Found CK->CK %p -> %#lx\n", (*it)->location, (*it)->value);

				_addressesTaken.insert((*it)->value);

				it = _raw_CK.erase(it);
			} else {
				it++;
			}
		}

		return false;
	}

	bool validateRawCC(uint64_t *value, uint64_t base, uint64_t rip) {
		return false;
	}

	void parseInstruction(InstructionAPI::Instruction::Ptr *ins_ptr, uint64_t start, uint64_t end) {
		bool isIndirect = false;
		bool isPIC = false;
		auto instruction = *ins_ptr;
		uint64_t CK = ~0LLU, CC = ~0LLU;
		uint64_t target;

		class OperandAnalyzerIsIndirect : public Dyninst::InstructionAPI::Visitor {
		public:
			bool &_isIndirect;
			bool &_isPIC;
			
			OperandAnalyzerIsIndirect(bool &indirect, bool &pic) : _isIndirect(indirect), _isPIC(pic) {
			}

			virtual void visit(InstructionAPI::BinaryFunction* op) {}
			virtual void visit(InstructionAPI::Dereference* op) {
				// This is a dereference, hence this might be an indirect call
				_isIndirect = true;
			}
			virtual void visit(InstructionAPI::RegisterAST* op) {
				// If RIP is used, it is most likely a PIC reference.
				_isPIC = (_isPIC || (op->getID() == (x86_64::irip)));

				// If any register but RIP is used this might be an indirect reference.
				_isIndirect = !_isPIC;
			}
			virtual void visit(InstructionAPI::Immediate* op) {}
		};

		class OperandAnalyzerIsPossibleAT : public Dyninst::InstructionAPI::Visitor {
		public:
			uint64_t &_CK, &_CC;

			const uint64_t _textBase;
			const uint64_t _textSize;
			const uint64_t _dataBase;
			const uint64_t _dataSize;
			const uint64_t _location;
			address_t _accumulator;
			size_t _nbImmediates;

			OperandAnalyzerIsPossibleAT(
				const uint64_t textBase, const uint64_t textSize,
				const uint64_t dataBase, const uint64_t dataSize,
				uint64_t location,
				uint64_t &CK, uint64_t &CC)
				: _textBase(textBase), _textSize(textSize),
				  _dataBase(dataBase), _dataSize(dataSize),
				  _location(location),
				  _CK(CK), _CC(CC) {
				_accumulator = 0;
				_nbImmediates = 0;
			}

			void isPossibleValues(uint64_t base) {
				if (1 == _nbImmediates) {
					if ((_textBase <= _accumulator + base) &&
						(_textBase + _textSize > _accumulator + base)) {
						_CK = _accumulator;
					}
					if ((_dataBase <= _accumulator + base) &&
						(_dataBase + _dataSize > _accumulator + base)) {
						_CK = _accumulator;
					}
				}

				if (1 < _nbImmediates) {
					_CK = ~0LLU;
				}
				_CC = _accumulator;
			}

			virtual void visit(InstructionAPI::BinaryFunction* op) {}
			virtual void visit(InstructionAPI::Dereference* op) {}
			virtual void visit(InstructionAPI::RegisterAST* op) {
				if (op->getID() == (x86_64::irip)) {
					isPossibleValues(_location);
				}
			}
			virtual void visit(InstructionAPI::Immediate* op) {
				address_t addr = 0;

				switch(op->eval().type) {
				case InstructionAPI::s32:
					addr = op->eval().val.s32val;
					break;
				case InstructionAPI::u32:
					addr = op->eval().val.u32val;
					break;
				case InstructionAPI::s64:
					addr = op->eval().val.s64val;
					break;
				case InstructionAPI::u64:
					addr = op->eval().val.u64val;
					break;
				default:
					return;
				}

				_accumulator += addr;
				_nbImmediates++;

				isPossibleValues(0);
			}
		};

		// Analyze the operands
		std::vector<InstructionAPI::Operand> ops;
		instruction->getOperands(ops);
		for (auto it = ops.begin(); !isIndirect && it != ops.end(); ++it) {
			InstructionAPI::Expression::Ptr expr = (*it).getValue();

			// Look for Indirect references
			OperandAnalyzerIsIndirect oaIsIndirect(isIndirect, isPIC);
			expr->apply(&oaIsIndirect);

			// Look for possible code pointers
			OperandAnalyzerIsPossibleAT oaIsPossibleAT(_textBase, _textSize, _dataBase, _dataSize, start, CK, CC);
			expr->apply(&oaIsPossibleAT);
		}

		if (CK != ~0LLU && !_raw_CK.empty()) {
			// Extensive search gets slower and slower, so this is used 
			// to short-circuit and stop searching as soon as we have at
			// least one hit. 
			bool targetFound = false;

			// Look for a possible Pointer constant at the CK position 
			// which points to a valid function.
			targetFound = validateRawCK((uint64_t *)CK, start);
			targetFound = targetFound || validateRawCK((uint64_t *)CK, _textBase);
			targetFound = targetFound || validateRawCK((uint64_t *)CK, _dataBase);

			// Is the pointed value a possible offset to a known function, PIC-based ?
			targetFound = targetFound || validateRawCC((uint64_t *)CK, 0, start);
		}

		// Analyze Instructions.
		InstructionAPI::Operation op = instruction->getOperation();
		switch(op.getID()) {
		case e_call:
			{
				if (!isIndirect && !isPIC) {
					//~ DEBUG("## Direct %s\n", instruction->format().c_str());
					break;
				}

				if (isPIC) {
					//~ DEBUG("## Indirect, PIC %s\n", instruction->format().c_str());
					break;
				}

				//~ DEBUG("## Indirect call: %#jx: %s\n", end, instruction->format().c_str());
				_callSites.insert(end);
				break;
			}
		default:
			{
				//~ DEBUG("## %s\n", instruction->format().c_str());
				break;
			}
		}
	}

	void parseBlock(BPatch_basicBlock *block) {
		std::vector<InstructionAPI::Instruction::Ptr> instructions;
		const uint64_t start = block->getStartAddress();
		const uint64_t end = block->getEndAddress();

		PRINT_PLACE();

		// loop over all assembly instructions
		block->getInstructions(instructions);
		for(auto it = instructions.begin(); it != instructions.end(); it++) {
			InstructionAPI::Instruction::Ptr ins = *it;
			parseInstruction(&ins, start, end);
		}
	}

	void parseProcedure(BPatch_function *function) {
		Dyninst::Address start, end;
		std::set<BPatch_basicBlock*> blocks;

		PRINT_PLACE();
		function->getAddressRange(start, end);
	
		//~ DEBUG("## %s(%lx)\n", function->getTypedName().c_str(), start);
		_addressesTakenMaybe.insert(start);

		// Loop over all blocks of the function.
		BPatch_flowGraph *fg = function->getCFG();
		// This is a way to detect functions containing indirect calls, but not the address taken.
		//~ if(fg->containsDynamicCallsites())
			//~ DEBUG("### FG: Has fct pointers: %s(%lx)\n", function->getTypedName().c_str(), start);

		fg->getAllBasicBlocks(blocks);
		for (auto it = blocks.begin(); it != blocks.end(); ++it) {
			parseBlock(*it);
        	}
	}

	void parseModule(BPatch_module *lib) {
		PRINT_PLACE();

#if PRINT_DEBUG > 2
		char modname[512];
		size_t modname_len = 511;
		lib->getFullName(modname, modname_len);
		DEBUG("AT - module: %s\n", modname);
#endif

		// Loop over all library functions.
		std::vector<BPatch_function *> *lib_functions = lib->getProcedures();
		for (auto it = lib_functions->begin(); it != lib_functions->end(); ++it) {
			parseProcedure(*it);
        	}
	}

	void getPLT(BPatch_module *module) {
		/* Get the symtab of this module.*/
		SymtabAPI::Module *sym_module = SymtabAPI::convert(module);
		SymtabAPI::Symtab *symtab = sym_module->exec();
		vector< SymtabAPI::Function * > fcts;

		symtab->getFuncBindingTable(_plt);

#if PRINT_DEBUG > 2
		for(auto it = _plt.begin(); it != _plt.end(); ++it) {
				DEBUG("## PLT %s: target %jx rel %jx addend %jx %s type %li\n", 
					it->name().c_str(),
					it->target_addr(),
					it->rel_addr(),
					it->addend(),
					it->getDynSym()->getTypedName().c_str(),
					it->getRelType()
					);
		}
#endif
	}

	void setTextInfo(std::vector< BPatch_object * > *objs) {
		PRINT_PLACE();

		for(auto et = objs->begin(); et != objs->end(); ++et) {
			std::vector< BPatch_object::Region > regions;
			(*et)->regions(regions);

			for(auto it = regions.begin(); it != regions.end(); ++it) {
				switch(it->type) {
				case BPatch_object::Region::CODE:
					_textBase = it->base;
					_textSize = it->size;
					break;
				case BPatch_object::Region::DATA:
					_dataBase = it->base;
					_dataSize = it->size;
					break;
				default:
					break;
				}
			}
		}
	}

	void parseImage(BPatch_image *image) {
		PRINT_PLACE();

		/* To find the ELF sections, we have to retrieve BPatch_object(s),
		 * which represent translation units in memory, and each of those
		 * contains Region(s), which represent an ELF section for that
		 * translation unit.
		 */
		std::vector< BPatch_object * > objs;
		image->getObjects(objs);

		// Using the BPatch_object(s), we can retrieve the relevent
		// text section information.
		setTextInfo(&objs);

		/* Finds all pointers constants and offset which might be used to
		 * point within the text section.
		 * 
		 * The generated lists contain everything, regardless of the
		 * pointed-to value being a valid instruction or not.
		 */
		findCKs(&objs, image);

		// Loop over all modules (shared libraries)
		std::vector<BPatch_module *> *modules = image->getModules();
		for (auto it = modules->begin(); it != modules->end(); ++it) {
			char modname[512];
			size_t modname_len = 511;
 
 			(*it)->getFullName(modname, modname_len);

			/* Skip these: */
			if (!strcmp(modname, "libwrappers.so") ||
				!strcmp(modname, "libdyninstAPI_RT.so.8.2")) {
				PRINT("AT * Skipping %s\n", modname);
				continue;
			}
			if (!strcmp(modname, "libdl.so.2")) {
				PRINT("AT * Skipping %s << NOT SUPPORTED!\n", modname);
				continue;
			}

			// Save the PLT of the current module
			getPLT(*it);

			parseModule(*it);
		}
	}

protected:
	/* ************************************************************************/
	void reset(void) {
		PRINT_PLACE();

		_addressesTaken.clear();
		_callSites.clear();

		_raw_CC.clear();
		_raw_CK.clear();
		_plt.clear();
		_addressesTakenMaybe.clear();

		parseImage(_AddressSpace->getImage());

#if 1
		/* XXX: optionally disable copying the maybe set into the addresses taken set */
		_addressesTaken = _addressesTakenMaybe;
#endif

		DEBUG("== code 0x%08lx-%lx\n", _textBase, _textSize);
		DEBUG("== data 0x%08lx-%lx\n", _dataBase, _dataSize);
		DEBUG("== #Possible pointers CK %zi\n", std::distance(_raw_CK.begin(), _raw_CK.end()));
		DEBUG("== #Possible pointers CC(offsets) %zi\n", std::distance(_raw_CC.begin(), _raw_CC.end()));
		DEBUG("== _callSites %zi\n", _callSites.size());
		DEBUG("== _addressesTaken %zi\n", _addressesTaken.size());
		DEBUG("== _addressesTakenMaybe %zi\n", _addressesTakenMaybe.size());
	}

public:
    AddressTakenParserBinCFI() : AddressTakenParserBase(), _raw_CK(),
		_raw_CC(), _plt(), _textBase(0), _textSize(0), _addressesTakenMaybe() {
		PRINT_PLACE();
	}

	/* ************************************************************************/
    const std::string name(void) {
		return "bin-cfi";
	}
};

/******************************************************************************
 *                       AddressTakenParserLockDown	                          *
 ******************************************************************************/
/* This policy implements the policy as found in:
 * "Fine-Grained Control-Flow Integrity through Binary Hardening" -
 * Mathias Payer, Antonio Barresi, and Thomas R. Gross
 ******************************************************************************/
class AddressTakenParserLockDown : public AddressTakenParserBase {
protected:
	virtual void reset(void) {
		PRINT_PLACE();

		_callSites.clear();
		_addressesTaken.clear();

		DEBUG("== _callSites %zi\n", _callSites.size());
		DEBUG("== _addressesTaken %zi\n", _addressesTaken.size());
	}

public:
    AddressTakenParserLockDown() : AddressTakenParserBase() {
		PRINT_PLACE();
	}

	/* ************************************************************************/
    const std::string name(void) {
		return "lockdown";
	}
};

/******************************************************************************
 *                          AddressTakenParserCFG                             *
 ******************************************************************************/
/* This policy implements the policy as first used in CFIPolicy, a.k.a simply
 * count from the given CFG.
 ******************************************************************************/
class AddressTakenParserCFG : public AddressTakenParserBase {
private:
	/* ************************************************************************/
	static int fct_count_calls(ArmsFunction * f, void * _list) {
		size_t i, j;
		ArmsBasicBlock *bb;
		ArmsEdge *e;

		std::set<uint64_t> *list = static_cast< std::set<uint64_t>* >(_list);

		if (f->is_lib_dummy()) {
			/* Don't count control transfers from PLT to library functions. */
			return 0;
		}

		for (i = 0; i < f->nentry_points(); i++) {
			bb = f->get_entry_point(i);
			for (j = 0; j < bb->incoming_edge_count(); j++) {
				e = bb->get_incoming_edge(j);
				if (e->is_hidden())
					continue;

				if (NULL == e->source())
					continue;

				if (!e->is_indirect_call())
					continue;

				//list->insert(e->source()->get_last_insn_address());
				list->insert(e->source()->get_end_address());

#if PRINT_DEBUG > 2
				DEBUG("## end_addr %jx last_ins_addr %jx target_start %jx\n",
						e->source()->get_end_address(),
						e->source()->get_last_insn_address(),
						e->target()->get_start_address());
#endif
			}
		}
		return 0;
	}

	/* ************************************************************************/
	static void insertATsFromCFG(ArmsBasicBlock *bb, std::set<uint64_t> *list) {
		for(int i = 0; i < bb->outgoing_edge_count(); i++) {
			ArmsEdge *e = bb->get_outgoing_edge(i);
			if (e->is_hidden())
				continue;

			if (e->is_intraprocedural())
				continue;

			if (!e->is_indirect_call())
				continue;

			if (NULL == e->target())
				continue;

			/* there is some weird glitch which adds edges to the
			 * plt entries weird targets. They're always in the
			 * 0x29XX range, so just filter them out.*/
			if (0x2900 == (e->target()->get_start_address() & 0xff00)) {
				continue;
			}

			list->insert(e->target()->get_start_address());
		}
	}

	static int fct_count_at(ArmsFunction * f, void * _list) {
		std::set<uint64_t> *list = static_cast< std::set<uint64_t>* >(_list);

		if (f->is_lib_dummy()) {
			/* Don't count control transfers from PLT to library functions. */
			return 0;
		}

		// Loop over all blocks of the function.
		std::set<ArmsBasicBlock*> *blocks = f->get_basic_blocks();
		for (auto it = blocks->begin(); it != blocks->end(); ++it) {
			insertATsFromCFG(*it, list);
#if PRINT_DEBUG > 2
			DEBUG("## start %jx end %jx bb start %jx\n",
					e->source()->get_last_insn_address(),
					e->target()->get_start_address(),
					e->source()->get_start_address());
#endif
	    }
		return 0;
	}

protected:
	virtual void reset(void) {
		PRINT_PLACE();

		_callSites.clear();
		_addressesTaken.clear();

		_cfg->foreach_function(fct_count_calls, &_callSites);
		_cfg->foreach_function(fct_count_at, &_addressesTaken);

		DEBUG("== _callSites %zi\n", _callSites.size());
		DEBUG("== _addressesTaken %zi\n", _addressesTaken.size());
	}

public:
    AddressTakenParserCFG() : AddressTakenParserBase() {
		PRINT_PLACE();
	}

	/* ************************************************************************/
    const std::string name(void) {
		return "CFG";
	}
};

/******************************************************************************
 *                        getAddressTakenParser()                             *
 ******************************************************************************/
/* Factory to simplify adding new AddressTakenParser Policies.
 * 
 * The policy_name paramater is taken into consideration only when there is no
 * policy yet instanciated, afterwards, the first policy ever instanciated is
 * returned, effectively preventing the policy from being changed later on.
 ******************************************************************************/
AddressTakenParser *
getAddressTakenParser(std::string policy_name) {
	static AddressTakenParser *bin_cfi = NULL, *lockdown = NULL, *dflt = NULL, *cfg_count = NULL;
	static AddressTakenParser *p = NULL;

	PRINT_PLACE();

	// If the parser has already been initialized, just return the current instance.
	if (NULL != p)
		return p;

	if (NULL == bin_cfi)
		bin_cfi = new AddressTakenParserBinCFI();

	if (NULL == lockdown)
		lockdown = new AddressTakenParserLockDown();

	if (NULL == cfg_count)
		cfg_count = new AddressTakenParserCFG();

	// Default to counting from the CFG
	if (NULL == dflt)
		dflt = cfg_count;

	if (policy_name.compare("bin-cfi") == 0) {
		p = bin_cfi;
	}

	if (policy_name.compare("lockdown") == 0) {
		p = lockdown;
	}

	if (policy_name.compare("cfg") == 0) {
		p = cfg_count;
	}

	if (policy_name.compare("default") == 0) {
		p = dflt;
	}

	/* If p is still NULL, then this is an unknown policy. Print a 
	 * warning, and fallback to the default. */
	if (NULL == p) {
		std::cerr << "AddressTaken Policy '" << policy_name
			<< "' unknown, using default policy!\n";
		std::cerr << "AddressTaken Policy '" << policy_name
			<< "' valid policies: default,cfg,bin-cfi,lockdown\n";
		p = dflt;
	}

	std::cerr << "AddressTaken Policy '" << p->name()
			<< "' was selected\n";

    return p;
}
