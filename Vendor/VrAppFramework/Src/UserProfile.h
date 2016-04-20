/************************************************************************************

Filename    :   UserProfile.h
Content     :   Container for user profile data.
Created     :   November 10, 2014
Authors     :   Caleb Leak

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/
#ifndef OVR_UserProfile_h
#define OVR_UserProfile_h

#include "VrApi_Types.h"

namespace OVR
{

//==============================================================
// UserProfile
// 
class UserProfile
{
public:
	UserProfile();

	ovrHeadModelParms	HeadModelParms;
};

UserProfile		LoadProfile();
void			SaveProfile( const UserProfile & profile );

}	// namespace OVR

#endif	// OVR_UserProfile_h
