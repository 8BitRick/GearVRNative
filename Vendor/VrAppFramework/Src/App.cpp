/************************************************************************************

Filename    :   App.cpp
Content     :   Native counterpart to VrActivity and VrApp
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "App.h"

#if defined( OVR_OS_ANDROID )
#include <jni.h>
#include <android/native_window_jni.h>	// for native window JNI
#include <unistd.h>						// gettid(), etc
#endif

#include <math.h>

#include "Kernel/OVR_System.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_TypesafeNumber.h"
#include "Kernel/OVR_JSON.h"
#include "Android/JniUtils.h"

#include "stb_image.h"
#include "stb_image_write.h"

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_LocalPrefs.h"

#include "GlSetup.h"
#include "GlTexture.h"
#include "VrCommon.h"
#include "AppLocal.h"
#include "DebugLines.h"
#include "BitmapFont.h"
#include "PackageFiles.h"
#include "UserProfile.h"
#include "Console.h"
#include "OVR_Uri.h"
#include "OVR_FileSys.h"
#include "Input.h"

#include "SystemActivities.h"

#include "embedded/dependency_error_de.h"
#include "embedded/dependency_error_en.h"
#include "embedded/dependency_error_es.h"
#include "embedded/dependency_error_fr.h"
#include "embedded/dependency_error_it.h"
#include "embedded/dependency_error_ja.h"
#include "embedded/dependency_error_ko.h"

// some parameters from the intent can be empty strings, which cannot be represented as empty strings for sscanf
// so we encode them as EMPTY_INTENT_STR.
// Because the message queue handling uses sscanf() to parse the message, the JSON text is 
// always placed at the end of the message because it can contain spaces while the package
// name and URI cannot. The handler will use sscanf() to parse the first two strings, then
// assume the JSON text is everything immediately following the space after the URI string.
static const char * EMPTY_INTENT_STR = "<EMPTY>";
void ComposeIntentMessage( char const * packageName, char const * uri, char const * jsonText, 
		char * out, size_t outSize )
{
	OVR::OVR_sprintf( out, outSize, "intent %s %s %s",
			packageName == NULL || packageName[0] == '\0' ? EMPTY_INTENT_STR : packageName,
			uri == NULL || uri[0] == '\0' ? EMPTY_INTENT_STR : uri,
			jsonText == NULL || jsonText[0] == '\0' ? "" : jsonText );
}

// Initialize and shutdown the app framework version of LibOVR.
static struct InitShutdown
{
	InitShutdown()
	{
		OVR::System::Init( OVR::Log::ConfigureDefaultLog( OVR::LogMask_All ) );
	}
	~InitShutdown()
	{
		OVR::System::Destroy();
	}
} GlobalInitShutdown;

namespace OVR
{

//=======================================================================================
// Default handlers for VrAppInterface

VrAppInterface::VrAppInterface() :
	app( NULL ),
	ActivityClass( NULL )
{
}

VrAppInterface::~VrAppInterface()
{
	if ( ActivityClass != NULL )
	{
		// FIXME:
		//jni->DeleteGlobalRef( ActivityClass );
		//ActivityClass = NULL;
	}
}

void VrAppInterface::Configure( ovrSettings & settings )
{ 
	LOG( "VrAppInterface::Configure - default handler called" );
	OVR_UNUSED( settings );
}

void VrAppInterface::OneTimeInit( const char * intentFromPackage, const char * intentJSON, const char * intentURI )
{
	ASSERT_WITH_TAG( !"OneTimeInit() is not overloaded!", "VrApp" );	// every native app must overload this. Why isn't it pure virtual?
	OVR_UNUSED( intentFromPackage );
	OVR_UNUSED( intentJSON );
	OVR_UNUSED( intentURI );
}

void VrAppInterface::OneTimeShutdown()
{
	LOG( "VrAppInterface::OneTimeShutdown - default handler called" );
}

void VrAppInterface::NewIntent( const char * intentFromPackage, const char * intentJSON, const char * intentURI )
{
	LOG( "VrAppInterface::NewIntent - default handler called - %s %s %s", intentFromPackage, intentJSON, intentURI );
	OVR_UNUSED( intentFromPackage );
	OVR_UNUSED( intentJSON );
	OVR_UNUSED( intentURI );
}

void VrAppInterface::EnteredVrMode()
{
	LOG( "VrAppInterface::EnteredVrMode - default handler called" );
}

void VrAppInterface::LeavingVrMode()
{
	LOG( "VrAppInterface::LeavingVrMode - default handler called" );
}

bool VrAppInterface::OnKeyEvent( const int keyCode, const int repeatCount, const KeyEventType eventType )
{ 
	LOG( "VrAppInterface::OnKeyEvent - default handler called" );
	OVR_UNUSED( keyCode );
	OVR_UNUSED( repeatCount );
	OVR_UNUSED( eventType );
	return false; 
}

Matrix4f VrAppInterface::Frame( const VrFrame & vrFrame )
{
	LOG( "VrAppInterface::Frame - default handler called" );
	OVR_UNUSED( vrFrame );
	return Matrix4f();
}

Matrix4f VrAppInterface::DrawEyeView( const int eye, const float fovDegreesX, const float fovDegreesY, ovrFrameParms & frameParms ) 
{ 
	LOG( "VrAppInterface::DrawEyeView - default handler called" );
	OVR_UNUSED( eye );
	OVR_UNUSED( fovDegreesX );
	OVR_UNUSED( fovDegreesY );
	OVR_UNUSED( frameParms );
	return Matrix4f(); 
}


//==============================
// WaitForDebuggerToAttach
//
// wait on the debugger... once it is attached, change waitForDebugger to false
void WaitForDebuggerToAttach()
{
	static volatile bool waitForDebugger = true;
	while ( waitForDebugger )
	{
		// put your breakpoint on the sleep to wait
		Thread::MSleep( 100 );
	}
}

//=======================================================================================

extern void DebugMenuBounds( void * appPtr, const char * cmd );
extern void DebugMenuHierarchy( void * appPtr, const char * cmd );
extern void DebugMenuPoses( void * appPtr, const char * cmd );

void ShowFPS( void * appPtr, const char * cmd )
{
	int show = 0;
	sscanf( cmd, "%i", &show );
	OVR_ASSERT( appPtr != NULL );	// something changed / broke in the OvrConsole code if this is NULL
	( ( App* )appPtr )->SetShowFPS( show != 0 );
}

App::~App()
{
	// avoids "undefined reference to 'vtable for OVR::App'" error
}

/*
 * AppLocal
 *
 * Called once at startup.
 *
 * ?still true?: exit() from here causes an immediate app re-launch,
 * move everything to first surface init?
 */
