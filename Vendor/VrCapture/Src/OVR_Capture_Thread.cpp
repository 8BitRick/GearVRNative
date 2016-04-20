/************************************************************************************

PublicHeader:   OVR_Capture.h
Filename    :   OVR_Capture_Thread.cpp
Content     :   Misc Threading functionality....
Created     :   January, 2015
Notes       :

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_Capture_Thread.h"

namespace OVR
{
namespace Capture
{


	ThreadLocalKey CreateThreadLocalKey(void (*destructor)(void*))
	{
	#if defined(OVR_CAPTURE_POSIX)
		ThreadLocalKey key = {0};
		pthread_key_create(&key, destructor);
		return key;
	#elif defined(OVR_CAPTURE_WINDOWS)
		return TlsAlloc();
	#else
		#error Unknown Platform!
	#endif
	}

	void DestroyThreadLocalKey(ThreadLocalKey key)
	{
	#if defined(OVR_CAPTURE_POSIX)
		pthread_key_delete(key);
	#elif defined(OVR_CAPTURE_WINDOWS)
		TlsFree(key);
	#else
		#error Unknown Platform!
	#endif
	}

	void SetThreadLocalValue(ThreadLocalKey key, void *value)
	{
	#if defined(OVR_CAPTURE_POSIX)
		pthread_setspecific(key, value);
	#elif defined(OVR_CAPTURE_WINDOWS)
		TlsSetValue(key, value);
	#else
		#error Unknown Platform!
	#endif
	}

	void *GetThreadLocalValue(ThreadLocalKey key)
	{
	#if defined(OVR_CAPTURE_POSIX)
		return pthread_getspecific(key);
	#elif defined(OVR_CAPTURE_WINDOWS)
		return TlsGetValue(key);
	#else
		#error Unknown Platform!
	#endif
	}

	void SetThreadName(const char *name)
	{
	#if defined(OVR_CAPTURE_DARWIN)
		pthread_setname_np(name);
	#elif defined(OVR_CAPTURE_ANDROID)
		pthread_setname_np(pthread_self(), name);
	#elif defined(OVR_CAPTURE_WINDOWS)
		// TODO: Windows doesn't have the concept of thread names...
	#else
		#error Unknown Platform!
	#endif
	}


	CriticalSection::CriticalSection(void)
	{
	#if defined(OVR_CAPTURE_WINDOWS)
		InitializeCriticalSection(&m_cs);
	#elif defined(OVR_CAPTURE_POSIX)
		pthread_mutex_init(&m_mutex, NULL);
	#endif
	}

	CriticalSection::~CriticalSection(void)
	{
	#if defined(OVR_CAPTURE_WINDOWS)
		DeleteCriticalSection(&m_cs);
	#elif defined(OVR_CAPTURE_POSIX)
		pthread_mutex_destroy(&m_mutex);
	#endif
	}

	void CriticalSection::Lock(void)
	{
	#if defined(OVR_CAPTURE_WINDOWS)
		EnterCriticalSection(&m_cs);
	#elif defined(OVR_CAPTURE_POSIX)
		pthread_mutex_lock(&m_mutex);
	#endif
	}

	bool CriticalSection::TryLock(void)
	{
	#if defined(OVR_CAPTURE_WINDOWS)
		return TryEnterCriticalSection(&m_cs) ? true : false;
	#elif defined(OVR_CAPTURE_POSIX)
		return pthread_mutex_trylock(&m_mutex)==0;
	#endif
	}

	void CriticalSection::Unlock(void)
	{
	#if defined(OVR_CAPTURE_WINDOWS)
		LeaveCriticalSection(&m_cs);
	#elif defined(OVR_CAPTURE_POSIX)
		pthread_mutex_unlock(&m_mutex);
	#endif
	}


	RWLock::RWLock()
	{
	#if defined(OVR_CAPTURE_WINDOWS)
		InitializeSRWLock(&m_lock);
	#elif defined(OVR_CAPTURE_POSIX)
		pthread_rwlock_init(&m_lock, NULL);
	#else
		#error Unknown Platform!
	#endif
	}

	RWLock::~RWLock(void)
	{
	#if defined(OVR_CAPTURE_WINDOWS)
		// No deallocation necessary...
	#elif defined(OVR_CAPTURE_POSIX)
		pthread_rwlock_destroy(&m_lock);
	#else
		#error Unknown Platform!
	#endif
	}

	void RWLock::ReadLock()
	{
	#if defined(OVR_CAPTURE_WINDOWS)
		AcquireSRWLockShared(&m_lock);
	#elif defined(OVR_CAPTURE_POSIX)
		pthread_rwlock_rdlock(&m_lock);
	#else
		#error Unknown Platform!
	#endif
	}

	void RWLock::WriteLock()
	{
	#if defined(OVR_CAPTURE_WINDOWS)
		AcquireSRWLockExclusive(&m_lock);
	#elif defined(OVR_CAPTURE_POSIX)
		pthread_rwlock_wrlock(&m_lock);
	#else
		#error Unknown Platform!
	#endif
	}

	void RWLock::ReadUnlock()
	{
	#if defined(OVR_CAPTURE_WINDOWS)
		ReleaseSRWLockShared(&m_lock);
	#elif defined(OVR_CAPTURE_POSIX)
		pthread_rwlock_unlock(&m_lock);
	#else
		#error Unknown Platform!
	#endif
	}

	void RWLock::WriteUnlock()
	{
	#if defined(OVR_CAPTURE_WINDOWS)
		ReleaseSRWLockExclusive(&m_lock);
	#elif defined(OVR_CAPTURE_POSIX)
		pthread_rwlock_unlock(&m_lock);
	#else
		#error Unknown Platform!
	#endif
	}


	ThreadGate::ThreadGate(void)
	{
	#if defined(OVR_CAPTURE_POSIX)
		pthread_mutex_init(&m_mutex, NULL);
		pthread_cond_init( &m_cond,  NULL);
	#elif defined(OVR_CAPTURE_WINDOWS)
		InitializeCriticalSection(&m_cs);
		InitializeConditionVariable(&m_cond);
	#endif
		Open();
	}

	ThreadGate::~ThreadGate(void)
	{
	#if defined(OVR_CAPTURE_POSIX)
		pthread_mutex_destroy(&m_mutex);
		pthread_cond_destroy( &m_cond);
	#elif defined(OVR_CAPTURE_WINDOWS)
		DeleteCriticalSection(&m_cs);
	#endif
	}

	void ThreadGate::Open(void)
	{
	#if defined(OVR_CAPTURE_POSIX)
		pthread_mutex_lock(&m_mutex);
		m_open = true;
		pthread_cond_broadcast(&m_cond);
		pthread_mutex_unlock(&m_mutex);
	#elif defined(OVR_CAPTURE_WINDOWS)
		EnterCriticalSection(&m_cs);
		m_open = true;
		WakeAllConditionVariable(&m_cond);
		LeaveCriticalSection(&m_cs);
	#endif
	}

	void ThreadGate::WaitForOpen(void)
	{
	#if defined(OVR_CAPTURE_POSIX)
		pthread_mutex_lock(&m_mutex);
		while(!m_open)
			pthread_cond_wait(&m_cond, &m_mutex);
		pthread_mutex_unlock(&m_mutex);
	#elif defined(OVR_CAPTURE_WINDOWS)
		EnterCriticalSection(&m_cs);
		while(!m_open)
			SleepConditionVariableCS(&m_cond, &m_cs, INFINITE);
		LeaveCriticalSection(&m_cs);
	#endif
	}

	void ThreadGate::Close(void)
	{
	#if defined(OVR_CAPTURE_POSIX)
		pthread_mutex_lock(&m_mutex);
		m_open = false;
		pthread_mutex_unlock(&m_mutex);
	#elif defined(OVR_CAPTURE_WINDOWS)
		EnterCriticalSection(&m_cs);
		m_open = false;
		LeaveCriticalSection(&m_cs);
	#endif
	}




	Thread::Thread(void)
	{
		m_thread       = 0;
		m_quitSignaled = 0;
	}

	Thread::~Thread(void)
	{
		QuitAndWait();
	}

	void Thread::Start(void)
	{
		OVR_CAPTURE_ASSERT(!m_thread && !m_quitSignaled);
	#if defined(OVR_CAPTURE_POSIX)
		pthread_attr_init(&m_threadAttrs);
		pthread_create(&m_thread, &m_threadAttrs, ThreadEntry, this);
	#elif defined(OVR_CAPTURE_WINDOWS)
		m_thread = CreateThread(NULL, 0, ThreadEntry, this, 0, NULL);
	#else
		#error Unknown Platform!
	#endif
	}

	void Thread::QuitAndWait(void)
	{
		if(m_thread)
		{
			AtomicExchange<UInt32>(m_quitSignaled, 1);
		#if defined(OVR_CAPTURE_POSIX)
			pthread_join(m_thread, 0);
			pthread_attr_destroy(&m_threadAttrs);
		#elif defined(OVR_CAPTURE_WINDOWS)
			WaitForSingleObject(m_thread, INFINITE);
		#else
			#error Unknown Platform!
		#endif
			m_thread       = 0;
			m_quitSignaled = 0;
		}
	}

	bool Thread::QuitSignaled(void)
	{
		return AtomicGet(m_quitSignaled) ? true : false;
	}

#if defined(OVR_CAPTURE_POSIX)
	void *Thread::ThreadEntry(void *arg)
	{
		Thread *thread = (Thread*)arg;
		thread->OnThreadExecute();
		return NULL;
	}
#elif defined(OVR_CAPTURE_WINDOWS)
	DWORD __stdcall Thread::ThreadEntry(void *arg)
	{
		Thread *thread = (Thread*)arg;
		thread->OnThreadExecute();
		return 0;
	}
#endif

} // namespace Capture
} // namespace OVR
