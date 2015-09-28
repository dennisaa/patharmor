#ifndef __address_taken_parser__
#define __address_taken_parser__

#include <set>
#include <string>

#include "defs.h"
#include "arms_edge.h"
#include "arms_utils.h"
#include "arms_bb.h"
#include "arms_function.h"
#include "arms_cfg.h"

class AddressTakenParser {
protected:
	friend AddressTakenParser *getAddressTakenParser(std::string policy_name);

public:
	/* Get ready for next LBR array to analyze. */
	virtual void set(BPatch_addressSpace *as, CFG *cfg) = 0;

	/* Retrieve CallSites */
	virtual std::set<uint64_t> *getCallSites(void) = 0;

	/* Retrieve AddressTaken functions */
	virtual std::set<uint64_t> *getAddressesTaken(void) = 0;

	/* Just print which the policy selected. */
	virtual void show(void) = 0;

	/* Return the name of the policy. */
	virtual const std::string name(void) = 0;
};

/* Factory to simplify adding new Address Taken policies. */
AddressTakenParser *getAddressTakenParser(std::string policy_name);

#endif /* __address_taken_parser__ */