AppLocal::AppLocal( JNIEnv & jni_, jobject activityObject_, VrAppInterface & interface_ ) :
			ExitOnDestroy( true ),
			OneTimeInitCalled( false ),
			pendingNativeWindow( NULL ),
			VrThreadSynced( false ),
			CreatedSurface( false ),
			ReadyToExit( false ),
			Resumed( false ),
			appInterface( NULL ),
			MessageQueue( 100 ),
			nativeWindow( NULL ),
			windowSurface( EGL_NO_SURFACE ),
			FramebufferIsSrgb( false ),
			FramebufferIsProtected( false ),
			OvrMobile( NULL ),
			EyeBuffers( NULL ),
			VrActivityClass( NULL ),
			finishActivityMethodId( NULL ),
			createVrToastMethodId( NULL ),
			clearVrToastsMethodId( NULL ),
			IntentURI(),
			IntentJSON(),
			IntentFromPackage(),
			IntentIsNew( false ),
			packageName( "" ),
			popupDistance( 2.0f ),
			popupScale( 1.0f ),
			dialogTexture( NULL ),
			dialogWidth( 0 ),
			dialogHeight( 0 ),
			dialogStopSeconds( 0.0f ),
			drawCalibrationLines( false ),
			calibrationLinesDrawn( false ),
			renderMonoMode( false ),
			SuggestedEyeFovDegreesX( 90.0f ),
			SuggestedEyeFovDegreesY( 90.0f ),
			InputEvents(),
			TheVrFrame(),
			VrThread( &ThreadStarter, this ),
			ShowFPS( false ),
			WasMounted( false ),	
			enableDebugOptions( false ),			
			InfoTextColor( 1.0f ),
			InfoTextOffset( 0.0f ),
			InfoTextEndFrame( -1 ),
			recenterYawFrameStart( 0 ),
			DebugFont( NULL ),
			DebugFontSurface( NULL ),
			DebugLines( NULL ),
			StoragePaths( NULL ),
			LoadingIconTextureChain( 0 ),
			ErrorTextureSwapChain( NULL ),
			ErrorTextureSize( 0 ),
			ErrorMessageEndTime( -1.0 )
{
	LOG( "----------------- AppLocal::AppLocal() -----------------");

	// Set the VrAppInterface
	appInterface = &interface_;

	StoragePaths = new OvrStoragePaths( &jni_, activityObject_);

	//WaitForDebuggerToAttach();

#if defined( OVR_OS_ANDROID )
	jni_.GetJavaVM( &Java.Vm );
	Java.Env = NULL;	// set from the VrThread
	Java.ActivityObject = jni_.NewGlobalRef( activityObject_ );

	VrActivityClass = ovr_GetGlobalClassReference( &jni_, activityObject_, "com/oculus/vrappframework/VrActivity" );

	finishActivityMethodId = ovr_GetMethodID( &jni_, VrActivityClass, "finishActivity", "()V" );
	createVrToastMethodId = ovr_GetMethodID( &jni_, VrActivityClass, "createVrToastOnUiThread", "(Ljava/lang/String;)V" );
	clearVrToastsMethodId = ovr_GetMethodID( &jni_, VrActivityClass, "clearVrToasts", "()V" );

	const jmethodID isHybridAppMethodId = ovr_GetStaticMethodID( &jni_, VrActivityClass, "isHybridApp", "(Landroid/app/Activity;)Z" );
	bool const isHybridApp = jni_.CallStaticBooleanMethod( VrActivityClass, isHybridAppMethodId, Java.ActivityObject );
#else
	Java.Vm = NULL;
	Java.Env = NULL;
	Java.ActivityObject = NULL;
	bool const isHybridApp = false;
#endif

	ExitOnDestroy = !isHybridApp;

	// Default time warp parms
	FrameParms = vrapi_DefaultFrameParms( &Java, VRAPI_FRAME_INIT_DEFAULT, vrapi_GetTimeInSeconds(), NULL );

	// Default display settings.
	VrSettings.ShowLoadingIcon = true;
	VrSettings.RenderMonoMode = false;
	VrSettings.UseSrgbFramebuffer = false;
	VrSettings.UseProtectedFramebuffer = false;
	VrSettings.FramebufferPixelsWide = 2560;
	VrSettings.FramebufferPixelsHigh = 1440;

	// Default ovrModeParms
	VrSettings.ModeParms = vrapi_DefaultModeParms( &Java );
	VrSettings.ModeParms.AllowPowerSave = true;
	VrSettings.ModeParms.ResetWindowFullscreen = true;	// Must reset the FLAG_FULLSCREEN window flag when using a SurfaceView

	// Default ovrPerformanceParms
	VrSettings.PerformanceParms = vrapi_DefaultPerformanceParms();
	VrSettings.PerformanceParms.CpuLevel = 2;
	VrSettings.PerformanceParms.GpuLevel = 2;

	// Default ovrEyeBufferParms (will be overwritten later based on hmdInfo)
	VrSettings.EyeBufferParms.resolutionWidth = 1024;
	VrSettings.EyeBufferParms.resolutionHeight = 1024;
	VrSettings.EyeBufferParms.multisamples = 4;
	VrSettings.EyeBufferParms.colorFormat = COLOR_8888;
	VrSettings.EyeBufferParms.depthFormat = DEPTH_24;

	// Load user profile data relevant to rendering
	const UserProfile profile = LoadProfile();
	VrSettings.HeadModelParms = profile.HeadModelParms;

#if defined( OVR_OS_ANDROID )
	// Get the path to the .apk and package name
	{
		char temp[1024];
		ovr_GetCurrentPackageName( &jni_, Java.ActivityObject, temp, sizeof( temp ) );
		packageName = temp;

		ovr_GetPackageCodePath( &jni_, Java.ActivityObject, temp, sizeof( temp ) );

		String	outPath;
		const bool validCacheDir = StoragePaths->GetPathIfValidPermission(
				EST_INTERNAL_STORAGE, EFT_CACHE, "", permissionFlags_t( PERMISSION_WRITE ) | PERMISSION_READ, outPath );
		ovr_OpenApplicationPackage( temp, validCacheDir ? outPath.ToCStr() : NULL );
	}
#endif
}

AppLocal::~AppLocal()
{
	LOG( "---------- ~AppLocal() ----------" );

	delete StoragePaths;
}

void AppLocal::StartVrThread()
{
	LOG( "StartVrThread" );

	if ( VrThread.Start() == false )
	{
		FAIL( "VrThread.Start() failed" );
	}

	// Wait for the thread to be up and running.
	MessageQueue.SendPrintf( "sync " );
}

void AppLocal::StopVrThread()
{
	LOG( "StopVrThread" );

	MessageQueue.PostPrintf( "quit " );

	if ( VrThread.Join() == false )
	{
		WARN( "VrThread failed to terminate." );
	}
}

void AppLocal::JoinVrThread()
{
	LOG( "JoinVrThread" );

	VrThread.Join();
}

void AppLocal::InitDebugFont()
{
	// Load the debug font and create the font surface for it
	// We always load EFIGS as the default font, strictly for purposes of debugging. EFIGS keeps 
	// growing, however, so it might make since to just have a separate English-only font for this.
	DebugFont = BitmapFont::Create();

	if ( !DebugFont->Load( GetFileSys(), "apk://font/res/raw/efigs.fnt" ) )	// load from language apk
	{
		if ( !DebugFont->Load( GetFileSys(), "apk:///res/raw/efigs.fnt" ) )	// load from application apk
		{
			WARN( "Failed to load debug font!" );
		}
	}

	DebugFontSurface = BitmapFontSurface::Create();
	DebugFontSurface->Init( 8192 );
}

void AppLocal::ShutdownDebugFont()
{
	BitmapFont::Free( DebugFont );
	BitmapFontSurface::Free( DebugFontSurface );
}

ovrMessageQueue & AppLocal::GetMessageQueue()
{
	return MessageQueue;
}

void AppLocal::SendIntent( const char * actionName, const char * toPackageName,
							const char * toClassName, const char * command, const char * uri )
{
	// Push black images to the screen to eliminate any frames of lost head tracking.
	ovrFrameParms blackFrameParms = vrapi_DefaultFrameParms( &Java, VRAPI_FRAME_INIT_BLACK_FINAL, vrapi_GetTimeInSeconds(), NULL );
	blackFrameParms.FrameIndex = TheVrFrame.Get().FrameNumber;
	vrapi_SubmitFrame( OvrMobile, &blackFrameParms );

	SystemActivities_SendIntent( &Java, actionName, toPackageName, toClassName, command, uri );
}

struct embeddedImage_t
{
	char const *	ImageName;
	void *			ImageBuffer;
	size_t			ImageSize;
};

// for each error type, add an array of errorImage_t with an entry for each language
static const embeddedImage_t EmbeddedImages[] =
{
	{ "dependency_error_en.png",		dependencyErrorEnData,		dependencyErrorEnSize },
	{ "dependency_error_de.png",		dependencyErrorDeData,		dependencyErrorDeSize },
	{ "dependency_error_en-rGB.png",	dependencyErrorEnData,		dependencyErrorEnSize },
	{ "dependency_error_es.png",		dependencyErrorEsData,		dependencyErrorEsSize },
	{ "dependency_error_es-rES.png",	dependencyErrorEsData,		dependencyErrorEsSize },
	{ "dependency_error_fr.png",		dependencyErrorFrData,		dependencyErrorFrSize },
	{ "dependency_error_it.png",		dependencyErrorItData,		dependencyErrorItSize },
	{ "dependency_error_ja.png",		dependencyErrorJaData,		dependencyErrorJaSize },
	{ "dependency_error_ko.png",		dependencyErrorKoData,		dependencyErrorKoSize },
	{ NULL, NULL, 0 }
};

static embeddedImage_t const * FindErrorImage( embeddedImage_t const * list, char const * name )
{
	for ( int i = 0; list[i].ImageName != NULL; ++i )
	{
		if ( OVR::OVR_stricmp( list[i].ImageName, name ) == 0 )
		{
			LOG( "Found embedded image for '%s'", name );
			return &list[i];
		}
	}

	return NULL;
}

static bool FindEmbeddedImage( char const * imageName, void ** buffer, int * bufferSize )
{
	*buffer = NULL;
	*bufferSize = 0;

	embeddedImage_t const * image = FindErrorImage( EmbeddedImages, imageName );
	if ( image == NULL ) 
	{
		WARN( "No embedded image named '%s' was found!", imageName );
		return false;
	}

	*buffer = image->ImageBuffer;
	*bufferSize = image->ImageSize;
	return true;
}

