#include "hypercall.h"

void hypercall(UINT64 id, UINT64 param)
{
	kAFL_Hypercall(id, param);
}

void hypercall_coverage_on()
{
	// TODO - uncomment
	 hypercall(HYPERCALL_COVERAGE_ON_ID, HYPERCALL_COVERAGE_ON_PARAM);
}

void hypercall_coverage_off()
{
	// TODO - uncomment
	 hypercall(HYPERCALL_COVERAGE_OFF_ID, HYPERCALL_COVERAGE_OFF_PARAM);
}