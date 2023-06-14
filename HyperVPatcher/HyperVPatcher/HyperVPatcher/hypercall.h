#pragma once

#include "Globals.h"

#define HYPERCALL_KAFL_RAX_ID				35
#define HYPERCALL_KAFL_ACQUIRE				0
#define HYPERCALL_KAFL_GET_PAYLOAD			1
#define HYPERCALL_KAFL_GET_PROGRAM			2
#define HYPERCALL_KAFL_GET_ARGV				3
#define HYPERCALL_KAFL_RELEASE				4

// TODO - replace with real values!
#define HYPERCALL_COVERAGE_ON_ID HYPERCALL_KAFL_ACQUIRE
#define HYPERCALL_COVERAGE_OFF_ID HYPERCALL_KAFL_RELEASE
#define HYPERCALL_COVERAGE_ON_PARAM 0
#define HYPERCALL_COVERAGE_OFF_PARAM 0


extern void kAFL_Hypercall();

void hypercall(UINT64 id, UINT64 param);
void hypercall_coverage_on();
void hypercall_coverage_off();
