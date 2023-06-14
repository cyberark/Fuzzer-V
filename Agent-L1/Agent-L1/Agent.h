#pragma once
#include "Windows.h"
#include <string>

#define IOCTL_INIT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PATCH_CHANNEL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x2, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INVERTED_NEXT_INPUT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x3, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define MAX_INPUT_SIZE 0x10000

#define SERVICE_NAME "PatchVAgent"
#define SERVICE_PATH "C:\\Hyper-V\\Agent\\HyperVPatcher.sys"

#define NT_DEVICE_NAME L"\\.\\\\Globalroot\\Device\\HyperVPatch"

#define INPUT_FILE_PATH "C:\\Hyper-V\\input\\input.bin"


typedef struct _GET_CHANNEL_INPUT
{
	GUID guid;
} GET_CHANNEL_INPUT, *PGET_CHANNEL_INPUT;

#define MAX_CONFIG_SIZE sizeof(GET_CHANNEL_INPUT)

int main(int argc, char* argv[]);
int init(int argc, char* argv[]);
int init_device();
int start_driver();
int read_file(std::string name, PBYTE buffer, DWORD size, DWORD * bytes_read);
int read_config_file();

int patch_channel(BYTE* guid);
int device_control_event();
int write_to_shared(BYTE *buffer, DWORD size);


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
