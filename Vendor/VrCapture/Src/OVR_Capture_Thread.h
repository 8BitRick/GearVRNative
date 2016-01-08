/************************************************************************************

PublicHeader:   OVR_Capture.h
Filename    :   OVR_Capture_Thread.h
Content     :   Misc Threading functionality....
Created     :   January, 2015
Notes       :

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPTURE_THREAD_H
#define OVR_CAPTURE_THREAD_H

#include <OVR_Capture.h>
#include <OVR_Capture_Types.h>

#if defined(OVR_CAPTURE_POSIX)
	#include <pthread.h>
	#include <sched.h>
	#include <unistd.h>
#elif defined(OVR_CAPTURE_WINDOWS)
	#define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
	#include <windows.h>
#endif

#include <string.h> // memcpy

namespace OVR
{
namespace Capture
{

#if defined(OVR_CAPTURE_POSIX)
	typedef pthread_key_t ThreadLocalKey;
	static const ThreadLocalKey NullThreadLocalKey = 0;
#elif defined(OVR_CAPTURE_WINDOWS)
	typedef DWORD ThreadLocalKey;
	static const ThreadLocalKey NullThreadLocalKey = TLS_OUT_OF_INDEXES;
#endif

	ThreadLocalKey CreateThreadLocalKey(void (*destructor)(void*)=NULL);
	void           DestroyThreadLocalKey(ThreadLocalKey key);
	void           SetThreadLocalValue(ThreadLocalKey key, void *value);
	void          *GetThreadLocalValue(ThreadLocalKey key);

	void           SetThreadName(const char *name);

	template<typename T>
	inline T AtomicExchange(volatile T &destination, T exchange)
	{
	#if defined(OVR_CAPTURE_POSIX)
		return __sync_lock_test_and_set(&destination, exchange);
	#elif defined(OVR_CAPTURE_WINDOWS)
		OVR_CAPTURE_STATIC_ASSERT(sizeof(LONG) == sizeof(T));
		return InterlockedExchange((volatile LONG*)&destination, exchange);
	#else
		#error Unknown platform!
	#endif
	}

	template<typename T>
	inline T AtomicAdd(volatile T &destination, T x)
	{
	#if defined(OVR_CAPTURE_POSIX)
		return __sync_fetch_and_add(&destination, x);
	#elif defined(OVR_CAPTURE_WINDOWS)
		OVR_CAPTURE_STATIC_ASSERT(sizeof(LONG) == sizeof(T));
		return InterlockedAdd((volatile LONG*)&destination, x);
	#else
		#error Unknown platform!
	#endif
	}

	template<typename T>
	inline T AtomicSub(volatile T &destination, T x)
	{
	#if defined(OVR_CAPTURE_POSIX)
		return __sync_fetch_and_sub(&destination, x);
	#elif defined(OVR_CAPTURE_WINDOWS)
		OVR_CAPTURE_STATIC_ASSERT(sizeof(LONG) == sizeof(T));
		return InterlockedAdd((volatile LONG*)&destination, -(LONG)x);
	#else
		#error Unknown platform!
	#endif
	}

	template<typename T>
	inline T AtomicGet(volatile T &x)
	{
		return AtomicAdd<T>(x, 0);
	}

	inline bool AtomicAcquireBarrier(volatile int &atomic)
	{
	#if defined(OVR_CAPTURE_POSIX)
		return __sync_lock_test_and_set(&atomic, 1) ? false : true;
	#elif defined(OVR_CAPTURE_WINDOWS)
		OVR_CAPTURE_STATIC_ASSERT(sizeof(LONG) == sizeof(int));
		return InterlockedCompareExchangeAcquire((volatile LONG*)&atomic, 1, 0) ? false : true;
	#else
		#error Unknown platform!
	#endif
	}

	inline void AtomicReleaseBarrier(volatile int &atomic)
	{
	#if defined(OVR_CAPTURE_POSIX)
		return __sync_lock_release(&atomic);
	#elif defined(OVR_CAPTURE_WINDOWS)
		OVR_CAPTURE_STATIC_ASSERT(sizeof(LONG) == sizeof(int));
		InterlockedCompareExchangeRelease((volatile LONG*)&atomic, 0, 1);
	#else
		#error Unknown platform!
	#endif
	}

	inline void ThreadYield(void)
	{
	#if defined(OVR_CAPTURE_POSIX)
		sched_yield();
	#elif defined(OVR_CAPTURE_WINDOWS)
		SwitchToThread();
	#else
		#error Unknown Platform!
	#endif
	}

	inline void ThreadSleepNanoseconds(UInt32 nanoseconds)
	{
	#if defined(OVR_CAPTURE_POSIX)
		// Because in our case when we sleep we really want to try and 
		// sleep the maximum amount of time. So we use nanosleep and loop
		// until we sleep the full amount of time.
		struct timespec req = {0,0};
		req.tv_nsec = nanoseconds;
		while(nanosleep(&req, &req) != 0)
			continue;
	#elif defined(OVR_CAPTURE_WINDOWS)
		// Nothing better than millisecond sleeping on Windows ;(
		Sleep(nanoseconds/1000000);
	#else
		#error Unknown Platform!
	#endif
	}

	inline void ThreadSleepMicroseconds(UInt32 microseconds)
	{
		ThreadSleepNanoseconds(microseconds * 1000);
	}

	inline void ThreadSleepMilliseconds(UInt32 milliseconds)
	{
		ThreadSleepMicroseconds(milliseconds * 1000);
	}

	inline bool SpinTryLock(volatile int &atomic)
	{
		return AtomicAcquireBarrier(atomic);
	}

	inline void SpinLock(volatile int &atomic)
	{
		// Do a quick test immediately because by definition the spin lock should
		// be extremely low contention.
		if(SpinTryLock(atomic))
			return;

		// Try without yielding for the first few cycles...
		for(int i=0; i<50; i++)
			if(SpinTryLock(atomic))
				return;

		// TODO: we should measure how often we end up going down the yield path...
		//       a histogram of how many iterations it takes to succeed would be perfect.

		// If we are taking a long time, start yielding...
		while(true)
		{
			ThreadYield();
			for(int i=0; i<10; i++)
				if(SpinTryLock(atomic))
					return;
		}
	}

	inline void SpinUnlock(volatile int &atomic)
	{
		AtomicReleaseBarrier(atomic);
	}

	class CriticalSection
	{
		public:
			CriticalSection(void);
			~CriticalSection(void);
			void Lock(void);
			bool TryLock(void);
			void Unlock(void);
		private:
		#if defined(OVR_CAPTURE_WINDOWS)
			CRITICAL_SECTION  m_cs;
		#elif defined(OVR_CAPTURE_POSIX)
			pthread_mutex_t   m_mutex;
		#else
			#error Unknown Platform!
		#endif
	};

	class RWLock
	{
		public:
			RWLock(void);
			~RWLock(void);

			void ReadLock(void);
			void WriteLock(void);  

			void ReadUnlock(void);
			void WriteUnlock(void);

		private:
		#if defined(OVR_CAPTURE_WINDOWS)
			SRWLOCK          m_lock;
		#elif defined(OVR_CAPTURE_POSIX)
			pthread_rwlock_t m_lock;
		#else
			#error Unknown Platform!
		#endif
	};

	class ThreadGate
	{
		public:
			ThreadGate(void);
			~ThreadGate(void);

			void Open(void);
			void WaitForOpen(void);
			void Close(void);

		private:
		#if defined(OVR_CAPTURE_POSIX)
			pthread_mutex_t    m_mutex;
			pthread_cond_t     m_cond;
		#elif defined(OVR_CAPTURE_WINDOWS)
			CRITICAL_SECTION   m_cs;
			CONDITION_VARIABLE m_cond;
		#else
			#error Unknown Platform!
		#endif
			bool               m_open;
	};

	class Thread
	{
		public:
			Thread(void);
			virtual ~Thread(void);

			void Start(void);
			void QuitAndWait(void);

		protected:
			bool QuitSignaled(void);

		private:
			virtual void OnThreadExecute(void) = 0;

		#if defined(OVR_CAPTURE_POSIX)
			static void *ThreadEntry(void *arg);
		#elif defined(OVR_CAPTURE_WINDOWS)
			static DWORD __stdcall ThreadEntry(void *arg);
		#endif

		private:
		#if defined(OVR_CAPTURE_POSIX)
			pthread_attr_t m_threadAttrs;
			pthread_t      m_thread;
		#elif defined(OVR_CAPTURE_WINDOWS)
			HANDLE         m_thread;
		#else
			#error Unknown Platform!
		#endif
			UInt32         m_quitSignaled;
	};

} // namespace Capture
} // namespace OVR

#endif
