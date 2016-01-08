/************************************************************************************

PublicHeader:   OVR_Capture.h
Filename    :   OVR_Capture_Variable.h
Content     :   Label to Mutable Variable mapping... underlying datastructure for GetVariable()
Created     :   January, 2015
Notes       :
Author      :   Amanda M. Watson

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPTURE_VARIABLE_H
#define OVR_CAPTURE_VARIABLE_H

#include <stdlib.h>
#include <string.h>
#include <vector>
#include <OVR_Capture_Types.h>
#include "OVR_Capture_Thread.h"

namespace OVR
{
namespace Capture
{	
	typedef float var_t;
	typedef UInt32 key_t;

	class VarInfo
	{
		public:
			VarInfo(key_t key_, var_t valCur_, bool isClient_) : key(key_), valCur(valCur_),
			isClient(isClient_) 
			 {} 

			key_t key;
			// current value of variable
			var_t valCur; 
			// whether valCur is a reply
			bool isClient;
	};

	class VarStore
	{
		public:
			~VarStore()
			{
				Clear();	
			}

			enum
			{
				// current value of variable comes from remote client
				ClientValue = 0,
				// value of variable comes from device
				DeviceValue,
				// entry does not currently exist
				NoValue,
			};

			UInt32 Get(key_t key, var_t &var);
			void Set(key_t key, var_t var, bool isClient = false);
			void Clear();
		private:
			// does not lock, should be called by functions that do
			VarInfo *FindUnsafe(key_t key);
			// we consider writers to be those adding or removing from the
			// table, not those editing entries
			RWLock m_tableLock;
			std::vector<VarInfo> m_varTable; 
	};
}
}
#endif