void AppLocal::ShowDependencyError()
{
#if defined( OVR_OS_ANDROID )
	// clear any pending exception to ensure no pending exception causes the error message to fail
	if ( Java.Env->ExceptionOccurred() )
	{
		Java.Env->ExceptionClear();
	}
#endif

	/// Android specific
	OVR::String imageName = "dependency_error_";

	// call into Java directly here to get the language code
	char const * localeCode = "en";
#if defined( OVR_OS_ANDROID )
	JavaClass javaLocaleClass( Java.Env, ovr_GetLocalClassReference( Java.Env, Java.ActivityObject, "java/util/Locale" ) );
	jmethodID getDefaultId = ovr_GetStaticMethodID( Java.Env, javaLocaleClass.GetJClass(), "getDefault", "()Ljava/util/Locale;" );
	if ( getDefaultId != NULL )
	{
		JavaObject javaDefaultLocaleObject( Java.Env, Java.Env->CallStaticObjectMethod( javaLocaleClass.GetJClass(),getDefaultId ) );
		if ( javaDefaultLocaleObject.GetJObject() != NULL )
		{
			jmethodID getLocaleId = Java.Env->GetMethodID( javaLocaleClass.GetJClass(), "getLanguage", "()Ljava/lang/String;" );
			if ( getLocaleId != NULL )
			{
				JavaUTFChars javaLocale( Java.Env, (jstring)Java.Env->CallObjectMethod( javaDefaultLocaleObject.GetJObject(), getLocaleId ) );
				if ( javaLocale.GetJString() != NULL )
				{
					imageName += javaLocale.ToStr();
					localeCode = NULL;
				}
			}
		}
	}
#endif

	if ( localeCode != NULL )
	{
		imageName += localeCode;
	}
	imageName += ".png";

	void * imageBuffer = NULL;
	int imageSize = 0;
	if ( !FindEmbeddedImage( imageName.ToCStr(), &imageBuffer, &imageSize ) )
	{
		// try to default to English
		imageName = "dependency_error_en.png";
		if ( !FindEmbeddedImage( imageName.ToCStr(), &imageBuffer, &imageSize ) )
		{
			FAIL( "Failed to load error message texture!" );
		}
	}

	// Load the error texture from a buffer.
	{
		int width = 0;
		int height = 0;
		int comp = 0;
		stbi_uc * image = stbi_load_from_memory( (unsigned char *)imageBuffer, imageSize, &width, &height, &comp, 4 );

		OVR_ASSERT( image != NULL );
		if ( image != NULL )
		{
			OVR_ASSERT( width == height );

			// Only 1 mip level needed.
			ErrorTextureSwapChain = vrapi_CreateTextureSwapChain( VRAPI_TEXTURE_TYPE_2D, VRAPI_TEXTURE_FORMAT_8888, width, height, 1, false );
			ErrorTextureSize = width;

			glBindTexture( GL_TEXTURE_2D, vrapi_GetTextureSwapChainHandle( ErrorTextureSwapChain, 0 ) );
			glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, image );
			glBindTexture( GL_TEXTURE_2D, 0 );

			free( image );
		}
	}

	ErrorMessageEndTime = vrapi_GetTimeInSeconds() + 15.0f;
}

void AppLocal::StartSystemActivity( const char * command )
{
	if ( SystemActivities_StartSystemActivity( &Java, command, NULL ) )
	{
		// Push black images to the screen to eliminate any frames of lost head tracking.
		ovrFrameParms blackFrameParms = vrapi_DefaultFrameParms( &Java, VRAPI_FRAME_INIT_BLACK_FINAL, vrapi_GetTimeInSeconds(), NULL );
		blackFrameParms.FrameIndex = TheVrFrame.Get().FrameNumber;
		vrapi_SubmitFrame( OvrMobile, &blackFrameParms );
		return;
	}
	if ( ErrorTextureSwapChain != NULL )
	{
		// already in an error state
		return;
	}

	LOG( "*************************************************************************" );
	LOG( "A fatal dependency error occured. Oculus SystemActivities failed to start.");
	LOG( "*************************************************************************" );
	SystemActivities_ReturnToHome( &Java );
}

void AppLocal::FinishActivity( const ovrAppFinishType type )
{
	// Push black images to the screen to eliminate any frames of lost head tracking.
	ovrFrameParms blackFrameParms = vrapi_DefaultFrameParms( &Java, VRAPI_FRAME_INIT_BLACK_FINAL, vrapi_GetTimeInSeconds(), NULL );
	blackFrameParms.FrameIndex = TheVrFrame.Get().FrameNumber;
	vrapi_SubmitFrame( OvrMobile, &blackFrameParms );

#if defined( OVR_OS_ANDROID )
	if ( type == FINISH_NORMAL )
	{
		const jmethodID mid = ovr_GetStaticMethodID( Java.Env, VrActivityClass,
				"finishOnUiThread", "(Landroid/app/Activity;)V" );

		if ( Java.Env->ExceptionOccurred() )
		{
			Java.Env->ExceptionClear();
			LOG( "Cleared JNI exception" );
		}
		LOG( "Calling activity.finishOnUiThread()" );
		Java.Env->CallStaticVoidMethod( VrActivityClass, mid, Java.ActivityObject );
		LOG( "Returned from activity.finishOnUiThread()" );
	}
	else if ( type == FINISH_AFFINITY )
	{
		const char * name = "finishAffinityOnUiThread";
		const jmethodID mid = ovr_GetStaticMethodID( Java.Env, VrActivityClass,
				name, "(Landroid/app/Activity;)V" );

		if ( Java.Env->ExceptionOccurred() )
		{
			Java.Env->ExceptionClear();
			LOG( "Cleared JNI exception" );
		}
		LOG( "Calling activity.finishAffinityOnUiThread()" );
		Java.Env->CallStaticVoidMethod( VrActivityClass, mid, Java.ActivityObject );
		LOG( "Returned from activity.finishAffinityOnUiThread()" );
	}
#endif
}

void AppLocal::FatalError( const ovrAppFatalError appError, const char * fileName, const char * messageFormat, ... )
{
	char errorMessage[2048];
	va_list argPtr;
	va_start( argPtr, messageFormat );
	OVR::OVR_vsprintf( errorMessage, sizeof( errorMessage ), messageFormat, argPtr );
	va_end( argPtr );

	// map the app fatal error to the SA fatal error
	ovrSystemActivitiesFatalError appErrorToFatalErrorMap[FATAL_ERROR_MAX] = {
		SYSTEM_ACTIVITIES_FATAL_ERROR_OUT_OF_MEMORY,
		SYSTEM_ACTIVITIES_FATAL_ERROR_OUT_OF_STORAGE,
		SYSTEM_ACTIVITIES_FATAL_ERROR_OSIG,
		SYSTEM_ACTIVITIES_FATAL_ERROR_MISC
	};
	// open SA with a fatal error message
	SystemActivities_DisplayError( &Java, appErrorToFatalErrorMap[appError], fileName, errorMessage );
}

void AppLocal::ReadFileFromApplicationPackage( const char * nameInZip, int &length, void * & buffer )
{
	ovr_ReadFileFromApplicationPackage( nameInZip, length, buffer );
}

static const char* vertexShaderSource =
		"uniform mat4 Mvpm;\n"
		"uniform mat4 Texm;\n"
		"attribute vec4 Position;\n"
		"attribute vec4 VertexColor;\n"
		"attribute vec2 TexCoord;\n"
		"uniform mediump vec4 UniformColor;\n"
		"varying  highp vec2 oTexCoord;\n"
		"varying  lowp vec4 oColor;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   oTexCoord = vec2( Texm * vec4(TexCoord,1,1) );\n"
		"   oColor = VertexColor * UniformColor;\n"
		"}\n";

/*
 * InitGlObjects
 *
 * Call once a GL context is created, either by us or a host engine.
 * The Java VM must be attached to this thread to allow SurfaceTexture
 * creation.
 */
