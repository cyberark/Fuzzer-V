// Agent.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "Agent.h"

// Globals
std::string new_input_event_name;
HANDLE new_input_event;
std::string input_file_name;
PBYTE input_buffer;
DWORD input_size;
std::string config_file_name;
BYTE config[MAX_CONFIG_SIZE];
DWORD config_size;
std::string device_file_name = "\\\\.\\HyperVAgent";
HANDLE device_handle;

int main(int argc, char *argv[])
{
	int rc = 0;
	bool terminate = false;

	LOG("screen", "agent starting\n");
	LOG("C:\\Hyper-V\\agent\\log.txt", "agent starting\n");

	rc = init(argc, argv);
	if (rc)
	{
		LOG("screen", "init failed!\n");
		LOG("C:\\Hyper-V\\agent\\log.txt", "init failed!\n");
		return rc;
	}

	while (!terminate)
	{
		WaitForSingleObject(new_input_event, INFINITE);

		LOG("screen", "incoming message\n");
		LOG("C:\\Hyper-V\\agent\\log.txt", "incoming message\n");

		rc = read_input_file();
		if (rc)
		{
			LOG("screen", "read_input_file failed! LastError=%08x\n", GetLastError());
			LOG("C:\\Hyper-V\\agent\\log.txt", "read_input_file failed! LastError=%08x\n", GetLastError());
			break;
		}

		LOG("screen", "input file read\n");
		LOG("C:\\Hyper-V\\agent\\log.txt", "input file read\n");

		rc = delete_input_file();
		if (rc)
		{
			LOG("screen", "delete_input_file failed! LastError=%08x\n", GetLastError());
			LOG("C:\\Hyper-V\\agent\\log.txt", "delete_input_file failed! LastError=%08x\n", GetLastError());
			break;
		}

		LOG("screen", "input file deleted\n");
		LOG("C:\\Hyper-V\\agent\\log.txt", "input file deleted\n");

		rc = send_ioctl();
		if (rc)
		{
			LOG("screen", "send_ioctl failed! LastError=%08x\n", GetLastError());
			LOG("C:\\Hyper-V\\agent\\log.txt", "send_ioctl failed! LastError=%08x\n", GetLastError());
			break;
		}

		LOG("screen", "message sent\n");
		LOG("C:\\Hyper-V\\agent\\log.txt", "message sent\n");
	}

	return rc;
}

int init(int argc, char *argv[])
{
	input_buffer = (PBYTE)malloc(MAX_INPUT_SIZE);
	if (input_buffer == NULL)
	{
		LOG("screen", "failed to allocate input_buffer\n");
		return -1;
	}

	if (argc < 4)
	{
		LOG("screen", "not enough arguments\nUsage: Agent.exe <new_input_event_name> <input_file_name> <config_file_name>\n");
		return -1;
	}

	new_input_event_name = argv[1];
	new_input_event = CreateEventA(NULL, false, false, new_input_event_name.c_str());
	if (new_input_event == NULL)
	{
		LOG("screen", "failed to create new_input_event\n");
		return -1;
	}

	input_file_name = argv[2];
	config_file_name = argv[3];

	if (init_config())
	{
		LOG("screen", "init_config failed\n");
		return -1;
	}

	return 0;
}

int init_config()
{
	int rc = 0;

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

	rc = config_device();
	if (rc)
	{
		LOG("screen", "config_device failed\n");
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

int config_device()
{
	DWORD bytes_returned;

	if (DeviceIoControl(device_handle, IOCTL_GET_CHANNEL, config, config_size, NULL, 0, &bytes_returned, NULL) == 0)
	{
		LOG("screen", "DeviceIoControl(IOCTL_GET_CHANNEL) failed. LastError=%08x\n", GetLastError());
		return -1;
	}

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

int read_input_file()
{
	return read_file(input_file_name, input_buffer, MAX_INPUT_SIZE, &input_size);
}

int read_config_file()
{
	return read_file(config_file_name, config, sizeof(config), &config_size);
}

int delete_input_file()
{
	return !DeleteFileA(input_file_name.c_str());
}

int send_ioctl()
{
	DWORD bytes_returned;

	if (DeviceIoControl(device_handle, IOCTL_SEND_PACKET, input_buffer, input_size, NULL, 0, &bytes_returned, NULL) == 0)
	{
		LOG("screen", "DeviceIoControl(IOCTL_SEND_PACKET) failed. LastError=%08x\n", GetLastError());
		return -1;
	}

	return 0;
}

