/************************************************************************************

Filename    :   SoundEffectContext.cpp
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "Android/JniUtils.h"
#include "SoundEffectContext.h"
#include "VrCommon.h"

namespace OVR {

ovrSoundEffectContext::ovrSoundEffectContext( JNIEnv & jni_, jobject activity_ )
    : SoundPool( jni_, activity_ )
{
#if defined( OVR_OS_ANDROID )
	JavaVM * vm = NULL;
    jni_.GetJavaVM( &vm );
    Ttj.Init( *vm, *this );
#endif
}

ovrSoundEffectContext::~ovrSoundEffectContext()
{
}

void ovrSoundEffectContext::Initialize()
{
    // TODO: kick this off in the background in the constructor?
    SoundAssetMapping.LoadSoundAssets();
}

void ovrSoundEffectContext::Play( const char * name )
{
#if defined( OVR_OS_ANDROID )
    Ttj.GetMessageQueue().PostPrintf( "sound %s", name );
#else
	OVR_UNUSED( name );
#endif
}

void ovrSoundEffectContext::PlayInternal( JNIEnv & env, const char * name )
{
	// Get sound from the asset mapping
	String soundFile;

    if ( SoundAssetMapping.GetSound( name, soundFile ) )
    {
		SoundPool.Play( env, soundFile.ToCStr() );
	}
	else
	{
		WARN( "ovrSoundEffectContext::Play called with non-asset-mapping-defined sound: %s", name );
		SoundPool.Play( env, name );
	}
}

void ovrSoundEffectContext::TtjCommand( JNIEnv & jni, const char * commandString )
{
	if ( MatchesHead( "sound ", commandString ) )
	{
        PlayInternal( jni, commandString + 6 );
		return;
	}
}

}
