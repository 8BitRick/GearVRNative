/************************************************************************************

Filename    :   OVR_ThreadsWinAPI.cpp
Platform    :   WinAPI
Content     :   Windows specific thread-related (safe) functionality
Created     :   September 19, 2012
Notes       : 

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_Threads.h"
#include "OVR_Hash.h"
#include "OVR_Log.h"

#ifdef OVR_ENABLE_THREADS

// For _beginthreadex / _endtheadex
#include <process.h>

namespace OVR {


//-----------------------------------------------------------------------------------
// *** Internal Mutex implementation class

class MutexImpl 
{
    // System mutex or semaphore
    HANDLE            hMutexOrSemaphore;
    bool              Recursive;
    volatile unsigned LockCount;
    
    friend class WaitConditionImpl;

public:
    // Constructor/destructor
    MutexImpl(bool recursive = 1);
    ~MutexImpl();

    // Locking functions
    void                DoLock();
    bool                TryLock();
    void                Unlock(Mutex* pmutex);
    // Returns 1 if the mutes is currently locked
    bool                IsLockedByAnotherThread(Mutex* pmutex);
};

// *** Constructor/destructor
MutexImpl::MutexImpl(bool recursive)
{    
    Recursive                   = recursive;
    LockCount                   = 0;
    hMutexOrSemaphore           = Recursive ? CreateMutex(NULL, 0, NULL) : CreateSemaphore(NULL, 1, 1, NULL);
}
MutexImpl::~MutexImpl()
{
    CloseHandle(hMutexOrSemaphore);
}


// Lock and try lock
void MutexImpl::DoLock()
{
    if (::WaitForSingleObject(hMutexOrSemaphore, INFINITE) != WAIT_OBJECT_0)
        return;
    LockCount++;
}

bool MutexImpl::TryLock()
{
    DWORD ret;
    if ((ret=::WaitForSingleObject(hMutexOrSemaphore, 0)) != WAIT_OBJECT_0)
        return 0;
    LockCount++;
    return 1;
}

void MutexImpl::Unlock(Mutex* pmutex)
{
    OVR_UNUSED(pmutex);

    LockCount--;

    // Release mutex
    if ((Recursive ? ReleaseMutex(hMutexOrSemaphore) :
                     ReleaseSemaphore(hMutexOrSemaphore, 1, NULL))  != 0)
    {
        // This used to call Wait handlers if LockCount == 0.
    }
}

bool MutexImpl::IsLockedByAnotherThread(Mutex* pmutex)
{
    // There could be multiple interpretations of IsLocked with respect to current thread
    if (LockCount == 0)
        return 0;
    if (!TryLock())
        return 1;
    Unlock(pmutex);
    return 0;
}

/*
bool    MutexImpl::IsSignaled() const
{
    // An mutex is signaled if it is not locked ANYWHERE
    // Note that this is different from IsLockedByAnotherThread function,
    // that takes current thread into account
    return LockCount == 0;
}
*/


// *** Actual Mutex class implementation

Mutex::Mutex(bool recursive)
{    
    pImpl = new MutexImpl(recursive);
}
Mutex::~Mutex()
{
    delete pImpl;
}

// Lock and try lock
void Mutex::DoLock()
{
    pImpl->DoLock();
}
bool Mutex::TryLock()
{
    return pImpl->TryLock();
}
void Mutex::Unlock()
{
    pImpl->Unlock(this);
}
bool Mutex::IsLockedByAnotherThread()
{
    return pImpl->IsLockedByAnotherThread(this);
}

//-----------------------------------------------------------------------------------
// ***** Event

bool Event::Wait(unsigned delay)
{
    Mutex::Locker lock(&StateMutex);

    // Do the correct amount of waiting
    if (delay == OVR_WAIT_INFINITE)
    {
        while(!State)
            StateWaitCondition.Wait(&StateMutex);
    }
    else if (delay)
    {
        if (!State)
            StateWaitCondition.Wait(&StateMutex, delay);
    }

    bool state = State;
    // Take care of temporary 'pulsing' of a state
    if (Temporary)
    {
        Temporary   = false;
        State       = false;
    }
    return state;
}

void Event::updateState(bool newState, bool newTemp, bool mustNotify)
{
    Mutex::Locker lock(&StateMutex);
    State       = newState;
    Temporary   = newTemp;
    if (mustNotify)
        StateWaitCondition.NotifyAll();    
}


//-----------------------------------------------------------------------------------
// ***** Win32 Wait Condition Implementation

