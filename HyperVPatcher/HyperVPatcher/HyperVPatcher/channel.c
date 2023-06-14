#include "channel.h"
#include "KernelModules.h"
#include "invertedioctl.h"
#include "hypercall.h"

VMBCHANNEL vmb_channel = NULL;

PCWSTR vmbkmclrModulePath = (PCWSTR)L"\\SystemRoot\\System32\\drivers\\vmbkmclr.sys";
PVOID vmbkmclrBaseAddress;
PVOID pKmclrChannelList;
channel_processing_callback original_packet_processing_callback;
channel_processing_complete_callback original_processing_complete_callback;

ERESOURCE packet_processing_lock;

VOID channel_init()
{
	ExInitializeResourceLite(&packet_processing_lock);
	FindDynamicAddresses();
}

VOID channel_terminate()
{
	ExDeleteResourceLite(&packet_processing_lock);
}

VOID FindVmbChannel(PGUID guid)
{
	LOG("[HyperVPatch] FindVmbChannel: starting\n");

	PVOID channel;
	PVOID first_channel;
	GUID current_guid = { 0 };
	VMBCHANNEL current_channel;

	LOG("[HyperVPatch] FindVmbChannel: guid %08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX provided\n",
		guid->Data1, guid->Data2, guid->Data3,
		guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
		guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);

	first_channel = *(PVOID *)pKmclrChannelList;
	channel = first_channel;
	do
	{
		current_channel = (VMBCHANNEL)((BYTE *)channel - NEXT_CHANNEL_OFFSET_IN_VMBCHANNEL);
		memcpy(&current_guid, (BYTE *)current_channel + TYPE_GUID_OFFSET_IN_VMBCHANNEL, sizeof(GUID));
		LOG("[HyperVPatch] FindVmbChannel: current_guid = %08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX\n",
			current_guid.Data1, current_guid.Data2, current_guid.Data3,
			current_guid.Data4[0], current_guid.Data4[1], current_guid.Data4[2], current_guid.Data4[3],
			current_guid.Data4[4], current_guid.Data4[5], current_guid.Data4[6], current_guid.Data4[7]);

		if (memcmp(&current_guid, guid, sizeof(GUID)) == 0)
		{
			vmb_channel = current_channel;

			LOG("[HyperVPatch] FindVmbChannel: Found guid at %p\n", vmb_channel);
			break;
		}

		channel = *(PVOID *)channel;
	} while (channel && channel != first_channel);

	if (!vmb_channel)
	{
		LOG("[HyperVPatch] FindVmbChannel: Could not find vmb_channel!\n");
	}
	else
	{
		LOG("[HyperVPatch] FindVmbChannel: found vmb_channel=%p\n", vmb_channel);
	}
}

VOID FindDynamicAddresses()
{
	LOG("[HyperVPatch] FindDynamicAddresses: starting\n");

	NTSTATUS status;
	UNICODE_STRING vmbkmclModuleName;

	// Locate vmbkmcl module in memory
	RtlInitUnicodeString(&vmbkmclModuleName, vmbkmclrModulePath);
	status = GetModuleAddress(&vmbkmclModuleName, &vmbkmclrBaseAddress);
	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVPatch] FindDynamicAddresses: GetModuleAddress vmbkmcl failed\n");
	}
	else
	{
		LOG("[HyperVPatch] FindDynamicAddresses: found vmbkmclBaseAddress=%p\n", vmbkmclrBaseAddress);
	}

	// Fetch the relevant functions from the module
	pKmclrChannelList = (PCHAR)vmbkmclrBaseAddress + KMCLR_CHANNEL_LIST_OFFSET;
	LOG("[HyperVPatch] FindDynamicAddresses: found pKmclrChannelList=%p\n", pKmclrChannelList);
}

VOID PatchChannelProcessing(PGUID guid)
{
	// find the channel
	FindVmbChannel(guid);

	// patch channel if it was found
	if (vmb_channel)
	{
		original_packet_processing_callback = (channel_processing_callback)*(PVOID *)((BYTE *)vmb_channel + PROCESSING_CALLBACK_IN_VMBCHANNEL);
		LOG("[HyperVPatch] PatchChannelProcessing: original_packet_processing_callback=%p\n", original_packet_processing_callback);
		*(PVOID *)((BYTE *)vmb_channel + PROCESSING_CALLBACK_IN_VMBCHANNEL) = (PVOID)&processing_hook_function;
	}
}

VOID processing_hook_function(VMBCHANNEL channel, VMBPACKETCOMPLETION packet, PVOID buffer, UINT32 size, UINT32 flags)
{
	//LOG("[HyperVPatch] hook_function starting\n");

	// check if this is our message
	if (is_fuzz_message(buffer, size))
	{
		LOG("[HyperVPatch] hook_function fuzz message\n");

		// 1. entering critical section
		KeEnterCriticalRegion();

		// 2. exclusive lock
		ExAcquireResourceExclusiveLite(&packet_processing_lock, TRUE);
		LOG("[HyperVPatch] hook_function exclusive in\n");

		// 3. start coverage
		hypercall_coverage_on();
		LOG("[HyperVPatch] hook_function hypercall_coverage_on called\n");
		//CompleteOneIoctlAndWaitForConfirmation(TRUE);
		//LOG("[HyperVPatch] hook_function CompleteOneIoctlAndWaitForConfirmation(TRUE) called\n");

		// 4. process packet (without marker)
		original_packet_processing_callback(channel, packet, (PVOID)((BYTE *)buffer + fuzz_marker_size), size - fuzz_marker_size, flags);
		LOG("[HyperVPatch] hook_function original_packet_processing_callback called\n");

		// 5. stop coverage
		hypercall_coverage_off();
		LOG("[HyperVPatch] hook_function hypercall_coverage_off called\n");
		//CompleteOneIoctlAndWaitForConfirmation(FALSE);
		//LOG("[HyperVPatch] hook_function CompleteOneIoctlAndWaitForConfirmation(FALSE) called\n");

		// 6. release lock
		ExReleaseResourceLite(&packet_processing_lock);

		// 7. exiting critical section
		KeLeaveCriticalRegion();

		LOG("[HyperVPatch] hook_function exclusive out\n");

		// 8. inform agent to query next input
		//CompleteOneIoctl();
	}
	else
	{
		// 1. entering critical section
		KeEnterCriticalRegion();

		// 2. non exclusive lock
		ExAcquireResourceSharedLite(&packet_processing_lock, TRUE);
		//LOG("[HyperVPatch] hook_function shared in\n");

		// 3. process packet
		original_packet_processing_callback(channel, packet, buffer, size, flags);
		//LOG("[HyperVPatch] hook_function original_packet_processing_callback called\n");

		// 4. release lock
		ExReleaseResourceLite(&packet_processing_lock);
		//LOG("[HyperVPatch] hook_function shared out\n");

		// 5. exiting critical section
		KeLeaveCriticalRegion();
	}

	//original_packet_processing_callback(channel, packet, buffer, size, flags);

	//if (is_fuzz_message(buffer, size))
	//{
	//	LOG("fuzz message\n");
	//	CompleteOneIoctl();
	//}
}

BOOLEAN is_fuzz_message(PVOID buffer, UINT32 size)
{
	if (size > fuzz_marker_size)
	{
		if (memcmp(buffer, fuzz_marker, fuzz_marker_size) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;
}
