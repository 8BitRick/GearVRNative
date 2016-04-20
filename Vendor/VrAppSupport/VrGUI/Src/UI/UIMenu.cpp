/************************************************************************************

Filename    :   UIMenu.cpp
Content     :
Created     :	1/5/2015
Authors     :   Jim Dose

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "UI/UIMenu.h"
#include "VRMenuMgr.h"
#include "GuiSys.h"
#include "App.h"

namespace OVR {

UIMenu::UIMenu( OvrGuiSys &guiSys ) :
	GuiSys( guiSys ),
	MenuName(),
	Menu( NULL ),
	MenuOpen( false ),
	IdPool( 1 )

{
	// This is called at library load time, so the system is not initialized
	// properly yet.
}

UIMenu::~UIMenu()
{
}

VRMenuId_t UIMenu::AllocId()
{
	VRMenuId_t id = IdPool;
	IdPool = VRMenuId_t( IdPool.Get() + 1 );

	return id;
}

void UIMenu::Open()
{
	LOG( "Open" );
	GuiSys.OpenMenu( MenuName.ToCStr() );
	MenuOpen = true;
}

void UIMenu::Close()
{
	LOG( "Close" );
	GuiSys.CloseMenu( Menu, true ); /// FIXME: App is not actually used so we pass NULL, but this interface should change
	MenuOpen = false;
}

//=======================================================================================

void UIMenu::Create( const char *menuName )
{
	MenuName = menuName;
	Menu = VRMenu::Create( menuName );
	Menu->Init( GuiSys, 0.0f, VRMenuFlags_t() );
    GuiSys.AddMenu( Menu );
}

void UIMenu::Destroy()
{
	GuiSys.DestroyMenu( Menu );
}

VRMenuFlags_t const & UIMenu::GetFlags() const
{
	return Menu->GetFlags();
}

void UIMenu::SetFlags( VRMenuFlags_t const & flags )
{
	Menu->SetFlags( flags );
}

} // namespace OVR