void AppLocal::InitGlObjects()
{
	// Let glUtils look up extensions
	GL_InitExtensions();

	externalTextureProgram2 = BuildProgram( vertexShaderSource, externalFragmentShaderSource );
	untexturedMvpProgram = BuildProgram(
			"uniform mat4 Mvpm;\n"
			"attribute vec4 Position;\n"
			"uniform mediump vec4 UniformColor;\n"
			"varying  lowp vec4 oColor;\n"
			"void main()\n"
			"{\n"
				"   gl_Position = Mvpm * Position;\n"
				"   oColor = UniformColor;\n"
			"}\n"
		,
			"varying lowp vec4	oColor;\n"
			"void main()\n"
			"{\n"
			"	gl_FragColor = oColor;\n"
			"}\n"
		);
	untexturedScreenSpaceProgram = BuildProgram( identityVertexShaderSource, untexturedFragmentShaderSource );
	OverlayScreenFadeMaskProgram = BuildProgram(
			"uniform mat4 Mvpm;\n"
			"attribute vec4 VertexColor;\n"
			"attribute vec4 Position;\n"
			"varying  lowp vec4 oColor;\n"
			"void main()\n"
			"{\n"
			"   gl_Position = Mvpm * Position;\n"
			"   oColor = vec4( 1.0, 1.0, 1.0, 1.0 - VertexColor.x );\n"
			"}\n"
		,
			"varying lowp vec4	oColor;\n"
			"void main()\n"
			"{\n"
			"	gl_FragColor = oColor;\n"
			"}\n"
		);
	OverlayScreenDirectProgram = BuildProgram(
			"uniform mat4 Mvpm;\n"
			"attribute vec4 Position;\n"
			"attribute vec2 TexCoord;\n"
			"varying  highp vec2 oTexCoord;\n"
			"void main()\n"
			"{\n"
			"   gl_Position = Mvpm * Position;\n"
			"   oTexCoord = TexCoord;\n"
			"}\n"
		,
			"uniform sampler2D Texture0;\n"
			"varying highp vec2 oTexCoord;\n"
			"void main()\n"
			"{\n"
			"	gl_FragColor = texture2D( Texture0, oTexCoord );\n"
			"}\n"
		);

	// Build some geometries we need
	panelGeometry = BuildTesselatedQuad( 32, 16 );	// must be large to get faded edge
	unitSquare = BuildTesselatedQuad( 1, 1 );
	unitCubeLines = BuildUnitCubeLines();
	//FadedScreenMaskSquare = BuildFadedScreenMask( 0.0f, 0.0f );	// TODO: clean up: app-specific values are being passed in on DrawScreenMask

	EyeDecorations.Init();
}

void AppLocal::ShutdownGlObjects()
{
	DeleteProgram( externalTextureProgram2 );
	DeleteProgram( untexturedMvpProgram );
	DeleteProgram( untexturedScreenSpaceProgram );
	DeleteProgram( OverlayScreenFadeMaskProgram );
	DeleteProgram( OverlayScreenDirectProgram );

	panelGeometry.Free();
	unitSquare.Free();
	unitCubeLines.Free();
	FadedScreenMaskSquare.Free();

	EyeDecorations.Shutdown();
}

#if 0
static void ToggleScreenColor()
{
	static int	color;

	color ^= 1;

	glEnable( GL_WRITEONLY_RENDERING_QCOM );
	glClearColor( color, 1-color, 0, 1 );
	glClear( GL_COLOR_BUFFER_BIT );

	// The Adreno driver has an unfortunate optimization so it doesn't
	// actually flush if all that was done was a clear.
	GL_Finish();
	glDisable( GL_WRITEONLY_RENDERING_QCOM );
}
#endif

void AppLocal::LatencyTests()
{
#if 0
	// Joypad latency test
	// When this is enabled, each tap of a button will toggle the screen
	// color, allowing the tap-to-photons latency to be measured.
	if ( 0 )
	{
		if ( TheVrFrame.Get().Input.buttonPressed )
		{
			LOG( "Input.buttonPressed" );
			static bool shut;
			if ( !shut )
			{
				// shut down timewarp, then go back into frontbuffer mode
				shut = true;
				vrapi_LeaveVrMode( OvrMobile );
				static DirectRender	dr;
				dr.InitForCurrentSurface( Java.Env, true, 19 );
			}
			ToggleScreenColor();
		}
		MessageQueue.SleepUntilMessage();
		continue;
	}
	// IMU latency test
	if ( 0 )
	{
		static bool buttonDown;
		const double predictedTime = vrapi_GetPredictedDisplayTime( OvrMobile, FrameParms.MinimumVsyncs, 2 );
		const ovrTracking tracking = vrapi_GetPredictedTracking( OvrMobile, predictedTime );
		const float acc = Vector3f( tracking.HeadPose.AngularVelocity ).Length();
		//const float acc = fabs( Vector3f( tracking.HeadPose.LinearAcceleration ).Length() - 9.8f );

		static int count;
		if ( ++count % 10 == 0 )
		{
			LOG( "acc: %f", acc );
		}
		const bool buttonNow = ( acc > 0.1f );
		if ( buttonNow != buttonDown )
		{
			LOG( "accel button" );
			buttonDown = buttonNow;
			static bool shut;
			if ( !shut )
			{
				// shut down timewarp, then go back into frontbuffer mode
				shut = true;
				vrapi_LeaveVrMode( OvrMobile );
				static DirectRender	dr;
				dr.InitForCurrentSurface( Java.Env, true, 19 );
			}
			ToggleScreenColor();
		}
		usleep( 1000 );
		continue;
	}
#endif
}

Vector3f ViewOrigin( const Matrix4f & view )
{
	return Vector3f( view.M[0][3], view.M[1][3], view.M[2][3] );
}

Vector3f ViewForward( const Matrix4f & view )
{
	return Vector3f( -view.M[0][2], -view.M[1][2], -view.M[2][2] );
}

Vector3f ViewUp( const Matrix4f & view )
{
	return Vector3f( view.M[0][1], view.M[1][1], view.M[2][1] );
}

Vector3f ViewRight( const Matrix4f & view )
{
	return Vector3f( view.M[0][0], view.M[1][0], view.M[2][0] );
}

void AppLocal::EnterVrMode()
{
	LOG( "AppLocal::EnterVrMode()" );

	// Check for local preference values that effect app framework behavior
	const char * enableDebugOptionsStr = ovr_GetLocalPreferenceValueForKey( LOCAL_PREF_APP_DEBUG_OPTIONS, "0" );
	enableDebugOptions = ( atoi( enableDebugOptionsStr ) > 0 );

	// Initialize the eye buffers.
	EyeBuffers->Initialize( VrSettings.EyeBufferParms );

	// Enter VR mode.
	OvrMobile = vrapi_EnterVrMode( &VrSettings.ModeParms );

	// Now that we are in VR mode, release the UI thread before doing a potentially long load.
	MessageQueue.NotifyMessageProcessed();

	// Update network state that doesn't need to be updated every frame.
	TheVrFrame.UpdateNetworkState( Java.Env, VrActivityClass, Java.ActivityObject );

	// Let the client app initialize only once by calling OneTimeInit().
	// This is called after entering VR mode to be able to show a time warp loading icon.

	if ( !OneTimeInitCalled || IntentIsNew )
	{
		if ( VrSettings.ShowLoadingIcon )
		{
			ovrFrameParms loadingIconFrameParms = vrapi_DefaultFrameParms( &Java, VRAPI_FRAME_INIT_LOADING_ICON_FLUSH, vrapi_GetTimeInSeconds(), LoadingIconTextureChain );
			loadingIconFrameParms.FrameIndex = TheVrFrame.Get().FrameNumber;
			vrapi_SubmitFrame( OvrMobile, &loadingIconFrameParms );
		}
	}

	if ( !OneTimeInitCalled )
	{
		LOG( "VrAppInterface::OneTimeInit()" );
		LOG( "IntentFromPackage: %s", IntentFromPackage.ToCStr() );
		LOG( "IntentJSON: %s", IntentJSON.ToCStr() );
		LOG( "IntentURI: %s", IntentURI.ToCStr() );

		appInterface->OneTimeInit( IntentFromPackage.ToCStr(), IntentJSON.ToCStr(), IntentURI.ToCStr() );
		OneTimeInitCalled = true;
	}
	else
	{
		// if this is a resume after we've already OneTimeInit-ialized, then automatically reorient
		//LOG( "EnterVrMode - Reorienting" );
		SetRecenterYawFrameStart( TheVrFrame.Get().FrameNumber + 1 );
	}

	// Let the client app know about any new intent.
	if ( IntentIsNew )
	{
		LOG( "VrAppInterface::NewIntent()" );
		LOG( "IntentFromPackage: %s", IntentFromPackage.ToCStr() );
		LOG( "IntentJSON: %s", IntentJSON.ToCStr() );
		LOG( "IntentURI: %s", IntentURI.ToCStr() );

		appInterface->NewIntent( IntentFromPackage.ToCStr(), IntentJSON.ToCStr(), IntentURI.ToCStr() );
		IntentIsNew = false;
	}

	// Notify the application that we are in VR mode.
	appInterface->EnteredVrMode();
}

