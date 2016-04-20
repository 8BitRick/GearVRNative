/************************************************************************************

Filename    :   OVR_Capture_StreamProcessor.h
Content     :   capture stream parser/dispatcher.  Copied
				mostly from OVR_Monitor_StreamProcessor
Created     :   June, 2015
Notes       : 
Author      :   James Dolan, Amanda M. Watson

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPTURE_STREAMPROCESSOR_H
#define OVR_CAPTURE_STREAMPROCESSOR_H

#include <OVR_Capture_LegacyPackets.h>

#include <vector>

namespace OVR
{
namespace Capture
{
	class StreamProcessor
	{
		public:
			StreamProcessor(void);
			virtual ~StreamProcessor(void);

			bool ProcessData(const void *buffer, size_t bufferSize);

			virtual void Close(void);

		private:
			// Implement these functions in the child class depending on what data you are trying to extract from the data stream...

			virtual void onStreamError(const char *msg) {}

			virtual void onThreadName(  Capture::UInt32 threadID, const char *name, size_t nameSize) {}
			virtual void onLabel(       Capture::UInt32 labelID, const char *str, size_t strSize) {}
			virtual void onVSync(       double timeInSeconds) {}
			virtual void onFrameIndex(  Capture::UInt32 threadID, Capture::UInt64 frameIndex, double timeInSeconds) {}
			virtual void onCPUZoneEnter(Capture::UInt32 threadID, Capture::UInt32 labelID, double timeInSeconds) {}
			virtual void onCPUZoneLeave(Capture::UInt32 threadID, double timeInSeconds) {}
			virtual void onGPUZoneEnter(Capture::UInt32 threadID, Capture::UInt32 labelID, double timeInSeconds) {}
			virtual void onGPUZoneLeave(Capture::UInt32 threadID, double timeInSeconds) {}
			virtual void onGPUClockSync(Capture::UInt32 threadID, double timeOffsetInSeconds) {}
			virtual void onSensorRange( Capture::UInt32 labelID, Capture::SensorInterpolator interpolator, Capture::SensorUnits units, float minValue, float maxValue) {}
			virtual void onSensor(      Capture::UInt32 labelID, double timeInSeconds, float value) {}
			virtual void onFrameBuffer( double timeInSeconds, Capture::FrameBufferFormat format, Capture::UInt32 width, Capture::UInt32 height, const void *imageData, size_t imageDataSize) {}
			virtual void onLog(         Capture::UInt32 threadID, Capture::LogPriority priority, double timeInSeconds, const char *message, size_t messageSize) {}
			virtual void onParamRange(  Capture::UInt32 labelID, float value, float valMin, float valMax) {}
			virtual void onParamValue(  Capture::UInt32 labelID, float value) {}
			virtual void onParamRange(  Capture::UInt32 labelID, Capture::Int32 value, Capture::Int32 valMin, Capture::Int32 valMax) {}
			virtual void onParamValue(  Capture::UInt32 labelID, Capture::Int32 value) {}
			virtual void onParamValue(  Capture::UInt32 labelID, bool value) {}

		private:
			typedef std::vector<UInt8> DataVector;

			typedef size_t (*ProcessPacketFunc)(StreamProcessor &p, DataVector::iterator curr, DataVector::iterator end);

			template<typename PacketType, bool hasPayload> struct PacketFuncAccessor;
			template<typename PacketType> struct PacketFuncAccessor<PacketType, true>
			{
				static ProcessPacketFunc GetFunc(void)
				{
					return &ProcessPacketWithPayload<PacketType>;
				}
			};
			template<typename PacketType> struct PacketFuncAccessor<PacketType, false>
			{
				static ProcessPacketFunc GetFunc(void)
				{
					return &ProcessPacket<PacketType>;
				}
			};

			struct PacketFuncDesc
			{
				UInt32            packetID;
				UInt32            version;
				ProcessPacketFunc func;

				template<typename PacketType> static PacketFuncDesc Create(void)
				{
					PacketFuncDesc d;
					d.packetID = PacketType::s_packetID;
					d.version  = PacketType::s_version;
					d.func     = PacketFuncAccessor<PacketType,PacketType::s_hasPayload>::GetFunc();
					return d;
				}
			};

		private:
			template<typename PacketType>
			void DispatchPacket(const PacketType &packet);

			template<typename PacketType>
			void DispatchPacket(const PacketType &packet, const void *payloadData, size_t payloadSize);

		private:
			template<typename PacketType>
			static size_t ProcessPacket(StreamProcessor &p, DataVector::iterator curr, DataVector::iterator bufferEnd);

			template<typename PacketType>
			static size_t ProcessPacketWithPayload(StreamProcessor &p, DataVector::iterator curr, DataVector::iterator bufferEnd);

			static size_t SkipPacket(size_t sizeofPacket, DataVector::iterator curr, DataVector::iterator bufferEnd);

			template<typename PayloadSizeType>
			static size_t SkipPacketWithPayload(size_t sizeofPacket, DataVector::iterator curr, DataVector::iterator bufferEnd);

			size_t DispatchProcessPacket(const UInt32 packetID, DataVector::iterator curr, DataVector::iterator bufferEnd);

			size_t LoadAndProcessNextPacket(DataVector::iterator curr, DataVector::iterator bufferEnd);

			static ProcessPacketFunc GetProcessPacketFunction(UInt32 packetID, UInt32 version);

		private:
			DataVector              m_buffer;

			bool                    m_hasReadConnectionHeader;
			bool                    m_hasReadPacketDescriptorHeader;
			bool                    m_hasReadPacketDescriptors;

			UInt32                  m_numPacketTypes;
			PacketDescriptorPacket *m_packetDescriptors;
			ProcessPacketFunc      *m_packetProcessors;

			UInt32                  m_streamThreadID;
			size_t                  m_streamBytesRemaining;
	};

} // namespace Capture
} // namespace OVR

#endif
