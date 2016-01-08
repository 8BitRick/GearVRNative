/************************************************************************************

Filename    :   OVR_System.cpp
Content     :   General kernel initialization/cleanup, including that
                of the memory allocator.
Created     :   September 19, 2012
Notes       : 

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_System.h"
#include "OVR_Threads.h"

namespace OVR {

//-----------------------------------------------------------------------------
// System

// Initializes System core, installing allocator.
void System::Init(Log* log, Allocator *palloc)
{    
    if (!Allocator::GetInstance())
    {
        Log::SetGlobalLog(log);
        Allocator::setInstance(palloc);
    }
    else
    {
        OVR_DEBUG_LOG(("[System] Init failed - duplicate call."));
    }
}

void System::Destroy()
{    
    if (Allocator::GetInstance())
    {
        // Shutdown heap and destroy SysAlloc singleton, if any.
        Allocator::GetInstance()->onSystemShutdown();
        Allocator::setInstance(0);

        Log::SetGlobalLog(Log::GetDefaultLog());
    }
    else
    {
        OVR_DEBUG_LOG(("[System] Destroy failed - System not initialized."));
    }
}

// Returns 'true' if system was properly initialized.
bool System::IsInitialized()
{
    return Allocator::GetInstance() != 0;
}


} // namespace OVR
