/************************************************************************************

Filename    :   OvrGuiSys.cpp
Content     :   Manager for native GUIs.
Created     :   June 6, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "GuiSys.h"

#include "Kernel/OVR_GlUtils.h"
#include "GlProgram.h"
#include "GlTexture.h"
#include "GlGeometry.h"
#include "VrCommon.h"
#include "App.h"
#include "GazeCursorLocal.h"
#include "VRMenuMgr.h"
#include "VRMenuComponent.h"
#include "SoundLimiter.h"
#include "VRMenuEventHandler.h"
#include "FolderBrowser.h"
#include "Input.h"
#include "DefaultComponent.h"
#include "VrApi.h"
#include "Android/JniUtils.h"
#include "VolumePopup.h"
#include "Kernel/OVR_JSON.h"
#include "Kernel/OVR_Lexer.h"
#include "SystemActivities.h"

#if defined( OVR_OS_ANDROID )
#include "Input.h"
#endif

namespace OVR {

#define IMPL_CONSOLE_FUNC_BOOL( var_name ) \
	ovrLexer lex( parms );	\
	int v;					\
	lex.ParseInt( v, 1 );	\
	var_name = v != 0;		\
	LOG( #var_name "( '%s' ) = %i", parms, var_name )

//==============================================================
// OvrGuiSysLocal
class OvrGuiSysLocal : public OvrGuiSys
{
public:
						OvrGuiSysLocal();
	virtual				~OvrGuiSysLocal();

	virtual void		Init( App * app, OvrGuiSys::SoundEffectPlayer & soundEffectPlayer,
                                char const * fontName, OvrDebugLines * debugLines );
	// Init with a custom font surface for larger-than-normal amounts of text.
	virtual void		Init( App * app_, OvrGuiSys::SoundEffectPlayer & soundEffectPlayer,
                                char const * fontName, BitmapFontSurface * fontSurface,
                                OvrDebugLines * debugLines );

	virtual void		Shutdown();
	virtual void		Frame( VrFrame const & vrFrame, 
								Matrix4f const & viewMatrix );

	virtual void 		RenderEyeView( Matrix4f const & centerViewMatrix, Matrix4f const & viewMatrix, 
								Matrix4f const & projectionMatrix ) const;

	virtual bool		OnKeyEvent( int const keyCode, 
								const int repeatCount, 
								KeyEventType const eventType );

	virtual void		ResetMenuOrientations( Matrix4f const & viewMatrix );

	virtual void		AddMenu( VRMenu * menu );
	virtual VRMenu *	GetMenu( char const * menuName ) const;
	virtual void		DestroyMenu( VRMenu * menu );
	
	virtual void		OpenMenu( char const * name );
	
	virtual void		CloseMenu( char const * menuName, bool const closeInstantly );
	virtual void		CloseMenu( VRMenu * menu, bool const closeInstantly );

	virtual bool		IsMenuActive( char const * menuName ) const;
	virtual bool		IsAnyMenuActive() const;
	virtual bool		IsAnyMenuOpen() const;
	
	virtual App *					GetApp() const { return app; }
	virtual OvrVRMenuMgr &			GetVRMenuMgr() { return *MenuMgr; }
	virtual OvrGazeCursor &			GetGazeCursor() { return *GazeCursor; }
	virtual BitmapFont &			GetDefaultFont() { return *DefaultFont; }
	virtual BitmapFontSurface &		GetDefaultFontSurface() { return *DefaultFontSurface; }
	virtual OvrDebugLines &			GetDebugLines() { return *DebugLines; }
    virtual SoundEffectPlayer &		GetSoundEffectPlayer() { return *SoundEffectPlayer; }

private:
	App *					app;
	OvrVRMenuMgr *			MenuMgr;
	OvrGazeCursor *			GazeCursor;
	BitmapFont *			DefaultFont;
	BitmapFontSurface *		DefaultFontSurface;
	OvrDebugLines *			DebugLines;
    OvrGuiSys::SoundEffectPlayer *     SoundEffectPlayer;
	OvrVolumePopup *		VolumePopup;

	Array< VRMenu* >		Menus;
	Array< VRMenu* >		ActiveMenus;

	bool					IsInitialized;
	static bool				SkipFrame;
	static bool				SkipRender;
	static bool				SkipSubmit;
	static bool				SkipFont;
	static bool				SkipCursor;

#if defined( OVR_OS_ANDROID )
	jclass					VolumeReceiverClass;
#endif

private:
	int						FindMenuIndex( char const * menuName ) const;
	int						FindMenuIndex( VRMenu const * menu ) const;
	int						FindActiveMenuIndex( VRMenu const * menu ) const;
	int						FindActiveMenuIndex( char const * menuName ) const;
	virtual void			MakeActive( VRMenu * menu );
	virtual void			MakeInactive( VRMenu * menu );

	Array< VRMenuComponent* > GetDefaultComponents();

	static void				GUISkipFrame( void * appPtr, char const * parms ) { IMPL_CONSOLE_FUNC_BOOL( SkipFrame ); }
	static void				GUISkipRender( void * appPtr, char const * parms ) { IMPL_CONSOLE_FUNC_BOOL( SkipRender ); }
	static void				GUISkipSubmit( void * appPtr, char const * parms ) { IMPL_CONSOLE_FUNC_BOOL( SkipSubmit ); }
	static void				GUISkipFont( void * appPtr, char const * parms ) { IMPL_CONSOLE_FUNC_BOOL( SkipFont ); }
	static void				GUISkipCursor( void * appPtr, char const * parms ) { IMPL_CONSOLE_FUNC_BOOL( SkipCursor ); }

};


Vector4f const OvrGuiSys::BUTTON_DEFAULT_TEXT_COLOR( 0.098f, 0.6f, 0.96f, 1.0f );
Vector4f const OvrGuiSys::BUTTON_HILIGHT_TEXT_COLOR( 1.0f );

//==============================
// OvrGuiSys::Create
OvrGuiSys * OvrGuiSys::Create()
{
	return new OvrGuiSysLocal;
}

//==============================
// OvrGuiSys::Destroy
void OvrGuiSys::Destroy( OvrGuiSys * & guiSys )
{
	if ( guiSys != NULL )
	{
		guiSys->Shutdown();
		delete guiSys;
		guiSys = NULL;
	}
}

bool OvrGuiSysLocal::SkipFrame = false;
bool OvrGuiSysLocal::SkipRender = false;
bool OvrGuiSysLocal::SkipSubmit = false;
bool OvrGuiSysLocal::SkipFont = false;
bool OvrGuiSysLocal::SkipCursor = false;

//==============================
// OvrGuiSysLocal::
OvrGuiSysLocal::OvrGuiSysLocal() 
	: app( NULL )
	, MenuMgr( NULL )
	, GazeCursor( NULL )
	, DefaultFont( NULL )
	, DefaultFontSurface( NULL )
	, DebugLines( NULL )
    , SoundEffectPlayer( NULL )
	, VolumePopup( NULL )
	, IsInitialized( false )
{
}

//==============================
// OvrGuiSysLocal::
OvrGuiSysLocal::~OvrGuiSysLocal()
{
	OVR_ASSERT( IsInitialized == false ); // Shutdown should already have been called

}

//==============================
// OvrGuiSysLocal::Init
void OvrGuiSysLocal::Init( App * app_, OvrGuiSys::SoundEffectPlayer & soundEffectPlayer, char const * fontName, 
		BitmapFontSurface * fontSurface, OvrDebugLines * debugLines )
{
	LOG( "OvrGuiSysLocal::Init" );

	app = app_;
	SoundEffectPlayer = &soundEffectPlayer;
	DebugLines = debugLines;

	MenuMgr = OvrVRMenuMgr::Create( *this );
	MenuMgr->Init( *this );

	GazeCursor = OvrGazeCursor::Create();

	DefaultFont = BitmapFont::Create();

	OVR_ASSERT( fontSurface->IsInitialized() );	// if you pass a font surface in, you must initialized it before calling OvrGuiSysLocal::Init()
	DefaultFontSurface = fontSurface;

	// choose a package to load the font from.
	// select the System Activities package first
	LOG( "GuiSys::Init - fontName is '%s'", fontName );

	char fontUri[1024];
	OVR_sprintf( fontUri, sizeof( fontUri ), "apk://font/res/raw/%s", fontName );
	if ( !DefaultFont->Load( app->GetFileSys(), fontUri ) )
	{
		// we can't just do a fatal error here because the /lang/ host is supposed to be System Activities
		// one case of the font failing to load is because System Activities is missing entirely.
		// Instead, we
		app->ShowDependencyError();
	}

	IsInitialized = true;

	// menus must be created after initialization
	VolumePopup = OvrVolumePopup::Create( *this );

	app->RegisterConsoleFunction( "GUISkipFrame", OvrGuiSysLocal::GUISkipFrame );
	app->RegisterConsoleFunction( "GUISkipRender", OvrGuiSysLocal::GUISkipRender );
	app->RegisterConsoleFunction( "GUISkipSubmit", OvrGuiSysLocal::GUISkipSubmit );
	app->RegisterConsoleFunction( "GUISkipFont", OvrGuiSysLocal::GUISkipFont );
	app->RegisterConsoleFunction( "GUISkipCursor", OvrGuiSysLocal::GUISkipCursor );
}

//==============================
// OvrGuiSysLocal::Init
void OvrGuiSysLocal::Init( App * app_, OvrGuiSys::SoundEffectPlayer & soundEffectPlayer, char const * fontName, OvrDebugLines * debugLines )
{
	BitmapFontSurface * fontSurface = BitmapFontSurface::Create();
	fontSurface->Init( 8192 );
	Init( app_, soundEffectPlayer, fontName, fontSurface, debugLines );
}

//==============================
// OvrGuiSysLocal::Shutdown
void OvrGuiSysLocal::Shutdown()
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return;
	}

	IsInitialized = false;

	// just clear the pointer to volume menu because it will be freed in the loop below
	VolumePopup = NULL;

	// pointers in this list will always be in Menus list, too, so just clear it
	ActiveMenus.Clear();

	// FIXME: we need to make sure we delete any child menus here -- it's not enough to just delete them
	// in the destructor of the parent, because they'll be left in the menu list since the destructor has
	// no way to call GuiSys->DestroyMenu() for them.
	for ( int i = 0; i < Menus.GetSizeI(); ++i )
	{
		VRMenu * menu = Menus[i];
		menu->Shutdown( *this );
		delete menu;
		Menus[i] = NULL;
	}
	Menus.Clear();

	BitmapFontSurface::Free( DefaultFontSurface );
	BitmapFont::Free( DefaultFont );
	OvrGazeCursor::Destroy( GazeCursor );
	OvrVRMenuMgr::Destroy( MenuMgr );
	DebugLines = NULL;
    SoundEffectPlayer = NULL;
	app = NULL;
}

//==============================
// OvrGuiSysLocal::RepositionMenus
// Reposition any open menus 
void OvrGuiSysLocal::ResetMenuOrientations( Matrix4f const & centerViewMatrix )
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return;
	}

	for ( int i = 0; i < Menus.GetSizeI(); ++i )
	{
		if ( VRMenu* menu = Menus.At( i ) )
		{
			LOG( "ResetMenuOrientation -> '%s'", menu->GetName( ) );
			menu->ResetMenuOrientation( centerViewMatrix );
		}
	}
}

//==============================
// OvrGuiSysLocal::AddMenu
void OvrGuiSysLocal::AddMenu( VRMenu * menu )
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return;
	}

	int menuIndex = FindMenuIndex( menu->GetName() );
	if ( menuIndex >= 0 )
	{
		WARN( "Duplicate menu name '%s'", menu->GetName() );
		OVR_ASSERT( menuIndex < 0 );
	}
	Menus.PushBack( menu );
}

//==============================
// OvrGuiSysLocal::GetMenu
VRMenu * OvrGuiSysLocal::GetMenu( char const * menuName ) const
{
	int menuIndex = FindMenuIndex( menuName );
	if ( menuIndex >= 0 )
	{
		return Menus[menuIndex];
	}
	return NULL;
}

//==============================
// OvrGuiSysLocal::DestroyMenu
void OvrGuiSysLocal::DestroyMenu( VRMenu * menu )
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return;
	}

	OVR_ASSERT( menu != NULL );

	MakeInactive( menu );

	menu->Shutdown( *this );
	delete menu;

	int idx = FindMenuIndex( menu );
	if ( idx >= 0 )
	{
		Menus.RemoveAt( idx );
	}
}

//==============================
// OvrGuiSysLocal::FindMenuIndex
int OvrGuiSysLocal::FindMenuIndex( char const * menuName ) const
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return -1;
	}

	for ( int i = 0; i < Menus.GetSizeI(); ++i )
	{
		if ( OVR_stricmp( Menus[i]->GetName(), menuName ) == 0 )
		{
			return i;
		}
	}
	return -1;
}

//==============================
// OvrGuiSysLocal::FindMenuIndex
int OvrGuiSysLocal::FindMenuIndex( VRMenu const * menu ) const
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return -1;
	}

	for ( int i = 0; i < Menus.GetSizeI(); ++i )
	{
		if ( Menus[i] == menu ) 
		{
			return i;
		}
	}
	return -1;
}

//==============================
// OvrGuiSysLocal::FindActiveMenuIndex
int OvrGuiSysLocal::FindActiveMenuIndex( VRMenu const * menu ) const
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return -1;
	}

	for ( int i = 0; i < ActiveMenus.GetSizeI(); ++i )
	{
		if ( ActiveMenus[i] == menu ) 
		{
			return i;
		}
	}
	return -1;
}

//==============================
// OvrGuiSysLocal::FindActiveMenuIndex
int OvrGuiSysLocal::FindActiveMenuIndex( char const * menuName ) const
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return -1;
	}

	for ( int i = 0; i < ActiveMenus.GetSizeI(); ++i )
	{
		if ( OVR_stricmp( ActiveMenus[i]->GetName(), menuName ) == 0 )
		{
			return i;
		}
	}
	return -1;
}

//==============================
// OvrGuiSysLocal::MakeActive
void OvrGuiSysLocal::MakeActive( VRMenu * menu )
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return;
	}

	int idx = FindActiveMenuIndex( menu );
	if ( idx < 0 )
	{
		ActiveMenus.PushBack( menu );
	}
}

//==============================
// OvrGuiSysLocal::MakeInactive
void OvrGuiSysLocal::MakeInactive( VRMenu * menu )
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return;
	}

	int idx = FindActiveMenuIndex( menu );
	if ( idx >= 0 )
	{
		ActiveMenus.RemoveAtUnordered( idx );
	}
}

//==============================
// OvrGuiSysLocal::OpenMenu
void OvrGuiSysLocal::OpenMenu( char const * menuName )
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return;
	}

	int menuIndex = FindMenuIndex( menuName );
	if ( menuIndex < 0 )
	{
		WARN( "No menu named '%s'", menuName );
		OVR_ASSERT( menuIndex >= 0 && menuIndex < Menus.GetSizeI() );
		return;
	}
	VRMenu * menu = Menus[menuIndex];
	OVR_ASSERT( menu != NULL );

	menu->Open( *this );
}

//==============================
// OvrGuiSysLocal::CloseMenu
void OvrGuiSysLocal::CloseMenu( VRMenu * menu, bool const closeInstantly )
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return;
	}

	OVR_ASSERT( menu != NULL );

	menu->Close( *this, closeInstantly );
}

//==============================
// OvrGuiSysLocal::CloseMenu
void OvrGuiSysLocal::CloseMenu( char const * menuName, bool const closeInstantly ) 
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return;
	}

	int menuIndex = FindMenuIndex( menuName );
	if ( menuIndex < 0 )
	{
		WARN( "No menu named '%s'", menuName );
		OVR_ASSERT( menuIndex >= 0 && menuIndex < Menus.GetSizeI() );
		return;
	}
	VRMenu * menu = Menus[menuIndex];
	CloseMenu( menu, closeInstantly );
}


//==============================
// OvrGuiSysLocal::IsMenuActive
bool OvrGuiSysLocal::IsMenuActive( char const * menuName ) const
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return false;
	}

	int idx = FindActiveMenuIndex( menuName );
	return idx >= 0;
}

//==============================
// OvrGuiSysLocal::IsAnyMenuOpen
bool OvrGuiSysLocal::IsAnyMenuActive() const 
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return false;
	}

	return ActiveMenus.GetSizeI() > 0;
}

//==============================
// OvrGuiSysLocal::IsAnyMenuOpen
bool OvrGuiSysLocal::IsAnyMenuOpen() const
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return false;
	}

	for ( int i = 0; i < ActiveMenus.GetSizeI(); ++i )
	{
		if ( ActiveMenus[i]->IsOpenOrOpening() )
		{
			return true;
		}
	}
	return false;
}

//==============================
// OvrGuiSysLocal::Frame
void OvrGuiSysLocal::Frame( const VrFrame & vrFrame, Matrix4f const & centerViewMatrix )
{
	if ( !IsInitialized || SkipFrame )
	{
		OVR_ASSERT( IsInitialized );
		return;
	}

	for ( int i = 0; i < vrFrame.AppEvents->NumEvents; ++i )
	{
		char const * jsonError;
		JSON * jsonObj = JSON::Parse( vrFrame.AppEvents->Events[i], &jsonError );
		JsonReader reader( jsonObj );
		OVR_ASSERT( jsonObj != NULL && reader.IsObject() );
		String command = reader.GetChildStringByName( "Command" );
		if ( OVR_stricmp( command.ToCStr(), SYSTEM_ACTIVITY_EVENT_REORIENT ) == 0 )
		{
			//LOG( "OvrGuiSysLocal::Frame - reorienting" );
			app->RecenterYaw( false );
			ResetMenuOrientations( app->GetLastViewMatrix() );
			// remove this event so the app doesn't handle it again
			SystemActivities_RemoveAppEvent( vrFrame.AppEvents, i );
			--i;
		}
		jsonObj->Release();
	}

	// update volume popup
	VolumePopup->CheckForVolumeChange( *this );

	// go backwards through the list so we can use unordered remove when a menu finishes closing
	for ( int i = ActiveMenus.GetSizeI() - 1; i >= 0; --i )
	{
		VRMenu * curMenu = ActiveMenus[i];
		OVR_ASSERT( curMenu != NULL );

		curMenu->Frame( *this, vrFrame, centerViewMatrix );

		if ( curMenu->GetCurMenuState() == VRMenu::MENUSTATE_CLOSED )
		{
			// remove from the active list
			ActiveMenus.RemoveAtUnordered( i );
			continue;
		}
	}

	GazeCursor->Frame( centerViewMatrix, vrFrame.DeltaSeconds );

	DefaultFontSurface->Finish( centerViewMatrix );

	MenuMgr->Finish( centerViewMatrix );
}

//==============================
// OvrGuiSysLocal::RenderEyeView
void OvrGuiSysLocal::RenderEyeView( Matrix4f const & centerViewMatrix, Matrix4f const & viewMatrix, 
		Matrix4f const & projectionMatrix ) const
{
	if ( !IsInitialized || SkipRender )
	{
		OVR_ASSERT( IsInitialized );
		return;
	}

	if ( !SkipSubmit )
	{
		// GL_CheckErrors( "Pre GUI" );
		OVR::Array< ovrDrawSurface > surfaceList;
		MenuMgr->RenderEyeView( centerViewMatrix, viewMatrix, projectionMatrix, surfaceList );
		RenderSurfaceList( surfaceList, viewMatrix, projectionMatrix );
		GL_CheckErrors( "Post GUI" );
	}
	
	Matrix4f vpForEye = projectionMatrix * viewMatrix;

	if ( !SkipFont )
	{
		DefaultFontSurface->Render3D( *DefaultFont, vpForEye );
	}

	if ( !SkipCursor )
	{
		GazeCursor->RenderForEye( vpForEye );
	}
}

//==============================
// OvrGuiSysLocal::OnKeyEvent
bool OvrGuiSysLocal::OnKeyEvent( int const keyCode, const int repeatCount, KeyEventType const eventType ) 
{
	if ( !IsInitialized )
	{
		OVR_ASSERT( IsInitialized );
		return false;
	}

	// The back key is special because it has to handle short-press, long-press and double-tap.
	if ( keyCode == OVR_KEY_BACK )
	{
		// If this is not system activities.
		if ( !ovr_IsCurrentActivity( app->GetJava()->Env, app->GetJava()->ActivityObject, PUI_CLASS_NAME ) )
		{
			// Update the gaze cursor timer.
			if ( eventType == KEY_EVENT_DOWN )
			{
				GazeCursor->StartTimer( BACK_BUTTON_LONG_PRESS_TIME_IN_SECONDS, BACK_BUTTON_DOUBLE_TAP_TIME_IN_SECONDS );
			} 
			else if ( eventType == KEY_EVENT_DOUBLE_TAP || eventType == KEY_EVENT_SHORT_PRESS )
			{
				GazeCursor->CancelTimer();
			}
			else if ( eventType == KEY_EVENT_LONG_PRESS )
			{
				GazeCursor->CancelTimer();
				GetApp()->StartSystemActivity( PUI_GLOBAL_MENU );
				return true;
			}
		}
	}

	// menus ignore key repeats? I do not know why this is here any longer :(
	if ( repeatCount != 0 )
	{
		return false;
	}

	for ( int i = 0; i < ActiveMenus.GetSizeI(); ++i )
	{
		VRMenu * curMenu = ActiveMenus[i];
		OVR_ASSERT( curMenu != NULL );

		if ( keyCode == OVR_KEY_BACK ) 
		{
			LOG( "OvrGuiSysLocal back key event '%s' for menu '%s'", KeyEventNames[eventType], curMenu->GetName() );
		}

		if ( curMenu->OnKeyEvent( *this, keyCode, repeatCount, eventType ) )
		{
			LOG( "VRMenu '%s' consumed key event", curMenu->GetName() );
			return true;
		}
	}
	// we ignore other keys in the app menu for now
	return false;
}

bool OvrGuiSys::ovrDummySoundEffectPlayer::Has( const char* name ) const
{
	LOG( "ovrDummySoundEffectPlayer::Has( %s )", name );
	return false;
}

void OvrGuiSys::ovrDummySoundEffectPlayer::Play( const char* name )
{
	LOG( "ovrDummySoundEffectPlayer::Play( %s )", name );
}

} // namespace OVR
