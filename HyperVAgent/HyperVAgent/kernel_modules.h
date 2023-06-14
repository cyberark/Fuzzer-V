#pragma once

#include "globals.h"
typedef struct _KernelModules
{
	AUX_MODULE_EXTENDED_INFO* modules;
	ULONG numberOfModules;

} KERNEL_MODULES, *PKERNEL_MODULES;

NTSTATUS init_kernel_modules(PKERNEL_MODULES kernel_modules);
VOID deinit_kernel_modules(PKERNEL_MODULES kernel_modules);
ULONG get_kernel_modules_count(PKERNEL_MODULES kernel_modules);
PCSZ get_kernel_module_name_by_index(PKERNEL_MODULES kernel_modules, ULONG i);
PVOID get_kernel_module_base_address_by_index(PKERNEL_MODULES kernel_modules, ULONG i);
NTSTATUS get_module_address(PUNICODE_STRING target_module_name, PVOID* target_base_address);
PVOID kernel_get_proc_address(PVOID module_base, PCHAR function_name);