void AppLocal::LeaveVrMode()
{
	LOG( "AppLocal::LeaveVrMode()" );

	// Notify the application that we are about to leave VR mode.
	appInterface->LeavingVrMode();

	// Push black images to the screen so that we don't see last front buffer image without any head tracking
	ovrFrameParms blackFrameParms = vrapi_DefaultFrameParms( &Java, VRAPI_FRAME_INIT_BLACK_FINAL, vrapi_GetTimeInSeconds(), NULL );
	blackFrameParms.FrameIndex = TheVrFrame.Get().FrameNumber;
	vrapi_SubmitFrame( OvrMobile, &blackFrameParms );

	vrapi_LeaveVrMode( OvrMobile );
	OvrMobile = NULL;
}

// Always make the panel upright, even if the head was tilted when created
Matrix4f PanelMatrix( const Matrix4f & lastViewMatrix, const float popupDistance,
		const float popupScale, const int width, const int height )
{
	// TODO: this won't be valid until a frame has been rendered
	const Matrix4f invView = lastViewMatrix.Inverted();
	const Vector3f forward = ViewForward( invView );
	const Vector3f levelforward = Vector3f( forward.x, 0.0f, forward.z ).Normalized();
	// TODO: check degenerate case
	const Vector3f up( 0.0f, 1.0f, 0.0f );
	const Vector3f right = levelforward.Cross( up );

	const Vector3f center = ViewOrigin( invView ) + levelforward * popupDistance;
	const float xScale = (float)width / 768.0f * popupScale;
	const float yScale = (float)height / 768.0f * popupScale;
	const Matrix4f panelMatrix = Matrix4f(
			xScale * right.x, yScale * up.x, forward.x, center.x,
			xScale * right.y, yScale * up.y, forward.y, center.y,
			xScale * right.z, yScale * up.z, forward.z, center.z,
			0, 0, 0, 1 );

//	LOG( "PanelMatrix center: %f %f %f", center.x, center.y, center.z );
//	LogMatrix( "PanelMatrix", panelMatrix );

	return panelMatrix;
}

/*
 * Command
 *
 * Process commands sent over the message queue for the VR thread.
 *
 */
void AppLocal::Command( const char *msg )
{
	// Always include the space in MatchesHead to prevent problems
	// with commands that have matching prefixes.

	if ( MatchesHead( "sync ", msg ) )
	{
		LOG( "%p msg: VrThreadSynced", this );
		VrThreadSynced = true;
		return;
	}

	if ( MatchesHead( "surfaceCreated ", msg ) )
	{
		LOG( "%p msg: surfaceCreated", this );
		nativeWindow = pendingNativeWindow;
		HandleVrModeChanges();
		return;
	}

	if ( MatchesHead( "surfaceDestroyed ", msg ) )
	{
		LOG( "%p msg: surfaceDestroyed", this );
		nativeWindow = NULL;
		HandleVrModeChanges();
		return;
	}

	if ( MatchesHead( "resume ", msg ) )
	{
		LOG( "%p msg: resume", this );
		Resumed = true;
		HandleVrModeChanges();
		return;
	}

	if ( MatchesHead( "pause ", msg ) )
	{
		LOG( "%p msg: pause", this );
		Resumed = false;
		HandleVrModeChanges();
		return;
	}

	if ( MatchesHead( "joy ", msg ) )
	{
		sscanf( msg, "joy %f %f %f %f",
				&InputEvents.JoySticks[0][0],
				&InputEvents.JoySticks[0][1],
				&InputEvents.JoySticks[1][0],
				&InputEvents.JoySticks[1][1] );
		return;
	}

	if ( MatchesHead( "touch ", msg ) )
	{
		sscanf( msg, "touch %i %f %f",
				&InputEvents.TouchAction,
				&InputEvents.TouchPosition[0],
				&InputEvents.TouchPosition[1] );
		return;
	}

	if ( MatchesHead( "key ", msg ) )
	{
		int keyCode, down, repeatCount;
		sscanf( msg, "key %i %i %i", &keyCode, &down, &repeatCount );
		if ( InputEvents.NumKeyEvents < MAX_INPUT_KEY_EVENTS )
		{
			InputEvents.KeyEvents[InputEvents.NumKeyEvents].KeyCode = static_cast< ovrKeyCode >( keyCode & ~BUTTON_JOYPAD_FLAG );
			InputEvents.KeyEvents[InputEvents.NumKeyEvents].RepeatCount = repeatCount;
			InputEvents.KeyEvents[InputEvents.NumKeyEvents].Down = ( down != 0 );
			InputEvents.KeyEvents[InputEvents.NumKeyEvents].IsJoypadButton = ( keyCode & BUTTON_JOYPAD_FLAG ) != 0;
			InputEvents.NumKeyEvents++;
		}
		return;	
	}

	if ( MatchesHead( "intent ", msg ) )
	{
		LOG( "%p msg: intent", this );

		// define the buffer sizes with macros so we can ensure that the sscanf sizes are also updated
		// if the actual buffer sizes are changed.
#define FROM_SIZE 511
#define URI_SIZE 1023

		char fromPackageName[FROM_SIZE + 1];
		char uri[URI_SIZE + 1];
		// since the package name and URI cannot contain spaces, but JSON can,
		// the JSON string is at the end and will come after the third space.
		sscanf( msg, "intent %" STRINGIZE_VALUE( FROM_SIZE ) "s %" STRINGIZE_VALUE( URI_SIZE ) "s", fromPackageName, uri );
		char const * jsonStart = NULL;
		size_t msgLen = OVR_strlen( msg );
		int spaceCount = 0;
		for ( size_t i = 0; i < msgLen; ++i ) {
			if ( msg[i] == ' ' ) {
				spaceCount++;
				if ( spaceCount == 3 ) {
					jsonStart = &msg[i+1];
					break;
				}
			}
		}

		if ( OVR_strcmp( fromPackageName, EMPTY_INTENT_STR ) == 0 )
		{
			fromPackageName[0] = '\0';
		}
		if ( OVR_strcmp( uri, EMPTY_INTENT_STR ) == 0 )
		{
			uri[0] = '\0';
		}

		// assign launchIntent to the intent command
		IntentFromPackage = fromPackageName;
		IntentJSON = jsonStart;
		IntentURI = uri;
		IntentIsNew = true;

		return;
	}

	if ( MatchesHead( "popup ", msg ) )
	{
#if defined( OVR_OS_ANDROID )
		int width, height;
		float seconds;
		sscanf( msg, "popup %i %i %f", &width, &height, &seconds );

		dialogWidth = width;
		dialogHeight = height;
		dialogStopSeconds = vrapi_GetTimeInSeconds() + seconds;

		dialogMatrix = PanelMatrix( lastViewMatrix, popupDistance, popupScale, width, height );

		glActiveTexture( GL_TEXTURE0 );
		LOG( "RC_UPDATE_POPUP dialogTexture %i", dialogTexture->GetTextureId() );
		dialogTexture->Update();
		glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );
#endif
		return;
	}

	if ( MatchesHead( "quit ", msg ) )
	{
		// "quit" is called fron onDestroy and onPause should have been called already
		OVR_ASSERT( OvrMobile == NULL );
		ReadyToExit = true;
		LOG( "VrThreadSynced=%d CreatedSurface=%d ReadyToExit=%d", VrThreadSynced, CreatedSurface, ReadyToExit );
		return;
	}
}