// Internal implementation class
class WaitConditionImpl 
{   
    // Event pool entries for extra events
    struct EventPoolEntry  
    {
        HANDLE          hEvent;
        EventPoolEntry  *pNext;
        EventPoolEntry  *pPrev;
    };
    
    Lock                WaitQueueLoc;
    // Stores free events that can be used later
    EventPoolEntry  *   pFreeEventList;
    
    // A queue of waiting objects to be signaled    
    EventPoolEntry*     pQueueHead;
    EventPoolEntry*     pQueueTail;

    // Allocation functions for free events
    EventPoolEntry*     GetNewEvent();
    void                ReleaseEvent(EventPoolEntry* pevent);

    // Queue operations
    void                QueuePush(EventPoolEntry* pentry);
    EventPoolEntry*     QueuePop();
    void                QueueFindAndRemove(EventPoolEntry* pentry);

public:

    // Constructor/destructor
    WaitConditionImpl();
    ~WaitConditionImpl();

    // Release mutex and wait for condition. The mutex is re-acqured after the wait.
    bool    Wait(Mutex *pmutex, unsigned delay = OVR_WAIT_INFINITE);

    // Notify a condition, releasing at one object waiting
    void    Notify();
    // Notify a condition, releasing all objects waiting
    void    NotifyAll();
};



WaitConditionImpl::WaitConditionImpl()
{
    pFreeEventList  = 0;
    pQueueHead      =
    pQueueTail      = 0;
}

WaitConditionImpl::~WaitConditionImpl()
{
    // Free all the resources
    EventPoolEntry* p       = pFreeEventList;
    EventPoolEntry* pentry;

    while(p)
    {
        // Move to next
        pentry = p;
        p = p->pNext;
        // Delete old
        ::CloseHandle(pentry->hEvent);
        delete pentry;  
    }   
    // Shouldn't we also consider the queue?

    // To be safe
    pFreeEventList  = 0;
    pQueueHead      =
    pQueueTail      = 0;
}


// Allocation functions for free events
WaitConditionImpl::EventPoolEntry* WaitConditionImpl::GetNewEvent()
{
    EventPoolEntry* pentry;

    // If there are any free nodes, use them
    if (pFreeEventList)
    {
        pentry          = pFreeEventList;
        pFreeEventList  = pFreeEventList->pNext;        
    }
    else
    {
        // Allocate a new node
        pentry          = new EventPoolEntry;
        pentry->pNext   = 0;
        pentry->pPrev   = 0;
        // Non-signaled manual event
        pentry->hEvent  = ::CreateEvent(NULL, TRUE, 0, NULL);
    }
    
    return pentry;
}

void WaitConditionImpl::ReleaseEvent(EventPoolEntry* pevent)
{
    // Mark event as non-signaled
    ::ResetEvent(pevent->hEvent);
    // And add it to free pool
    pevent->pNext   = pFreeEventList;
    pevent->pPrev   = 0;
    pFreeEventList  = pevent;
}

// Queue operations
void WaitConditionImpl::QueuePush(EventPoolEntry* pentry)
{
    // Items already exist? Just add to tail
    if (pQueueTail)
    {
        pentry->pPrev       = pQueueTail;
        pQueueTail->pNext   = pentry;
        pentry->pNext       = 0;        
        pQueueTail          = pentry;       
    }
    else
    {
        // No items in queue
        pentry->pNext   = 
        pentry->pPrev   = 0;
        pQueueHead      =
        pQueueTail      = pentry;
    }
}

WaitConditionImpl::EventPoolEntry* WaitConditionImpl::QueuePop()
{
    EventPoolEntry* pentry = pQueueHead;

    // No items, null pointer
    if (pentry)
    {
        // More items after this one? just grab the first item
        if (pQueueHead->pNext)
        {       
            pQueueHead  = pentry->pNext;
            pQueueHead->pPrev = 0;      
        }
        else
        {
            // Last item left
            pQueueTail =
            pQueueHead = 0;
        }
    }   
    return pentry;
}

void WaitConditionImpl::QueueFindAndRemove(EventPoolEntry* pentry)
{
    // Do an exhaustive search looking for an entry
    EventPoolEntry* p = pQueueHead;

    while(p)
    {
        // Entry found? Remove.
        if (p == pentry)
        {
            
            // Remove the node form the list
            // Prev link
            if (pentry->pPrev)
                pentry->pPrev->pNext = pentry->pNext;
            else
                pQueueHead = pentry->pNext;
            // Next link
            if (pentry->pNext)
                pentry->pNext->pPrev = pentry->pPrev;
            else
                pQueueTail = pentry->pPrev;
            // Done
            return;
        }

        // Move to next item
        p = p->pNext;
    }
}
    

