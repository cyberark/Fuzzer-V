#pragma once

#include "Globals.h"
typedef struct _KernelModules
{
	AUX_MODULE_EXTENDED_INFO* modules;
	ULONG numberOfModules;

} KERNEL_MODULES, *PKERNEL_MODULES;

NTSTATUS InitKernelModules(PKERNEL_MODULES pKernelModules);
VOID DeinitKernelModules(PKERNEL_MODULES pKernelModules);
ULONG GetKernelModulesCount(PKERNEL_MODULES pKernelModules);
PCSZ GetKernelModuleNameByIndex(PKERNEL_MODULES pKernelModules, ULONG i);
PVOID GetKernelModuleBaseAddressByIndex(PKERNEL_MODULES pKernelModules, ULONG i);
NTSTATUS GetModuleAddress(PUNICODE_STRING targetModuleName, PVOID* targetBaseAddr);
PVOID KernelGetProcAddress(PVOID ModuleBase, PCHAR pFunctionName);