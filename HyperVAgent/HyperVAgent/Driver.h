#pragma once
#include "globals.h"

#define DOS_DEVICE_NAME L"\\DosDevices\\HyperVAgent"
#define NT_DEVICE_NAME L"\\Device\\HyperVAgent"

DECLARE_CONST_UNICODE_STRING(dos_device_name, DOS_DEVICE_NAME);
DECLARE_CONST_UNICODE_STRING(nt_device_name, NT_DEVICE_NAME);

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry_path);
VOID driver_unload(WDFDRIVER driver);
NTSTATUS device_add(WDFDRIVER driver, PWDFDEVICE_INIT wdf_init);
