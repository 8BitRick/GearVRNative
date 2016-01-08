/************************************************************************************

Filename    :   App.cpp
Content     :   Native counterpart to VrActivity and VrApp
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "App.h"
#include "AppLocal.h"

#if defined( OVR_OS_WIN32 )

#include "Android/JniUtils.h"
#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_System.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_TypesafeNumber.h"
#include "Kernel/OVR_JSON.h"

#include "VrApi.h"
#include "SystemActivities.h"

namespace OVR
{
long long VrAppInterface::SetActivity( JNIEnv * jni, jclass clazz, jobject activity, jstring javaFromPackageNameString,
		jstring javaCommandString, jstring javaUriString )
{
	OVR_UNUSED( jni );
	OVR_UNUSED( clazz );
	OVR_UNUSED( activity );
	OVR_UNUSED( javaFromPackageNameString );
	OVR_UNUSED( javaCommandString );
	OVR_UNUSED( javaUriString );

	ovrJava java;
	const ovrInitParms initParms = vrapi_DefaultInitParms( &java );
	int32_t initResult = vrapi_Initialize( &initParms );
	if ( initResult != VRAPI_INITIALIZE_SUCCESS )
	{
		char const * msg = initResult == VRAPI_INITIALIZE_PERMISSIONS_ERROR ? 
										"Thread priority security exception. Make sure the APK is signed." :
										"VrApi initialization error.";
		SystemActivities_DisplayError( &java, SYSTEM_ACTIVITIES_FATAL_ERROR_OSIG, __FILE__, msg );
	}

	AppLocal * appLocal = static_cast< AppLocal * >( app );
	if ( appLocal == NULL )
	{
		appLocal = new AppLocal( *jni, activity, *this );
		app = appLocal;

		// Start the VrThread and wait for it to initialize.
		appLocal->StartVrThread();

		// TODO: Better way to map lifecycle
		appLocal->GetMessageQueue().SendPrintf( "resume " );
		appLocal->GetMessageQueue().SendPrintf( "surfaceCreated " );

		appLocal->JoinVrThread();

		delete appLocal;
	}

	vrapi_Shutdown();

	return 0;
}

void AppLocal::SetActivity( JNIEnv * jni, jobject activity )
{
	OVR_UNUSED( jni );
	OVR_UNUSED( activity );
}

void AppLocal::CreateWindowSurface()
{
	LOG( "AppLocal::CreateWindowSurface()" );

	const int displayPixelsWide = vrapi_GetSystemPropertyInt( &Java, VRAPI_SYS_PROP_DISPLAY_PIXELS_WIDE );
	const int displayPixelsHigh = vrapi_GetSystemPropertyInt( &Java, VRAPI_SYS_PROP_DISPLAY_PIXELS_HIGH );

	VrSettings.ShowLoadingIcon = true;
	VrSettings.RenderMonoMode = false;
	VrSettings.UseSrgbFramebuffer = false;
	VrSettings.UseProtectedFramebuffer = false;
	VrSettings.FramebufferPixelsWide = displayPixelsWide;
	VrSettings.FramebufferPixelsHigh = displayPixelsHigh;

	VrSettings.EyeBufferParms.resolutionWidth = vrapi_GetSystemPropertyInt( &Java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH );
	VrSettings.EyeBufferParms.resolutionHeight = vrapi_GetSystemPropertyInt( &Java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT );
	VrSettings.EyeBufferParms.multisamples = vrapi_GetSystemPropertyInt( &Java, VRAPI_SYS_PROP_MAX_FULLSPEED_FRAMEBUFFER_SAMPLES );
	VrSettings.EyeBufferParms.colorFormat = COLOR_8888;
	VrSettings.EyeBufferParms.depthFormat = DEPTH_24;

	VrSettings.ModeParms.Java = Java;

	// Allow the app to override any settings.
	appInterface->Configure( VrSettings );

	// TODO: Temp until we have final lifecycle mapping for PC.
	pendingNativeWindow = (ANativeWindow *)GetActiveWindow();
	nativeWindow = pendingNativeWindow;
	windowSurface = (EGLSurface)wglGetCurrentDC();

	CreatedSurface = true;
}

void AppLocal::DestroyWindowSurface()
{
}

void AppLocal::HandleVrModeChanges()
{
	// FIXME: Temp until we have final lifecycle mapping for PC.
	if ( nativeWindow == NULL && windowSurface == 0 )
	{
		CreateWindowSurface();
	}

	if ( Resumed != false && nativeWindow != NULL )
	{
		if ( OvrMobile == NULL )
		{
			EnterVrMode();
		}
	}
	else
	{
		if ( OvrMobile != NULL )
		{
			LeaveVrMode();
		}
	}

	//if ( nativeWindow == NULL && windowSurface != EGL_NO_SURFACE )
	//{
	//	DestroyWindowSurface();
	//}
}

// This callback happens from the java thread, after a string has been
// pulled off the message queue
void AppLocal::TtjCommand( JNIEnv & jni, const char * commandString )
{
	OVR_UNUSED( commandString );
}

void AppLocal::CreateToast( const char * fmt, ... )
{
	OVR_UNUSED( fmt );
}

} // OVR

#endif	// OVR_OS_WIN32
