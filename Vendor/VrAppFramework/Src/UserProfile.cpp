/************************************************************************************

Filename    :   UserProfile.cpp
Content     :   Container for user profile data.
Created     :   November 10, 2014
Authors     :   Caleb Leak

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "UserProfile.h"
#include "Kernel/OVR_JSON.h"
#include "Kernel/OVR_LogUtils.h"
#include "VrApi_Helpers.h"

static const char* PROFILE_PATH = "/sdcard/Oculus/userprofile.json";

namespace OVR
{

UserProfile::UserProfile()
{
	HeadModelParms = vrapi_DefaultHeadModelParms();
}

UserProfile LoadProfile()
{
	// TODO: Switch this over to using a content provider when available.

	Ptr<JSON> root = *JSON::Load( PROFILE_PATH );
	UserProfile profile;

	if ( !root )
	{
		WARN("Failed to load user profile \"%s\". Using defaults.", PROFILE_PATH);
	}
	else
	{
		profile.HeadModelParms.InterpupillaryDistance = root->GetItemByName( "ipd" )->GetFloatValue();
		profile.HeadModelParms.EyeHeight = root->GetItemByName( "eyeHeight" )->GetFloatValue();
		profile.HeadModelParms.HeadModelHeight = root->GetItemByName( "headModelHeight" )->GetFloatValue();
		profile.HeadModelParms.HeadModelDepth = root->GetItemByName( "headModelDepth" )->GetFloatValue();
	}

	return profile;
}

void SaveProfile(const UserProfile& profile)
{
	Ptr<JSON> root = *JSON::CreateObject();

	root->AddNumberItem( "ipd", profile.HeadModelParms.InterpupillaryDistance );
	root->AddNumberItem( "eyeHeight", profile.HeadModelParms.EyeHeight );
	root->AddNumberItem( "headModelHeight", profile.HeadModelParms.HeadModelHeight );
	root->AddNumberItem( "headModelDepth", profile.HeadModelParms.HeadModelDepth );

	if ( root->Save( PROFILE_PATH ) )
	{
		WARN( "Failed to save user profile %s", PROFILE_PATH );
	}
}

}	// namespace OVR
