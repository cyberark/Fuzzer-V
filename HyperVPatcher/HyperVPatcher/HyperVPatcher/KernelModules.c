#include "Globals.h"
#include "KernelModules.h"

NTSTATUS InitKernelModules(PKERNEL_MODULES pKernelModules)
{
	/* Based on https://github.com/thomhastings/mimikatz-en/blob/master/driver/modules.c */

	NTSTATUS status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(pKernelModules);
	ULONG modulesSize = 0;
	ULONG numberOfModules = 0;
	PVOID getRequiredBufferSize = NULL;
	AUX_MODULE_EXTENDED_INFO* modules = NULL;

	LOG("Harness: InitKernelModules starting\n");

	status = AuxKlibInitialize();
	if (!NT_SUCCESS(status))
	{
		LOG("Harness: Failed in AuxKlibInitialize, 0x%x\n", status);
		goto exit;
	}

	LOG("Harness: InitKernelModules after AuxKlibInitialize\n");

	/* Get the size of the struct for requested information */
	status = AuxKlibQueryModuleInformation(
		&modulesSize,
		sizeof(AUX_MODULE_EXTENDED_INFO),
		getRequiredBufferSize
	);
	if (!NT_SUCCESS(status))
	{
		LOG("Harness: Failed to get kernel modules information\n");
		goto exit;
	}

	LOG("Harness: InitKernelModules after AuxKlibQueryModuleInformation 1\n");

	// Create a new buffer for the modules
	numberOfModules = modulesSize / sizeof(AUX_MODULE_EXTENDED_INFO);
	modules = (AUX_MODULE_EXTENDED_INFO*)ExAllocatePoolWithTag(PagedPool, modulesSize, POOL_TAG);
	if (modules == NULL)
	{
		LOG("Harness: Failed to allocate modules buffer\n");
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto exit;
	}
	RtlZeroMemory(modules, modulesSize);

	/* Now get the actual information... */
	status = AuxKlibQueryModuleInformation(
		&modulesSize,
		sizeof(AUX_MODULE_EXTENDED_INFO),
		modules
	);
	if (!NT_SUCCESS(status))
	{
		LOG("Harness: Failed to query module information, status: 0x%x\n", status);
		ExFreePoolWithTag(pKernelModules->modules, POOL_TAG);
		goto exit;
	}

	LOG("Harness: InitKernelModules after AuxKlibQueryModuleInformation 2 numberOfModules=%ld\n", numberOfModules);

	pKernelModules->modules = modules;
	pKernelModules->numberOfModules = numberOfModules;

exit:
	return status;
}

VOID DeinitKernelModules(PKERNEL_MODULES pKernelModules)
{
	ExFreePoolWithTag(pKernelModules->modules, POOL_TAG);
}

ULONG GetKernelModulesCount(PKERNEL_MODULES pKernelModules)
{
	return pKernelModules->numberOfModules;
}

PCSZ GetKernelModuleNameByIndex(PKERNEL_MODULES pKernelModules, ULONG i)
{
	if (i >= pKernelModules->numberOfModules)
	{
		return NULL;
	}
	return (PCSZ)(pKernelModules->modules[i].FullPathName);
}

PVOID GetKernelModuleBaseAddressByIndex(PKERNEL_MODULES pKernelModules, ULONG i)
{
	if (i >= pKernelModules->numberOfModules)
	{
		return NULL;
	}
	return pKernelModules->modules[i].BasicInfo.ImageBase;
}

NTSTATUS GetModuleAddress(PUNICODE_STRING targetModuleName, PVOID* targetBaseAddr)
{
	NTSTATUS status = STATUS_SUCCESS;
	KERNEL_MODULES kernelModules;
	ULONG numberOfModules;
	UNICODE_STRING currentUnicode = { 0 };
	ANSI_STRING currentAnsi = { 0 };
	LONG stringCompareRes = 0;
	BOOLEAN caseInsensitive = TRUE;
	BOOLEAN allocateDestinationString = TRUE;


	status = InitKernelModules(&kernelModules);
	if (!NT_SUCCESS(status))
	{
		LOG("Harness: Couldn't InitKernelModules\n");
		goto exit;
	}

	status = STATUS_NOT_FOUND;
	numberOfModules = GetKernelModulesCount(&kernelModules);
	for (ULONG i = 0; i < numberOfModules; i++)
	{
		RtlInitAnsiString(&currentAnsi, GetKernelModuleNameByIndex(&kernelModules, i));
		status = RtlAnsiStringToUnicodeString(&currentUnicode, &currentAnsi, allocateDestinationString);
		if (!NT_SUCCESS(status))
		{
			LOG("Harness: Could not convert an Ansi string to a Unicode one\n");
			goto deinit;
		}

		stringCompareRes = RtlCompareUnicodeString(&currentUnicode, targetModuleName, caseInsensitive);
		if (stringCompareRes)
		{
			LOG("Harness: Found Module %wZ address at %p\n", currentUnicode, GetKernelModuleBaseAddressByIndex(&kernelModules, i));
			continue;
		}


		*targetBaseAddr = GetKernelModuleBaseAddressByIndex(&kernelModules, i);
		LOG("Harness: Found Target Module %wZ address at %p\n", targetModuleName, *targetBaseAddr);
		status = STATUS_SUCCESS;
		break;
	}

deinit:
	DeinitKernelModules(&kernelModules);
exit:
	return status;
}

PVOID KernelGetProcAddress(PVOID ModuleBase, PCHAR pFunctionName)
{
	ASSERT(ModuleBase && pFunctionName);
	PVOID pFunctionAddress = NULL;

	ULONG size = 0;
	PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)
		RtlImageDirectoryEntryToData(ModuleBase, TRUE, IMAGE_DIRECTORY_ENTRY_EXPORT, &size);

	ULONG_PTR addr = (ULONG_PTR)(PUCHAR)((UINT64)exports - (UINT64)ModuleBase);

	PULONG functions = (PULONG)((ULONG_PTR)ModuleBase + exports->AddressOfFunctions);
	PSHORT ordinals = (PSHORT)((ULONG_PTR)ModuleBase + exports->AddressOfNameOrdinals);
	PULONG names = (PULONG)((ULONG_PTR)ModuleBase + exports->AddressOfNames);
	ULONG  max_name = exports->NumberOfNames;
	ULONG  max_func = exports->NumberOfFunctions;

	ULONG i;

	LOG("KernelGetProcAddress ModuleBase=%x pFunctionName=%s\n", ModuleBase, pFunctionName);

	for (i = 0; i < max_name; i++)
	{
		ULONG ord = ordinals[i];
		if (i >= max_name || ord >= max_func) {
			return NULL;
		}
		if (functions[ord] < addr || functions[ord] >= addr + size)
		{
			LOG("Found function %s\n", (PCHAR)ModuleBase + names[i]);

			if (strcmp((PCHAR)ModuleBase + names[i], pFunctionName) == 0)
			{
				pFunctionAddress = (PVOID)((PCHAR)ModuleBase + functions[ord]);
				break;
			}
		}
	}
	return pFunctionAddress;
}