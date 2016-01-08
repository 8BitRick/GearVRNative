/************************************************************************************

Filename    :   VRMenuComponent.h
Content     :   Menuing system for VR apps.
Created     :   June 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "VRMenuComponent.h"

#include "VrCommon.h"
#include "App.h"
#include "Input.h"
#include "VRMenuMgr.h"

namespace OVR {

	const char * VRMenuComponent::TYPE_NAME = "";

//==============================
// VRMenuComponent::OnEvent
eMsgStatus VRMenuComponent::OnEvent( OvrGuiSys & guiSys, VrFrame const & vrFrame, 
		VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_ASSERT( self != NULL );

	//-------------------
	// do any pre work that every event handler must do
	//-------------------

	//LOG_WITH_TAG( "VrMenu", "OnEvent '%s' for '%s'", VRMenuEventTypeNames[event.EventType], self->GetText().ToCStr() );

	// call the overloaded implementation
	eMsgStatus status = OnEvent_Impl( guiSys, vrFrame, self, event );

	//-------------------
	// do any post work that every event handle must do
	//-------------------

	return status;
}


} // namespace OVR