bool WaitConditionImpl::Wait(Mutex *pmutex, unsigned delay)
{
    bool            result = 0;
    unsigned        i;
    unsigned        lockCount = pmutex->pImpl->LockCount;
    EventPoolEntry* pentry;

    // Mutex must have been locked
    if (lockCount == 0)
        return 0;
    
    // Add an object to the wait queue
    WaitQueueLoc.DoLock();
    QueuePush(pentry = GetNewEvent());
    WaitQueueLoc.Unlock();

    // Finally, release a mutex or semaphore
    if (pmutex->pImpl->Recursive)
    {
        // Release the recursive mutex N times
        pmutex->pImpl->LockCount = 0;
        for(i=0; i<lockCount; i++)
            ::ReleaseMutex(pmutex->pImpl->hMutexOrSemaphore);
    }
    else
    {
        pmutex->pImpl->LockCount = 0;
        ::ReleaseSemaphore(pmutex->pImpl->hMutexOrSemaphore, 1, NULL);
    }

    // Note that there is a gap here between mutex.Unlock() and Wait(). However,
    // if notify() comes in at this point in the other thread it will set our
    // corresponding event so wait will just fall through, as expected.

    // Block and wait on the event
    DWORD waitResult = ::WaitForSingleObject(pentry->hEvent,
                            (delay == OVR_WAIT_INFINITE) ? INFINITE : delay);
    /*
repeat_wait:
    DWORD waitResult =

    ::MsgWaitForMultipleObjects(1, &pentry->hEvent, FALSE,
                                (delay == OVR_WAIT_INFINITE) ? INFINITE : delay,
                                QS_ALLINPUT);
    */

    WaitQueueLoc.DoLock();
    switch(waitResult)
    {
        case WAIT_ABANDONED:
        case WAIT_OBJECT_0: 
            result = 1;
            // Wait was successful, therefore the event entry should already be removed
            // So just add entry back to a free list
            ReleaseEvent(pentry);
            break;
            /*
        case WAIT_OBJECT_0 + 1:
            // Messages in WINDOWS queue
            {
                MSG msg;
                PeekMessage(&msg, NULL, 0U, 0U, PM_NOREMOVE);             
                WaitQueueLoc.Unlock();
                goto repeat_wait;
            }
            break; */
        default:
            // Timeout, our entry should still be in a queue
            QueueFindAndRemove(pentry);
            ReleaseEvent(pentry);
    }
    WaitQueueLoc.Unlock();

    // Re-aquire the mutex
    for(i=0; i<lockCount; i++)
        pmutex->DoLock(); 

    // Return the result
    return result;
}

// Notify a condition, releasing the least object in a queue
void WaitConditionImpl::Notify()
{
    Lock::Locker   lock(&WaitQueueLoc);
    
    // Pop last entry & signal it
    EventPoolEntry* pentry = QueuePop();    
    if (pentry)
        ::SetEvent(pentry->hEvent); 
}

// Notify a condition, releasing all objects waiting
void WaitConditionImpl::NotifyAll()
{
    Lock::Locker   lock(&WaitQueueLoc);

    // Pop and signal all events
    // NOTE : There is no need to release the events, it's the waiters job to do so 
    EventPoolEntry* pentry = QueuePop();
    while (pentry)
    {
        ::SetEvent(pentry->hEvent);
        pentry = QueuePop();
    }
}



// *** Actual implementation of WaitCondition

WaitCondition::WaitCondition()
{
    pImpl = new WaitConditionImpl;
}
WaitCondition::~WaitCondition()
{
    delete pImpl;
}
    
// Wait without a mutex
bool    WaitCondition::Wait(Mutex *pmutex, unsigned delay)
{
    return pImpl->Wait(pmutex, delay);
}
// Notification
void    WaitCondition::Notify()
{
    pImpl->Notify();
}
void    WaitCondition::NotifyAll()
{
    pImpl->NotifyAll();
}



//-----------------------------------------------------------------------------------
// ***** Thread Class

