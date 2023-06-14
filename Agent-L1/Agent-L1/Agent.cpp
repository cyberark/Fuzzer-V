// Agent.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "Agent.h"

// Globals
std::string output_file_name;
std::string config_file_name;
BYTE config[MAX_CONFIG_SIZE];
DWORD config_size;
std::string device_file_name = "\\\\.\\HyperVPatch";
HANDLE device_handle;

int main(int argc, char* argv[]) 
{
	// TODO - remove - testing!

	BYTE input[] = { 1,2,3,4,5,6,7,8 };

	// end of remove

	int rc = 0;
	DWORD bytes_returned = 0;

	LOG("screen", "agent starting L1\n");

	rc = init(argc, argv);
	if (rc)
	{
		LOG("screen", "init failed!\n");
		return rc;
	}

	while (true)
	{
		//Sleep(1000);

		LOG("screen", "starting main loop\n");
		LOG("C:\\Hyper-V\\Agent-L1\\log.txt", "starting main loop\n");

		//Hypercall Get input from L0 (Host)
		//Sync with L0

		// write file to shared folder
		rc = write_to_shared(input, sizeof(input));
		if (rc)
		{
			LOG("screen", "write_to_shared failed!\n");
			LOG("C:\\Hyper-V\\Agent-L1\\log.txt", "write_to_shared failed!\n");
			continue;
		}

		// wait until HyperVPatcher tells us the input was processed
		if (!DeviceIoControl(device_handle, IOCTL_INVERTED_NEXT_INPUT, NULL, 0, NULL, 0, &bytes_returned, NULL))
		{
			LOG("screen", "device_control_event DeviceIoControl(IOCTL_INVERTED_CALL_CONFIRMATION) failed!\n");
			LOG("C:\\Hyper-V\\Agent-L1\\log.txt", "device_control_event DeviceIoControl(IOCTL_INVERTED_CALL_CONFIRMATION) failed!\n");
			continue;
		}

		LOG("screen", "message sent\n");
		LOG("C:\\Hyper-V\\Agent-L1\\log.txt", "message sent\n");
	}

	return rc;
}


int init(int argc, char* argv[])
{
	int rc = 0;

	if (argc < 3)
	{
		LOG("screen", "not enough arguments\nUsage: Agent-L1.exe <output_file_name> <config_file_name>\n");
		return -1;
	}

	output_file_name = argv[1];
	config_file_name = argv[2];

	rc = read_config_file();
	if (rc)
	{
		LOG("screen", "failed to read config file\n");
		return rc;
	}

	rc = init_device();
	if (rc)
	{
		LOG("screen", "init_device failed\n");
		return rc;
	}

	rc = patch_channel(config);
	if (rc)
	{
		LOG("screen", "patch_channel failed\n");
		return rc;
	}

	return 0;
}

