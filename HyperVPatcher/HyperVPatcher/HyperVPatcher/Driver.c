#define INITGUID

#include "Driver.h"
#include "channel.h"
#include "ioctl.h"

NTSTATUS DriverEntry(IN OUT PDRIVER_OBJECT   DriverObject, IN PUNICODE_STRING RegistryPath)
{
	NTSTATUS                       status;
	WDF_DRIVER_CONFIG              config;
	WDFDRIVER                      hDriver;
	PWDFDEVICE_INIT                pInit = NULL;

	LOG("[HyperVPatch] DriverEntry\n");

	WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);
	config.DriverInitFlags |= WdfDriverInitNonPnpDriver;
	config.EvtDriverUnload = DriverUnload;

	status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, &hDriver);
	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVPatch] WdfDriverCreate failed with status 0x%x\n", status);
		return status;
	}

	pInit = WdfControlDeviceInitAllocate(hDriver, &MY_SDDL_DEVOBJ_SYS_ALL_ADM_ALL);
	if (pInit == NULL)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		return status;
	}

	KeInitializeEvent(&ioctl_pending_event, SynchronizationEvent, FALSE);
	KeInitializeEvent(&confirmation_event, SynchronizationEvent, FALSE);

	status = DeviceAdd(hDriver, pInit);

	return status;
}

VOID DriverUnload(WDFDRIVER DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);

	PAGED_CODE();

	channel_terminate();

	LOG("[HyperVPatch] DriverUnload\n");
	return;
}

NTSTATUS DeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit)
{
	LOG("[HyperVPatch] DeviceAdd\n");

	UNREFERENCED_PARAMETER(Driver);

	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES FdoAttributes;
	WDFDEVICE Device;
	WDF_IO_QUEUE_CONFIG ioQueueConfig;
	WDFQUEUE hQueue;
	PDEVICE_OBJECT deviceObject;

	WDF_OBJECT_ATTRIBUTES_INIT(&FdoAttributes);

	status = WdfDeviceInitAssignName(DeviceInit, &ntDeviceName);
	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVPatch] DeviceAdd: WdfDeviceInitAssignName Failed: %x\n", status);
		goto EvtDeviceAddEnd;
	}
	LOG("[HyperVPatch] DeviceAdd: WdfDeviceInitAssignName succeeded\n");

	status = WdfDeviceInitAssignSDDLString(DeviceInit, &MY_SDDL_DEVOBJ_SYS_ALL_ADM_ALL);
	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVPatch] DeviceAdd: WdfDeviceInitAssignSDDLString Failed: %x\n", status);
		goto EvtDeviceAddEnd;
	}
	LOG("[HyperVPatch] DeviceAdd: WdfDeviceInitAssignSDDLString succeeded\n");

	status = WdfDeviceCreate(&DeviceInit, &FdoAttributes, &Device);
	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVPatch] DeviceAdd: WdfDeviceCreate Failed: %x\n", status);
		goto EvtDeviceAddEnd;
	}
	LOG("[HyperVPatch] DeviceAdd: WdfDeviceCreate succeeded\n");

	deviceObject = WdfDeviceWdmGetDeviceObject(Device);
	if (deviceObject == NULL)
	{
		LOG("[HyperVPatch] DeviceAdd: WdfDeviceWdmGetDeviceObject Failed\n");
		goto EvtDeviceAddEnd;
	}
	LOG("[HyperVPatch] DeviceAdd: WdfDeviceWdmGetDeviceObject Succeeded\n");

	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchParallel);

	ioQueueConfig.EvtIoDeviceControl = IoctlHandler;

	status = WdfIoQueueCreate(Device, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, &hQueue);

	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVPatch] DeviceAdd: WdfIoQueueCreate Failed: %x\n", status);
		return status;
	}
	LOG("[HyperVPatch] DeviceAdd: WdfIoQueueCreate succeeded\n");

	WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig, WdfIoQueueDispatchManual);
	ioQueueConfig.PowerManaged = WdfFalse;
	status = WdfIoQueueCreate(Device, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, &NotificationQueue);
	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVPatch] DeviceAdd: WdfIoQueueCreate Failed: %x\n", status);
		return status;
	}
	LOG("[HyperVPatch] DeviceAdd: WdfIoQueueCreate succeeded\n");

	status = WdfDeviceCreateSymbolicLink(Device, &dosDeviceName);
	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVPatch] DeviceAdd: WdfDeviceCreateSymbolicLink Failed: %x\n", status);
		return status;
	}
	LOG("[HyperVPatch] DeviceAdd: WdfDeviceCreateSymbolicLink succeeded\n");

EvtDeviceAddEnd:
	return status;
}