//  Per-thread variable
//  MA: Don't use TLS for now - portability issues with DLLs, etc.
/*
#if !defined(OVR_CC_MSVC) || (OVR_CC_MSVC < 1300)
__declspec(thread)  Thread*    pCurrentThread      = 0;
#else
#pragma data_seg(".tls$")
__declspec(thread)  Thread*    pCurrentThread      = 0;
#pragma data_seg(".rwdata")
#endif
*/

// *** Thread constructors.

Thread::Thread(size_t stackSize, int processor)
{    
    CreateParams params;
    params.stackSize = stackSize;
    params.processor = processor;
    Init(params);
}

Thread::Thread(Thread::ThreadFn threadFunction, void*  userHandle, size_t stackSize, 
                 int processor, Thread::ThreadState initialState)
{
    CreateParams params(threadFunction, userHandle, stackSize, processor, initialState);
    Init(params);
}

Thread::Thread(const CreateParams& params)
{
    Init(params);
}
void Thread::Init(const CreateParams& params)
{
    // Clear the variables    
    ThreadFlags     = 0;
    ThreadHandle    = 0;
    IdValue         = 0;
    ExitCode        = NULL;
    StackSize       = params.stackSize;
    Processor       = params.processor;
    Priority        = params.priority;

    // Clear Function pointers
    ThreadFunction  = params.threadFunction;
    UserHandle      = params.userHandle;
    if (params.initialState != NotRunning)
        Start(params.initialState);

}

Thread::~Thread()
{
    // Thread should not running while object is being destroyed,
    // this would indicate ref-counting issue.
    //OVR_ASSERT(IsRunning() == 0);
  
    // Clean up thread.    
    CleanupSystemThread();
    ThreadHandle = 0;
}


// *** Overridable User functions.

// Default Run implementation
threadReturn_t Thread::Run()
{
    // Call pointer to function, if available.
    return ( ThreadFunction != NULL ) ? ThreadFunction( this, UserHandle ) : 0;
}
void Thread::OnExit()
{   
}

// Finishes the thread and releases internal reference to it.
void Thread::Finish()
{
    // Note: thread must be US.
    ThreadFlags &= (uint32_t)~(OVR_THREAD_STARTED);
    ThreadFlags |= OVR_THREAD_FINISHED;
}


// *** Run override

void Thread::PRun()
{
    // Suspend us on start, if requested
    if (ThreadFlags & OVR_THREAD_START_SUSPENDED)
    {
        if (::SuspendThread(ThreadHandle) != 0xFFFFFFFF)
        {
        }
        ThreadFlags &= (uint32_t)~OVR_THREAD_START_SUSPENDED;
    }

    // Call the virtual run function
    ExitCode = Run();
}

// *** User overridables

bool Thread::GetExitFlag() const
{
    return (ThreadFlags & OVR_THREAD_EXIT) != 0;
}       

void Thread::SetExitFlag(bool exitFlag)
{
    // The below is atomic since ThreadFlags is AtomicInt.
    if (exitFlag)
        ThreadFlags |= OVR_THREAD_EXIT;
    else
        ThreadFlags &= (uint32_t) ~OVR_THREAD_EXIT;
}

// Determines whether the thread was running and is now finished
bool Thread::IsFinished() const
{
    return (ThreadFlags & OVR_THREAD_FINISHED) != 0;
}

// Returns current thread state
Thread::ThreadState Thread::GetThreadState() const
{
    if (ThreadFlags & OVR_THREAD_START_SUSPENDED)
        return Suspended;    
    if (ThreadFlags & OVR_THREAD_STARTED)
        return Running;    
    return NotRunning;
}

// ***** Thread management
/* static */
int Thread::GetOSPriority(ThreadPriority p)
{
    switch(p)
    {
    case Thread::CriticalPriority:      return THREAD_PRIORITY_TIME_CRITICAL;
    case Thread::HighestPriority:       return THREAD_PRIORITY_HIGHEST;
    case Thread::AboveNormalPriority:   return THREAD_PRIORITY_ABOVE_NORMAL;
    case Thread::NormalPriority:        return THREAD_PRIORITY_NORMAL;
    case Thread::BelowNormalPriority:   return THREAD_PRIORITY_BELOW_NORMAL;
    case Thread::LowestPriority:        return THREAD_PRIORITY_LOWEST;
    case Thread::IdlePriority:          return THREAD_PRIORITY_IDLE;
    }
    return THREAD_PRIORITY_NORMAL;
}

