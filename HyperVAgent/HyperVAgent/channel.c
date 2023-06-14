#include "channel.h"
#include "kernel_modules.h"

VMBCHANNEL vmb_channel;
KMCL_CLIENT_INTERFACE_V1 kmcl;

PCWSTR vmbkmclModulePath = (PCWSTR)L"\\SystemRoot\\System32\\drivers\\vmbkmcl.sys";
CHAR vmbChannelSendSynchronousRequest[] = "VmbChannelSendSynchronousRequest";
PVOID vmbkmclBaseAddress;
PFN_VMB_CHANNEL_SEND_SYNCHRONOUS_REQUEST pVmbChannelSendSynchronousRequest;
PFN_VMB_PACKET_ALLOCATE pVmbPacketAllocate;
PFN_VMB_PACKET_INITIALIZE pVmbPacketInitialize;
PFN_VMB_PACKET_SET_COMPLETION_ROUTINE pVmbPacketSetCompletionRoutine;
PFN_VMB_PACKET_FREE pVmbPacketFree;
PFN_VMB_PACKET_SEND pVmbPacketSend;
PFN_VMB_PACKET_SEND_WITH_EXTERNAL_MDL pVmbPacketSendWithExternalMdl;
PFN_VMB_CHANNEL_GET_INTERFACE_INSTANCE pVmbChannelGetInterfaceInstance;
PFN_VMB_CHANNEL_SIZEOF_PACKET pVmbChannelSizeofPacket;
PFN_VMB_PACKET_SET_POINTER pVmbPacketSetPointer;
PVOID pKmclChannelList;

ULONG msg_counter = 0;

extern POBJECT_TYPE * IoDriverObjectType;

ULONG sentPacketHandled = FALSE;

GUID null_guid = { 0 };

POUT_MESSAGE out_message_buffer = NULL;
ULONG out_message_buffer_size = 0;

PVOID hw_device_extension = NULL;

BYTE fuzz_marker[FUZZ_MARKER_SIZE] = { 0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88 };

//BYTE storvsp_header[] = { 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, };
						  //0x00, 0x00, 0x00, 0x00, 0x10, 0x14, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x88, 0x00, 0x00, 0x00,
						  //0x00, 0x00, 0x01, 0x39, 0x6f, 0x48, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						  //0x00, 0x00, 0x2a, 0x20, 0x42, 0x03, 0x20, 0x40, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#ifdef DBG_REPEAT_FIRST
BOOLEAN first_msg = TRUE;
PVOID first_msg_buffer = NULL;
ULONG first_msg_size = 0;
#endif // DBG_REPEAT_FIRST

VOID find_hw_device_extension(const WCHAR * driver_name)
{
	LOG("[HyperVAgent] find_hw_device_extension: starting name=%S\n", driver_name);

	PUCHAR temp;

	NTSTATUS status;
	UNICODE_STRING name;

	PDRIVER_OBJECT driver = NULL;
	PDEVICE_OBJECT device;

	RtlInitUnicodeString(&name, driver_name);

	status = ObReferenceObjectByName(&name,
		OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
		NULL,
		0,
		*IoDriverObjectType,
		KernelMode,
		NULL,
		&driver);

	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVAgent] find_hw_device_extension: ObReferenceObjectByName failed. status=%08x\n", status);
		return;
	}

	LOG("[HyperVAgent] driver=%p\n", driver);

	device = driver->DeviceObject;

	LOG("[HyperVAgent] device=%p\n", device);

	// ? poi(poi(poi(ffff940bc96a7050 + 40) + 18) + 230) + 10
	temp = (PUCHAR)device;
	LOG("[HyperVAgent] temp=%p\n", temp);
	
	temp = (PUCHAR) * (PUCHAR*)(temp + 0x40);
	LOG("[HyperVAgent] temp=%p\n", temp);

	temp = (PUCHAR) * (PUCHAR*)(temp + 0x18);
	LOG("[HyperVAgent] temp=%p\n", temp);

	temp = (PUCHAR) * (PUCHAR*)(temp + 0x230);
	LOG("[HyperVAgent] temp=%p\n", temp);

	temp += 0x10;
	LOG("[HyperVAgent] temp=%p\n", temp);

	hw_device_extension = temp;
	
	// clear reference
	ObDereferenceObject(driver);
}

