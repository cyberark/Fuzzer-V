#pragma once
#include "Globals.h"

#define DOS_DEVICE_NAME L"\\DosDevices\\HyperVPatch"
#define NT_DEVICE_NAME L"\\Device\\HyperVPatch"

DECLARE_CONST_UNICODE_STRING(dosDeviceName, DOS_DEVICE_NAME);
DECLARE_CONST_UNICODE_STRING(ntDeviceName, NT_DEVICE_NAME);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
VOID DriverUnload(WDFDRIVER DriverObject);
NTSTATUS DeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit);
