#define INITGUID

#include "driver.h"
#include "ioctl.h"

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry_path)
{
	NTSTATUS                       status;
	WDF_DRIVER_CONFIG              config;
	WDFDRIVER                      driver_handle;
	PWDFDEVICE_INIT                wdf_init = NULL;

	LOG("[HyperVAgent] DriverEntry\n");

	WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);
	config.DriverInitFlags |= WdfDriverInitNonPnpDriver;
	config.EvtDriverUnload = driver_unload;

	status = WdfDriverCreate(driver, registry_path, WDF_NO_OBJECT_ATTRIBUTES, &config, &driver_handle);
	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVAgent] WdfDriverCreate failed with status 0x%x\n", status);
		return status;
	}

	wdf_init = WdfControlDeviceInitAllocate(driver_handle, &MY_SDDL_DEVOBJ_SYS_ALL_ADM_ALL);
	if (wdf_init == NULL)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		return status;
	}

	status = device_add(driver_handle, wdf_init);

	return status;
}

VOID driver_unload(WDFDRIVER driver)
{
	UNREFERENCED_PARAMETER(driver);

	PAGED_CODE();
	LOG("[HyperVAgent] driver_unload\n");
	return;
}

NTSTATUS device_add(WDFDRIVER driver, PWDFDEVICE_INIT wdf_init)
{
	LOG("[HyperVAgent] DeviceAdd\n");

	UNREFERENCED_PARAMETER(driver);

	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES object_attributes;
	WDFDEVICE device;
	WDF_IO_QUEUE_CONFIG queue_config;
	WDFQUEUE queue_handle;
	PDEVICE_OBJECT device_object;

	WDF_OBJECT_ATTRIBUTES_INIT(&object_attributes);

	status = WdfDeviceInitAssignName(wdf_init, &nt_device_name);
	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVAgent] DeviceAdd: WdfDeviceInitAssignName Failed: %x\n", status);
		goto EvtDeviceAddEnd;
	}
	LOG("[HyperVAgent] DeviceAdd: WdfDeviceInitAssignName succeeded\n");

	status = WdfDeviceInitAssignSDDLString(wdf_init, &MY_SDDL_DEVOBJ_SYS_ALL_ADM_ALL);
	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVAgent] DeviceAdd: WdfDeviceInitAssignSDDLString Failed: %x\n", status);
		goto EvtDeviceAddEnd;
	}
	LOG("[HyperVAgent] DeviceAdd: WdfDeviceInitAssignSDDLString succeeded\n");

	status = WdfDeviceCreate(&wdf_init, &object_attributes, &device);
	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVAgent] DeviceAdd: WdfDeviceCreate Failed: %x\n", status);
		goto EvtDeviceAddEnd;
	}
	LOG("[HyperVAgent] DeviceAdd: WdfDeviceCreate succeeded\n");

	device_object = WdfDeviceWdmGetDeviceObject(device);
	if (device_object == NULL)
	{
		LOG("[HyperVAgent] DeviceAdd: WdfDeviceWdmGetDeviceObject Failed\n");
		goto EvtDeviceAddEnd;
	}
	LOG("[HyperVAgent] DeviceAdd: WdfDeviceWdmGetDeviceObject Succeeded\n");

	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue_config, WdfIoQueueDispatchSequential);

	queue_config.EvtIoDeviceControl = IoctlHandler;

	status = WdfIoQueueCreate(device, &queue_config, WDF_NO_OBJECT_ATTRIBUTES, &queue_handle);

	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVAgent] DeviceAdd: WdfIoQueueCreate Failed: %x\n", status);
		return status;
	}
	LOG("[HyperVAgent] DeviceAdd: WdfIoQueueCreate succeeded\n");

	status = WdfDeviceCreateSymbolicLink(device, &dos_device_name);
	if (!NT_SUCCESS(status))
	{
		LOG("[HyperVAgent] DeviceAdd: WdfDeviceCreateSymbolicLink Failed: %x\n", status);
		return status;
	}
	LOG("[HyperVAgent] DeviceAdd: WdfDeviceCreateSymbolicLink succeeded\n");

EvtDeviceAddEnd:
	return status;
}

