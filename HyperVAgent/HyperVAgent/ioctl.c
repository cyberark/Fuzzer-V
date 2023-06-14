#include "ioctl.h"
#include "channel.h"

DECLARE_CONST_UNICODE_STRING(
MY_SDDL_DEVOBJ_SYS_ALL_ADM_ALL,
L"D:P(A;;GA;;;SY)(A;;GA;;;BA)"
);

//typedef struct _GET_CHANNEL_INPUT
//{
//	ULONG size;
//	PVOID pointer;
//	GUID guid;
//	WCHAR name[1];
//} GET_CHANNEL_INPUT, *PGET_CHANNEL_INPUT;

typedef struct _GET_CHANNEL_INPUT
{
	GUID guid;
} GET_CHANNEL_INPUT, * PGET_CHANNEL_INPUT;

VOID IoctlHandler(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength, size_t InputBufferLength, ULONG IoControlCode)
{
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(Queue);

	NTSTATUS status = STATUS_SUCCESS;
	PVOID in_buffer = NULL;
	SIZE_T in_buffer_size = 0;

	LOG("[HyperVAgent] IoctlHandler: IoControlCode=0x%x\n", IoControlCode);

	if (InputBufferLength > 0)
	{
		status = WdfRequestRetrieveInputBuffer(Request, 0, &in_buffer, &in_buffer_size);
		if (!NT_SUCCESS(status))
		{
			LOG("[HyperVAgent] IoctlHandler: WdfRequestRetrieveInputBuffer failed 0x%x\n", IoControlCode);
			WdfRequestComplete(Request, status);
			return;
		}
	}

	switch (IoControlCode) 
	{
	case IOCTL_INIT:
		LOG("[HyperVAgent] IoctlHandler: 0x%x - IOCTL_INIT\n", IoControlCode);
		
		find_dynamic_addresses();
		WdfRequestComplete(Request, STATUS_SUCCESS);
	return;

	case IOCTL_GET_CHANNEL:
		LOG("[HyperVAgent] IoctlHandler: 0x%x - IOCTL_GET_CHANNEL\n", IoControlCode);
		//WCHAR target_driver_name[120];
		PGET_CHANNEL_INPUT get_channel_input = (PGET_CHANNEL_INPUT)in_buffer;
		if (in_buffer_size < sizeof(GET_CHANNEL_INPUT))
		{
			WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
			return;
		}

		//wcsncpy(target_driver_name, get_channel_input->name, BUFFER_COUNT(target_driver_name));

		//find_vmb_channel(get_channel_input->pointer, target_driver_name, &get_channel_input->guid);
		find_vmb_channel(NULL, NULL, &get_channel_input->guid);

		find_hw_device_extension(L"\\Driver\\storvsc");

		WdfRequestComplete(Request, STATUS_SUCCESS);
		return;

	case IOCTL_SEND_PACKET:
		LOG("[HyperVAgent] IoctlHandler: 0x%x - IOCTL_SEND_PACKET\n", IoControlCode);
		
		send_channel_message((BYTE *)in_buffer, (ULONG)in_buffer_size);
		
		WdfRequestComplete(Request, STATUS_SUCCESS);
		return;

	default:
		LOG("[HyperVAgent] IoctlHandler: Unknown IOCTL 0x%x, returning\n", IoControlCode);
		WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
		return;
	}

	return;
}

NTSTATUS InternalDeviceIoControl(PUNICODE_STRING DeviceName, DWORD32 IoControlCode, PVOID InputBuffer, DWORD32 InputBufferLength, PVOID OutputBuffer, DWORD32 OutputBufferLength)
{
	PFILE_OBJECT FileObject = NULL;
	NTSTATUS NtStatus = 0;
	PDEVICE_OBJECT DeviceObject = NULL;
	WDFDEVICE WdfDeviceObject = NULL;
	WDFIOTARGET IoTarget;

	WDF_MEMORY_DESCRIPTOR   inputDesc, outputDesc;
	PWDF_MEMORY_DESCRIPTOR  pInputDesc = NULL, pOutputDesc = NULL;
	ULONG_PTR               bytesReturned;

	// Get WDM device object pointer
	NtStatus = IoGetDeviceObjectPointer(DeviceName, FILE_ALL_ACCESS, &FileObject, &DeviceObject);
	if (!NT_SUCCESS(NtStatus))
	{
		LOG("Could not open a handle to the device 0x%x\n", NtStatus);
		goto DeviceNameError;
	}
	// We don't need the FileObject so we dereference it
	if (FileObject)
		ObfDereferenceObject(&FileObject);

	// Convert WDM PDEVICE_OBJECT to WDFDEVICE
	WdfDeviceObject = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);

	// Get an Io target that would allow us to send internal ioctl request
	IoTarget = WdfDeviceGetIoTarget(WdfDeviceObject);

	// Create memory descriptors that would act as input/output buffers
	if (InputBuffer) {
		WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputDesc,
			InputBuffer,
			InputBufferLength);
		pInputDesc = &inputDesc;
	}

	if (OutputBuffer) {
		WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc,
			OutputBuffer,
			OutputBufferLength);
		pOutputDesc = &outputDesc;

	}

	// Send Ioctl request which is Synchronously, might be better 
	NtStatus = WdfIoTargetSendIoctlSynchronously(IoTarget, WDF_NO_HANDLE, IoControlCode, pInputDesc, pOutputDesc, NULL, &bytesReturned);


	//WdfIoTargetSendIoctlSynchronously();



DeviceNameError:

	return NtStatus;


}