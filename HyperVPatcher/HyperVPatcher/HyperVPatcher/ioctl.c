//#include "ioctl.h"
#include "ioctl.h"
#include "channel.h"

WDFQUEUE NotificationQueue;

BYTE fuzz_marker[] = { 0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88 };
ULONG fuzz_marker_size = sizeof(fuzz_marker);

DECLARE_CONST_UNICODE_STRING(
MY_SDDL_DEVOBJ_SYS_ALL_ADM_ALL,
L"D:P(A;;GA;;;SY)(A;;GA;;;BA)"
);


typedef struct _GET_CHANNEL_INPUT
{
	GUID guid;
} GET_CHANNEL_INPUT, *PGET_CHANNEL_INPUT;

VOID IoctlHandler(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength, size_t InputBufferLength, ULONG IoControlCode)
{
	UNREFERENCED_PARAMETER(Queue);
	UNREFERENCED_PARAMETER(OutputBufferLength);

	NTSTATUS status = STATUS_SUCCESS;
	PVOID in_buffer = NULL;
	SIZE_T in_buffer_size = 0;

	LOG("[HyperVPatch] IoctlHandler: IoControlCode=0x%x\n", IoControlCode);

	if (InputBufferLength > 0)
	{
		status = WdfRequestRetrieveInputBuffer(Request, 0, &in_buffer, &in_buffer_size);
		if (!NT_SUCCESS(status))
		{
			LOG("[HyperVPatch] IoctlHandler: WdfRequestRetrieveInputBuffer failed 0x%x\n", IoControlCode);
			WdfRequestComplete(Request, status);
			return;
		}
	}

	switch (IoControlCode)
	{
	case IOCTL_INIT:
		LOG("[HyperVPatch] IoctlHandler: 0x%x - IOCTL_INIT\n", IoControlCode);

		channel_init();
		FindDynamicAddresses();

		WdfRequestComplete(Request, STATUS_SUCCESS);
		return;

	case IOCTL_PATCH_CHANNEL:
		LOG("[HyperVPatch] IoctlHandler: 0x%x - IOCTL_PATCH_CHANNEL\n", IoControlCode);
		PGET_CHANNEL_INPUT get_channel_input = (PGET_CHANNEL_INPUT)in_buffer;
		if (in_buffer_size < sizeof(GET_CHANNEL_INPUT))
		{
			WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
			return;
		}

		PatchChannelProcessing(&get_channel_input->guid);

		WdfRequestComplete(Request, STATUS_SUCCESS);
		return;

	case IOCTL_INVERTED_NEXT_INPUT:

		//if (!thread_running)
		//{
		//	HANDLE thread_handle;
		//	thread_running = TRUE;
		//	PsCreateSystemThread(&thread_handle, GENERIC_ALL, NULL, NULL, NULL, &thread_test_function, NULL);
		//}

		LOG("[HyperVPatch] IoctlHandler: 0x%x - IOCTL_INVERTED_NEXT_INPUT\n", IoControlCode);

		status = WdfRequestForwardToIoQueue(Request, NotificationQueue);
		if (!NT_SUCCESS(status))
		{
			LOG("[HyperVPatch] IoctlHandler: WdfRequestForwardToIoQueue failed!\n");
			WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
		}
		return;

	default:
		LOG("[HyperVPatch] IoctlHandler: Unknown IOCTL 0x%x, returning\n", IoControlCode);
		WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
		return;
	}

	return;
}