VOID find_vmb_channel(PVOID pointer, const WCHAR * driver_name, PGUID guid)
{
	LOG("[HyperVAgent] find_vmb_channel: starting driver_name=%S\n", driver_name);

	PVOID channel;
	PVOID first_channel;

	NTSTATUS status;

	PDRIVER_OBJECT driver = NULL;
	PDEVICE_OBJECT device;
	UNICODE_STRING name;

	GUID current_guid = { 0 };

	VMBCHANNEL current_channel;

	if (pointer)
	{
		// if the user mode provided the channel pointer, we simply save it
		LOG("[HyperVAgent] find_vmb_channel: pointer %p provided\n", pointer);

		vmb_channel = (VMBCHANNEL)pointer;
	}
	else if (guid && memcmp(guid, &null_guid, sizeof(GUID)) != 0)
	{
		LOG("[HyperVAgent] find_vmb_channel: guid %08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX provided\n", 
			guid->Data1, guid->Data2, guid->Data3,
			guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
			guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);

		first_channel = *(PVOID *)pKmclChannelList;
		channel = first_channel;
		do
		{
			current_channel = (VMBCHANNEL)((BYTE *)channel - NEXT_CHANNEL_OFFSET_IN_VMBCHANNEL);
			memcpy(&current_guid, (BYTE *)current_channel + TYPE_GUID_OFFSET_IN_VMBCHANNEL, sizeof(GUID));
			LOG("[HyperVAgent] find_vmb_channel: current_guid = %08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX\n",
				current_guid.Data1, current_guid.Data2, current_guid.Data3,
				current_guid.Data4[0], current_guid.Data4[1], current_guid.Data4[2], current_guid.Data4[3],
				current_guid.Data4[4], current_guid.Data4[5], current_guid.Data4[6], current_guid.Data4[7]);

			if (memcmp(&current_guid, guid, sizeof(GUID)) == 0)
			{
				vmb_channel = current_channel;

				LOG("[HyperVAgent] find_vmb_channel: Found guid at %p\n", vmb_channel);
				break;
			}

			channel = *(PVOID *)channel;
		} while (channel && channel != first_channel);
	}
	else
	{
		// the channel can usually be found by 
		// name ---> driver ---> device ---> extension ---> channel

		RtlInitUnicodeString(&name, driver_name);

		status = ObReferenceObjectByName(&name,
			OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
			NULL,
			0,
			*IoDriverObjectType,
			KernelMode,
			NULL,
			&driver);

		if (!NT_SUCCESS(status))
		{
			LOG("[HyperVAgent] find_vmb_channel: ObReferenceObjectByName failed. status=%08x\n", status);
			return;
		}

		device = driver->DeviceObject;

		// TODO: different extraction method for different driver names...

		// TODO: case insensitive check + verify that this is the correct path
		if (wcscmp(driver_name, L"\\driver\\hyperkbd") == 0)
		{
			vmb_channel = (VMBCHANNEL)((PULONG_PTR)(device->DeviceExtension))[2];
		}
		else if (wcscmp(driver_name, L"\\driver\\vpci") == 0)
		{
			vmb_channel = (VMBCHANNEL)((PULONG_PTR)(device->DeviceExtension))[3];
		}
		else if (wcscmp(driver_name, L"\\driver\\vmstorfl") == 0)
		{
			for (ULONG i = 0; i < 30; i++)
			{
				LOG("[HyperVAgent] find_vmb_channel: [%ld] found??? vmb_channel=%p\n", i, ((PULONG_PTR)(device->DeviceExtension))[i]);
			}
			vmb_channel = (VMBCHANNEL)((PULONG_PTR)(device->DeviceExtension))[3];
		}
		else if (wcscmp(driver_name, L"\\driver\\storvsc") == 0)
		{
			for (ULONG i = 0; i < 50; i++)
			{
				LOG("[HyperVAgent] find_vmb_channel: [%ld] found??? vmb_channel=%p\n", i, ((PULONG_PTR)(device->DeviceExtension))[i]);
			}
			vmb_channel = (VMBCHANNEL)((PULONG_PTR)(device->DeviceExtension))[3];
		}
		else if (wcscmp(driver_name, L"\\driver\\netvsc") == 0)
		{
			for (ULONG i = 0; i < 30; i++)
			{
				LOG("[HyperVAgent] find_vmb_channel: [%ld] found??? vmb_channel=%p\n", i, ((PULONG_PTR)(device->DeviceExtension))[i]);
			}
			vmb_channel = (VMBCHANNEL)((PULONG_PTR)(device->DeviceExtension))[3];
		}

		// clear reference
		ObDereferenceObject(driver);
	}

	//if (vmb_channel)
	//{
	//	vmbPacket = pVmbPacketAllocate(vmb_channel);
	//}

	LOG("[HyperVAgent] find_vmb_channel: found vmb_channel=%p\n", vmb_channel);
}

