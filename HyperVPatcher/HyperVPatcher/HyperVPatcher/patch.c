// Copyright (c) 2021, SafeBreach & Guardicore
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// AUTHORS: Peleg Hadar (@peleghd), Ophir Harpaz (@OphirHarpaz)

#include "patch.h"

_IRQL_requires_max_(APC_LEVEL)
static NTSTATUS WriteToRXMemory(PVOID dst, PVOID src, size_t size)
{
	PVOID pPatchMdl = NULL;
	NTSTATUS status = STATUS_INVALID_ADDRESS;

	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DEBUG_LEVEL, "Harness: [PATCHING] Calling IoAllocateMdl...!\n");
	pPatchMdl = IoAllocateMdl(dst, (ULONG)size, FALSE, FALSE, NULL);
	if (pPatchMdl == NULL) {
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DEBUG_LEVEL, "Harness: [PATCHING] IoAllocateMdl was failed!\n");
		return status;
	}

	__try
	{
		MmProbeAndLockPages(pPatchMdl, KernelMode, IoReadAccess);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		IoFreeMdl(pPatchMdl);
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DEBUG_LEVEL, "Harness: [PATCHING] MmProbeAndLockPages failed!\n");
		return STATUS_INVALID_ADDRESS;
	}

	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DEBUG_LEVEL, "Harness: [PATCHING] Calling MmMapLockedPagesSpecifyCache!\n");

	PLONG64 RwMapping = MmMapLockedPagesSpecifyCache(
		pPatchMdl,
		KernelMode,
		MmNonCached,
		NULL,
		FALSE,
		NormalPagePriority
	);

	if (RwMapping == NULL)
	{
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DEBUG_LEVEL, "Harness: [PATCHING] MmMapLockedPagesSpecifyCache Failed!\n");
		MmUnlockPages(pPatchMdl);
		IoFreeMdl(pPatchMdl);

		return STATUS_INTERNAL_ERROR;
	}
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DEBUG_LEVEL, "Harness: [PATCHING] Calling MmProtectMdlSystemAddress ...!\n");
	status = MmProtectMdlSystemAddress(pPatchMdl, PAGE_READWRITE);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DEBUG_LEVEL, "Harness: [PATCHING] MmProtectMdlSystemAddress was failed!\n");
		MmUnmapLockedPages(RwMapping, pPatchMdl);
		MmUnlockPages(pPatchMdl);
		IoFreeMdl(pPatchMdl);
		return status;
	}
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DEBUG_LEVEL, "Harness: [PATCHING] Patching...!\n");
	memcpy((char*)RwMapping, src, size);

	MmUnmapLockedPages(RwMapping, pPatchMdl);
	MmUnlockPages(pPatchMdl);
	IoFreeMdl(pPatchMdl);


	return STATUS_SUCCESS;
}

NTSTATUS PatchLogic()
{
	NTSTATUS status = STATUS_SUCCESS;

	// TODO:
	// 1. find patch address (start of target function)
	// 2. add jump to marker check

	// garbage data - TODO get actual data for every hook
	CHAR *data_register = "RCX";
	ULONG skipped_instructions_size = 5;
	BYTE marker_offset = 0x10;
	BYTE marker_mask = 0x01;
	BYTE *target_function_address = (BYTE *)(PVOID)0x1234123412341234u;

	status = insert_marker_check(target_function_address, data_register, skipped_instructions_size, marker_offset, marker_mask);

	return status;
}

NTSTATUS insert_marker_check(BYTE *target_function_address, CHAR *data_register, ULONG skipped_instructions_size, BYTE marker_offset, BYTE marker_mask)
{
	NTSTATUS status;
	INSTRUCTION instructions[MAX_INSTRUCTION_COUNT] = { 0 };

	// 1. add check marker instruction
	if (strcmp(data_register, "RCX"))
	{
		instructions[0].size = 4;

		// test byte ptr [rcx+offset], mask
		instructions[0].instruction[0] = 0xF6;					// test
		instructions[0].instruction[1] = 0x41;					// rcx
		instructions[0].instruction[2] = marker_offset;			// offset
		instructions[0].instruction[3] = marker_mask;			// mask
	}
	// else if ...
	else
	{
		return STATUS_UNSUCCESSFUL;
	}

	// 2. add the following 2 instructions as one, so the jump always works 
	//		a. jmp past b if marker off
	//		b. turn marker off
	instructions[1].size = 6;
	
	// jz 4
	instructions[1].instruction[0] = 0x74;					// jz
	instructions[1].instruction[1] = 0x04;					// jmp size

	if (strcmp(data_register, "RCX"))
	{
		// sub byte ptr [rcx+offset], mask
		instructions[1].instruction[2] = 0x80;				// sub
		instructions[1].instruction[3] = 0x69;				// rcx
		instructions[1].instruction[4] = marker_offset;		// offset
		instructions[1].instruction[5] = marker_mask;		// mask
	}
	// else if ...
	else
	{
		return STATUS_UNSUCCESSFUL;
	}

	// 3. add the skipped instructions (as one for simplicity)
	instructions[2].size = skipped_instructions_size;
	memcpy(instructions[2].instruction, target_function_address, skipped_instructions_size);

	// insert the instructions!
	status = insert_instructions_between_functions(target_function_address, skipped_instructions_size, instructions, 3, 0x10000);

	return status;
}

