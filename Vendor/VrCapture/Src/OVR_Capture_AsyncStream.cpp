/************************************************************************************

PublicHeader:   OVR_Capture.h
Filename    :   OVR_Capture_AsyncStream.cpp
Content     :   Oculus performance capture library. Interface for async data streaming.
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_Capture_AsyncStream.h"
#include "OVR_Capture_Local.h"
#include "OVR_Capture_Socket.h"
#include "OVR_Capture_FileIO.h"

#include <stdio.h>

namespace OVR
{
namespace Capture
{

	static ThreadLocalKey     g_tlskey   = NullThreadLocalKey;
	static CriticalSection    g_listlock;

	static const Label        g_flushAllLabel("AsyncStream_FlushAll");
	static const Label        g_flushLabel("AsyncStream_Flush");

	template<typename T>
	inline void Swap(T &a, T &b)
	{
		T temp = a;
		a      = b;
		b      = temp;
	}


	/******************
	* SocketOutStream *
	******************/

	SocketOutStream::SocketOutStream(Socket &s) :
		m_socket(s)
	{
	}

	bool SocketOutStream::Send(const void *buffer, UInt32 size)
	{
		return m_socket.Send(buffer, size);
	}


	/******************
	* FileOutStream   *
	******************/

	FileOutStream::FileOutStream(FileHandle f) :
		m_file(f)
	{
	}

	bool FileOutStream::Send(const void *buffer, UInt32 size)
	{
		return (WriteFile(m_file, buffer, size) == (int)size);
	}


	/******************
	* AsyncStream     *
	******************/

	AsyncStream *AsyncStream::s_head = NULL;


	// Initialize the per-thread stream system... MUST be called before being connected!
	void AsyncStream::Init(void)
	{
		OVR_CAPTURE_ASSERT(g_tlskey == NullThreadLocalKey);
		g_tlskey = CreateThreadLocalKey();
		OVR_CAPTURE_ASSERT(s_head == NULL);
	}

	// Release the per-thread streams... Should be called when connection is closed.
	void AsyncStream::Shutdown(void)
	{
		// destroy our thread local key...
		OVR_CAPTURE_ASSERT(g_tlskey != NullThreadLocalKey);
		DestroyThreadLocalKey(g_tlskey);
		g_tlskey = NullThreadLocalKey;

		// delete all the async streams...
		g_listlock.Lock();
		while(s_head)
		{
			AsyncStream *curr = s_head;
			s_head = s_head->m_next;
			delete curr;
		}
		g_listlock.Unlock();
	}

	// Acquire a per-thread stream for the current thread...
	AsyncStream *AsyncStream::Acquire(void)
	{
		AsyncStream *stream = (AsyncStream*)GetThreadLocalValue(g_tlskey);
		if(!stream)
		{
			stream = new AsyncStream();
			SetThreadLocalValue(g_tlskey, stream);
		}
		return stream;
	}

	// Flush all existing thread streams... returns false on socket error
	bool AsyncStream::FlushAll(OutStream &outStream)
	{
		const CPUScope cpuzone(g_flushAllLabel);
		bool okay = true;
		g_listlock.Lock();
		for(AsyncStream *curr=s_head; curr; curr=curr->m_next)
		{
			okay = curr->Flush(outStream);
			if(!okay) break;
		}
		g_listlock.Unlock();
		return okay;
	}

	// Clears the contents of all streams.
	void AsyncStream::ClearAll(void)
	{
		g_listlock.Lock();
		for(AsyncStream *curr=s_head; curr; curr=curr->m_next)
		{
			SpinLock(curr->m_bufferLock);
			curr->m_cacheTail = curr->m_cacheBegin;
			curr->m_flushTail = curr->m_flushBegin;
			SpinUnlock(curr->m_bufferLock);
			curr->m_gate.Open();
		}
		g_listlock.Unlock();
	}


	// Flushes all available packets over the network... returns number of bytes sent
	bool AsyncStream::Flush(OutStream &outStream)
	{
		const CPUScope cpuzone(g_flushLabel);

		bool okay = true;

		// Take ownership of any pending data...
		SpinLock(m_bufferLock);
		Swap(m_cacheBegin, m_flushBegin);
		Swap(m_cacheTail,  m_flushTail);
		Swap(m_cacheEnd,   m_flushEnd);
		SpinUnlock(m_bufferLock);

		// Signal that we just swapped in a new buffer... wake up any threads that were waiting on us to flush.
		m_gate.Open();

		if(m_flushTail > m_flushBegin)
		{
			const UInt32 sendSize = (UInt32)(size_t)(m_flushTail - m_flushBegin);

			// first send stream header...
			StreamHeaderPacket streamheader;
			streamheader.threadID   = m_threadID;
			streamheader.streamSize = sendSize;
			okay = outStream.Send(&streamheader, sizeof(streamheader));

			// This send payload...
			okay = okay && outStream.Send(m_flushBegin, sendSize);
			m_flushTail = m_flushBegin;
		}

		OVR_CAPTURE_ASSERT(m_flushBegin == m_flushTail); // should be empty at this point...

		return okay;
	}

	AsyncStream::AsyncStream(void)
	{
		m_bufferLock  = 0;

	#if defined(OVR_CAPTURE_DARWIN)
		OVR_CAPTURE_STATIC_ASSERT(sizeof(mach_port_t) <= sizeof(UInt32));
		union
		{
			UInt32       i;
			mach_port_t  t;
		};
		i = 0;
		t = pthread_mach_thread_np(pthread_self());
		m_threadID = i;
	#elif defined(OVR_CAPTURE_POSIX)
		OVR_CAPTURE_STATIC_ASSERT(sizeof(pid_t) <= sizeof(UInt32));
		union
		{
			UInt32 i;
			pid_t  t;
		};
		i = 0;
		t = gettid();
		m_threadID = i;
	#elif defined(OVR_CAPTURE_WINDOWS)
		OVR_CAPTURE_STATIC_ASSERT(sizeof(DWORD) == sizeof(UInt32));
		m_threadID = static_cast<UInt32>(GetCurrentThreadId());
	#else
		#error UNKNOWN PLATFORM!
	#endif

		m_cacheBegin  = new UInt8[s_bufferSize];
		m_cacheTail   = m_cacheBegin;
		m_cacheEnd    = m_cacheBegin + s_bufferSize;

		m_flushBegin  = new UInt8[s_bufferSize];
		m_flushTail   = m_flushBegin;
		m_flushEnd    = m_flushBegin + s_bufferSize;

		// Make sure we are open by default... we don't close until we fill the buffer...
		m_gate.Open();

		// when we are finally initialized... add ourselves to the linked list...
		g_listlock.Lock();
		m_next = s_head;
		s_head = this;
		g_listlock.Unlock();

		// Try and acquire thread name...
		SendThreadName();
	}

	AsyncStream::~AsyncStream(void)
	{
		if(m_cacheBegin) delete [] m_cacheBegin;
		if(m_flushBegin) delete [] m_flushBegin;
	}

	void AsyncStream::SendThreadName(void)
	{
		ThreadNamePacket packet = {0};
		char name[64] = {0};
	#if defined(OVR_CAPTURE_ANDROID)
		char commpath[64] = {0};
		FormatString(commpath, sizeof(commpath), "/proc/%d/task/%d/comm", getpid(), gettid());
		ReadFileLine(commpath, name, sizeof(name));
	#elif defined(OVR_CAPTURE_DARWIN)
		pthread_getname_np(pthread_self(), name, sizeof(name));
	#endif
		if(name[0])
		{
			WritePacket(packet, name, (UInt32)strlen(name));
		}
	}

} // namespace Capture
} // namespace OVR
