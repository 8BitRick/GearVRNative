/************************************************************************************

Filename	:	GearVRNative.cpp
Content		:	Main source file for "Gear VR Native".
Created		:	January, 2016
Authors		:	Richard Leon Terrell

Copyright	:	None.

*************************************************************************************/

#include "App.h"
#include "GuiSys.h"
#include "OVR_Locale.h"
#include "SoundEffectContext.h"
#include "VrCubeWorld.h"
#include "GVRAudioMgr.h"
#include <memory>

#if 0
	#define GL( func )		func; EglCheckErrors();
#else
	#define GL( func )		func;
#endif

using namespace OVR;

namespace GVR
{
static const int CPU_LEVEL			= 2;
static const int GPU_LEVEL			= 3;

class GearVRNative : public VrAppInterface
{
public:
						GearVRNative();
						~GearVRNative();

	virtual void 		Configure( ovrSettings & settings );

	virtual void		OneTimeInit( const char * fromPackage, const char * launchIntentJSON, const char * launchIntentURI );
	virtual void		OneTimeShutdown();
	virtual bool		OnKeyEvent( const int keyCode, const int repeatCount, const KeyEventType eventType );
	virtual Matrix4f	Frame( const VrFrame & vrFrame );
	virtual Matrix4f	DrawEyeView( const int eye, const float fovDegreesX, const float fovDegreesY, ovrFrameParms & frameParms );

	ovrLocale &			GetLocale() { return *Locale; }

private:
	VrCubeWorld *vrCubeWorld;
	GVR::AudioMgr *audioMgr;
	ovrSoundEffectContext * SoundEffectContext;
	OvrGuiSys::SoundEffectPlayer * SoundEffectPlayer;
	OvrGuiSys *			GuiSys;
	ovrLocale *			Locale;
	ovrMatrix4f			CenterEyeViewMatrix;

};

GearVRNative::GearVRNative() :
	SoundEffectContext( NULL ),
	SoundEffectPlayer( NULL ),
	GuiSys( OvrGuiSys::Create() ),
	Locale( NULL )
{
	vrCubeWorld = new VrCubeWorld();
	audioMgr = new GVR::AudioMgr();
	CenterEyeViewMatrix = ovrMatrix4f_CreateIdentity();
}

GearVRNative::~GearVRNative()
{
	delete audioMgr;
	delete vrCubeWorld;
	OvrGuiSys::Destroy( GuiSys );
}

void GearVRNative::OneTimeInit( const char * fromPackageName, const char * launchIntentJSON, const char * launchIntentURI )
{
	OVR_UNUSED( fromPackageName );
	OVR_UNUSED( launchIntentJSON );
	OVR_UNUSED( launchIntentURI );

	const ovrJava * java = app->GetJava();
	SoundEffectContext = new ovrSoundEffectContext( *java->Env, java->ActivityObject );
	SoundEffectContext->Initialize();
	SoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();

	Locale = ovrLocale::Create( *app, "default" );

	String fontName;
	GetLocale().GetString( "@string/font_name", "efigs.fnt", fontName );
	GuiSys->Init( this->app, *SoundEffectPlayer, fontName.ToCStr(), &app->GetDebugLines() );

	// FMOD Initialization
	audioMgr->OneTimeInit();
	vrCubeWorld->OneTimeInit();
}

void GearVRNative::OneTimeShutdown()
{
	vrCubeWorld->OneTimeShutdown();
	audioMgr->OneTimeShutdown();

	delete SoundEffectPlayer;
	SoundEffectPlayer = NULL;

	delete SoundEffectContext;
	SoundEffectContext = NULL;
}

void GearVRNative::Configure( ovrSettings & settings )
{
	settings.PerformanceParms.CpuLevel = CPU_LEVEL;
	settings.PerformanceParms.GpuLevel = GPU_LEVEL;
	settings.EyeBufferParms.multisamples = 4;
}

bool GearVRNative::OnKeyEvent( const int keyCode, const int repeatCount, const KeyEventType eventType )
{
	if ( GuiSys->OnKeyEvent( keyCode, repeatCount, eventType ) )
	{
		return true;
	}
	return false;
}

Matrix4f GearVRNative::Frame( const VrFrame & vrFrame )
{
	audioMgr->Frame(vrFrame);
	vrCubeWorld->Frame(vrFrame);

	CenterEyeViewMatrix = vrapi_GetCenterEyeViewMatrix( &app->GetHeadModelParms(), &vrFrame.Tracking, NULL );

	// Update GUI systems last, but before rendering anything.
	GuiSys->Frame( vrFrame, CenterEyeViewMatrix );

	return CenterEyeViewMatrix;
}

Matrix4f GearVRNative::DrawEyeView( const int eye, const float fovDegreesX, const float fovDegreesY, ovrFrameParms & frameParms )
{
	OVR_UNUSED( frameParms );

	const Matrix4f eyeViewMatrix = vrapi_GetEyeViewMatrix( &app->GetHeadModelParms(), &CenterEyeViewMatrix, eye );
	const Matrix4f eyeProjectionMatrix = ovrMatrix4f_CreateProjectionFov( fovDegreesX, fovDegreesY, 0.0f, 0.0f, 1.0f, 0.0f );
	const Matrix4f eyeViewProjection = eyeProjectionMatrix * eyeViewMatrix;

	vrCubeWorld->Draw(eyeViewMatrix, eyeProjectionMatrix);

	GuiSys->RenderEyeView( CenterEyeViewMatrix, eyeViewMatrix, eyeProjectionMatrix );

	frameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	return eyeViewProjection;
}

} // namespace OVR

#if defined( OVR_OS_ANDROID )
extern "C"
{

long Java_com_yourcomp_gearvrnative_MainActivity_nativeSetAppInterface( JNIEnv *jni, jclass clazz, jobject activity,
	jstring fromPackageName, jstring commandString, jstring uriString )
{
	// This is called by the java UI thread.
	LOG( "nativeSetAppInterface" );
	return (new GVR::GearVRNative())->SetActivity( jni, clazz, activity, fromPackageName, commandString, uriString );
}

} // extern "C"

#endif
