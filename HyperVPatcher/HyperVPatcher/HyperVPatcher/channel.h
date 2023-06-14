#pragma once

#include "Globals.h"

typedef VOID (*channel_processing_callback)(VMBCHANNEL, VMBPACKETCOMPLETION, PVOID, UINT32, UINT32);
typedef VOID (*channel_processing_complete_callback)(VMBCHANNEL, UINT32);

#define KMCLR_CHANNEL_LIST_OFFSET 0x141a0

#define NEXT_CHANNEL_OFFSET_IN_VMBCHANNEL 0x7a0

#define TYPE_GUID_OFFSET_IN_VMBCHANNEL 0x990

#define PROCESSING_CALLBACK_IN_VMBCHANNEL 0x710

#define PROCESSING_COMPLETE_CALLBACK_IN_VMBCHANNEL 0x718

extern BYTE fuzz_marker[];
extern ULONG fuzz_marker_size;

VOID channel_init();
VOID channel_terminate();
VOID FindVmbChannel(PGUID guid);
VOID FindDynamicAddresses();
VOID PatchChannelProcessing(PGUID guid);
VOID processing_hook_function(VMBCHANNEL channel, VMBPACKETCOMPLETION packet, PVOID buffer, UINT32 size, UINT32 flags);
BOOLEAN is_fuzz_message(PVOID buffer, UINT32 size);
