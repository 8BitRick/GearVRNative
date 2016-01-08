/************************************************************************************

PublicHeader:   OVR_Capture.h
Filename    :   OVR_Capture_Variable.cpp
Content     :   Label to Mutable Variable mapping... underlying datastructure for GetVariable()
Created     :   January, 2015
Notes       :
Author      :   Amanda M. Watson

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_Capture_Variable.h"

namespace OVR
{
namespace Capture
{
	
	// does not check whether value is new
	void VarStore::Set(key_t key, var_t var, bool isClient)
	{
		m_tableLock.WriteLock();
		VarInfo *varRet = FindUnsafe(key);
		if (varRet == NULL)
		{
			m_varTable.push_back(VarInfo(key, var, isClient));
		}			
		else
		{
			OVR_CAPTURE_ASSERT(varRet->key == key);
			varRet->valCur = var;
			varRet->isClient = isClient;
		}
		OVR_CAPTURE_ASSERT(FindUnsafe(key) != NULL);
		m_tableLock.WriteUnlock();
	}

	UInt32 VarStore::Get(key_t key, var_t &var)
	{
		m_tableLock.ReadLock();
		VarInfo *varRet = FindUnsafe(key);	

		if (varRet == NULL)
		{
			m_tableLock.ReadUnlock();
			return NoValue;
		}

		var = varRet->valCur;
		m_tableLock.ReadUnlock();

		if (varRet->isClient)
		{
			return ClientValue;
		}

		return DeviceValue;
	}


	void VarStore::Clear()
	{
		m_tableLock.WriteLock();
		m_varTable.clear();
		m_tableLock.WriteUnlock();
	}

	VarInfo *VarStore::FindUnsafe(key_t key)
	{
		std::vector<VarInfo>::iterator it;
		for (it = m_varTable.begin(); it != m_varTable.end(); it++)
		{
			if (it->key == key)
			{
				return &(*it);				
			}			
		}
		return NULL;
	}

}
}