// The actual first function called on thread start
unsigned WINAPI Thread_Win32StartFn(void * phandle)
{
    Thread *   pthread = (Thread*)phandle;
    if (pthread->Processor != -1)
    {
        DWORD_PTR ret = SetThreadAffinityMask(GetCurrentThread(), (DWORD)pthread->Processor);
        if (ret == 0)
            OVR_DEBUG_LOG(("Could not set hardware processor for the thread"));
    }
    BOOL ret = ::SetThreadPriority(GetCurrentThread(), Thread::GetOSPriority(pthread->Priority));
    if (ret == 0)
        OVR_DEBUG_LOG(("Could not set thread priority"));
    OVR_UNUSED(ret);

    // Ensure that ThreadId is assigned once thread is running, in case
    // beginthread hasn't filled it in yet.
    pthread->IdValue = (ThreadId)::GetCurrentThreadId();

    pthread->PRun();
    // Signal the thread as done and release it atomically.
    pthread->Finish();
    return 0;
}

bool Thread::Start(ThreadState initialState)
{
    if (initialState == NotRunning)
        return 0;
    if (GetThreadState() != NotRunning)
    {
        OVR_DEBUG_LOG(("Thread::Start failed - thread %p already running", this));
        return 0;
    }

    // Free old thread handle before creating the new one
    CleanupSystemThread();

    ExitCode        = NULL;
    ThreadFlags     = (initialState == Running) ? OVR_THREAD_STARTED : OVR_THREAD_START_SUSPENDED;
    ThreadHandle = (HANDLE) _beginthreadex(0, (unsigned)StackSize,
                                           Thread_Win32StartFn, this, 0, (unsigned*)&IdValue);

    // Failed? Fail the function
    if (ThreadHandle == 0)
    {
        ThreadFlags = 0;
        return 0;
    }
    return 1;
}

// Resumes currently suspended thread
bool Thread::Resume()
{
    // Can't suspend a thread that wasn't started
    if (!(ThreadFlags & OVR_THREAD_STARTED))
        return 0;

    if (::ResumeThread(ThreadHandle) != 0xFFFFFFFF)            
        return 1;            

    return 0;
}

bool Thread::Join()
{
    int r = WaitForSingleObject( ThreadHandle, INFINITE );
    return ( r != WAIT_FAILED );
}

void Thread::CleanupSystemThread()
{
    if (ThreadHandle != 0)
    {
        ::CloseHandle(ThreadHandle);
        ThreadHandle = 0;
    }
}

// static
int Thread::GetCPUCount()
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return (int) sysInfo.dwNumberOfProcessors;
}

// *** Sleep functions

// static
bool Thread::Sleep(unsigned secs)
{
    ::Sleep(secs*1000);
    return 1;
}

// static
bool Thread::MSleep(unsigned msecs)
{
    ::Sleep(msecs);
    return 1;
}

void Thread::SetThreadName( const char* name )
{
#if !defined(OVR_BUILD_SHIPPING) || defined(OVR_BUILD_PROFILING)
    // Looks ugly, but it is the recommended way to name a thread.
    typedef struct tagTHREADNAME_INFO {
        DWORD dwType;     // Must be 0x1000
        LPCSTR szName;    // Pointer to name (in user address space)
        DWORD dwThreadID; // Thread ID (-1 for caller thread)
        DWORD dwFlags;    // Reserved for future use; must be zero
    } THREADNAME_INFO;

    THREADNAME_INFO info;

    info.dwType = 0x1000;
    info.szName = name;
    info.dwThreadID = reinterpret_cast<DWORD>(GetThreadId());
    info.dwFlags = 0;

    __try
    {
#ifdef _WIN64
        RaiseException( 0x406D1388, 0, sizeof(info)/sizeof(DWORD), (const ULONG_PTR *)&info );
#else
        RaiseException( 0x406D1388, 0, sizeof(info)/sizeof(DWORD), (DWORD *)&info );
#endif
    }
    __except( GetExceptionCode()==0x406D1388 ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_EXECUTE_HANDLER )
    {
    }
#endif // OVR_BUILD_SHIPPING
}

// Returns the unique Id of a thread it is called on, intended for
// comparison purposes.
ThreadId GetCurrentThreadId()
{
    return (ThreadId)::GetCurrentThreadId();
}

bool ThreadRefCounted::Start(ThreadState initialState)
{
    bool started = Thread::Start( initialState );
    if ( started )
    {
        AddRef();
    }
    return started;
}

void ThreadRefCounted::Finish()
{
    Thread::Finish();
    Release();
}

} // namespace OVR

#endif	// OVR_ENABLE_THREADS
