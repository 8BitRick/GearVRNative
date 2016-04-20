/************************************************************************************

Filename    :   OVR_Capture_StreamProcessor.cpp
Content     :   capture stream parser/dispatcher.  Copied
				mostly from OVR_Monitor_StreamProcessor
Created     :   June, 2015
Notes       : 
Author      :   James Dolan, Amanda M. Watson

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include <OVR_Capture_StreamProcessor.h>

#include <algorithm>

namespace OVR
{
namespace Capture
{

	static double TimestampToSeconds(UInt64 nanoseconds)
	{
		return ((double)nanoseconds) * (1.0/1000000000.0);
	}

	static bool IsAligned(const void *ptr, const size_t alignment)
	{
		return (((size_t)ptr) & (alignment-1)) == 0 ? true : false;
	}


	StreamProcessor::StreamProcessor(void)
	{
		m_hasReadConnectionHeader       = false;
		m_hasReadPacketDescriptorHeader = false;
		m_hasReadPacketDescriptors      = false;
		m_numPacketTypes                = 0;
		m_packetDescriptors             = NULL;
		m_packetProcessors              = NULL;
		m_streamThreadID                = 0;
		m_streamBytesRemaining          = 0;
	}

	StreamProcessor::~StreamProcessor(void)
	{
		Close();
	}

	bool StreamProcessor::ProcessData(const void *buffer, size_t bufferSize)
	{
		// First, append the incoming data to our unprocessed data buffer...
		m_buffer.insert(m_buffer.end(), (const UInt8*)buffer, ((const UInt8*)buffer)+bufferSize);

		const DataVector::iterator begin = m_buffer.begin();
		const DataVector::iterator end   = m_buffer.end();
		DataVector::iterator       curr  = begin;

		// 1) read ConnectionHeaderPacket
		if(!m_hasReadConnectionHeader && std::distance(curr, end) > (DataVector::difference_type)sizeof(ConnectionHeaderPacket))
		{
			ConnectionHeaderPacket connectionHeader;
			memcpy(&connectionHeader, &*curr, sizeof(connectionHeader));
			curr += sizeof(connectionHeader);
			m_hasReadConnectionHeader = true;

			if(connectionHeader.size != sizeof(connectionHeader))
			{
				onStreamError("Connection header size mismatch!");
				return false;
			}

			if(connectionHeader.version != ConnectionHeaderPacket::s_version)
			{
				onStreamError("Connection header version mismatch!");
				return false;
			}

			if(connectionHeader.flags == 0)
			{
				onStreamError("No capture features enabled!");
				return false;
			}
		}

		// Have not successfully read the connection header but no error, so return until we have more data
		if(!m_hasReadConnectionHeader)
			return true;

		// 2) read PacketDescriptorHeaderPacket
		if(!m_hasReadPacketDescriptorHeader && std::distance(curr, end) > (DataVector::difference_type)sizeof(PacketDescriptorHeaderPacket))
		{
			PacketDescriptorHeaderPacket packetDescHeader;
			memcpy(&packetDescHeader, &*curr, sizeof(packetDescHeader));
			curr += sizeof(packetDescHeader);
			m_hasReadPacketDescriptorHeader = true;
			m_numPacketTypes = packetDescHeader.numPacketTypes;

			if(m_numPacketTypes == 0)
			{
				onStreamError("No packet types received!");
				return false;
			}

			if(m_numPacketTypes > 1024)
			{
				onStreamError("Too many packet types received!");
				return false;
			}
		}

		// Have not successfully read the packet descriptor header but no error, so return until we have more data
		if(!m_hasReadPacketDescriptorHeader)
			return true;

		// 3) read array of PacketDescriptorPacket
		if(!m_hasReadPacketDescriptors && std::distance(curr, end) > (DataVector::difference_type)(sizeof(PacketDescriptorPacket)*m_numPacketTypes))
		{
			m_packetDescriptors = new PacketDescriptorPacket[m_numPacketTypes];
			m_packetProcessors  = new ProcessPacketFunc[m_numPacketTypes];
			for(UInt32 i=0; i<m_numPacketTypes; i++)
			{
				memcpy(&m_packetDescriptors[i], &*curr, sizeof(PacketDescriptorPacket));
				curr += sizeof(PacketDescriptorPacket);
				m_packetProcessors[i] = GetProcessPacketFunction(m_packetDescriptors[i].packetID, m_packetDescriptors[i].version);
			}
			m_hasReadPacketDescriptors = true;
		}

		// Have not successfully read the packet descriptors but no error, so return until we have more data
		if(!m_hasReadPacketDescriptors)
			return true;

		// 4) read streams...
		while(curr < end)
		{
			// Compute the end of the readable stream...
			// We take the minimum end point between end of buffer actually read in and stream size...
			// because we don't want to overrun the stream or the buffer that is actually read in...
			const DataVector::iterator streamEnd = curr + std::min((size_t)(end - curr), m_streamBytesRemaining);

			// If we are currently in a stream... try parsing packets out of it...
			while(curr < streamEnd)
			{
				const size_t s = LoadAndProcessNextPacket(curr, streamEnd);
				if(!s)
					break;
				OVR_CAPTURE_ASSERT(curr+s <= streamEnd);
				curr                   += s;
				m_streamBytesRemaining -= s;
			}

			// If we are at the end of the stream... that means the next data available will be a stream header...
			// so load that if we can. And once loaded stay in the loop and try parsing the following packets...
			if(curr == streamEnd && sizeof(StreamHeaderPacket) <= (size_t)(end - curr))
			{
				StreamHeaderPacket streamHeader = {0};
				memcpy(&streamHeader, &*curr, sizeof(streamHeader));
				m_streamThreadID       = streamHeader.threadID;
				m_streamBytesRemaining = streamHeader.streamSize;
				curr += sizeof(streamHeader);
			}
			else
			{
				// If we ran out of parsable data we need to exit and allow the CaptureThread to load more data...
				break;
			}
		}

		// Finally, remove processed data...
		// Note: we so cannot do this if we decide the DataVector should
		// shrink on popAll
		if(curr > begin)
		{
			m_buffer.erase(m_buffer.begin(), curr);
		}

		return true;
	}

	void StreamProcessor::Close(void) 
	{
		m_buffer.clear();
		if(m_packetDescriptors)
		{
			delete [] m_packetDescriptors;
			m_packetDescriptors = NULL;
		}
		if(m_packetProcessors)
		{
			delete [] m_packetProcessors;
			m_packetProcessors = NULL;
		}
		m_hasReadConnectionHeader       = false;
		m_hasReadPacketDescriptorHeader = false;
		m_hasReadPacketDescriptors      = false;
		m_numPacketTypes                = 0;
		m_streamThreadID                = 0;
		m_streamBytesRemaining          = 0;
	}

	template<>
	void StreamProcessor::DispatchPacket<ThreadNamePacket>(const ThreadNamePacket &packet, const void *payloadData, size_t payloadSize)
	{
		onThreadName(m_streamThreadID, (const char*)payloadData, payloadSize);
	}

	template<>
	void StreamProcessor::DispatchPacket<LabelPacket>(const LabelPacket &packet, const void *payloadData, size_t payloadSize)
	{
		onLabel(packet.labelID, (const char*)payloadData, payloadSize);
	}

	template<>
	void StreamProcessor::DispatchPacket<VSyncPacket>(const VSyncPacket &packet)
	{
		onVSync(TimestampToSeconds(packet.timestamp));
	}

	template<>
	void StreamProcessor::DispatchPacket<FrameIndexPacket>(const FrameIndexPacket &packet)
	{
		onFrameIndex(m_streamThreadID, packet.frameIndex, TimestampToSeconds(packet.timestamp));
	}

	template<>
	void StreamProcessor::DispatchPacket<CPUZoneEnterPacket>(const CPUZoneEnterPacket &packet)
	{
		onCPUZoneEnter(m_streamThreadID, packet.labelID, TimestampToSeconds(packet.timestamp));
	}

	template<>
	void StreamProcessor::DispatchPacket<CPUZoneLeavePacket>(const CPUZoneLeavePacket &packet)
	{
		onCPUZoneLeave(m_streamThreadID, TimestampToSeconds(packet.timestamp));
	}

	template<>
	void StreamProcessor::DispatchPacket<GPUZoneEnterPacket>(const GPUZoneEnterPacket &packet)
	{
		onGPUZoneEnter(m_streamThreadID, packet.labelID, TimestampToSeconds(packet.timestamp));
	}

	template<>
	void StreamProcessor::DispatchPacket<GPUZoneLeavePacket>(const GPUZoneLeavePacket &packet)
	{
		onGPUZoneLeave(m_streamThreadID, TimestampToSeconds(packet.timestamp));
	}

	template<>
	void StreamProcessor::DispatchPacket<GPUClockSyncPacket>(const GPUClockSyncPacket &packet)
	{
		const double timestampCPU = TimestampToSeconds(packet.timestampCPU);
		const double timestampGPU = TimestampToSeconds(packet.timestampGPU);
		const double offset       = timestampCPU - timestampGPU;
		onGPUClockSync(m_streamThreadID, offset);
	}

	template<>
	void StreamProcessor::DispatchPacket<SensorRangePacket>(const SensorRangePacket &packet)
	{
		onSensorRange(packet.labelID, (SensorInterpolator)packet.interpolator, (SensorUnits)packet.units, packet.minValue, packet.maxValue);
	}

	template<>
	void StreamProcessor::DispatchPacket<SensorPacket>(const SensorPacket &packet)
	{
		onSensor(packet.labelID, TimestampToSeconds(packet.timestamp), packet.value);
	}

	template<>
	void StreamProcessor::DispatchPacket<FrameBufferPacket>(const FrameBufferPacket &packet, const void *payloadData, size_t payloadSize)
	{
		onFrameBuffer(TimestampToSeconds(packet.timestamp), (FrameBufferFormat)packet.format, packet.width, packet.height, payloadData, payloadSize);
	}

	template<>
	void StreamProcessor::DispatchPacket<LogPacket>(const LogPacket &packet, const void *payloadData, size_t payloadSize)
	{
		onLog(m_streamThreadID, (LogPriority)packet.priority, TimestampToSeconds(packet.timestamp), (const char*)payloadData, payloadSize);
	}

	template<>
	void StreamProcessor::DispatchPacket<FloatParamRangePacket>(const FloatParamRangePacket &packet)
	{
		onParamRange(packet.labelID, packet.value, packet.valMin, packet.valMax);
	}

	template<>
	void StreamProcessor::DispatchPacket<FloatParamPacket>(const FloatParamPacket &packet)
	{
		onParamValue(packet.labelID, packet.value);
	}

	template<>
	void StreamProcessor::DispatchPacket<IntParamRangePacket>(const IntParamRangePacket &packet)
	{
		onParamRange(packet.labelID, packet.value, packet.valMin, packet.valMax);
	}

	template<>
	void StreamProcessor::DispatchPacket<IntParamPacket>(const IntParamPacket &packet)
	{
		onParamValue(packet.labelID, packet.value);
	}

	template<>
	void StreamProcessor::DispatchPacket<BoolParamPacket>(const BoolParamPacket &packet)
	{
		onParamValue(packet.labelID, packet.value ? true : false);
	}

	template<typename PacketType>
	size_t StreamProcessor::ProcessPacket(StreamProcessor &p, DataVector::iterator curr, DataVector::iterator bufferEnd)
	{
		OVR_CAPTURE_STATIC_ASSERT(PacketType::s_hasPayload == false);

		PacketType packet = {0};

		// Check to see if we have enough room for the packet...
		if(sizeof(packet) > (size_t)std::distance(curr, bufferEnd))
			return 0;

		// Load the packet...
		memcpy(&packet, &*curr, sizeof(packet));

		// Dispatch to callback...
		p.DispatchPacket(packet);

		// Return the total size of the packet, including the header...
		return sizeof(PacketHeader) + sizeof(packet);
	}

	template<typename PacketType>
	size_t StreamProcessor::ProcessPacketWithPayload(StreamProcessor &p, DataVector::iterator curr, DataVector::iterator bufferEnd)
	{
		OVR_CAPTURE_STATIC_ASSERT(PacketType::s_hasPayload == true);

		PacketType                           packet      = {0};
		typename PacketType::PayloadSizeType payloadSize = 0;

		// Check to see if we have enough room for the packet...
		if(sizeof(packet) > (size_t)std::distance(curr, bufferEnd))
			return 0;

		// Load the packet...
		memcpy(&packet, &*curr, sizeof(packet));
		curr += sizeof(packet);

		// Check to see if we have enough room for the payload header...
		if(sizeof(payloadSize) > (size_t)std::distance(curr, bufferEnd))
			return 0;

		// Load the payload header...
		memcpy(&payloadSize, &*curr, sizeof(payloadSize));
		curr += sizeof(payloadSize);

		// Check to see if we have enough room for the payload...
		if(((DataVector::difference_type)payloadSize) > std::distance(curr, bufferEnd))
			return 0;

		void *payloadUnaligned = &*curr;
		void *payloadAligned   = NULL;
		if(payloadSize > 0)
		{
			if(IsAligned(payloadUnaligned, PacketType::s_payloadAlignment))
			{
				// avoid re-allocation+copy unless necessary...
				payloadAligned = payloadUnaligned;
			}
			else
			{
				// TODO: reusable scratch memory blob...
				payloadAligned = malloc(payloadSize);
				memcpy(payloadAligned, payloadUnaligned, payloadSize);
			}
		}

		// Dispatch to callback...
		p.DispatchPacket(packet, payloadAligned, (size_t)payloadSize);

		if(payloadAligned && payloadAligned != payloadUnaligned)
		{
			free(payloadAligned);
		}

		// Return the total size of the packet, including the header...
		return sizeof(PacketHeader) + sizeof(packet) + sizeof(payloadSize) + payloadSize;
	}

	size_t StreamProcessor::SkipPacket(size_t sizeofPacket, DataVector::iterator curr, DataVector::iterator bufferEnd)
	{
		// Check to see if we have enough room for the packet...
		if(((DataVector::difference_type)sizeofPacket) > std::distance(curr, bufferEnd))
			return 0;

		// Return the total size of the packet, including the header...
		return sizeof(PacketHeader) + sizeofPacket;
	}

	template<typename PayloadSizeType>
	size_t StreamProcessor::SkipPacketWithPayload(size_t sizeofPacket, DataVector::iterator curr, DataVector::iterator bufferEnd)
	{
		PayloadSizeType payloadSize = 0;

		// Check to see if we have enough room for the packet...
		if(((DataVector::difference_type)sizeofPacket) > std::distance(curr, bufferEnd))
			return 0;

		// Skip past the packet to the payload header...
		curr += sizeofPacket;

		// Check to see if we have enough room for the payload header...
		if(sizeof(payloadSize) > (size_t)std::distance(curr, bufferEnd))
			return 0;

		// Load the payload header...
		memcpy(&payloadSize, &*curr, sizeof(payloadSize));
		curr += sizeof(payloadSize);

		// Check to see if we have enough room for the payload...
		if(((DataVector::difference_type)payloadSize) > std::distance(curr, bufferEnd))
			return 0;

		// Return the total size of the packet, including the header...
		return sizeof(PacketHeader) + sizeofPacket + sizeof(payloadSize) + payloadSize;
	}


	size_t StreamProcessor::DispatchProcessPacket(const UInt32 packetID, DataVector::iterator curr, DataVector::iterator bufferEnd) 
	{
		for(UInt32 i=0; i<m_numPacketTypes; i++)
		{
			const PacketDescriptorPacket &desc = m_packetDescriptors[i];
			if(desc.packetID == packetID)
			{
				const ProcessPacketFunc processFunc = m_packetProcessors[i];
				if(processFunc)
				{
					return processFunc(*this, curr, bufferEnd);
				}
				else
				{
					// No process function available for this packet type! Attempt to skip it!
					switch(desc.sizeofPayloadSizeType)
					{
						case 0: return SkipPacket(                    (size_t)desc.sizeofPacket, curr, bufferEnd );
						case 1: return SkipPacketWithPayload<UInt8>(  (size_t)desc.sizeofPacket, curr, bufferEnd );
						case 2: return SkipPacketWithPayload<UInt16>( (size_t)desc.sizeofPacket, curr, bufferEnd );
						case 4: return SkipPacketWithPayload<UInt32>( (size_t)desc.sizeofPacket, curr, bufferEnd );
					}
				}
			}
		}

		// We should never reach here...
		// typically means unknown packet type, so any further reading from the socket will yield undefined results...
		OVR_CAPTURE_ASSERT(0);
		return 0;
	}

	size_t StreamProcessor::LoadAndProcessNextPacket(DataVector::iterator curr, DataVector::iterator bufferEnd)
	{
		PacketHeader packetHeader;

		// Check to see if we have enough room for the header...
		if(curr + sizeof(packetHeader) > bufferEnd)
			return 0;

		// Peek at the header...
		memcpy(&packetHeader, &*curr, sizeof(packetHeader));
		curr += sizeof(packetHeader);
		return DispatchProcessPacket(packetHeader.packetID, curr, bufferEnd);
	}

	StreamProcessor::ProcessPacketFunc StreamProcessor::GetProcessPacketFunction(UInt32 packetID, UInt32 version)
	{
		static const PacketFuncDesc descs[] =
		{
			PacketFuncDesc::Create<ThreadNamePacket>(),
			PacketFuncDesc::Create<LabelPacket>(),
			PacketFuncDesc::Create<FrameIndexPacket>(),
			PacketFuncDesc::Create<VSyncPacket>(),
			PacketFuncDesc::Create<CPUZoneEnterPacket>(),
			PacketFuncDesc::Create<CPUZoneLeavePacket>(),
			PacketFuncDesc::Create<GPUZoneEnterPacket>(),
			PacketFuncDesc::Create<GPUZoneLeavePacket>(),
			PacketFuncDesc::Create<GPUClockSyncPacket>(),
			PacketFuncDesc::Create<SensorRangePacket>(),
			PacketFuncDesc::Create<SensorPacket>(),
			PacketFuncDesc::Create<FrameBufferPacket>(),
			PacketFuncDesc::Create<LogPacket>(),
			PacketFuncDesc::Create<FloatParamRangePacket>(),
			PacketFuncDesc::Create<FloatParamPacket>(),
			PacketFuncDesc::Create<IntParamRangePacket>(),
			PacketFuncDesc::Create<IntParamPacket>(),
			PacketFuncDesc::Create<BoolParamPacket>(),
		};
		static const UInt32 numDescs = sizeof(descs) / sizeof(descs[0]);

		for(UInt32 i=0; i<numDescs; i++)
		{
			const PacketFuncDesc &d = descs[i];
			if(d.packetID == packetID && d.version == version)
			{
				return d.func;
			}
		}
		return NULL;
	}

} // namespace Capture
} // namespace OVR
