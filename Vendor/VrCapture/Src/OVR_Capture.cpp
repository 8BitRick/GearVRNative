/************************************************************************************

PublicHeader:   OVR_Capture.h
Filename    :   OVR_Capture.cpp
Content     :   Oculus performance capture library
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include <OVR_Capture.h>
#include "OVR_Capture_Local.h"
#include "OVR_Capture_Variable.h"
#include "OVR_Capture_Socket.h"
#include "OVR_Capture_AsyncStream.h"
#include "OVR_Capture_StandardSensors.h"
#include "OVR_Capture_Packets.h"
#include "OVR_Capture_FileIO.h"

#if defined(OVR_CAPTURE_DARWIN)
	#include <mach/mach_time.h>
	#include <pthread.h>
#elif defined(OVR_CAPTURE_POSIX)
	#include <pthread.h>
	#include <malloc.h>
#endif

#if defined(OVR_CAPTURE_POSIX)
	#include <pthread.h>
#endif

#if defined(OVR_CAPTURE_HAS_MACH_ABSOLUTE_TIME)
	#include <mach/mach_time.h>
#endif

#if defined(OVR_CAPTURE_USE_CLOCK_GETTIME)
	#include <time.h>
#endif

#if defined(OVR_CAPTURE_HAS_GETTIMEOFDAY)
	#include <sys/time.h>
#endif

#if defined(OVR_CAPTURE_HAS_ZLIB)
	#include <zlib.h>
#endif

#include <memory.h>
#include <new>

#include <stdio.h>

namespace OVR
{
namespace Capture
{

	static const UInt16         g_zeroConfigPort  = ZeroConfigPacket::s_broadcastPort;
	static const UInt16         g_socketPortBegin = 3030;
	static const UInt16         g_socketPortEnd   = 3040;

	static Thread              *g_server          = NULL;

	static volatile UInt32      g_initFlags       = 0;
	static volatile UInt32      g_connectionFlags = 0;

	static CriticalSection      g_labelLock;

	// reference count for the number of current functions depending on
	// an active connection. i.e., when we disconnect we wait for this
	// refcount to reach zero before cleaning up any data structures or
	// output streams.
	static volatile int         g_refcount        = 0;

	static OnConnectFunc        g_onConnect       = NULL;
	static OnDisconnectFunc     g_onDisconnect    = NULL;

	static VarStore				g_varStore;

	/***********************************
	 * Helper Functions                 *
	 ***********************************/

	bool TryLockConnection(void)
	{
		// First check to see if we are even connected, if not early out without acquiring a lock...
		if(!IsConnected())
			return false;
		// increment ref count locking out the Server thread from deleting the AsyncStream.
		AtomicAdd(g_refcount, 1);
		// Once locked, check again to see if we are still connected... if not unlock and return false.
		if(!IsConnected())
		{
			AtomicSub(g_refcount, 1);
			return false;
		}
		// success! we must be connected and acquired a lock!
		return true;
	}

	bool TryLockConnection(const CaptureFlag feature)
	{
		// First check to see if we are even connected, if not early out without acquiring a lock...
		if(!CheckConnectionFlag(feature))
			return false;
		// increment ref count locking out the Server thread from deleting the AsyncStream.
		AtomicAdd(g_refcount, 1);
		// Once locked, check again to see if we are still connected... if not unlock and return false.
		if(!CheckConnectionFlag(feature))
		{
			AtomicSub(g_refcount, 1);
			return false;
		}
		// success! we must be connected and acquired a lock!
		return true;
	}

	void UnlockConnection(void)
	{
		// decrement refcount and do sanity check to make sure we don't go negative.
		const int i = AtomicSub(g_refcount, 1);
		OVR_CAPTURE_ASSERT(i >= 1);
		OVR_CAPTURE_UNUSED(i);
	}

	static bool InitInternal(UInt32 flags, OnConnectFunc onConnect, OnDisconnectFunc onDisconnect)
	{
		// If we are already initialized... don't re-initialize.
		if(g_initFlags)
			return false;

		// sanitze input flags;
		flags = flags & All_Flags;

		// If no capture features are enabled... then don't initialize anything!
		if(!flags)
			return false;

		g_initFlags       = flags;
		g_connectionFlags = 0;

		g_onConnect    = onConnect;
		g_onDisconnect = onDisconnect;

		OVR_CAPTURE_ASSERT(!g_server);

		return true;
	}

	static void SendLabelPacket(const Label &label)
	{
		if(TryLockConnection())
		{
			LabelPacket packet;
			packet.labelID = label.GetIdentifier();
			const char *name = label.GetName();
			AsyncStream::Acquire()->WritePacket(packet, name, (UInt32)strlen(name));
			UnlockConnection();
		}
	}

	static UInt32 StringHash32(const char *str)
	{
		// Tested on 235,886 words in '/usr/share/dict/words'
	#if defined(OVR_CAPTURE_HAS_ZLIB)
		// Multiple iterations of zlib crc32, which improves entropy significantly
		// 1 Collisions Total with 3 passes... Chance Of Collisions 0.000424%
		// (8 Collisions with a single pass)
		UInt32 h = 0;
		const uInt len = strlen(str);
		for(UInt32 i=0; i<3; i++)
			h = static_cast<UInt32>(::crc32(h, (const Bytef*)str, len));
		return h
	#else
		// 2 Collisions Total with 3 passes... Chance Of Collisions 0.000848%
		// (9 Collisions  with a single pass)
		// So nearly as good as multi pass zlib crc32...
		// Seeded with a bunch of prime numbers...
		const unsigned int a = 54059;
		const unsigned int b = 76963;
		unsigned int       h = 0;
		for(unsigned int i=0; i<3; i++)
		{
			h = (h>>2) | 31;
			for(const char *c=str; *c; c++)
			{
				h = (h * a) ^ ((*c) * b);
			}
		}
		return h;
	#endif
	}

	template<typename PacketType, bool hasPayload> struct PayloadSizer;
	template<typename PacketType> struct PayloadSizer<PacketType, true>
	{
		static UInt32 GetSizeOfPayloadSizeType(void)
		{
			return sizeof(typename PacketType::PayloadSizeType);
		}
	};
	template<typename PacketType> struct PayloadSizer<PacketType, false>
	{
		static UInt32 GetSizeOfPayloadSizeType(void)
		{
			return 0;
		}
	};

	template<typename PacketType> static PacketDescriptorPacket BuildPacketDescriptorPacket(void)
	{
		PacketDescriptorPacket desc = {0};
		desc.packetID              = PacketType::s_packetID;
		desc.version               = PacketType::s_version;
		desc.sizeofPacket          = sizeof(PacketType);
		desc.sizeofPayloadSizeType = PayloadSizer<PacketType, PacketType::s_hasPayload>::GetSizeOfPayloadSizeType();
		return desc;
	}

	static const PacketDescriptorPacket g_packetDescs[] =
	{
		BuildPacketDescriptorPacket<ThreadNamePacket>(),
		BuildPacketDescriptorPacket<LabelPacket>(),
		BuildPacketDescriptorPacket<FrameIndexPacket>(),
		BuildPacketDescriptorPacket<VSyncPacket>(),
		BuildPacketDescriptorPacket<CPUZoneEnterPacket>(),
		BuildPacketDescriptorPacket<CPUZoneLeavePacket>(),
		BuildPacketDescriptorPacket<GPUZoneEnterPacket>(),
		BuildPacketDescriptorPacket<GPUZoneLeavePacket>(),
		BuildPacketDescriptorPacket<GPUClockSyncPacket>(),
		BuildPacketDescriptorPacket<SensorRangePacket>(),
		BuildPacketDescriptorPacket<SensorPacket>(),
		BuildPacketDescriptorPacket<FrameBufferPacket>(),
		BuildPacketDescriptorPacket<LogPacket>(),
		BuildPacketDescriptorPacket<VarRangePacket>(),
	};
	static const UInt32 g_numPacketDescs = sizeof(g_packetDescs) / sizeof(g_packetDescs[0]);

	/***********************************
	 * Servers                         *
	 ***********************************/

	// Thread/Socket that sits in the background waiting for incoming connections...
	// Not to be confused with the ZeroConfig hosts who advertises our existance, this
	// is the socket that actually accepts the incoming connections and creates the socket.
	class RemoteServer : public Thread
	{
		public:
			static RemoteServer *Create(void)
			{
			#if defined(OVR_CAPTURE_WINDOWS)
				// Make sure winsock is initialized...
				WSADATA wsdata = {0};
				if(WSAStartup(MAKEWORD(2,2), &wsdata) != 0)
					return NULL;
			#endif

				// Find the first open port...
				for(UInt16 port=g_socketPortBegin; port<g_socketPortEnd; port++)
				{
					SocketAddress listenAddr = SocketAddress::Any(port);
					Socket *listenSocket     = Socket::Create(Socket::Type_Stream);
					if(listenSocket && !listenSocket->Bind(listenAddr))
					{
						listenSocket->Release();
						listenSocket = NULL;
					}
					if(listenSocket && !listenSocket->Listen(1))
					{
						listenSocket->Release();
						listenSocket = NULL;
					}
					if(listenSocket)
					{
						return new RemoteServer(listenSocket, port);
					}
				}
				return NULL;
			}

			virtual ~RemoteServer(void)
			{
				// Signal and wait for quit...
				if(m_listenSocket) m_listenSocket->Shutdown();
				QuitAndWait();

				// Cleanup just in case the thread doesn't...
				if(m_streamSocket) m_streamSocket->Release();
				if(m_listenSocket) m_listenSocket->Release();

			#if defined(OVR_CAPTURE_WINDOWS)
				// WinSock reference counts...
				WSACleanup();
			#endif
			}

		private:
			RemoteServer(Socket *listenSocket, UInt16 listenPort)
			{
				m_streamSocket = NULL;
				m_listenSocket = listenSocket;
				m_listenPort   = listenPort;
			}

			virtual void OnThreadExecute(void)
			{
				SetThreadName("CaptureServer");

				// Acquire the process name...
			#if defined(OVR_CAPTURE_WINDOWS)
				char packageName[64] = {0};
				GetModuleFileNameA(NULL, packageName, sizeof(packageName));
				if(!packageName[0])
				{
					StringCopy(packageName, "Unknown", sizeof(packageName));
				}
			#else
				char packageName[64] = {0};
				char cmdlinepath[64] = {0};
				FormatString(cmdlinepath, sizeof(cmdlinepath), "/proc/%u/cmdline", (unsigned)getpid());
				if(ReadFileLine(cmdlinepath, packageName, sizeof(packageName)) <= 0)
				{
					StringCopy(packageName, "Unknown", sizeof(packageName));
				}
			#endif

				while(m_listenSocket && !QuitSignaled())
				{
					// Start auto-discovery thread...
					ZeroConfigHost *zeroconfig = ZeroConfigHost::Create(g_zeroConfigPort, m_listenPort, packageName);
					zeroconfig->Start();

					// try and accept a new socket connection...
					SocketAddress streamAddr;
					m_streamSocket = m_listenSocket->Accept(streamAddr);

					// Once connected, shut the auto-discovery thread down.
					zeroconfig->Release();

					// If no connection was established, something went totally wrong and we should just abort...
					if(!m_streamSocket)
						break;

					// Before we start sending capture data... first must exchange connection headers...
					// First attempt to read in the request header from the Client...
					ConnectionHeaderPacket clientHeader = {0};
					if(!m_streamSocket->Receive(&clientHeader, sizeof(clientHeader)))
					{
						m_streamSocket->Release();
						m_streamSocket = NULL;
						continue;
					}

					// Load our connection flags...
					const UInt32 connectionFlags = clientHeader.flags & g_initFlags;

					// Build and send return header... We *always* send the return header so that if we don't
					// like something (like version number or feature flags), the client has some hint as to
					// what we didn't like.
					ConnectionHeaderPacket serverHeader = {0};
					serverHeader.size    = sizeof(serverHeader);
					serverHeader.version = ConnectionHeaderPacket::s_version;
					serverHeader.flags   = connectionFlags;
					if(!m_streamSocket->Send(&serverHeader, sizeof(serverHeader)))
					{
						m_streamSocket->Release();
						m_streamSocket = NULL;
						continue;
					}

					// Check version number...
					if(clientHeader.version != serverHeader.version)
					{
						m_streamSocket->Release();
						m_streamSocket = NULL;
						continue;
					}

					// Check that we have any capture features even turned on...
					if(!connectionFlags)
					{
						m_streamSocket->Release();
						m_streamSocket = NULL;
						continue;
					}

					// Finally, send our packet descriptors...
					const PacketDescriptorHeaderPacket packetDescHeader = { g_numPacketDescs };
					if(!m_streamSocket->Send(&packetDescHeader, sizeof(packetDescHeader)))
					{
						m_streamSocket->Release();
						m_streamSocket = NULL;
						continue;
					}
					if(!m_streamSocket->Send(&g_packetDescs, sizeof(g_packetDescs)))
					{
						m_streamSocket->Release();
						m_streamSocket = NULL;
						continue;
					}

					// Connection established!

					// Initialize the per-thread stream system before flipping on g_connectionFlags...
					AsyncStream::Init();

					if(g_onConnect)
					{
						// Call back into the app to notify a connection is being established.
						// We intentionally do this before enabling the connection flags.
						g_onConnect(connectionFlags);
					}

					// Signal that we are connected!
					AtomicExchange(g_connectionFlags, connectionFlags);

					// Technically any Labels that get initialized on another thread bettween the barrier and loop
					// will get sent over the network twice, but OVRMonitor will handle that.
					g_labelLock.Lock();
					for(Label *l=Label::GetHead(); l; l=l->GetNext())
					{
						SendLabelPacket(*l);
					}
					g_labelLock.Unlock();

					// Start CPU/GPU/Thermal sensors...
					StandardSensors stdsensors;
					if(CheckConnectionFlag(Enable_CPU_Clocks) || CheckConnectionFlag(Enable_GPU_Clocks) || CheckConnectionFlag(Enable_Thermal_Sensors))
					{
						stdsensors.Start();
					}

					// Spin as long as we are connected flushing data from our data stream...
					while(!QuitSignaled())
					{
						const UInt64 flushBeginTime = GetNanoseconds();
						const UInt32 waitflags = m_streamSocket->WaitFor(Socket::WaitFlag_Read | Socket::WaitFlag_Write | Socket::WaitFlag_Timeout, 2);
						if(waitflags & Socket::WaitFlag_Timeout)
						{
							// Connection likely failed somehow...
							break;
						}
						if(waitflags & Socket::WaitFlag_Read)
						{
							PacketHeader header;
							VarSetPacket packet;
							m_streamSocket->Receive((char*)&header, sizeof(header));
							if (header.packetID == Packet_Var_Set)
							{
								m_streamSocket->Receive((char*)&packet, sizeof(packet));
								g_varStore.Set(packet.labelID, packet.value, true);
							}
							else
							{
								Logf(Log_Warning, "OVR::Capture::RemoteServer; Received Invalid Capture Packet");
							}
						}
						if(waitflags & Socket::WaitFlag_Write)
						{
							// Socket is ready to write data... so now is a good time to flush pending capture data.
							SocketOutStream outStream(*m_streamSocket);
							if(!AsyncStream::FlushAll(outStream))
							{
								// Error occured... shutdown the connection.
								break;
							}
						}
						const UInt64 flushEndTime   = GetNanoseconds();
						const UInt64 flushDeltaTime = flushEndTime - flushBeginTime;
						const UInt64 sleepTime      = 4000000; // 4ms
						if(flushDeltaTime < sleepTime)
						{
							// Sleep just a bit to keep the thread from killing a core and to let a good chunk of data build up
							ThreadSleepNanoseconds((UInt32)(sleepTime - flushDeltaTime));
						}
					}

					// Clear the connection flags...
					AtomicExchange(g_connectionFlags, (UInt32)0);

					// Close down our sensor thread...
					stdsensors.QuitAndWait();

					// Connection was closed at some point, lets clean up our socket...
					m_streamSocket->Shutdown();
					m_streamSocket->Release();
					m_streamSocket = NULL;

					if(g_onDisconnect)
					{
						// After the connection is fully shut down, notify the app.
						g_onDisconnect();
					}

					// Clear the buffers for all AsyncStreams to guarantee that no event is 
					// stalled waiting for room on a buffer. Then we wait until there there
					// are no events still writing out.
					AsyncStream::ClearAll();
					while(AtomicGet(g_refcount) > 0)
					{
						ThreadSleepMilliseconds(1);
					}

					// Finally, release any AsyncStreams that were created during this session
					// now that we can safely assume there are no events actively trying to
					// write out to a stream.
					AsyncStream::Shutdown();

					g_varStore.Clear();
				} // while(m_listenSocket && !QuitSignaled())
			}

		private:
			Socket *m_streamSocket;
			Socket *m_listenSocket;
			UInt16  m_listenPort;
	};

	// Connection "server" that stores capture stream straight to disk
	class LocalServer : public Thread
	{
		public:
			static LocalServer *Create(const char *outPath)
			{
				// Load our connection flags...
				const UInt32 connectionFlags = g_initFlags;

				// Check that we have any capture features even turned on...
				if(!connectionFlags)
				{
					return NULL;
				}

				// Attempt to open our destination file...
				FileHandle file = OpenFile(outPath, true);
				if(file == NullFileHandle)
				{
					return NULL;
				}
				FileOutStream outStream(file);

				// Build and send return header...
				ConnectionHeaderPacket serverHeader = {0};
				serverHeader.size    = sizeof(serverHeader);
				serverHeader.version = ConnectionHeaderPacket::s_version;
				serverHeader.flags   = connectionFlags;
				if(!outStream.Send(&serverHeader, sizeof(serverHeader)))
				{
					CloseFile(file);
					return NULL;
				}

				// Finally, send our packet descriptors...
				const PacketDescriptorHeaderPacket packetDescHeader = { g_numPacketDescs };
				if(!outStream.Send(&packetDescHeader, sizeof(packetDescHeader)))
				{
					CloseFile(file);
					return NULL;
				}
				if(!outStream.Send(&g_packetDescs, sizeof(g_packetDescs)))
				{
					CloseFile(file);
					return NULL;
				}

				// "Connection" established!

				return new LocalServer(file, connectionFlags);
			}

			virtual ~LocalServer(void)
			{
				QuitAndWait();

				// Clear the connection flags...
				AtomicExchange(g_connectionFlags, (UInt32)0);

				if(g_onDisconnect)
				{
					// After the connection is fully shut down, notify the app.
					g_onDisconnect();
				}

				// Clear the buffers for all AsyncStreams to guarantee that no event is 
				// stalled waiting for room on a buffer. Then we wait until there there
				// are no events still writing out.
				AsyncStream::ClearAll();
				while(AtomicGet(g_refcount) > 0)
				{
					ThreadSleepMilliseconds(1);
				}

				// Finally, release any AsyncStreams that were created during this session
				// now that we can safely assume there are no events actively trying to
				// write out to a stream.
				AsyncStream::Shutdown();

				g_varStore.Clear();

				if(m_file != NullFileHandle)
				{
					CloseFile(m_file);
					m_file = NullFileHandle;
				}
			}

		private:
			LocalServer(FileHandle file, UInt32 connectionFlags) :
				m_file(file)
			{
				// Initialize the per-thread stream system before flipping on g_connectionFlags...
				AsyncStream::Init();

				if(g_onConnect)
				{
					// Call back into the app to notify a connection is being established.
					// We intentionally do this before enabling the connection flags.
					g_onConnect(connectionFlags);
				}

				// Signal that we are connected!
				AtomicExchange(g_connectionFlags, connectionFlags);
			}

			virtual void OnThreadExecute(void)
			{
				SetThreadName("CaptureServer");

				FileOutStream outStream(m_file);

				// Technically any Labels that get initialized on another thread bettween the barrier and loop
				// will get recorded twice, but OVRMonitor will handle that scenario gracefully.
				g_labelLock.Lock();
				for(Label *l=Label::GetHead(); l; l=l->GetNext())
				{
					SendLabelPacket(*l);
				}
				g_labelLock.Unlock();

				// Start CPU/GPU/Thermal sensors...
				StandardSensors stdsensors;
				if(CheckConnectionFlag(Enable_CPU_Clocks) || CheckConnectionFlag(Enable_GPU_Clocks) || CheckConnectionFlag(Enable_Thermal_Sensors))
				{
					stdsensors.Start();
				}

				// as long as we are running, continuously flush the latest stream data to disk...
				while(!QuitSignaled())
				{
					const UInt64 flushBeginTime = GetNanoseconds();
					if(!AsyncStream::FlushAll(outStream))
					{
						break;
					}
					const UInt64 flushEndTime   = GetNanoseconds();
					const UInt64 flushDeltaTime = flushEndTime - flushBeginTime;
					const UInt64 sleepTime      = 4000000; // 4ms
					if(flushDeltaTime < sleepTime)
					{
						// Sleep just a bit to keep the thread from killing a core and to let a good chunk of data build up
						ThreadSleepNanoseconds((UInt32)(sleepTime - flushDeltaTime));
					}
				}

				// Clear the connection flags...
				AtomicExchange(g_connectionFlags, (UInt32)0);

				// Close down our sensor thread...
				stdsensors.QuitAndWait();
			}

		private:
			FileHandle m_file;
	};

	/***********************************
	 * Public API                       *
	 ***********************************/


	// Get current time in microseconds...
	UInt64 GetNanoseconds(void)
	{
	#if defined(OVR_CAPTURE_HAS_MACH_ABSOLUTE_TIME)
		// OSX/iOS doesn't have clock_gettime()... but it does have gettimeofday(), or even better mach_absolute_time()
		// which is about 50% faster than gettimeofday() and higher precision!
		// Only 24.5ns per GetNanoseconds() call! But we can do better...
		// It seems that modern Darwin already returns nanoseconds, so numer==denom
		// when we test that assumption it brings us down to 16ns per GetNanoseconds() call!!!
		// Timed on MacBookPro running OSX.
		static mach_timebase_info_data_t info = {0};
		if(!info.denom)
			mach_timebase_info(&info);
		const UInt64 t = mach_absolute_time();
		if(info.numer==info.denom)
			return t;
		return (t * info.numer) / info.denom;

	#elif defined(OVR_CAPTURE_HAS_CLOCK_GETTIME)
		// 23ns per call on i7 Desktop running Ubuntu 64
		// >800ns per call on Galaxy Note 4 running Android 4.3!!!
		struct timespec tp;
		clock_gettime(CLOCK_MONOTONIC, &tp);
		return ((UInt64)tp.tv_sec)*1000000000 + (UInt64)tp.tv_nsec;

	#elif defined(OVR_CAPTURE_HAS_GETTIMEOFDAY)
		// Just here for reference... this timer is only microsecond level of precision, and >2x slower than the mach timer...
		// And on non-mach platforms clock_gettime() is the preferred method...
		// 34ns per call on MacBookPro running OSX...
		// 23ns per call on i7 Desktop running Ubuntu 64
		// >800ns per call on Galaxy Note 4 running Android 4.3!!!
		struct timeval tv;
		gettimeofday(&tv, 0);
		const UInt64 us = ((UInt64)tv.tv_sec)*1000000 + (UInt64)tv.tv_usec;
		return us*1000;

	#elif defined(OVR_CAPTURE_WINDOWS)
		static double tonano = 0.0;
		if(!tonano)
		{
			LARGE_INTEGER f;
			QueryPerformanceFrequency(&f);
			tonano = 1000000000.0 / f.QuadPart;
		}
		LARGE_INTEGER c;
		QueryPerformanceCounter(&c);
		return (UInt64)(c.QuadPart * tonano);

	#else
		#error Unknown Platform!

	#endif
	}

	// Initializes the Capture system remote server.
	// should be called before any other Capture call.
	bool InitForRemoteCapture(UInt32 flags, OnConnectFunc onConnect, OnDisconnectFunc onDisconnect)
	{
		if(!InitInternal(flags, onConnect, onDisconnect))
			return false;

		g_server = RemoteServer::Create();
		if(g_server)
			g_server->Start();

		return true;
	}

	// Initializes the Capture system to store capture stream to disk, starting immediately.
	// should be called before any other Capture call.
	bool InitForLocalCapture(const char *outPath, UInt32 flags, OnConnectFunc onConnect, OnDisconnectFunc onDisconnect)
	{
		if(!InitInternal(flags, onConnect, onDisconnect))
			return false;

		g_server = LocalServer::Create(outPath);
		if(g_server)
			g_server->Start();

		return true;
	}

	// Closes the capture system... no other Capture calls on *any* thead should be called after this.
	void Shutdown(void)
	{
		if(g_server)
		{
			delete g_server;
			g_server = NULL;
		}
		OVR_CAPTURE_ASSERT(!g_connectionFlags);
		g_initFlags       = 0;
		g_connectionFlags = 0;
		g_onConnect       = NULL;
		g_onDisconnect    = NULL;
	}

	// Indicates that the capture system is currently connected...
	bool IsConnected(void)
	{
		return AtomicGet(g_connectionFlags) != 0;
	}

	// Check to see if (a) a connection is established and (b) that a particular capture feature is enabled on the connection.
	bool CheckConnectionFlag(const CaptureFlag feature)
	{
		return (AtomicGet(g_connectionFlags) & feature) != 0;
	}

	// Mark the currently referenced frame index on this thread...
	// You may call this from any thread that generates frame data to help track latency and missed frames.
	void FrameIndex(const UInt64 frameIndex)
	{
		if(TryLockConnection())
		{
			FrameIndexPacket packet;
			packet.timestamp  = GetNanoseconds();
			packet.frameIndex = frameIndex;
			AsyncStream::Acquire()->WritePacket(packet);
			UnlockConnection();
		}
	}

	// Mark the start of vsync... this value should be comparable to the same reference point as GetNanoseconds()
	void VSyncTimestamp(UInt64 nanoseconds)
	{
		if(TryLockConnection())
		{
			VSyncPacket packet;
			packet.timestamp = nanoseconds;
			AsyncStream::Acquire()->WritePacket(packet);
			UnlockConnection();
		}
	}

	// Upload the framebuffer for the current frame... should be called once a frame!
	void FrameBuffer(UInt64 timestamp, FrameBufferFormat format, UInt32 width, UInt32 height, const void *buffer)
	{
		if(TryLockConnection(Enable_FrameBuffer_Capture))
		{
			UInt32 pixelSize = 0;
			switch(format)
			{
				case FrameBuffer_RGB_565:   pixelSize=16; break;
				case FrameBuffer_RGBA_8888: pixelSize=32; break;
				case FrameBuffer_DXT1:      pixelSize=4;
					if(width&3 || height&3)
					{
						Logf(Log_Warning, "OVR::Capture::FrameBuffer(): requires DXT1 texture dimensions to be multiples of 4");
						return;
					}
					break;
			}
			if(!pixelSize)
			{
				Logf(Log_Warning, "OVR::Capture::FrameBuffer(): Format (%u) not supported!", (UInt32)format);
				return;
			}
			const UInt32 payloadSize = (pixelSize * width * height) >> 3; // pixelSize is in bits, divide by 8 to give us payload byte size
			FrameBufferPacket packet;
			packet.format     = format;
			packet.width      = width;
			packet.height     = height;
			packet.timestamp  = timestamp;
			// TODO: we should probably just send framebuffer packets directly over the network rather than
			//       caching them due to their size and to reduce latency.
			AsyncStream::Acquire()->WritePacket(packet, buffer, payloadSize);
			UnlockConnection();
		}
	}

	// Misc application message logging...
	void Logf(LogPriority priority, const char *format, ...)
	{
		if(TryLockConnection(Enable_Logging))
		{
			va_list args;
			va_start(args, format);
			Logv(priority, format, args);
			va_end(args);
			UnlockConnection();
		}
	}

	void Logv(LogPriority priority, const char *format, va_list args)
	{
		if(TryLockConnection(Enable_Logging))
		{
			const size_t bufferMaxSize = 512;
			char buffer[bufferMaxSize];
			const int bufferSize = FormatStringV(buffer, bufferMaxSize, format, args);
			if(bufferSize > 0)
			{
				Log(priority, buffer);
			}
			UnlockConnection();
		}
	}

	void Log(LogPriority priority, const char *str)
	{
		if(str && str[0] && TryLockConnection(Enable_Logging))
		{
			LogPacket packet;
			packet.timestamp = GetNanoseconds();
			packet.priority  = priority;
			AsyncStream::Acquire()->WritePacket(packet, str, (UInt32)strlen(str));
			UnlockConnection();
		}
	}

	// Mark a CPU profiled region.... Begin(); DoSomething(); End();
	// Nesting is allowed. And every Begin() should ALWAYS have a matching End()!!!
	void EnterCPUZone(const LabelIdentifier label)
	{
		if(TryLockConnection(Enable_CPU_Zones))
		{
			CPUZoneEnterPacket packet;
			packet.labelID   = label.GetIdentifier();
			packet.timestamp = GetNanoseconds();
			AsyncStream::Acquire()->WritePacket(packet);
			UnlockConnection();
		}
	}

	void LeaveCPUZone(void)
	{
		if(TryLockConnection(Enable_CPU_Zones))
		{
			CPUZoneLeavePacket packet;
			packet.timestamp = GetNanoseconds();
			AsyncStream::Acquire()->WritePacket(packet);
			UnlockConnection();
		}
	}

	// Set sensor range of values.
	void SensorSetRange(const LabelIdentifier label, float minValue, float maxValue, SensorInterpolator interpolator, SensorUnits units)
	{
		if(TryLockConnection())
		{
			SensorRangePacket packet;
			packet.labelID      = label.GetIdentifier();
			packet.interpolator = interpolator;
			packet.units        = units;
			packet.minValue     = minValue;
			packet.maxValue     = maxValue;
			AsyncStream::Acquire()->WritePacket(packet);
			UnlockConnection();
		}
	}

	// Set the absolute value of a sensor, may be called at any frequency.
	void SensorSetValue(const LabelIdentifier label, float value)
	{
		if(TryLockConnection())
		{
			SensorPacket packet;
			packet.labelID   = label.GetIdentifier();
			packet.timestamp = GetNanoseconds();
			packet.value     = value;
			AsyncStream::Acquire()->WritePacket(packet);
			UnlockConnection();
		}
	}

	float GetVariable(const LabelIdentifier label, float valDefault, float valMin, float valMax)
	{
		// if this is defined, we allow the device to overwrite variables that haven't
		// been modified by the client
	#ifdef VAR_OVERWRITE
		float valCur = valDefault;
	#endif
		if (TryLockConnection())
		{
			const UInt32 hash = label.GetIdentifier();
		#ifdef VAR_OVERWRITE
			UInt32 valueType = g_varStore.Get(hash, valCur);
			// if we have gotten a reply from the client, no longer update with
			// a default 
			if (valueType != VarStore::ClientValue)
			{
				// if we have already sent this value, don't send it again
				if (valueType == VarStore::DeviceValue && valCur == valDefault)
				{
					UnlockConnection();
					return valDefault;
				}
				g_varStore.Set(hash, valDefault);
				VarRangePacket packet;
				packet.labelID = hash; 
				packet.value = valDefault;
				packet.valMin = valMin;
				packet.valMax = valMax;
				AsyncStream::Acquire()->WritePacket(packet);
			}  
		#else
			if (g_varStore.Get(hash, valDefault) == VarStore::NoValue)
			{
				g_varStore.Set(hash, valDefault);
				VarRangePacket packet;
				packet.labelID = hash; 
				packet.value = valDefault;
				packet.valMin = valMin;
				packet.valMax = valMax;
				AsyncStream::Acquire()->WritePacket(packet);
			}
		#endif
			UnlockConnection();
		}
	#ifdef VAR_OVERWRITE
		return valCur;
	#else
		return valDefault;
	#endif
	}  


	/***********************************
	 * Label                            *
	 ***********************************/

	Label  *Label::s_head      = NULL;

	// Use this constructor if Label is a global variable (not local static).
	Label::Label(const char *name)
	{
		ConditionalInit(name);
	}

	bool Label::ConditionalInit(const char *name)
	{
		g_labelLock.Lock();
		if(!m_name)
		{
			m_next       = s_head;
			s_head       = this;
			m_identifier = StringHash32(name);
			m_name       = name;
			SendLabelPacket(*this);
		}
		g_labelLock.Unlock();
		return true;
	}

	Label *Label::GetHead(void)
	{
		Label *head = s_head;
		return head;
	}

	Label *Label::GetNext(void) const
	{
		return m_next;
	}

} // namespace Capture
} // namespace OVR
