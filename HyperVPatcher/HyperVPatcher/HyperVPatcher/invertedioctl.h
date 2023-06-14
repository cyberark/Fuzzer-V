#pragma once

#include "Globals.h"

extern KEVENT ioctl_pending_event;
extern KEVENT confirmation_event;
extern WDFQUEUE NotificationQueue;

VOID CompletePendingIoctl();
VOID WaitForPendingIoctl();
VOID CompleteOneIoctl();

//VOID thread_test_function(PVOID context);