VOID find_dynamic_addresses()
{
	LOG("[HyperVAgent] find_dynamic_addresses: starting\n");

	NTSTATUS status;
	UNICODE_STRING vmbkmclModuleName;

	// Locate vmbkmcl module in memory
	RtlInitUnicodeString(&vmbkmclModuleName, vmbkmclModulePath);
	status = get_module_address(&vmbkmclModuleName, &vmbkmclBaseAddress);
	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVAgent] find_dynamic_addresses: GetModuleAddress vmbkmcl failed\n");
	}
	else
	{
		LOG("[HyperVAgent] find_dynamic_addresses: found vmbkmclBaseAddress=%p\n", vmbkmclBaseAddress);
	}

	// Fetch the relevant functions from the module
	pVmbChannelSendSynchronousRequest = (PFN_VMB_CHANNEL_SEND_SYNCHRONOUS_REQUEST)kernel_get_proc_address(vmbkmclBaseAddress, (PCHAR)&vmbChannelSendSynchronousRequest);
	LOG("[HyperVAgent] find_dynamic_addresses: found pVmbChannelSendSynchronousRequest=%p\n", pVmbChannelSendSynchronousRequest);

	pVmbPacketAllocate = (PFN_VMB_PACKET_ALLOCATE)kernel_get_proc_address(vmbkmclBaseAddress, "VmbPacketAllocate");
	LOG("[HyperVAgent] find_dynamic_addresses: found pVmbPacketAllocate=%p\n", pVmbPacketAllocate);

	pVmbPacketInitialize = (PFN_VMB_PACKET_INITIALIZE)kernel_get_proc_address(vmbkmclBaseAddress, "VmbPacketInitialize");
	LOG("[HyperVAgent] find_dynamic_addresses: found pVmbPacketInitialize=%p\n", pVmbPacketInitialize);

	pVmbPacketSetCompletionRoutine = (PFN_VMB_PACKET_SET_COMPLETION_ROUTINE)kernel_get_proc_address(vmbkmclBaseAddress, "VmbPacketSetCompletionRoutine");
	LOG("[HyperVAgent] find_dynamic_addresses: found pVmbPacketSetCompletionRoutine=%p\n", pVmbPacketSetCompletionRoutine);

	pVmbPacketFree = (PFN_VMB_PACKET_FREE)kernel_get_proc_address(vmbkmclBaseAddress, "VmbPacketFree");
	LOG("[HyperVAgent] find_dynamic_addresses: found pVmbPacketFree=%p\n", pVmbPacketFree);

	pVmbPacketSend = (PFN_VMB_PACKET_SEND)kernel_get_proc_address(vmbkmclBaseAddress, "VmbPacketSend");
	LOG("[HyperVAgent] find_dynamic_addresses: found pVmbPacketSend=%p\n", pVmbPacketSend);

	pVmbPacketSendWithExternalMdl = (PFN_VMB_PACKET_SEND_WITH_EXTERNAL_MDL)kernel_get_proc_address(vmbkmclBaseAddress, "VmbPacketSendWithExternalMdl");
	LOG("[HyperVAgent] find_dynamic_addresses: found pVmbPacketSendWithExternalMdl=%p\n", pVmbPacketSendWithExternalMdl);

	pVmbChannelGetInterfaceInstance = (PFN_VMB_CHANNEL_GET_INTERFACE_INSTANCE)kernel_get_proc_address(vmbkmclBaseAddress, "VmbChannelGetInterfaceInstance");
	LOG("[HyperVAgent] find_dynamic_addresses: found pVmbChannelGetInterfaceInstance=%p\n", pVmbChannelGetInterfaceInstance);

	pVmbChannelSizeofPacket = (PFN_VMB_CHANNEL_SIZEOF_PACKET)kernel_get_proc_address(vmbkmclBaseAddress, "VmbChannelSizeofPacket");
	LOG("[HyperVAgent] find_dynamic_addresses: found pVmbChannelSizeofPacket=%p\n", pVmbChannelSizeofPacket);

	pVmbPacketSetPointer = (PFN_VMB_PACKET_SET_POINTER)kernel_get_proc_address(vmbkmclBaseAddress, "VmbPacketSetPointer");
	LOG("[HyperVAgent] find_dynamic_addresses: found pVmbPacketSetPointer=%p\n", pVmbPacketSetPointer);

	pKmclChannelList = (PCHAR)vmbkmclBaseAddress + KMCL_CHANNEL_LIST_OFFSET;
	LOG("[HyperVAgent] find_dynamic_addresses: found pKmclChannelList=%p\n", pKmclChannelList);
}

