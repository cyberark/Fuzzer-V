#pragma once
#include "Windows.h"
#include <string>

#define IOCTL_INIT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_CHANNEL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x2, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SEND_PACKET CTL_CODE(FILE_DEVICE_UNKNOWN, 0x3, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define MAX_INPUT_SIZE 0x10000

#define SERVICE_NAME "HyperVAgent"
#define SERVICE_PATH "C:\\Hyper-V\\Agent\\HyperVAgent.sys"

typedef struct _GET_CHANNEL_INPUT
{
	ULONG size;
	PVOID pointer;
	GUID guid;
	WCHAR name[1];
} GET_CHANNEL_INPUT, *PGET_CHANNEL_INPUT;

#define MAX_CONFIG_SIZE sizeof(GET_CHANNEL_INPUT) + MAX_PATH

int main(int argc, char *argv[]);
int init(int argc, char *argv[]);
int init_config();
int init_device();
int start_driver();
int config_device();
int read_file(std::string name, PBYTE buffer, DWORD size, DWORD * bytes_read);
int read_input_file();
int read_config_file();
int delete_input_file();
int send_ioctl();


#define LOG(log, ...)											\
do {							                                \
	if (strcmp(log, "screen") == 0)								\
	{															\
		printf(__VA_ARGS__);									\
	}															\
	else														\
	{															\
		FILE *log_file;											\
		if (fopen_s(&log_file, log, "a") == 0)					\
		{														\
			fprintf(log_file, __VA_ARGS__);						\
			fflush(log_file);									\
			fclose(log_file);				                    \
		}														\
	}															\
} while (0)