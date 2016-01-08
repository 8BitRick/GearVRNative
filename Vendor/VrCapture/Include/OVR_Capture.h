/************************************************************************************

PublicHeader:   OVR_Capture.h
Filename    :   OVR_Capture.h
Content     :   Oculus performance capture library
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPTURE_H
#define OVR_CAPTURE_H

#include <OVR_Capture_Config.h>
#include <OVR_Capture_Types.h>

#include <stddef.h>
#include <stdarg.h>


// Builds unique variable name for local-static capture labels
#define OVR_CAPTURE_LABEL_NAME(name) _ovrcap_label_##name

// Thread-Safe creation of local static label...
#if defined(OVR_CAPTURE_CXX_11)
	// C++11 guarantees thread safe static initialization.
	#define OVR_CAPTURE_CREATE_LABEL(name) static const OVR::Capture::Label OVR_CAPTURE_LABEL_NAME(name)(#name);
#else
	// But previous standards do not.
	#define OVR_CAPTURE_CREATE_LABEL(name)                                                 \
		static OVR::Capture::Label OVR_CAPTURE_LABEL_NAME(name);                           \
		static bool                _ovrcap_label_initialized_##name = false;               \
		if(!_ovrcap_label_initialized_##name)                                              \
			_ovrcap_label_initialized_##name = OVR_CAPTURE_LABEL_NAME(name).ConditionalInit(#name);
#endif

// Quick drop in performance zone.
#define OVR_CAPTURE_CPU_ZONE(name)                                          \
	OVR_CAPTURE_CREATE_LABEL(name);                                         \
	OVR::Capture::CPUScope _ovrcap_cpuscope_##name(OVR_CAPTURE_LABEL_NAME(name));

#define OVR_CAPTURE_SENSOR_SET(name, value)                                 \
	if(OVR::Capture::IsConnected())                                         \
	{                                                                       \
		OVR_CAPTURE_CREATE_LABEL(name);                                     \
		OVR::Capture::SensorSetValue(OVR_CAPTURE_LABEL_NAME(name), value);  \
	}

namespace OVR
{
namespace Capture
{

	class CPUScope;

	// Get current time in nanoseconds...
	UInt64 GetNanoseconds(void);

	// Initializes the Capture system remote server.
	// should be called before any other Capture call.
	bool InitForRemoteCapture(UInt32 flags=Default_Flags, OnConnectFunc onConnect=NULL, OnDisconnectFunc onDisconnect=NULL);

	// Initializes the Capture system to store capture stream to disk, starting immediately.
	// should be called before any other Capture call.
	bool InitForLocalCapture(const char *outPath, UInt32 flags=Default_Flags, OnConnectFunc onConnect=NULL, OnDisconnectFunc onDisconnect=NULL);

	// Closes the capture system... no other Capture calls on *any* thead should be called after this.
	void Shutdown(void);

	// Indicates that the capture system is currently connected...
	bool IsConnected(void);

	// Check to see if (a) a connection is established and (b) that a particular capture feature is enabled on the connection.
	bool CheckConnectionFlag(const CaptureFlag feature);

	// Mark the currently referenced frame index on this thread...
	// You may call this from any thread that generates frame data to help track latency and missed frames.
	void FrameIndex(const UInt64 frameIndex);

	// Mark the start of vsync... this value should be comparable to the same reference point as GetNanoseconds()
	void VSyncTimestamp(UInt64 nanoseconds);

	// Upload the framebuffer for the current frame... should be called once a frame!
	void FrameBuffer(UInt64 nanoseconds, FrameBufferFormat format, UInt32 width, UInt32 height, const void *buf);

	// Misc application message logging...
	void Logf(LogPriority priority, const char *format, ...);
	void Logv(LogPriority priority, const char *format, va_list args);
	void Log( LogPriority priority, const char *str);

	// Mark a CPU profiled region.... Begin(); DoSomething(); End();
	// Nesting is allowed. And every Begin() should ALWAYS have a matching End()!!!
	void EnterCPUZone(const LabelIdentifier label);
	void LeaveCPUZone(void);

	// Set sensor range of values.
	void SensorSetRange(const LabelIdentifier label, float minValue, float maxValue, SensorInterpolator interpolator=Sensor_Interp_Linear, SensorUnits units=Sensor_Unit_None);

	// Set the absolute value of a sensor, may be called at any frequency.
	void SensorSetValue(const LabelIdentifier label, float value);

	float GetVariable(const LabelIdentifier label, float valDefault, float valMin, float valMax);

	class CPUScope
	{
		public:
			inline CPUScope(const LabelIdentifier label) :
				m_isReady(CheckConnectionFlag(Enable_CPU_Zones))
			{
				if(m_isReady) EnterCPUZone(label);
			}
			inline ~CPUScope(void)
			{
				if(m_isReady) LeaveCPUZone();
			}

		private:
			inline CPUScope(const CPUScope &) : m_isReady(false) {}
			inline void operator=(const CPUScope &) {}

		private:
			const bool m_isReady;
	};


} // namespace Capture
} // namespace OVR

#endif
