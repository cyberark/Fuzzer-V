#pragma once

#include "globals.h"
#include "Storport.h"

// vmbkmcl offsets for Win11
#define KMCL_CHANNEL_LIST_OFFSET 0x151e0
#define NEXT_CHANNEL_OFFSET_IN_VMBCHANNEL 0x860
#define TYPE_GUID_OFFSET_IN_VMBCHANNEL 0xac0
#define PROCESSING_CALLBACK_IN_VMBCHANNEL 0x818

#define OUT_MSG_HEADER_SIZE  40
#define MAX_OUT_MESSAGE_SIZE 0x10000

#define MIN_MDL_SIZE		0x400

#define FUZZ_MARKER_SIZE 8

// TODO - remove! covrage testing
//#define DBG_REPEAT_FIRST	1

typedef struct _OUT_MESSAGE_PREBUF
{
	BYTE prebuf[0x60];
} OUT_MESSAGE_PREBUF, * POUT_MESSAGE_PREBUF;

typedef struct _OUT_MESSAGE_HEADER
{
	BYTE header_data[0x40];
} OUT_MESSAGE_HEADER, * POUT_MESSAGE_HEADER;

typedef struct _OUT_PACKET
{
	BYTE packet_data[0x1000];
} OUT_PACKET, * POUT_PACKET;

typedef struct _OUT_MESSAGE
{
	//BYTE marker[FUZZ_MARKER_SIZE];
	OUT_MESSAGE_PREBUF prebuf;
	OUT_MESSAGE_HEADER header;
	OUT_PACKET packet;
	BYTE extra_data[MAX_OUT_MESSAGE_SIZE];
} OUT_MESSAGE, *POUT_MESSAGE;

// Declare ObReferenceObjectByName - check if this works or if we need to find it dynamically
NTSTATUS
NTAPI
ObReferenceObjectByName(
	PUNICODE_STRING ObjectName,
	ULONG Attributes,
	PACCESS_STATE Passed,
	ACCESS_MASK DesiredAccess,
	POBJECT_TYPE ObjectType,
	KPROCESSOR_MODE Access,
	PVOID ParseContext,
	PVOID* ObjectPtr
);

extern ULONG msg_counter;


VOID find_hw_device_extension(const WCHAR* driver_name);

VOID find_vmb_channel(PVOID pointer, const WCHAR * driver_name, PGUID guid);

VOID find_dynamic_addresses();

VOID send_channel_message(BYTE * buffer, ULONG size);