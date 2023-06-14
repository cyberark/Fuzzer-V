#include "invertedioctl.h"

KEVENT ioctl_pending_event;
KEVENT confirmation_event;

volatile LONG waiting_for_confirmation = FALSE;

VOID CompletePendingIoctl()
{
	NTSTATUS status;
	WDFREQUEST request;

	status = WdfIoQueueRetrieveNextRequest(NotificationQueue, &request);
	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVPatch] CompletePendingIoctl: WdfIoQueueRetrieveNextRequest failed!\n");
		return;
	}

	LOG("[HyperVPatch] CompletePendingIoctl: WdfIoQueueRetrieveNextRequest success\n");

	WdfRequestComplete(request, STATUS_SUCCESS);
	LOG("[HyperVPatch] CompletePendingIoctl: after WdfRequestComplete\n");
}

VOID WaitForPendingIoctl()
{
	WDF_IO_QUEUE_STATE state = STATUS_TIMEOUT;
	ULONG queue_requests;
	ULONG driver_requests;

	LOG("[HyperVPatch] WaitForPendingIoctl: starting\n");
	
	state = WdfIoQueueGetState(NotificationQueue, &queue_requests, &driver_requests);
	while (queue_requests == 0)
	{
		KeStallExecutionProcessor(10);
		WdfIoQueueGetState(NotificationQueue, &queue_requests, &driver_requests);
	}

	LOG("[HyperVPatch] WaitForPendingIoctl: done\n");
}

VOID CompleteOneIoctl()
{
	WaitForPendingIoctl();
	CompletePendingIoctl();
}

//
//
//VOID thread_test_function(PVOID context)
//{
//	UNREFERENCED_PARAMETER(context);
//
//	LOG("[HyperVPatch] thread_test_function: starting\n");
//
//	LARGE_INTEGER timeout;
//	timeout.QuadPart = -1000;
//
//	for (ULONG i = 0; i < 10; i++)
//	{
//		LOG("[HyperVPatch] thread_test_function: i=%ld starting\n", i);
//
//		WaitForPendingIoctl();
//
//		KeDelayExecutionThread(KernelMode, FALSE, &timeout);
//
//		LOG("[HyperVPatch] thread_test_function: i=%ld after WaitForPendingIoctl\n", i);
//
//		waiting_for_confirmation = TRUE;
//		CompletePendingIoctl(TRUE);
//
//		KeDelayExecutionThread(KernelMode, FALSE, &timeout);
//
//		LOG("[HyperVPatch] thread_test_function: i=%ld after CompletePendingIoctl\n", i);
//
//		WaitForConfirmation();
//
//		KeDelayExecutionThread(KernelMode, FALSE, &timeout);
//
//		LOG("[HyperVPatch] thread_test_function: i=%ld after WaitForConfirmation\n", i);
//	}
//
//	LOG("[HyperVPatch] thread_test_function: done\n");
//}
