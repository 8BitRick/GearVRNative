/************************************************************************************

Filename    :   UIMenu.h
Content     :
Created     :	1/5/2015
Authors     :   Jim Dose

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( UIMenu_h )
#define UIMenu_h

#include "VRMenu.h"
#include "GuiSys.h"

namespace OVR {

class UIMenu
{
public:
										UIMenu( OvrGuiSys & guiSys );
										~UIMenu();

	VRMenuId_t 							AllocId();

	void 								Create( const char * menuName );
	void								Destroy();

	void 								Open();
	void 								Close();

	bool								IsOpen() const { return MenuOpen; }

	VRMenu *							GetVRMenu() const { return Menu; }

    VRMenuFlags_t const &				GetFlags() const;
	void								SetFlags( VRMenuFlags_t	const & flags );

private:
	OvrGuiSys &							GuiSys;
    String								MenuName;
	VRMenu *							Menu;

	bool								MenuOpen;

	VRMenuId_t							IdPool;

private:
	// private assignment operator to prevent copying
	UIMenu &	operator = ( UIMenu & );
};

} // namespace OVR

#endif // UIMenu_h
