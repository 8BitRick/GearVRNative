/************************************************************************************

Filename    :   App.h
Content     :   Native counterpart to VrActivity
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_AppLocal_h
#define OVR_AppLocal_h

#include "App.h"
#include "GlSetup.h"
#include "PointTracker.h"
#include "VrFrameBuilder.h"
#include "Kernel/OVR_Threads.h"

namespace OVR {

//==============================================================
// AppLocal
//
// NOTE: do not define any of the functions in here inline (inside of the class 
// definition).  When AppLocal.h is included in multiple files (App.cpp and AppRender.cpp)
// this causes bugs with accessor functions.
class AppLocal : public App
{
public:
								AppLocal( JNIEnv & jni_, jobject activityObject_,
										VrAppInterface & interface_ );
	virtual						~AppLocal();

	virtual VrAppInterface *	GetAppInterface();

	virtual void				CreateToast( const char * fmt, ... );

	virtual void				RecenterYaw( const bool showBlack );
	virtual void				SetRecenterYawFrameStart( const long long frameNumber );
	virtual long long			GetRecenterYawFrameStart() const;

	virtual void				SendIntent( const char * actionName, const char * toPackageName,
									const char * toClassName, const char * command, const char * uri );

	virtual void				StartSystemActivity( const char * command );
	virtual void				FinishActivity( const ovrAppFinishType type );
	virtual void				FatalError( const ovrAppFatalError error, const char * fileName, const char * messageFormat, ... );
	virtual void				ShowDependencyError();

	//-----------------------------------------------------------------
	// interfaces
	//-----------------------------------------------------------------

	virtual BitmapFont &        	GetDebugFont();
	virtual BitmapFontSurface & 	GetDebugFontSurface();
	virtual OvrDebugLines &     	GetDebugLines();
	virtual const OvrStoragePaths & GetStoragePaths();
	virtual SurfaceTexture *		GetDialogTexture();

	//-----------------------------------------------------------------
	// system settings
	//-----------------------------------------------------------------

	virtual int					GetSystemProperty( const ovrSystemProperty propType );
	virtual const VrDeviceStatus & GetDeviceStatus() const;

	//-----------------------------------------------------------------
	// accessors
	//-----------------------------------------------------------------

	virtual	const ovrEyeBufferParms &	GetEyeBufferParms() const;
	virtual void						SetEyeBufferParms( const ovrEyeBufferParms & parms );

	virtual const ovrHeadModelParms &	GetHeadModelParms() const;
	virtual void						SetHeadModelParms( const ovrHeadModelParms & parms );

	virtual int							GetCpuLevel() const;
	virtual void						SetCpuLevel( const int cpuLevel );

	virtual int							GetGpuLevel() const;
	virtual void						SetGpuLevel( const int gpuLevel );

	virtual int							GetMinimumVsyncs() const;
	virtual void						SetMinimumVsyncs( const int mininumVsyncs );

	virtual	bool						GetFramebufferIsSrgb() const;
	virtual bool						GetFramebufferIsProtected() const;

	virtual	void						SetPopupDistance( float const d );
	virtual float						GetPopupDistance() const;

	virtual	void						SetPopupScale( float const s );
	virtual float						GetPopupScale() const;

	virtual Matrix4f const &			GetLastViewMatrix() const;
	virtual void						SetLastViewMatrix( Matrix4f const & m );

	virtual ovrMobile *					GetOvrMobile();
	virtual ovrFileSys &				GetFileSys();

	//-----------------------------------------------------------------
	// Localization
	//-----------------------------------------------------------------

	virtual const char *				GetPackageName() const;
	// Returns the path to an installed package in outPackagePath. If the call fails or
	// the outPackagePath buffer is too small to hold the package path, false is returned.
	virtual bool						GetInstalledPackagePath( char const * packageName, 
												char * outPackagePath, size_t const outMaxSize ) const;

	//-----------------------------------------------------------------
	// Java accessors
	//-----------------------------------------------------------------

	virtual const ovrJava *				GetJava() const;
	virtual jclass	&					GetVrActivityClass();

	//-----------------------------------------------------------------
	// TimeWarp
	//-----------------------------------------------------------------

	virtual void						DrawScreenMask( const ovrMatrix4f & mvp, const float fadeFracX, const float fadeFracY );
	virtual void						DrawScreenDirect( const ovrMatrix4f & mvp, const GLuint texid );

	//-----------------------------------------------------------------
	// debugging
	//-----------------------------------------------------------------

	virtual void						SetShowFPS( bool const show );
	virtual bool						GetShowFPS() const;

	virtual	void						ShowInfoText( float const duration, const char * fmt, ... );
	virtual	void						ShowInfoText( float const duration, Vector3f const & offset, Vector4f const & color, const char * fmt, ... );

	virtual void						RegisterConsoleFunction( char const * name, consoleFn_t function );

	// Public functions and variables used by native function calls from Java.
public:
	ovrMessageQueue &	GetMessageQueue();
	void				SetActivity( JNIEnv * jni, jobject activity );
	void				StartVrThread();
	void				StopVrThread();
	void				JoinVrThread();

	// Primary apps will exit(0) when they get an onDestroy() so we
	// never leave any cpu-sucking process running, but some apps
	// need to just return to the primary activity.
	volatile bool		ExitOnDestroy;
	volatile bool		OneTimeInitCalled;

	ANativeWindow *		pendingNativeWindow;

private:
	bool				VrThreadSynced;
	bool				CreatedSurface;
	bool				ReadyToExit;		// start exit procedure
	bool				Resumed;

	VrAppInterface *	appInterface;

	// Most calls from java should communicate with the VrThread through this message queue.
	ovrMessageQueue		MessageQueue;

	// gl setup information
	glSetup_t			glSetup;

	// window surface
	ANativeWindow *		nativeWindow;
	EGLSurface 			windowSurface;
	bool				FramebufferIsSrgb;			// requires KHR_gl_colorspace
	bool				FramebufferIsProtected;		// requires GPU trust zone extension

	// From vrapi_EnterVrMode, used for vrapi_SubmitFrame and vrapi_LeaveVrMode
	ovrMobile *			OvrMobile;

	// Handles creating, destroying, and re-configuring the buffers
	// for drawing the eye views, which might be in different texture
	// configurations for CPU warping, etc.
	ovrEyeBuffers *		EyeBuffers;

	ovrJava				Java;
	jclass				VrActivityClass;

	jmethodID			finishActivityMethodId;
	jmethodID			createVrToastMethodId;
	jmethodID			clearVrToastsMethodId;

	String				IntentURI;			// URI app was launched with
	String				IntentJSON;			// extra JSON data app was launched with
	String				IntentFromPackage;	// package that sent us the launch intent
	bool				IntentIsNew;

	String				packageName;		// package name 

	float 				popupDistance;
	float 				popupScale;

	// Every application gets a basic dialog surface.
	SurfaceTexture *	dialogTexture;
	int					dialogWidth;
	int					dialogHeight;
	float				dialogStopSeconds;
	Matrix4f			dialogMatrix;			// Dialogs will be oriented base down in the view when they were generated.

	Matrix4f			lastViewMatrix;

	bool				drawCalibrationLines;	// currently toggled by right trigger
	bool				calibrationLinesDrawn;	// after draw, go to static time warp test

	// Only render a single eye view, which will get warped for both
	// screen eyes.
	bool				renderMonoMode;

	float				SuggestedEyeFovDegreesX;
	float				SuggestedEyeFovDegreesY;

	ovrInputEvents		InputEvents;
	VrFrameBuilder		TheVrFrame;			// passed to VrAppInterface::Frame()

	ovrSettings			VrSettings;			// passed to VrAppInterface::Configure()
	ovrFrameParms		FrameParms;			// passed to vrapi_SubmitFrame()

	GlProgram			externalTextureProgram2;
	GlProgram			untexturedMvpProgram;
	GlProgram			untexturedScreenSpaceProgram;
	GlProgram			OverlayScreenFadeMaskProgram;
	GlProgram			OverlayScreenDirectProgram;

	GlGeometry			unitCubeLines;		// 12 lines that outline a 0 to 1 unit cube, intended to be scaled to cover bounds.
	GlGeometry			panelGeometry;		// used for dialogs
	GlGeometry			unitSquare;			// -1 to 1 in x and Y, 0 to 1 in texcoords
	GlGeometry			FadedScreenMaskSquare;// faded screen mask for overlay rendering

	EyePostRender		EyeDecorations;

	Thread				VrThread;			// thread

#if defined( OVR_OS_ANDROID )
	// For running java commands on another thread to
	// avoid hitches.
	TalkToJava			Ttj;
#endif

	bool				ShowFPS;			// true to show FPS on screen
	bool				WasMounted;			// true if the HMT was mounted on the previous frame
	bool				enableDebugOptions;	// enable debug key-commands for development testing
	
	String				InfoText;			// informative text to show in front of the view
	Vector4f			InfoTextColor;		// color of info text
	Vector3f			InfoTextOffset;		// offset from center of screen in view space
	long long			InfoTextEndFrame;	// time to stop showing text
	OvrPointTracker		InfoTextPointTracker;	// smoothly tracks to text ideal location
	OvrPointTracker		FPSPointTracker;		// smoothly tracks to ideal FPS text location

	long long 			recenterYawFrameStart;	// Enables reorient before sensor data is read.  Allows apps to reorient without having invalid orientation information for that frame.

	BitmapFont *		DebugFont;
	BitmapFontSurface *	DebugFontSurface;

	OvrDebugLines *		DebugLines;
	OvrStoragePaths *	StoragePaths;

	ovrTextureSwapChain *	LoadingIconTextureChain;
	ovrTextureSwapChain *	ErrorTextureSwapChain;
	int						ErrorTextureSize;
	double					ErrorMessageEndTime;

	ovrFileSys *		FileSys;

	//-----------------------------------------------------------------

	// Read a file from the apk zip file.  Free buffer with free() when done.
	// Files put in the eclipse res/raw directory will be found as "res/raw/<NAME>"
	// Files put in the eclipse assets directory will be found as "assets/<name>"
	// The filename comparison is case insensitive.
	void 				ReadFileFromApplicationPackage( const char * nameInZip, int &length, void * & buffer );

	//-----------------------------------------------------------------

	void				FrameworkInputProcessing( const VrInput & input );

	// One time init of GL objects.
	void				InitGlObjects();
	void				ShutdownGlObjects();

	void 				DrawEyeViews( Matrix4f const & viewMatrix );

	void				DrawDialog( const Matrix4f & mvp );
	void				DrawPanel( const GLuint externalTextureId, const Matrix4f & dialogMvp, const float alpha );

	void				DrawBounds( const Vector3f &mins, const Vector3f &maxs, const Matrix4f &mvp, const Vector3f &color );

	static threadReturn_t ThreadStarter( Thread *, void * parm );
	void 				VrThreadFunction();

	// Process commands forwarded from other threads.
	// Commands can be processed even when the window surfaces
	// are not setup.
	//
	// The msg string will be freed by the framework after
	// command processing.
	void    			Command( const char * msg );

	// Android Activity/Surface life cycle handling.
	void				CreateWindowSurface();
	void				DestroyWindowSurface();
	void				EnterVrMode();
	void				LeaveVrMode();
	void				HandleVrModeChanges();
	
	// TalkToJavaInterface
	virtual void		TtjCommand( JNIEnv & jni, const char * commandString );

	void				LatencyTests();

	void				BuildVrFrame();

	void				InitDebugFont();
	void				ShutdownDebugFont();
};

} // namespace OVR

#endif // OVR_AppLocal_h
