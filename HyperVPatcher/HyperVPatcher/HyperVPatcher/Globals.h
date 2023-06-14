#pragma once
#define NDIS620 1
#pragma warning(disable : 4201)
#pragma comment(lib, "wdmsec.lib")
#pragma comment(lib, "aux_klib.lib")

#include <ntddk.h>
#include <wdf.h>
#include <aux_klib.h>
#include <Ndis.h>
#include <Ntstrsafe.h>
#include <Wdm.h>
#include <wdmsec.h>
#include <vmbuskernelmodeclientlibapi.h>

#define BUFFER_COUNT(buffer) sizeof(buffer) / sizeof(buffer[0])

#define DEBUG_LEVEL 0
#define POOL_TAG 'MODs'
#define HARNESS_POOL_TAG 'snrH'
#define NVSP_RNDIS_SEND_PACKET_SIZE 0x28
#define FAKE_ARRAY_SIZE 0x800
#define PACKET_BUF_SIZE 0x100
PVOID NTAPI RtlImageDirectoryEntryToData(
	PVOID ImageBase,
	BOOLEAN MappedAsImage,
	USHORT DirectoryEntry,
	PULONG Size);

#define LOG(fmt, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DEBUG_LEVEL, fmt, __VA_ARGS__);
