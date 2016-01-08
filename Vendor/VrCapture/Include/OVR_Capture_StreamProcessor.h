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

#include <stdio.h>
#include <string.h>
#include <vector>

#include <OVR_Capture_LegacyPackets.h>

namespace OVR
{
namespace Capture
{
	class StreamProcessor
	{
		private:
			typedef std::vector<char> DataVector;

		public:
			StreamProcessor()
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

			virtual ~StreamProcessor(void)
			{
				Close();
			}

			bool ProcessData(const void *buffer, size_t bufferSize)
			{
				// First, append the incoming data to our unprocessed data buffer...
				m_buffer.insert(m_buffer.end(), (const char*)buffer, ((const char*)buffer)+bufferSize);

				const DataVector::iterator begin = m_buffer.begin();
				const DataVector::iterator end   = m_buffer.end();
				DataVector::iterator       curr  = begin;

				// 1) read ConnectionHeaderPacket
				if(!m_hasReadConnectionHeader && std::distance(curr, end) > sizeof(ConnectionHeaderPacket))
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
				if(!m_hasReadPacketDescriptorHeader && std::distance(curr, end) > sizeof(PacketDescriptorHeaderPacket))
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
					const DataVector::iterator streamEnd = curr + min((size_t)(end - curr), m_streamBytesRemaining);

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

			virtual void Close(void) 
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

		private:
			virtual void onStreamError(const char *msg) {}

			// Implement these functions in the child class depending on what data you are trying to extract from the data stream...
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
			virtual void onVarRange(Capture::UInt32 labelID, float value, float valMin, float valMax) {}
			virtual void onVarSet(Capture::UInt32 labelID, float value) {}

		private:
			typedef size_t (*ProcessPacketFunc)(StreamProcessor &p, DataVector::iterator curr, DataVector::iterator end);

		private:
			template <typename T> static T min(T a, T b) 
			{
				return !(b<a)?a:b;     
			}

			static double timestampToSeconds(UInt64 nanoseconds)
			{
				return ((double)nanoseconds) * (1.0/1000000000.0);
			}

			static bool IsAligned(const void *ptr, const size_t alignment)
			{
				return (((size_t)ptr) & (alignment-1)) == 0 ? true : false;
			}

		private:
			void ProcessPacket(const Capture::ThreadNamePacket &packet, const void *payloadData, size_t payloadSize)
			{
				onThreadName(m_streamThreadID, (const char*)payloadData, payloadSize);
			}

			void ProcessPacket(const Capture::LabelPacket &packet, const void *payloadData, size_t payloadSize)
			{
				onLabel(packet.labelID, (const char*)payloadData, payloadSize);
			}

			void ProcessPacket(const Capture::VSyncPacket &packet)
			{
				onVSync(timestampToSeconds(packet.timestamp));
			}

			void ProcessPacket(const Capture::FrameIndexPacket &packet)
			{
				onFrameIndex(m_streamThreadID, packet.frameIndex, timestampToSeconds(packet.timestamp));
			}

			void ProcessPacket(const Capture::CPUZoneEnterPacket &packet)
			{
				onCPUZoneEnter(m_streamThreadID, packet.labelID, timestampToSeconds(packet.timestamp));
			}

			void ProcessPacket(const Capture::CPUZoneLeavePacket &packet)
			{
				onCPUZoneLeave(m_streamThreadID, timestampToSeconds(packet.timestamp));
			}

			void ProcessPacket(const Capture::GPUZoneEnterPacket &packet)
			{
				onGPUZoneEnter(m_streamThreadID, packet.labelID, timestampToSeconds(packet.timestamp));
			}

			void ProcessPacket(const Capture::GPUZoneLeavePacket &packet)
			{
				onGPUZoneLeave(m_streamThreadID, timestampToSeconds(packet.timestamp));
			}

			void ProcessPacket(const Capture::GPUClockSyncPacket &packet)
			{
				const double timestampCPU = timestampToSeconds(packet.timestampCPU);
				const double timestampGPU = timestampToSeconds(packet.timestampGPU);
				const double offset       = timestampCPU - timestampGPU;
				onGPUClockSync(m_streamThreadID, offset);
			}

			void ProcessPacket(const Capture::SensorRangePacket &packet)
			{
				onSensorRange(packet.labelID, (SensorInterpolator)packet.interpolator, (SensorUnits)packet.units, packet.minValue, packet.maxValue);
			}

			void ProcessPacket(const Capture::SensorPacket &packet)
			{
				onSensor(packet.labelID, timestampToSeconds(packet.timestamp), packet.value);
			}	

			void ProcessPacket(const Capture::FrameBufferPacket &packet, const void *payloadData, size_t payloadSize)
			{
				onFrameBuffer(timestampToSeconds(packet.timestamp), (FrameBufferFormat)packet.format, packet.width, packet.height, payloadData, payloadSize);
			}

			void ProcessPacket(const Capture::LogPacket &packet, const void *payloadData, size_t payloadSize)
			{
				onLog(m_streamThreadID, (LogPriority)packet.priority, timestampToSeconds(packet.timestamp), (const char*)payloadData, payloadSize);
			}

			void ProcessPacket(const Capture::VarRangePacket &packet)
			{
				onVarRange(packet.labelID, packet.value, packet.valMin, packet.valMax);
			}	

			void ProcessPacket(const Capture::VarSetPacket &packet)
			{
				onVarSet(packet.labelID, packet.value);
			}	

		private:
			template<typename PacketType>
			static size_t ProcessPacket(StreamProcessor &p, DataVector::iterator curr, DataVector::iterator bufferEnd)
			{
				OVR_CAPTURE_STATIC_ASSERT(PacketType::s_hasPayload == false);

				PacketType packet = {0};

				// Check to see if we have enough room for the packet...
				if(sizeof(packet) > std::distance(curr, bufferEnd))
					return 0;

				// Load the packet...
				memcpy(&packet, &*curr, sizeof(packet));

				// Process...
				p.ProcessPacket(packet);

				// Return the total size of the packet, including the header...
				return sizeof(Capture::PacketHeader) + sizeof(packet);
			}

			template<typename PacketType>
			static size_t ProcessPacketWithPayload(StreamProcessor &p, DataVector::iterator curr, DataVector::iterator bufferEnd)
			{
				OVR_CAPTURE_STATIC_ASSERT(PacketType::s_hasPayload == true);

				PacketType                           packet      = {0};
				typename PacketType::PayloadSizeType payloadSize = 0;

				// Check to see if we have enough room for the packet...
				if(sizeof(packet) > std::distance(curr, bufferEnd))
					return 0;

				// Load the packet...
				memcpy(&packet, &*curr, sizeof(packet));
				curr += sizeof(packet);

				// Check to see if we have enough room for the payload header...
				if(sizeof(payloadSize) > std::distance(curr, bufferEnd))
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

				// Process...
				p.ProcessPacket(packet, payloadAligned, (size_t)payloadSize);

				if(payloadAligned && payloadAligned != payloadUnaligned)
				{
					free(payloadAligned);
				}

				// Return the total size of the packet, including the header...
				return sizeof(Capture::PacketHeader) + sizeof(packet) + sizeof(payloadSize) + payloadSize;
			}

			static size_t SkipPacket(size_t sizeofPacket, DataVector::iterator curr, DataVector::iterator bufferEnd)
			{
				// Check to see if we have enough room for the packet...
				if(((DataVector::difference_type)sizeofPacket) > std::distance(curr, bufferEnd))
					return 0;

				// Return the total size of the packet, including the header...
				return sizeof(Capture::PacketHeader) + sizeofPacket;
			}

			template<typename PayloadSizeType>
			static size_t SkipPacketWithPayload(size_t sizeofPacket, DataVector::iterator curr, DataVector::iterator bufferEnd)
			{
				PayloadSizeType payloadSize = 0;

				// Check to see if we have enough room for the packet...
				if(((DataVector::difference_type)sizeofPacket) > std::distance(curr, bufferEnd))
					return 0;

				// Skip past the packet to the payload header...
				curr += sizeofPacket;

				// Check to see if we have enough room for the payload header...
				if(sizeof(payloadSize) > std::distance(curr, bufferEnd))
					return 0;

				// Load the payload header...
				memcpy(&payloadSize, &*curr, sizeof(payloadSize));
				curr += sizeof(payloadSize);

				// Check to see if we have enough room for the payload...
				if(((DataVector::difference_type)payloadSize) > std::distance(curr, bufferEnd))
					return 0;

				// Return the total size of the packet, including the header...
				return sizeof(Capture::PacketHeader) + sizeofPacket + sizeof(payloadSize) + payloadSize;
			}


			size_t DispatchProcessPacket(const UInt32 packetID, DataVector::iterator curr, DataVector::iterator bufferEnd) 
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

			size_t LoadAndProcessNextPacket(DataVector::iterator curr, DataVector::iterator bufferEnd)
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

		private:
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

			static ProcessPacketFunc GetProcessPacketFunction(UInt32 packetID, UInt32 version)
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
					PacketFuncDesc::Create<VarRangePacket>(),
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
