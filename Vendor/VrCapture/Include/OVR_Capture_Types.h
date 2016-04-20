/************************************************************************************

PublicHeader:   OVR_Capture.h
Filename    :   OVR_Capture_Types.h
Content     :   Oculus performance capture library.
Created     :   January, 2015
Notes       :

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPTURE_TYPES_H
#define OVR_CAPTURE_TYPES_H

#include <OVR_Capture_Config.h>

namespace OVR
{
namespace Capture
{

	// We choose very specific types for the network protocol...
	typedef unsigned long long UInt64;
	typedef unsigned int       UInt32;
	typedef unsigned short     UInt16;
	typedef unsigned char      UInt8;

	typedef signed long long   Int64;
	typedef signed int         Int32;
	typedef signed short       Int16;
	typedef signed char        Int8;

	OVR_CAPTURE_STATIC_ASSERT(sizeof(UInt64) == 8);
	OVR_CAPTURE_STATIC_ASSERT(sizeof(UInt32) == 4);
	OVR_CAPTURE_STATIC_ASSERT(sizeof(UInt16) == 2);
	OVR_CAPTURE_STATIC_ASSERT(sizeof(UInt8)  == 1);

	OVR_CAPTURE_STATIC_ASSERT(sizeof(Int64)  == 8);
	OVR_CAPTURE_STATIC_ASSERT(sizeof(Int32)  == 4);
	OVR_CAPTURE_STATIC_ASSERT(sizeof(Int16)  == 2);
	OVR_CAPTURE_STATIC_ASSERT(sizeof(Int8)   == 1);

	OVR_CAPTURE_STATIC_ASSERT(sizeof(float)  == 4);
	OVR_CAPTURE_STATIC_ASSERT(sizeof(double) == 8);

	typedef void (*OnConnectFunc)(UInt32 connectionFlags);
	typedef void (*OnDisconnectFunc)(void);

	enum CaptureFlag
	{
		Enable_CPU_Zones           = 1<<0,
		Enable_GPU_Zones           = 1<<1,
		Enable_CPU_Clocks          = 1<<2,
		Enable_GPU_Clocks          = 1<<3,
		Enable_Thermal_Sensors     = 1<<4,
		Enable_FrameBuffer_Capture = 1<<5,
		Enable_Logging             = 1<<6,
		Enable_SysTrace            = 1<<7,
		Enable_GraphicsAPI         = 1<<8,

		Default_Flags           = Enable_CPU_Zones | Enable_GPU_Zones,
		All_Flags               = Enable_CPU_Zones | Enable_GPU_Zones | Enable_CPU_Clocks | Enable_GPU_Clocks | Enable_Thermal_Sensors | Enable_FrameBuffer_Capture | Enable_Logging | Enable_SysTrace | Enable_GraphicsAPI,
	};

	enum FrameBufferFormat
	{
		FrameBuffer_RGB_565 = 0,
		FrameBuffer_RGBA_8888,
		FrameBuffer_DXT1,
	};

	enum SensorInterpolator
	{
		Sensor_Interp_Linear = 0,
		Sensor_Interp_Nearest,
	};

	enum SensorUnits
	{
		Sensor_Unit_None = 0,

		// Frequencies...
		Sensor_Unit_Hz,
		Sensor_Unit_KHz,
		Sensor_Unit_MHz,
		Sensor_Unit_GHz,

		// Memory size...
		Sensor_Unit_Byte,
		Sensor_Unit_KByte,
		Sensor_Unit_MByte,
		Sensor_Unit_GByte,

		// Memory Bandwidth...
		Sensor_Unit_Byte_Second,
		Sensor_Unit_KByte_Second,
		Sensor_Unit_MByte_Second,
		Sensor_Unit_GByte_Second,

		// Temperature
		Sensor_Unit_Celsius,
	};

	enum LogPriority
	{
		Log_Info = 0,
		Log_Warning,
		Log_Error,
	};

	class LabelIdentifier
	{
		public:
			inline LabelIdentifier(void) {}

			inline LabelIdentifier(const LabelIdentifier &other) :
				m_identifier(other.m_identifier)
			{
			}

			inline const LabelIdentifier &operator=(const LabelIdentifier other)
			{
				m_identifier = other.m_identifier;
				return *this;
			}

			inline UInt32 GetIdentifier(void) const
			{
				return m_identifier;
			}

		protected:
			UInt32 m_identifier;
	};

	class Label : public LabelIdentifier
	{
		friend class RemoteServer;
		friend class LocalServer;
		public:
			// Use this constructor if Label is a global variable (not local static).
			explicit Label(const char *name);

			// we assume we are already initialized to zero... C++ spec guarantees this for global/static memory.
			// use this function if Label is a local static variable. and initialize using conditionalInit()
			inline Label(void) {}

			bool ConditionalInit(const char *name);

			inline const char  *GetName(void) const
			{
				return m_name;
			}

		private:
			static Label *GetHead(void);
			Label *GetNext(void) const;

			inline Label(const Label &other) : LabelIdentifier() {}
			inline void operator=(const Label&) {}

		private:
			static Label  *s_head;
			Label         *m_next;
			const char    *m_name;
	};

	template<typename T>
	struct Rect
	{
		T x;
		T y;
		T width;
		T height;

		inline bool operator==(const Rect<T> &other) const
		{
			return x==other.x && y==other.y && width==other.width && height==other.height;
		}

		inline bool operator!=(const Rect<T> &other) const
		{
			return x!=other.x || y!=other.y || width!=other.width || height!=other.height;
		}
	};

} // namespace Capture
} // namespace OVR

#endif