void AppLocal::FrameworkInputProcessing( const VrInput & input )
{
	// Process key events.
	for ( int i = 0; i < input.NumKeyEvents; i++ )
	{
		const int keyCode = input.KeyEvents[i].KeyCode;
		const int repeatCount = input.KeyEvents[i].RepeatCount;
		const KeyEventType eventType = input.KeyEvents[i].EventType;

#if 0	/// FIXME: OvrGazeCursor is no longer part of the VrAppFramework so this can not be done here any longer
		// The back key is special because it has to handle short-press, long-press and double-tap.
		if ( keyCode == OVR_KEY_BACK )
		{
			// If this is not system activities.
			if ( !ovr_IsCurrentActivity( Java.Env, Java.ActivityObject, PUI_CLASS_NAME ) )
			{
				// Update the gaze cursor timer.
				if ( eventType == KEY_EVENT_DOWN )
				{
					GetGazeCursor().StartTimer( BACK_BUTTON_LONG_PRESS_TIME_IN_SECONDS, BACK_BUTTON_DOUBLE_TAP_TIME_IN_SECONDS );
				} 
				else if ( eventType == KEY_EVENT_DOUBLE_TAP || eventType == KEY_EVENT_SHORT_PRESS )
				{
					GetGazeCursor().CancelTimer();
				}
				else if ( eventType == KEY_EVENT_LONG_PRESS )
				{
					GetGazeCursor().CancelTimer();
					StartSystemActivity( PUI_GLOBAL_MENU );
					continue;
				}
			}
		}

		// The app menu is always the first consumer so it cannot be usurped.
		if ( GetGuiSys().OnKeyEvent( keyCode, repeatCount, eventType ) )
		{
			continue;
		}
#endif

		// For all other keys, allow VrAppInterface to handle and consume the key first.
		// VrAppInterface needs to get the key first and have a chance to consume it completely
		// because keys are context sensitive and only the app knows the current context the key
		// should apply to.
		if ( appInterface->OnKeyEvent( keyCode, repeatCount, eventType ) )
		{
			continue;
		}

		// If nothing consumed the key and it's a short-press of the back key, then exit the application to OculusHome.
		if ( keyCode == OVR_KEY_BACK )
		{
			if ( eventType == KEY_EVENT_SHORT_PRESS )
			{
				StartSystemActivity( PUI_CONFIRM_QUIT );
				continue;
			}
		}

		// Handle debug key actions.
		if ( enableDebugOptions )
		{
			if ( eventType == KEY_EVENT_DOWN && repeatCount == 0 )	// first down only
			{
				float const IPD_STEP = 0.001f;

				if ( keyCode == OVR_KEY_S )
				{
					EyeBuffers->ScreenShot();
					CreateToast( "screenshot" );
					continue;
				}
				else if ( keyCode == OVR_KEY_F )
				{
					SetShowFPS( !GetShowFPS() );
					continue;
				}
				else if ( keyCode == OVR_KEY_COMMA )
				{
					float const IPD_MIN_CM = 0.0f;
					VrSettings.HeadModelParms.InterpupillaryDistance = Alg::Max( IPD_MIN_CM * 0.01f, VrSettings.HeadModelParms.InterpupillaryDistance - IPD_STEP );
					ShowInfoText( 1.0f, "%.3f", VrSettings.HeadModelParms.InterpupillaryDistance );
					continue;
				}
				else if ( keyCode == OVR_KEY_PERIOD )
				{
					float const IPD_MAX_CM = 8.0f;
					VrSettings.HeadModelParms.InterpupillaryDistance = Alg::Min( IPD_MAX_CM * 0.01f, VrSettings.HeadModelParms.InterpupillaryDistance + IPD_STEP );
					ShowInfoText( 1.0f, "%.3f", VrSettings.HeadModelParms.InterpupillaryDistance );
					continue;
				}
			}
		}
	}

	// Process button presses.
	bool const rightTrigger = ( input.buttonState & BUTTON_RIGHT_TRIGGER ) != 0;
	bool const leftTrigger = ( input.buttonState & BUTTON_LEFT_TRIGGER ) != 0;

	if ( leftTrigger && rightTrigger && ( input.buttonPressed & BUTTON_START ) != 0 )
	{
		time_t rawTime;
		time( &rawTime );
		struct tm * timeInfo = localtime( &rawTime );
		char timeStr[128];
		strftime( timeStr, sizeof( timeStr ), "%H:%M:%S", timeInfo );
		LOG_WITH_TAG( "QAEvent", "%s (%.3f) - QA event occurred", timeStr, vrapi_GetTimeInSeconds() );
	}

	// Display tweak testing, only when holding right trigger
	if ( enableDebugOptions && rightTrigger )
	{
#if defined( OVR_OS_ANDROID )
		if ( input.buttonPressed & BUTTON_DPAD_RIGHT )
		{
			jclass vmDebugClass = Java.Env->FindClass( "dalvik/system/VMDebug" );
			jmethodID dumpId = Java.Env->GetStaticMethodID( vmDebugClass, "dumpReferenceTables", "()V" );
			Java.Env->CallStaticVoidMethod( vmDebugClass, dumpId );
			Java.Env->DeleteLocalRef( vmDebugClass );
		}
#endif
	}
}

/*
 * VrThreadFunction
 *
 * Continuously renders frames when active, checking for commands
 * from the main thread between frames.
 */