// returns status of success
// 
// parameters:
// start_address						address to hook
// skipped_instructions_size			size of instructions copied from start_address to the hook code
// instructions							array of INSTRUCTIONs to insert (should end with the skipped instructions)
// count								number of instructions to insert
// max_size								limit of total available space size 
//
// legitimate range for insertions is [start_address, start_address + max_size]
NTSTATUS insert_instructions_between_functions(IN OUT BYTE * start_address, IN ULONG skipped_instructions_size, IN PINSTRUCTION instructions, IN ULONG count, IN ULONG max_size)
{
	NTSTATUS status = STATUS_SUCCESS;
	ULONG i = 0;
	ULONG insert_space = 0;
	ULONG jmp_size;
	BYTE *next_insert = start_address;
	BYTE *previous_insert = start_address;
	ULONG previous_insert_space = 0;
	INSTRUCTION jmp_instruction;

	ULONG remaining_size = max_size;

	// this loop finds the available spaces between functions and inserts the supplied instructions into them
	// when exiting it (successfully):
	// previous_insert = address where the last jmp command should go (to reconnect to the original code)
	// previous_insert_space = remaining space in previous_insert (guaranteed to be enough for the jmp)
	while (NT_SUCCESS(status) && i < count)
	{
		status = find_next_insert(&next_insert, instructions[i].size + JMP_INSTRUCTION_SIZE, &insert_space, &remaining_size);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		// if this is not the first instruction inserted we need to insert a jmp
		if (previous_insert)
		{
			jmp_size = (ULONG)(next_insert - (previous_insert + JMP_INSTRUCTION_SIZE));

			jmp_instruction.size = JMP_INSTRUCTION_SIZE;
			jmp_instruction.instruction[0] = 0xe9;								// jmp relative dword
			*(ULONG *)(&jmp_instruction.instruction[1]) = jmp_size;				// dword jmp size

			status = insert_instruction(&previous_insert, &jmp_instruction, &previous_insert_space);	// insert instruction
		}

		// this loop inserts as many instructions as possible to the found space
		// when exiting it (successfully):
		// previous_insert = where the jmp instruction should go
		// previous_insert_space = remaining space (guaranteed to be enough for the jmp)
		while (NT_SUCCESS(status) && i < count && instructions[i].size + JMP_INSTRUCTION_SIZE < insert_space)
		{
			status = insert_instruction(&next_insert, instructions + i, &insert_space);
			previous_insert = next_insert;
			previous_insert_space = insert_space;
			i++;
		}
	}

	if (NT_SUCCESS(status))
	{
		// connect the dots! 
		// add a jmp from the end of the inserted code to start_address + skipped_instructions_size
		jmp_size = (ULONG)(previous_insert - (start_address + skipped_instructions_size + JMP_INSTRUCTION_SIZE));

		jmp_instruction.size = JMP_INSTRUCTION_SIZE;
		jmp_instruction.instruction[0] = 0xe9;								// jmp relative dword
		*(ULONG *)(&jmp_instruction.instruction[1]) = jmp_size;				// dword jmp size

		status = insert_instruction(&previous_insert, &jmp_instruction, &previous_insert_space);	// insert instruction
	}

	return status;
}

NTSTATUS find_next_insert(IN OUT BYTE **insert, IN ULONG insert_size, OUT ULONG *space, IN OUT ULONG *size)
{
	ULONG offset = 0;
	while (offset + insert_size < *size)
	{
		// check if we have a 'ret' followed by enough space starting at this offset
		if (available_insert(*insert + offset, insert_size, space))
		{
			*insert += (offset + 1);				// skip the 'ret'
			*size -= (offset + 1);					// adjust the size

			return STATUS_SUCCESS;
		}

		offset++;
	}
	return STATUS_INSUFFICIENT_RESOURCES;
}

BOOLEAN available_insert(IN BYTE *start, IN ULONG min_size, OUT ULONG *available_size)
{
	BYTE *current = start;

	// check that we start with a 'ret' instruction
	if (*current != 0xC3)
	{
		return FALSE;
	}

	// skip the 'ret'
	current++;

	// count free space before next function
	*available_size = 0;
	while (*current == 0xCC)
	{
		*available_size++;
		current++;
	}

	// check if we found enough space
	if (*available_size < min_size)
	{
		return FALSE;
	}

	return TRUE;
}

NTSTATUS insert_instruction(IN OUT BYTE **insert_address, IN PINSTRUCTION instruction, IN OUT ULONG *space)
{
	NTSTATUS status;

	// verify we have enough space
	if (instruction->size > *space)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// write the instruction
	status = WriteToRXMemory(*insert_address, instruction->instruction, instruction->size);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	*insert_address += instruction->size;				// move over the inserted instruction
	*space -= instruction->size;						// adjust the remaining space

	return STATUS_SUCCESS;
}