int init_device()
{
	int rc;
	DWORD bytes_returned;

	// start the driver
	rc = start_driver();
	if (rc)
	{
		LOG("screen", "failed to start driver\n");
		return -1;
	}

	// open the device
	device_handle = CreateFileA(device_file_name.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (device_handle == INVALID_HANDLE_VALUE)
	{
		LOG("screen", "failed to open device_handle\n");
		return -1;
	}

	// send init ioctl to device
	if (DeviceIoControl(device_handle, IOCTL_INIT, NULL, 0, NULL, 0, &bytes_returned, NULL) == 0)
	{
		LOG("screen", "DeviceIoControl(IOCTL_INIT) failed. LastError=%08x\n", GetLastError());
		return -1;
	}

	return 0;
}

int start_driver()
{
	SC_HANDLE sc_manager_handle;
	SC_HANDLE service_handle;
	DWORD LastError;

	// Get a handle to the SCM database. 

	sc_manager_handle = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (sc_manager_handle == NULL)
	{
		LOG("screen", "OpenSCManagerA failed. LastError=%08x\n", GetLastError());
		return -1;
	}

	// Get a handle to the service.

	service_handle = CreateServiceA(sc_manager_handle, SERVICE_NAME, SERVICE_NAME, SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
		SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, SERVICE_PATH, NULL, NULL, NULL, NULL, NULL);
	if (service_handle == NULL)
	{
		LastError = GetLastError();
		if (LastError != ERROR_SERVICE_EXISTS)
		{
			CloseServiceHandle(sc_manager_handle);

			LOG("screen", "CreateServiceA failed. LastError=%08x\n", LastError);
			return -1;
		}
		else
		{
			LOG("screen", "Service already exists. Trying to open\n");
			service_handle = OpenServiceA(sc_manager_handle, SERVICE_NAME, SERVICE_ALL_ACCESS);
			if (service_handle == NULL)
			{
				CloseServiceHandle(sc_manager_handle);

				LOG("screen", "OpenServiceA failed. LastError=%08x\n", GetLastError());
				return -1;
			}
		}
	}

	if (!StartServiceA(service_handle, 0, NULL))
	{
		LastError = GetLastError();
		if (LastError != ERROR_SERVICE_ALREADY_RUNNING)
		{
			CloseServiceHandle(service_handle);
			CloseServiceHandle(sc_manager_handle);

			LOG("screen", "StartServiceA failed. LastError=%08x\n", LastError);
			return -1;
		}
	}

	CloseServiceHandle(service_handle);
	CloseServiceHandle(sc_manager_handle);

	return 0;
}

int read_file(std::string name, PBYTE buffer, DWORD size, DWORD * bytes_read)
{
	int rc = 0;
	HANDLE file_handle;
	*bytes_read = 0;

	file_handle = CreateFileA(name.c_str(), FILE_ALL_ACCESS, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file_handle == INVALID_HANDLE_VALUE)
	{
		LOG("screen", "failed to open file %s\n", name.c_str());
		rc = -1;
	}
	else
	{
		if (!ReadFile(file_handle, buffer, size, bytes_read, NULL))
		{
			LOG("screen", "failed to read file %s\n", name.c_str());
			rc = -1;
		}

		if (*bytes_read == 0)
		{
			LOG("screen", "no content read from file %s\n", name.c_str());
			rc = -1;
		}

		CloseHandle(file_handle);
	}

	return rc;
}

int read_config_file()
{
	return read_file(config_file_name, config, sizeof(config), &config_size);
}

int patch_channel(BYTE* guid)
{
	int rc = 0;
	DWORD bytes_returned = 0;
	DWORD size = sizeof(GET_CHANNEL_INPUT);

	if (!DeviceIoControl(device_handle, IOCTL_PATCH_CHANNEL, guid, size, NULL, 0, &bytes_returned, NULL))
	{
		rc = -1;
	}

	return rc;
};

int write_to_shared(BYTE *buffer, DWORD size)
{
	HANDLE file;
	DWORD bytes_written;
	DWORD rc;

	LOG("screen", "write_to_shared starting\n");
	LOG("C:\\Hyper-V\\Agent-L1\\log.txt", "write_to_shared starting\n");

	file = CreateFileA(output_file_name.c_str(), FILE_ALL_ACCESS, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		LOG("screen", "write_to_shared CreateFileA failed. LastError=%08x\n", GetLastError());
		LOG("C:\\Hyper-V\\Agent-L1\\log.txt", "write_to_shared CreateFileA failed. LastError=%08x\n", GetLastError());
		return -1;
	}

	rc = WriteFile(file, buffer, size, &bytes_written, NULL);
	CloseHandle(file);
	if (!rc)
	{
		LOG("screen", "write_to_shared WriteFile failed. LastError=%08x\n", GetLastError());
		LOG("C:\\Hyper-V\\Agent-L1\\log.txt", "write_to_shared WriteFile failed. LastError=%08x\n", GetLastError());
		return -1;
	}

	LOG("screen", "write_to_shared done\n");
	LOG("C:\\Hyper-V\\Agent-L1\\log.txt", "write_to_shared done\n");

	return 0;
}