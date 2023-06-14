#include "globals.h"
#include "kernel_modules.h"

NTSTATUS init_kernel_modules(PKERNEL_MODULES kernel_modules)
{
	/* Based on https://github.com/thomhastings/mimikatz-en/blob/master/driver/modules.c */

	NTSTATUS status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(kernel_modules);
	ULONG modules_size = 0;
	ULONG modules_count = 0;
	PVOID required_buffer_size = NULL;
	AUX_MODULE_EXTENDED_INFO* modules = NULL;

	LOG("init_kernel_modules: InitKernelModules starting\n");

	status = AuxKlibInitialize();
	if (!NT_SUCCESS(status))
	{
		LOG("init_kernel_modules: Failed in AuxKlibInitialize, 0x%x\n", status);
		goto exit;
	}

	LOG("init_kernel_modules: after AuxKlibInitialize\n");

	/* Get the size of the struct for requested information */
	status = AuxKlibQueryModuleInformation(&modules_size, sizeof(AUX_MODULE_EXTENDED_INFO), required_buffer_size);
	if (!NT_SUCCESS(status))
	{
		LOG("init_kernel_modules: Failed to get kernel modules information\n");
		goto exit;
	}

	LOG("init_kernel_modules: after AuxKlibQueryModuleInformation 1\n");

	// Create a new buffer for the modules
	modules_count = modules_size / sizeof(AUX_MODULE_EXTENDED_INFO);
	modules = (AUX_MODULE_EXTENDED_INFO*)ExAllocatePoolWithTag(PagedPool, modules_size, POOL_TAG);
	if (modules == NULL)
	{
		LOG("init_kernel_modules: Failed to allocate modules buffer\n");
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto exit;
	}
	RtlZeroMemory(modules, modules_size);

	/* Now get the actual information... */
	status = AuxKlibQueryModuleInformation(&modules_size, sizeof(AUX_MODULE_EXTENDED_INFO), modules);
	if (!NT_SUCCESS(status))
	{
		LOG("init_kernel_modules: Failed to query module information, status: 0x%x\n", status);
		ExFreePoolWithTag(kernel_modules->modules, POOL_TAG);
		goto exit;
	}

	LOG("init_kernel_modules: after AuxKlibQueryModuleInformation 2 numberOfModules=%ld\n", modules_count);

	kernel_modules->modules = modules;
	kernel_modules->numberOfModules = modules_count;

exit:
	return status;
}

VOID deinit_kernel_modules(PKERNEL_MODULES kernel_modules)
{
	ExFreePoolWithTag(kernel_modules->modules, POOL_TAG);
}

ULONG get_kernel_modules_count(PKERNEL_MODULES kernel_modules)
{
	return kernel_modules->numberOfModules;
}

PCSZ get_kernel_module_name_by_index(PKERNEL_MODULES kernel_modules, ULONG i)
{
	if (i >= kernel_modules->numberOfModules)
	{
		return NULL;
	}
	return (PCSZ)(kernel_modules->modules[i].FullPathName);
}

PVOID get_kernel_module_base_address_by_index(PKERNEL_MODULES kernel_modules, ULONG i)
{
	if (i >= kernel_modules->numberOfModules)
	{
		return NULL;
	}
	return kernel_modules->modules[i].BasicInfo.ImageBase;
}

NTSTATUS get_module_address(PUNICODE_STRING target_module_name, PVOID* target_base_address)
{
	NTSTATUS status = STATUS_SUCCESS;
	KERNEL_MODULES kernel_modules;
	ULONG modules_count;
	UNICODE_STRING current_name_unicode = { 0 };
	ANSI_STRING current_namei = { 0 };
	LONG compare_result = 0;
	BOOLEAN case_insensitive = TRUE;
	BOOLEAN allocate_string = TRUE;


	status = init_kernel_modules(&kernel_modules);
	if (!NT_SUCCESS(status))
	{
		LOG("get_module_address: Couldn't InitKernelModules\n");
		goto exit;
	}

	status = STATUS_NOT_FOUND;
	modules_count = get_kernel_modules_count(&kernel_modules);
	for (ULONG i = 0; i < modules_count; i++)
	{
		RtlInitAnsiString(&current_namei, get_kernel_module_name_by_index(&kernel_modules, i));
		status = RtlAnsiStringToUnicodeString(&current_name_unicode, &current_namei, allocate_string);
		if (!NT_SUCCESS(status))
		{
			LOG("get_module_address: Could not convert an Ansi string to a Unicode one\n");
			goto deinit;
		}

		compare_result = RtlCompareUnicodeString(&current_name_unicode, target_module_name, case_insensitive);
		if (compare_result)
		{
			LOG("get_module_address: Found Module %wZ address at %p\n", current_name_unicode, get_kernel_module_base_address_by_index(&kernel_modules, i));
			continue;
		}


		*target_base_address = get_kernel_module_base_address_by_index(&kernel_modules, i);
		LOG("get_module_address: Found Target Module %wZ address at %p\n", target_module_name, *target_base_address);
		status = STATUS_SUCCESS;
		break;
	}

deinit:
	deinit_kernel_modules(&kernel_modules);
exit:
	return status;
}

PVOID kernel_get_proc_address(PVOID module_base, PCHAR function_name)
{
	ASSERT(module_base && function_name);
	PVOID function_address = NULL;

	ULONG size = 0;
	PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)RtlImageDirectoryEntryToData(module_base, TRUE, IMAGE_DIRECTORY_ENTRY_EXPORT, &size);

	ULONG_PTR addr = (ULONG_PTR)(PUCHAR)((UINT64)exports - (UINT64)module_base);

	PULONG functions = (PULONG)((ULONG_PTR)module_base + exports->AddressOfFunctions);
	PSHORT ordinals = (PSHORT)((ULONG_PTR)module_base + exports->AddressOfNameOrdinals);
	PULONG names = (PULONG)((ULONG_PTR)module_base + exports->AddressOfNames);
	ULONG  max_name = exports->NumberOfNames;
	ULONG  max_func = exports->NumberOfFunctions;

	ULONG i;

	LOG("KernelGetProcAddress ModuleBase=%x pFunctionName=%s\n", module_base, function_name);

	for (i = 0; i < max_name; i++)
	{
		ULONG ord = ordinals[i];
		if (i >= max_name || ord >= max_func)
		{
			return NULL;
		}
		if (functions[ord] < addr || functions[ord] >= addr + size)
		{
			LOG("Found function %s\n", (PCHAR)module_base + names[i]);

			if (strcmp((PCHAR)module_base + names[i], function_name) == 0)
			{
				function_address = (PVOID)((PCHAR)module_base + functions[ord]);
				break;
			}
		}
	}
	return function_address;
}