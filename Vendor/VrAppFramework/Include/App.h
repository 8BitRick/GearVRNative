/************************************************************************************

Filename    :   App.h
Content     :   Native counterpart to VrActivity
                Applications that use this framework should NOT include any VrApi
                headers other than VrApi_Types.h and VrApi_Helpers.h included here.
                The framework should provide a complete superset of the VrApi.
                Avoid including this header file as much as possible in the
                framework, so individual components are not tied to the native
                application framework, and can be reused more easily by Unity
                or other hosting applications.
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_App_h
#define OVR_App_h

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_GlUtils.h"
#include "Kernel/OVR_LogUtils.h"
#include "VrApi_Types.h"
#include "VrApi_Helpers.h"
#include "GlProgram.h"
#include "GlTexture.h"
#include "GlGeometry.h"
#include "SurfaceTexture.h"
#include "EyeBuffers.h"
#include "EyePostRender.h"
#include "VrCommon.h"
#include "Input.h"
#include "TalkToJava.h"
#include "Console.h"

namespace OVR
{

//==============================================================
// forward declarations
class BitmapFont;
class BitmapFontSurface;
class OvrDebugLines;
class App;
class OvrStoragePaths;
class ovrLocale;
class ovrFileSys;

// Passed to an application to configure various VR settings.
struct ovrSettings
{
	bool				ShowLoadingIcon;
	bool				RenderMonoMode;
	bool				UseSrgbFramebuffer;			// EGL_GL_COLORSPACE_KHR,  EGL_GL_COLORSPACE_SRGB_KHR
	bool				UseProtectedFramebuffer;	// EGL_PROTECTED_CONTENT_EXT, EGL_TRUE
	int					FramebufferPixelsWide;
	int					FramebufferPixelsHigh;
	ovrModeParms		ModeParms;
	ovrPerformanceParms	PerformanceParms;
	ovrEyeBufferParms	EyeBufferParms;
	ovrHeadModelParms	HeadModelParms;
};

/*

VrAppInterface
==============

The VrAppInterface class implements the application life cycle. A class that implements
an application is derived from VrAppInterface. The application then receives life cycle
events by implementing the virtual functions from the VrAppInterface class. The virtual
functions of the VrAppInterface will be called by the application framework. An application
should never call these virtual functions itself.

All of the VrAppInterface virtual functions will be called from the VR application thread.
All functions except for VrAppInterface::Configure() and VrAppInterface::OneTimeShutdown()
will be called while the application is in VR mode. An applications that does not care about
a particular virtual function, is not required to implement it.

VrAppInterface life cycle
-------------------------

                                  <--------+
    1.  Configure(settings)                |
    2.  if (firstTime) OneTimeInit(intent) |
    3.  if (newIntent) NewIntent(intent)   |
    4.  EnteredVrMode()                    |
                                    <--+   |
    5.  while(keyEvent) OnKeyEvent()   |   |
    6.  Frame()                        |   |
    7.  DrawEyeView(left)              |   |
    8.  DrawEyeView(right)             |   |
                                    ---+   |
    9.  LeavingVrMode()                    |
                                  ---------+
    10. OneTimeShutdown()

*/
class VrAppInterface
{
public:
							VrAppInterface();
	virtual					~VrAppInterface();

	// Each onCreate in java will allocate a new java object.
	jlong SetActivity( JNIEnv * jni, jclass clazz, jobject activity, 
			jstring javaFromPackageNameString, jstring javaCommandString, 
			jstring javaUriString );

	// All subclasses communicate with App through this member.
	App *	app;				// Allocated in the first call to SetActivity()
	jclass	ActivityClass;		// global reference to clazz passed to SetActivity

	// This is called on each resume, before entering VR Mode, to allow
	// the application to make changes.
	virtual void Configure( ovrSettings & settings );

