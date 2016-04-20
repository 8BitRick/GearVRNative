/************************************************************************************

PublicHeader:   OVR_Capture.h
Filename    :   OVR_Capture_FileIO.cpp
Content     :   Misc File IO functionality....
Created     :   January, 2015
Notes       :

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_Capture_FileIO.h"
#include "OVR_Capture_Local.h"

#include <stdlib.h> // for atoi
#include <string.h> // for strlen

#if defined(OVR_CAPTURE_USE_POSIX_FILES)
	#include <unistd.h>
	#pragma GCC diagnostic ignored "-Wconversion"
	#include <sys/stat.h>
	#pragma GCC diagnostic warning "-Wconversion"
#endif

#if defined(OVR_CAPTURE_WINDOWS)
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <windows.h>
#endif

namespace OVR
{
namespace Capture
{

#if OVR_CAPTURE_USE_POSIX_FILES
	FileHandle OpenFile(const char *path, bool writable)
	{
		if(writable)
			return creat(path, S_IRUSR | S_IWUSR);
		else
			return open(path, O_RDONLY);
	}
	void CloseFile(FileHandle file)
	{
		close(file);
	}
	int ReadFile(FileHandle file, void *buf, int bufsize)
	{
		return static_cast<int>(pread(file, buf, static_cast<size_t>(bufsize), 0));
	}
	int WriteFile(FileHandle file, const void *buf, int bufsize)
	{
		return static_cast<int>(write(file, buf, static_cast<size_t>(bufsize)));
	}
#else
	FileHandle OpenFile(const char *path, bool writable)
	{
	#if defined(OVR_CAPTURE_WINDOWS)
		FileHandle file = NullFileHandle;
		fopen_s(&file, path, writable ? "w" : "r");
		return file;
	#else
		return fopen(path, writable ? "w" : "r");
	#endif
	}
	void CloseFile(FileHandle file)
	{
		fclose(file);
	}
	int ReadFile(FileHandle file, void *buf, int bufsize)
	{
		fseek(file, 0, SEEK_SET);
		return (int)fread(buf, 1, bufsize, file);
	}
	int WriteFile(FileHandle file, const void *buf, int bufsize)
	{
		return (int)fwrite(buf, 1, bufsize, file);
	}
#endif

	bool CheckFileExists(const char *path)
	{
	#if defined(OVR_CAPTURE_WINDOWS)
		if(GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
			return false;
		return true;
	#else
		return (access(path, F_OK ) == 0);
	#endif
	}

	int ReadFileLine(const char *path, char *buf, int bufsize)
	{
		int ret = -1;
		FileHandle file = OpenFile(path);
		if(file != NullFileHandle)
		{
			ret = ReadFile(file, buf, bufsize);
			CloseFile(file);
		}
		if(ret > 0)
		{
			// Move to the end of the line and apply null terminator...
			char *end = buf+ret;
			*end = 0;
			for(char *curr=buf; curr<end; curr++)
			{
				if(*curr=='\n' || *curr=='\r')
				{
					*curr = 0;
					break;
				}
			}
		}
		return ret;
	}

	int ReadIntFile(FileHandle file)
	{
		int value = 0;
		if(file != NullFileHandle)
		{
			char buf[16];
			const int bytesRead = ReadFile(file, buf, 15);
			if(bytesRead > 0)
			{
				buf[bytesRead] = 0;
				value = atoi(buf);
			}
		}
		return value;
	}

	int ReadIntFile(const char *path)
	{
		int value = 0;
		FileHandle file = OpenFile(path);
		if(file != NullFileHandle)
		{
			value = ReadIntFile(file);
			CloseFile(file);
		}
		return value;
	}

	void WriteIntFile(FileHandle  file, int value)
	{
		if(file != NullFileHandle)
		{
			char buf[16];
			FormatString(buf, sizeof(buf), "%d", value);
			WriteFile(file, buf, (int)strlen(buf));
		}
	}

	void WriteIntFile(const char *path, int value)
	{
		FileHandle file = OpenFile(path, true);
		if(file != NullFileHandle)
		{
			WriteIntFile(file, value);
			CloseFile(file);
		}
	}

} // namespace Capture
} // namespace OVR

