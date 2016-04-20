/************************************************************************************

Filename    :   ActionComponents.h
Content     :   Misc. VRMenu Components to handle actions
Created     :   September 12, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_ActionComponents_h )
#define OVR_ActionComponents_h

#include "VRMenuComponent.h"
#include "VRMenu.h"

namespace OVR {

class VRMenu;

//==============================================================
// OvrButton_OnUp
// This is a generic component that forwards a touch up to a menu (normally its owner)
class OvrButton_OnUp : public VRMenuComponent_OnTouchUp
{
public:
	static const int TYPE_ID = 1010;

	OvrButton_OnUp( VRMenu * menu, VRMenuId_t const buttonId ) :
		VRMenuComponent_OnTouchUp(),
		Menu( menu ),
		ButtonId( buttonId )
	{
	}

	void SetID( VRMenuId_t	newButtonId ) { ButtonId = newButtonId; }

	virtual int		GetTypeId( ) const { return TYPE_ID; }
	
private:
	virtual eMsgStatus  OnEvent_Impl( OvrGuiSys & guiSys, VrFrame const & vrFrame, 
		VRMenuObject * self, VRMenuEvent const & event );

private:
	VRMenu *	Menu;		// menu that holds the button
	VRMenuId_t	ButtonId;	// id of the button this control handles
};

}

#endif //OVR_ActionComponents_h