	// This will be called one time only, when first entering VR mode.
	//
	// It is called from the VR thread with an OpenGL context current.
	//
	// If the app was launched without a specific intent, launchIntent
	// will be an empty string.
	virtual void OneTimeInit( const char * intentFromPackage, const char * intentJSON, const char * intentURI );

	// This will be called one time only, when the app is about to shut down.
	//
	// It is called from the VR thread before the OpenGL context is destroyed.
	//
	// If the app needs to free OpenGL resources this is the place to do so.
	virtual void OneTimeShutdown();

	// If the app receives a new intent after launch, it will be sent to
	// this function.
	virtual void NewIntent( const char * intentFromPackage, const char * intentJSON, const char * intentURI );

	// This will be called right after entering VR mode.
	virtual void EnteredVrMode();

	// This will be called right before leaving VR mode.
	virtual void LeavingVrMode();

	// The app should return true if it consumes the key.
	virtual bool OnKeyEvent( const int keyCode, const int repeatCount, const KeyEventType eventType );

	// Frame will only be called if the window surfaces have been created.
	//
	// Any GPU operations that are relevant for both eye views, like
	// buffer updates or dynamic texture rendering, should be done first.
	//
	// Return the center view matrix the framework should use for positioning
	// new pop up dialogs.
	virtual Matrix4f Frame( const VrFrame & vrFrame );

	// The color buffer will have already been cleared or discarded, as
	// appropriate for the GPU. The viewport and scissor will already be set.
	//
	// 0 = left eye, 1 = right eye
	// fovDegrees may be different on different devices.
	// frameParms will be default initialized but can be modified as needed.
	//
	// Return the eye view-projection matrix for the framework to use for drawing the
	// pop up dialog and debug graphics.
	virtual Matrix4f DrawEyeView( const int eye, const float fovDegreesX, const float fovDegreesY, ovrFrameParms & frameParms );
};

//==============================================================
// App
class App : public TalkToJavaInterface
{
public:
	enum ovrAppFatalError
	{
		FATAL_ERROR_OUT_OF_MEMORY,
		FATAL_ERROR_OUT_OF_STORAGE,
		FATAL_ERROR_OSIG,
		FATAL_ERROR_MISC,
		FATAL_ERROR_MAX
	};

	enum ovrAppFinishType
	{
		FINISH_NORMAL,		// This will finish the current activity.
		FINISH_AFFINITY		// This will finish all activities on the stack.
	};

	virtual						~App();

	// When VrAppInterface::SetActivity is called, the App member is
	// allocated and hooked up to the VrAppInterface.
	virtual VrAppInterface *	GetAppInterface() = 0;

	// Request the java framework to draw a toast popup and
	// present it to the VR framework.  It may be several frames
	// before it is displayed.
	virtual void				CreateToast( const char * fmt, ... ) = 0;

	// Reorient was triggered - inform all menus etc.
	virtual void				RecenterYaw( const bool showBlack ) = 0;
	// Enables reorient before sensor data is read.
	// Allows apps to reorient without having invalid orientation information for that frame.
	virtual void				SetRecenterYawFrameStart( const long long frameNumber ) = 0;
	virtual long long			GetRecenterYawFrameStart() const = 0;

	// Send an intent to another activity.
	virtual void				SendIntent( const char * actionName, const char * toPackageName,
									const char * toClassName, const char * command, const char * uri ) = 0;

	// Switch to SystemActivities, return to Home or end activity.
	virtual void				StartSystemActivity( const char * command ) = 0;
	virtual void				FinishActivity( const ovrAppFinishType type ) = 0;
	virtual void				FatalError( const ovrAppFatalError error, const char * fileName, const char * fmt, ... ) = 0;
	virtual void				ShowDependencyError() = 0;

	//-----------------------------------------------------------------
	// interfaces
	//-----------------------------------------------------------------

	virtual BitmapFont &        	GetDebugFont() = 0;
	virtual BitmapFontSurface & 	GetDebugFontSurface() = 0;
	virtual OvrDebugLines &     	GetDebugLines() = 0;
	virtual const OvrStoragePaths &	GetStoragePaths() = 0;
	virtual SurfaceTexture *		GetDialogTexture() = 0;