void AppLocal::VrThreadFunction()
{
	// Set the name that will show up in systrace
	VrThread.SetThreadName( "OVR::VrThread" );

	// Initialize the VR thread
	{
		LOG( "AppLocal::VrThreadFunction - init" );

		// The Java VM needs to be attached on each thread that will use
		// it.  We need it to call UpdateTexture on surfaceTextures, which
		// must be done on the thread with the openGL context that created
		// the associated texture object current.
		ovr_AttachCurrentThread( Java.Vm, &Java.Env, NULL );

		// this must come after ovr_AttachCurrentThread so that Java is valid.
		FileSys = ovrFileSys::Create( *GetJava() );

		VrSettings.ModeParms.Java = Java;

		FrameParms.PerformanceParms = VrSettings.PerformanceParms;
#if defined( OVR_OS_ANDROID )
		FrameParms.PerformanceParms.MainThreadTid = gettid();
#else
		FrameParms.PerformanceParms.MainThreadTid = 0;
#endif
		FrameParms.PerformanceParms.RenderThreadTid = 0;
		FrameParms.Java = Java;

#if defined( OVR_OS_ANDROID )
		// Set up another thread for making longer-running java calls
		// to avoid hitches.
		Ttj.Init( *Java.Vm, *this );
#endif

		InitInput();

		SuggestedEyeFovDegreesX = vrapi_GetSystemPropertyFloat( &Java, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_X );
		SuggestedEyeFovDegreesY = vrapi_GetSystemPropertyFloat( &Java, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_Y );

#if defined( OVR_OS_ANDROID )
		// Create a new context and pbuffer surface
		const int windowDepth = 0;
		const int windowSamples = 0;
		const GLuint contextPriority = EGL_CONTEXT_PRIORITY_MEDIUM_IMG;
		glSetup = GL_Setup( EGL_NO_CONTEXT, GL_ES_VERSION,	// no share context,
				8,8,8, windowDepth, windowSamples, // r g b
				contextPriority );
#else
		const int displayPixelsWide = vrapi_GetSystemPropertyInt( &Java, VRAPI_SYS_PROP_DISPLAY_PIXELS_WIDE );
		const int displayPixelsHigh = vrapi_GetSystemPropertyInt( &Java, VRAPI_SYS_PROP_DISPLAY_PIXELS_HIGH );
		glSetup = GL_Setup( displayPixelsWide / 2, displayPixelsHigh / 2, false, this );
#endif

		// Create our GL data objects
		InitGlObjects();

		EyeBuffers = new ovrEyeBuffers;
		DebugLines = OvrDebugLines::Create();

		void * 	imageBuffer;
		int		imageSize;
		ovr_ReadFileFromApplicationPackage( "res/raw/loading_indicator.png", imageSize, imageBuffer );
		if ( imageBuffer != NULL )
		{
			int width = 0;
			int height = 0;
			int comp = 0;
			stbi_uc * image = stbi_load_from_memory( (unsigned char *)imageBuffer, imageSize, &width, &height, &comp, 4 );

			OVR_ASSERT( image != NULL );
			if ( image != NULL )
			{
				OVR_ASSERT( width == height );

				// Only 1 mip level needed.
				LoadingIconTextureChain = vrapi_CreateTextureSwapChain( VRAPI_TEXTURE_TYPE_2D, VRAPI_TEXTURE_FORMAT_8888, width, height, 1, false );

				glBindTexture( GL_TEXTURE_2D, vrapi_GetTextureSwapChainHandle( LoadingIconTextureChain, 0 ) );
				glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, image );
				glBindTexture( GL_TEXTURE_2D, 0 );

				free( image );
			}
		}

		// Create the SurfaceTexture for dialog rendering.
		dialogTexture = new SurfaceTexture( Java.Env );

		InitDebugFont();

		GetDebugLines().Init();

		SystemActivities_Init( &Java );

#if defined( OVR_OS_ANDROID )
		// Register the headset receiver.
		{
			JavaClass javaHeadsetReceiverClass( Java.Env, ovr_GetLocalClassReference( Java.Env, Java.ActivityObject, "com/oculus/vrappframework/HeadsetReceiver" ) );
			const jmethodID startReceiverMethodId = ovr_GetStaticMethodID( Java.Env, javaHeadsetReceiverClass.GetJClass(), "startReceiver", "(Landroid/app/Activity;)V" );
			if ( startReceiverMethodId != NULL )
			{
				Java.Env->CallStaticVoidMethod( javaHeadsetReceiverClass.GetJClass(), startReceiverMethodId, Java.ActivityObject );
			}
		}
#endif

		// Init the adb 'console' and register console functions
		InitConsole( Java );
		RegisterConsoleFunction( "print", OVR::DebugPrint );
		RegisterConsoleFunction( "showFPS", OVR::ShowFPS );		
	}

	while( !( VrThreadSynced && CreatedSurface && ReadyToExit ) )
	{
		//SPAM( "FRAME START" );

		// Process incoming messages until the queue is empty.
		for ( ; ; )
		{
			const char * msg = MessageQueue.GetNextMessage();
			if ( msg == NULL )
			{
				break;
			}
			Command( msg );
			free( (void *)msg );
		}

		// process any SA events - events that aren't always handled internally ( returnToLauncher )
		// will be added to the event list
		SystemActivitiesAppEventList_t appEvents;
		SystemActivities_Update( OvrMobile, &Java, &appEvents );

		// Wait for messages until we are in VR mode.
		if ( OvrMobile == NULL )
		{
			// Don't wait if the exit conditions are satisfied.
			if ( !( VrThreadSynced && CreatedSurface && ReadyToExit ) )
			{
				MessageQueue.SleepUntilMessage();
			}
			continue;
		}

		// if there is an error condition, warp swap and nothing else
		if ( ErrorTextureSwapChain != NULL )
		{
			if ( vrapi_GetTimeInSeconds() >= ErrorMessageEndTime )
			{
				// Push black images to the screen to eliminate any frames of lost head tracking.
				ovrFrameParms blackFrameParms = vrapi_DefaultFrameParms( &Java, VRAPI_FRAME_INIT_BLACK_FINAL, vrapi_GetTimeInSeconds(), NULL );
				blackFrameParms.FrameIndex = TheVrFrame.Get().FrameNumber;
				vrapi_SubmitFrame( OvrMobile, &blackFrameParms );

				SystemActivities_ReturnToHome( &Java );
			} 
			else 
			{
				ovrFrameParms warpSwapMessageParms = vrapi_DefaultFrameParms( &Java, VRAPI_FRAME_INIT_MESSAGE, vrapi_GetTimeInSeconds(), ErrorTextureSwapChain );
				warpSwapMessageParms.FrameIndex = TheVrFrame.Get().FrameNumber;
				warpSwapMessageParms.Layers[1].ProgramParms[0] = 0.0f;							// rotation in radians
				warpSwapMessageParms.Layers[1].ProgramParms[1] = 1024.0f / ErrorTextureSize;	// message size factor
				vrapi_SubmitFrame( OvrMobile, &warpSwapMessageParms );
			}
			continue;
		}

		bool recenter = recenterYawFrameStart == ( TheVrFrame.Get().FrameNumber + 1 );
		if ( !WasMounted && TheVrFrame.Get().DeviceStatus.HeadsetIsMounted )
		{
			recenter = true;	//  We just mounted so push a reorient event to be handled at the app level (if desired)
		}
		WasMounted = TheVrFrame.Get().DeviceStatus.HeadsetIsMounted;

		if ( recenter )
		{
			// add a reorient message so we pass it down as an event that VrGUI (or the app) can get it and call ResetMenuOrientations()
			char reorientMessage[1024];
			SystemActivities_CreateSystemActivitiesCommand( "", SYSTEM_ACTIVITY_EVENT_REORIENT, "", "", reorientMessage, sizeof( reorientMessage ) );
			SystemActivities_AppendAppEvent( &appEvents, reorientMessage );
		}

		// Update VrFrame.
		TheVrFrame.AdvanceVrFrame( InputEvents, OvrMobile, FrameParms, VrSettings.HeadModelParms, &appEvents );
		InputEvents.NumKeyEvents = 0;

		// Resend any debug lines that have expired.
		GetDebugLines().BeginFrame( TheVrFrame.Get().FrameNumber );

		// Process input.
		FrameworkInputProcessing( TheVrFrame.Get().Input );

		// Process Window Events.
		if ( GL_ProcessEvents() )
		{
			break;
		}

		LatencyTests();

		if ( ShowFPS )
		{
			const int FPS_NUM_FRAMES_TO_AVERAGE = 30;
			static double  LastFrameTime = vrapi_GetTimeInSeconds();
			static double  AccumulatedFrameInterval = 0.0;
			static int   NumAccumulatedFrames = 0;
			static float LastFrameRate = 60.0f;

			double currentFrameTime = vrapi_GetTimeInSeconds();
			double frameInterval = currentFrameTime - LastFrameTime;
			AccumulatedFrameInterval += frameInterval;
			NumAccumulatedFrames++;
			if ( NumAccumulatedFrames > FPS_NUM_FRAMES_TO_AVERAGE ) {
				double interval = ( AccumulatedFrameInterval / NumAccumulatedFrames );  // averaged
				AccumulatedFrameInterval = 0.0;
				NumAccumulatedFrames = 0;
				LastFrameRate = 1.0f / float( interval > 0.000001 ? interval : 0.00001 );
			}    

			Vector3f viewPos = GetViewMatrixPosition( lastViewMatrix );
			Vector3f viewFwd = GetViewMatrixForward( lastViewMatrix );
			Vector3f newPos = viewPos + viewFwd * 1.5f;
			FPSPointTracker.Update( vrapi_GetTimeInSeconds(), newPos );

			fontParms_t fp;
			fp.AlignHoriz = HORIZONTAL_CENTER;
			fp.Billboard = true;
			fp.TrackRoll = false;
			DebugFontSurface->DrawTextBillboarded3Df( *DebugFont, fp, FPSPointTracker.GetCurPosition(), 
					0.8f, Vector4f( 1.0f, 0.0f, 0.0f, 1.0f ), "%.1f fps", LastFrameRate );
			LastFrameTime = currentFrameTime;
		}

		// draw info text
		if ( InfoTextEndFrame >= TheVrFrame.Get().FrameNumber )
		{
			Vector3f viewPos = GetViewMatrixPosition( lastViewMatrix );
			Vector3f viewFwd = GetViewMatrixForward( lastViewMatrix );
			Vector3f viewUp( 0.0f, 1.0f, 0.0f );
			Vector3f viewLeft = viewUp.Cross( viewFwd );
			Vector3f newPos = viewPos + viewFwd * InfoTextOffset.z + viewUp * InfoTextOffset.y + viewLeft * InfoTextOffset.x;
			InfoTextPointTracker.Update( vrapi_GetTimeInSeconds(), newPos );

			fontParms_t fp;
			fp.AlignHoriz = HORIZONTAL_CENTER;
			fp.AlignVert = VERTICAL_CENTER;
			fp.Billboard = true;
			fp.TrackRoll = false;
			DebugFontSurface->DrawTextBillboarded3Df( *DebugFont, fp, InfoTextPointTracker.GetCurPosition(), 
					1.0f, InfoTextColor, InfoText.ToCStr() );
		}

		// Main loop logic and draw/update code common to both eyes.
		this->lastViewMatrix = appInterface->Frame( TheVrFrame.Get() );

		// Handle any events that weren't eaten by the app
		SystemActivities_PostUpdate( OvrMobile, &Java, &appEvents );

		// Draw the eye views.
		DrawEyeViews( this->lastViewMatrix );

		// Set the frame number and push the latest parms to warp swap.
		FrameParms.FrameIndex = TheVrFrame.Get().FrameNumber;
		vrapi_SubmitFrame( OvrMobile, &FrameParms );

		//SPAM( "FRAME END" );
	}

	// Shutdown the VR thread
	{
		LOG( "AppLocal::VrThreadFunction - shutdown" );

		ShutdownConsole( Java );

		SystemActivities_Shutdown( &Java );

		// Shut down the message queue so it cannot overflow.
		MessageQueue.Shutdown();

		appInterface->OneTimeShutdown();

		delete appInterface;
		appInterface = NULL;

		GetDebugLines().Shutdown();

		ShutdownDebugFont();

		delete dialogTexture;
		dialogTexture = NULL;

		delete EyeBuffers;
		EyeBuffers = NULL;

		if ( LoadingIconTextureChain != NULL )
		{
			vrapi_DestroyTextureSwapChain( LoadingIconTextureChain );
			LoadingIconTextureChain = NULL;
		}
		if ( ErrorTextureSwapChain != NULL )
		{
			vrapi_DestroyTextureSwapChain( ErrorTextureSwapChain );
			ErrorTextureSwapChain = NULL;
		}

		OvrDebugLines::Free( DebugLines );

		ShutdownGlObjects();

		GL_Shutdown( glSetup );

		ovrFileSys::Destroy( FileSys );

#if defined( OVR_OS_ANDROID )
		// Unregister the Headset receiver
		{
			JavaClass javaHeadsetReceiverClass( Java.Env, ovr_GetLocalClassReference( Java.Env, Java.ActivityObject, "com/oculus/vrappframework/HeadsetReceiver" ) );
			const jmethodID stopReceiverMethodId = ovr_GetStaticMethodID( Java.Env, javaHeadsetReceiverClass.GetJClass(), "stopReceiver", "(Landroid/content/Context;)V" );
			if ( stopReceiverMethodId != NULL )
			{
				Java.Env->CallStaticVoidMethod( javaHeadsetReceiverClass.GetJClass(), stopReceiverMethodId, Java.ActivityObject );
			}
		}
#endif

		// Detach from the Java VM before exiting.
		ovr_DetachCurrentThread( Java.Vm );
		VrSettings.ModeParms.Java.Env = NULL;
		Java.Env = NULL;

		LOG( "AppLocal::VrThreadFunction - exit" );
	}
}

