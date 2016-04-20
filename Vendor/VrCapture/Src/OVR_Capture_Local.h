/************************************************************************************

PublicHeader:   OVR_Capture.h
Filename    :   OVR_Capture_Local.h
Content     :   Internal Capture API
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPTURE_LOCAL_H
#define OVR_CAPTURE_LOCAL_H

#include <OVR_Capture_Config.h>
#include <OVR_Capture_Types.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

namespace OVR
{
namespace Capture
{

	bool TryLockConnection(void);
	bool TryLockConnection(const CaptureFlag feature);
	void UnlockConnection(void);

	static inline int FormatStringV(char *buffer, size_t bufferSize, const char *format, va_list args)
	{
	#if defined(OVR_CAPTURE_WINDOWS)
		return vsprintf_s(buffer, bufferSize, format, args);
	#else
		return vsnprintf(buffer, bufferSize, format, args);
	#endif
	}

	static inline int FormatString(char *buffer, size_t bufferSize, const char *format, ...)
	{
		va_list args;
		va_start(args, format);
		const int ret = FormatStringV(buffer, bufferSize, format, args);
		va_end(args);
		return ret;
	}

	static inline void StringCopy(char *dest, const char *source, size_t size)
	{
	#if defined(OVR_CAPTURE_WINDOWS)
		strncpy_s(dest, size, source, size);
	#else
		strncpy(dest, source, size);
	#endif
	}

} // namespace Capture
} // namespace OVR

#endif
