/************************************************************************************

Filename    :   SoundPool.cpp
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "SoundPool.h"
#include "Android/JniUtils.h"

namespace OVR {

ovrSoundPool::ovrSoundPool( JNIEnv & jni_, jobject activity_ )
    : jni( jni_ )
{
#if defined( OVR_OS_ANDROID )
	jclass cls =
		ovr_GetLocalClassReference( &jni_, activity_, "com/oculus/sound/SoundPooler" );

	jmethodID ctor = ovr_GetMethodID( &jni_, cls, "<init>", "(Landroid/content/Context;)V" );
	jobject localPooler = jni_.NewObject( cls, ctor, activity_ );
	pooler = jni_.NewGlobalRef( localPooler );
	jni_.DeleteLocalRef( localPooler );

	playMethod = ovr_GetMethodID( &jni_, cls, "play", "(Ljava/lang/String;)V" );
	releaseMethod = ovr_GetMethodID( &jni, cls, "release", "()V" );
	jni_.DeleteLocalRef( cls );
#else
	OVR_UNUSED( activity_ );
#endif
}

ovrSoundPool::~ovrSoundPool()
{
#if defined( OVR_OS_ANDROID )
    jni.CallVoidMethod( pooler, releaseMethod );
    // TODO: check for exceptions? Maybe this should be in LibOVRKernel as a
    // call wrapper.
    jni.DeleteGlobalRef( pooler );
#endif
}

void ovrSoundPool::Play( JNIEnv & env, const char * soundName )
{
#if defined( OVR_OS_ANDROID )
    jstring cmdString = (jstring) ovr_NewStringUTF( &env, soundName );
    env.CallVoidMethod( pooler, playMethod, cmdString );
    env.DeleteLocalRef( cmdString );
#else
	OVR_UNUSED( &env );
	OVR_UNUSED( soundName );
#endif
}

}