// Shim to call a C++ object from an OVR::Thread::Start.
threadReturn_t AppLocal::ThreadStarter( Thread *, void * parm )
{
	((AppLocal *)parm)->VrThreadFunction();

	return NULL;
}

BitmapFont & AppLocal::GetDebugFont() 
{ 
    return *DebugFont; 
}

BitmapFontSurface & AppLocal::GetDebugFontSurface() 
{ 
    return *DebugFontSurface; 
}

OvrDebugLines & AppLocal::GetDebugLines() 
{ 
    return *DebugLines; 
}

const OvrStoragePaths & AppLocal::GetStoragePaths()
{
	return *StoragePaths;
}

int AppLocal::GetSystemProperty( const ovrSystemProperty propType )
{
	return vrapi_GetSystemPropertyInt( &Java, propType );
}

const VrDeviceStatus & AppLocal::GetDeviceStatus() const
{
	return TheVrFrame.Get().DeviceStatus;
}

const ovrEyeBufferParms & AppLocal::GetEyeBufferParms() const
{
	return VrSettings.EyeBufferParms;
}

void AppLocal::SetEyeBufferParms( const ovrEyeBufferParms & parms )
{
	VrSettings.EyeBufferParms = parms;
}

const ovrHeadModelParms & AppLocal::GetHeadModelParms() const
{
	return VrSettings.HeadModelParms;
}

void AppLocal::SetHeadModelParms( const ovrHeadModelParms & parms ) 
{
	VrSettings.HeadModelParms = parms;
}

int AppLocal::GetCpuLevel() const
{
	return FrameParms.PerformanceParms.CpuLevel;
}

void AppLocal::SetCpuLevel( const int cpuLevel )
{
	FrameParms.PerformanceParms.CpuLevel = cpuLevel;
}

int AppLocal::GetGpuLevel() const
{
	return FrameParms.PerformanceParms.GpuLevel;
}

void AppLocal::SetGpuLevel( const int gpuLevel )
{
	FrameParms.PerformanceParms.GpuLevel = gpuLevel;
}

int AppLocal::GetMinimumVsyncs() const
{
	return FrameParms.MinimumVsyncs;
}

void AppLocal::SetMinimumVsyncs( const int minimumVsyncs )
{
	FrameParms.MinimumVsyncs = minimumVsyncs;
}

bool AppLocal::GetFramebufferIsSrgb() const
{
	return FramebufferIsSrgb;
}

bool AppLocal::GetFramebufferIsProtected() const
{
	return FramebufferIsProtected;
}

Matrix4f const & AppLocal::GetLastViewMatrix() const
{
	return lastViewMatrix; 
}

void AppLocal::SetLastViewMatrix( Matrix4f const & m )
{
	lastViewMatrix = m; 
}

void AppLocal::SetPopupDistance( float const d )
{
	popupDistance = d; 
}

float AppLocal::GetPopupDistance() const
{
	return popupDistance; 
}

void AppLocal::SetPopupScale( float const s )
{
	popupScale = s; 
}

float AppLocal::GetPopupScale() const
{
	return popupScale; 
}

const ovrJava * AppLocal::GetJava() const
{
	return &Java;
}

jclass & AppLocal::GetVrActivityClass()
{
	return VrActivityClass;
}

SurfaceTexture * AppLocal::GetDialogTexture()
{
	return dialogTexture;
}

void AppLocal::SetShowFPS( bool const show )
{
	bool wasShowing = ShowFPS;
	ShowFPS = show;
	if ( !wasShowing && ShowFPS )
	{
		FPSPointTracker.Reset();
	}
}

bool AppLocal::GetShowFPS() const
{
	return ShowFPS;
}

VrAppInterface * AppLocal::GetAppInterface() 
{
	return appInterface;
}

void AppLocal::ShowInfoText( float const duration, const char * fmt, ... )
{
	char buffer[1024];
	va_list args;
	va_start( args, fmt );
	vsnprintf( buffer, sizeof( buffer ), fmt, args );
	va_end( args );
	InfoText = buffer;
	InfoTextColor = Vector4f( 1.0f );
	InfoTextOffset = Vector3f( 0.0f, 0.0f, 1.5f );
	InfoTextPointTracker.Reset();
	InfoTextEndFrame = TheVrFrame.Get().FrameNumber + (long long)( duration * 60.0f ) + 1;
}

void AppLocal::ShowInfoText( float const duration, Vector3f const & offset, Vector4f const & color, const char * fmt, ... )
{
	char buffer[1024];
	va_list args;
	va_start( args, fmt );
	vsnprintf( buffer, sizeof( buffer ), fmt, args );
	va_end( args );
	InfoText = buffer;
	InfoTextColor = color;
	if ( offset != InfoTextOffset || InfoTextEndFrame < TheVrFrame.Get().FrameNumber )
	{
		InfoTextPointTracker.Reset();
	}
	InfoTextOffset = offset;
	InfoTextEndFrame = TheVrFrame.Get().FrameNumber + (long long)( duration * 60.0f ) + 1;
}

ovrMobile * AppLocal::GetOvrMobile()
{
	return OvrMobile;
}

const char * AppLocal::GetPackageName() const
{
	return packageName.ToCStr();
}

bool AppLocal::GetInstalledPackagePath( char const * packageName, char * outPackagePath, size_t const outMaxSize ) const 
{
#if defined( OVR_OS_ANDROID )
	// Find the system activities apk so that we can load font data from it.
	jmethodID getInstalledPackagePathId = ovr_GetStaticMethodID( Java.Env, VrActivityClass, "getInstalledPackagePath", "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;" );
	if ( getInstalledPackagePathId != NULL )
	{
		JavaString packageNameObj( Java.Env, packageName );
		JavaUTFChars utfPath( Java.
			Env, static_cast< jstring >( Java.Env->CallStaticObjectMethod( VrActivityClass, 
				getInstalledPackagePathId, Java.ActivityObject, packageNameObj.GetJString() ) ) );
		if ( !Java.Env->ExceptionOccurred() )
		{
			char const * pathStr = utfPath.ToStr();
			bool result = outMaxSize >= OVR_strlen( pathStr );	// return false if the buffer is too small
			OVR_sprintf( outPackagePath, outMaxSize, "%s", pathStr );
			return result;
		}
		WARN( "Exception occurred when calling getInstalledPackagePathId" );
		Java.Env->ExceptionClear();
	}
#endif
	return false;
}

void AppLocal::RecenterYaw( const bool showBlack )
{
	LOG( "AppLocal::RecenterYaw" );
	if ( showBlack )
	{
		ovrFrameParms blackFrameParms = vrapi_DefaultFrameParms( &Java, VRAPI_FRAME_INIT_BLACK_FLUSH, vrapi_GetTimeInSeconds(), NULL );
		blackFrameParms.FrameIndex = TheVrFrame.Get().FrameNumber;
		vrapi_SubmitFrame( OvrMobile, &blackFrameParms );
	}
	vrapi_RecenterPose( OvrMobile );

	// Change lastViewMatrix to mirror what is done to the tracking orientation by vrapi_RecenterPose.
	// Get the current yaw rotation and cancel it out. This is necessary so that subsystems that
	// rely on lastViewMatrix do not end up using the orientation from before the recenter if they
	// are called before the beginning of the next frame.
	float yaw;
	float pitch;
	float roll;
	lastViewMatrix.ToEulerAngles< Axis_Y, Axis_X, Axis_Z, Rotate_CCW, Handed_R >( &yaw, &pitch, &roll );

	// undo the yaw
	Matrix4f unrotYawMatrix( Quatf( Axis_Y, -yaw ) );
	lastViewMatrix = lastViewMatrix * unrotYawMatrix;
}

void AppLocal::SetRecenterYawFrameStart( const long long frameNumber )
{
	LOG( "SetRecenterYawFrameStart( %l )", frameNumber );
	recenterYawFrameStart = frameNumber;
}

long long AppLocal::GetRecenterYawFrameStart() const
{
	return recenterYawFrameStart;
}

ovrFileSys & AppLocal::GetFileSys()
{
	return *FileSys;
}

void AppLocal::RegisterConsoleFunction( char const * name, consoleFn_t function )
{
	OVR::RegisterConsoleFunction( name, function );
}

}	// namespace OVR