	//-----------------------------------------------------------------
	// system settings
	//-----------------------------------------------------------------

	virtual int					GetSystemProperty( const ovrSystemProperty propType ) = 0;
	virtual const VrDeviceStatus & GetDeviceStatus() const = 0;

	//-----------------------------------------------------------------
	// accessors
	//-----------------------------------------------------------------

	// The parms will be initialized to sensible values on startup, and
	// can be freely changed at any time with minimal overhead.
	virtual	const ovrEyeBufferParms &	GetEyeBufferParms() const = 0;
	virtual void						SetEyeBufferParms( const ovrEyeBufferParms & parms ) = 0;

	virtual const ovrHeadModelParms &	GetHeadModelParms() const = 0;
	virtual void						SetHeadModelParms( const ovrHeadModelParms & parms ) = 0;

	virtual int							GetCpuLevel() const = 0;
	virtual void						SetCpuLevel( const int cpuLevel ) = 0;

	virtual int							GetGpuLevel() const = 0;
	virtual void						SetGpuLevel( const int gpuLevel ) = 0;

	virtual int							GetMinimumVsyncs() const = 0;
	virtual void						SetMinimumVsyncs( const int mininumVsyncs ) = 0;

	// The framebuffer may not be setup the way it was requested through VrAppInterface::Configure().
	virtual bool						GetFramebufferIsSrgb() const = 0;
	virtual bool						GetFramebufferIsProtected() const = 0;
	
	virtual	void						SetPopupDistance( float const d ) = 0;
	virtual float						GetPopupDistance() const = 0;

	virtual	void						SetPopupScale( float const s ) = 0;
	virtual float						GetPopupScale() const = 0;

	virtual Matrix4f const &			GetLastViewMatrix() const = 0;
	virtual void						SetLastViewMatrix( Matrix4f const & m ) = 0;

	// FIXME: remove
	virtual ovrMobile *					GetOvrMobile() = 0;
	virtual ovrFileSys &				GetFileSys() = 0;

	//-----------------------------------------------------------------
	// Localization
	//-----------------------------------------------------------------
	virtual const char *				GetPackageName() const = 0;
	// Returns the path to an installed package in outPackagePath. If the call fails or
	// the outPackagePath buffer is too small to hold the package path, false is returned.
	virtual bool						GetInstalledPackagePath( char const * packageName, 
												char * outPackagePath, size_t const outMaxSize ) const = 0;

	//-----------------------------------------------------------------
	// Java accessors
	//-----------------------------------------------------------------

	virtual const ovrJava *				GetJava() const = 0;
	virtual jclass	&					GetVrActivityClass() = 0;

	//-----------------------------------------------------------------
	// Overlay plane helper functions
	// FIXME: move these to separate static functions that are not dependent on App
	//-----------------------------------------------------------------

	// Draw a zero to destination alpha
	virtual void						DrawScreenMask( const ovrMatrix4f & mvp, const float fadeFracX, const float fadeFracY ) = 0;
	// Draw a screen to an eye buffer the same way it would be drawn as a time warp overlay.
	virtual void						DrawScreenDirect( const ovrMatrix4f & mvp, const GLuint texid ) = 0;

	//-----------------------------------------------------------------
	// debugging
	//-----------------------------------------------------------------

	virtual void						SetShowFPS( bool const show ) = 0;
	virtual bool						GetShowFPS() const = 0;

	virtual	void						ShowInfoText( float const duration, const char * fmt, ... ) = 0;
	virtual	void						ShowInfoText( float const duration, Vector3f const & offset, Vector4f const & color, const char * fmt, ... ) = 0;

	virtual void						RegisterConsoleFunction( char const * name, consoleFn_t function ) = 0;
};

}	// namespace OVR

#endif	// OVR_App