//VOID packet_handled(VMBPACKET packet, NTSTATUS status, PVOID buffer, UINT32 size)
//{
//	LOG("[HyperVAgent] packet_handled: packet=%p, status=%ld, buffer=%p size=%ld\n", packet, status, buffer, size);
//
//	sentPacketHandled = TRUE;
//}

VOID setPacketHandled(PVOID obj, PVOID packet, NTSTATUS status, PVOID buffer, UINT32 bufferLen)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(packet);
	UNREFERENCED_PARAMETER(status);
	UNREFERENCED_PARAMETER(buffer);
	UNREFERENCED_PARAMETER(bufferLen);

	sentPacketHandled = TRUE;
}



VOID send_channel_message(BYTE * in_buffer, ULONG _size)
{
	PMDL pmdl = NULL;
	NTSTATUS status;
	ULONG packet_size = pVmbChannelSizeofPacket(vmb_channel);
	ULONG size = _size;
	BYTE* buffer = in_buffer;

#ifdef DBG_REPEAT_FIRST
	if (first_msg)
	{
		first_msg_size = _size;
		first_msg_buffer = ExAllocatePoolWithTag(NonPagedPool, first_msg_size, POOL_TAG);

		if (first_msg_buffer)
		{
			memcpy(first_msg_buffer, in_buffer, first_msg_size);
			first_msg = FALSE;
		}
	}

	size = first_msg_size;
	buffer = first_msg_buffer;
#endif // DBG_REPEAT_FIRST


	VMBPACKET vmbpacket = NULL;
	BYTE reachable_states[] = { 0, 1, 2, 3, 7, 8, 9, 10};

	BYTE state_index = buffer[0] % sizeof(reachable_states);
	BYTE state = reachable_states[state_index];
	size--;
	buffer++;

	LOG("[HyperVAgent] send_channel_message: packet_size=%08x\n", packet_size);

	//ULONG out_size = size;
	//ULONG out_size = packet_size - sizeof(OUT_MESSAGE_HEADER);

	TIME_LOG("send_channel_message starting\n");

	msg_counter++;
	LOG("[HyperVAgent] send_channel_message: starting buffer=%p size=%ld msg_counter=%ld\n", buffer, size, msg_counter);

	//if (pmdl)
	//{
	//	IoFreeMdl(pmdl);
	//	pmdl = NULL;
	//}



	if (out_message_buffer == NULL)
	{
		out_message_buffer_size = sizeof(OUT_MESSAGE);
		//out_message_buffer_size = packet_size + sizeof(OUT_MESSAGE_HEADER);
		out_message_buffer = ExAllocatePoolWithTag(NonPagedPool, out_message_buffer_size, POOL_TAG);

		if (out_message_buffer == NULL)
		{
			LOG("[HyperVAgent] send_channel_message: failed to allocate buffer\n");
			return;
		}

		//memcpy(out_message_buffer->marker, fuzz_marker, FUZZ_MARKER_SIZE);
		//out_message_buffer->header.header_data[0] = 0x6b;
		//memcpy(out_message_buffer->header.header_data, storvsp_header, sizeof(storvsp_header));
	}

	memset(out_message_buffer, 0, out_message_buffer_size);

	//if (out_size > MAX_OUT_MESSAGE_SIZE)
	//{
	//	out_size = MAX_OUT_MESSAGE_SIZE;
	//}

	//if (out_size >= 2)
	//{
	//	//out_message_buffer->header.header_data[8] = buffer[0];
	//	out_message_buffer->header.header_data[8] = 0x9f;
	//	//out_message_buffer->header.header_data[12] = buffer[1];
	//	out_message_buffer->header.header_data[12] = 0x97;
	//	memcpy(out_message_buffer->extra_data, buffer + 2, out_size - 2);
	//}

	//LARGE_INTEGER timeout;
	//timeout.QuadPart = -10000;

	//__try
	//{
	//	status = pVmbChannelSendSynchronousRequest(vmb_channel, buffer, size, NULL, 0, NULL, 0, &timeout);

	//	if (NT_SUCCESS(status))
	//	{
	//		LOG("[HyperVAgent] send_channel_message: pVmbChannelSendSynchronousRequest success\n");
	//	}
	//	else
	//	{
	//		LOG("[HyperVAgent] send_channel_message: pVmbChannelSendSynchronousRequest failure status=0x%x\n", status);
	//	}
	//}
	//__except (EXCEPTION_EXECUTE_HANDLER)
	//{
	//	LOG("[HyperVAgent] send_channel_message: pVmbChannelSendSynchronousRequest exception exception=0x%x\n", GetExceptionCode());
	//}

	//memset(out_message_buffer->header.header_data, 0, sizeof(out_message_buffer->header.header_data));
	//memcpy(out_message_buffer->header.header_data /*+ sizeof(storvsp_header)*/, buffer, out_size);
	memcpy(out_message_buffer, buffer, min(size, out_message_buffer_size));
	// TODO - fix this - 3 is a very interesting case 
	// it is the only case after initialization
	//if (out_message_buffer->header.header_data[0] == 3)
	//{
	//	out_message_buffer->header.header_data[0] = 4;
	//}

	vmbpacket = (VMBPACKET)&out_message_buffer->packet;

	pVmbPacketInitialize(vmb_channel, vmbpacket, packet_size);

	*((ULONG*)out_message_buffer->header.header_data + 3) = sizeof(out_message_buffer->header.header_data);

	pVmbPacketSetPointer(vmbpacket, &out_message_buffer->prebuf);

	//*((ULONG*)&out_message_buffer->packet + 33) = 0x7000;

	for (ULONG i = 0; i < packet_size / (2*sizeof(PVOID)); i++)
	{
		LOG("%p  %p %p\n", (PVOID*)vmbpacket + i, ((PVOID*)vmbpacket)[i], ((PVOID*)vmbpacket)[i+1]);
	}

	out_message_buffer->header.header_data[1] |= 0x80;
	out_message_buffer->header.header_data[2] |= 0x80;
	out_message_buffer->header.header_data[3] = 0x80 | state;

	__try
	{
		TIME_LOG("send_channel_message before pVmbPacketAllocate\n");

		sentPacketHandled = FALSE;
		//VMBPACKET vmbPacket = pVmbPacketAllocate(vmb_channel);
		//pVmbPacketSetCompletionRoutine(vmbPacket, &packet_handled);


		

		if (size > sizeof(OUT_MESSAGE) - sizeof(out_message_buffer->extra_data)/*- sizeof(storvsp_header)*/)
		{
			ULONG mdl_size = max(MIN_MDL_SIZE, size - (sizeof(OUT_MESSAGE) - sizeof(out_message_buffer->extra_data)));
			*((ULONG*)out_message_buffer->header.header_data + 6) = mdl_size;
		//if (!pmdl)
		//{
			ULONG mdl_result = StorPortAllocateMdl(hw_device_extension, (PVOID)&out_message_buffer->extra_data, mdl_size /*- sizeof(out_message_buffer->header.header_data)*/, &pmdl);
			if (mdl_result != STOR_STATUS_SUCCESS)
			{
				pmdl = NULL;
				LOG("[HyperVAgent] send_channel_message: StorPortAllocateMdl failed. status=%08x\n", mdl_result);
			}
			//pmdl = IoAllocateMdl(&out_message_buffer->extra_data, out_size - sizeof(out_message_buffer->header.header_data) /*- sizeof(storvsp_header)*/, FALSE, FALSE, NULL);
			//((PUINT64)vmbPacket)[8] = (UINT64)&setPacketHandled;	
			TIME_LOG("send_channel_message before pVmbPacketSend\n");
			//status = pVmbPacketSend(vmbPacket, out_message_buffer, sizeof(out_message_buffer->header) + FUZZ_MARKER_SIZE, pmdl, 0);
			//status = pVmbPacketSend(vmbpacket, &out_message_buffer->header, sizeof(out_message_buffer->header), pmdl, 0);
			status = pVmbPacketSendWithExternalMdl(vmbpacket, &out_message_buffer->header, sizeof(out_message_buffer->header), pmdl, pmdl->ByteOffset, pmdl->ByteCount, 0x13);

			StorPortFreeMdl(hw_device_extension, pmdl);

		}
		else
		{
			status = pVmbPacketSend(vmbpacket, &out_message_buffer->header, sizeof(out_message_buffer->header), pmdl, 0);
		}


		if (NT_SUCCESS(status))
		{
			LOG("[HyperVAgent] send_channel_message: VmbPacketSend success\n");

			//while (!sentPacketHandled)
			//{
			//	LARGE_INTEGER to;
			//	to.QuadPart = -100000;
			//	KeDelayExecutionThread(KernelMode, FALSE, &to);

			//	//LOG("[HyperVAgent] send_channel_message: waiting for sentPacketHandled\n");
			//}

			//LOG("[HyperVAgent] send_channel_message: packet handled\n");
		}
		else
		{
			LOG("[HyperVAgent] send_channel_message: VmbPacketSend failure status=0x%x\n", status);
		}

		//pVmbPacketFree(vmbPacket);

		//if (pmdl)
		//{
		//	StorPortFreeMdl(hw_device_extension, pmdl);
		//	pmdl = NULL;
		//}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LOG("[HyperVAgent] send_channel_message: VmbPacketSend exception exception=0x%x\n", GetExceptionCode());
	}

	TIME_LOG("send_channel_message done\n");
}