/************************************************************************************

Filename    :   VolumePopup.h
Content     :   Popup dialog for when user changes sound volume.
Created     :   September 18, 2014
Authors     :   Jim Dose

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_VolumePopup_h )
#define OVR_VolumePopup_h

#include "VRMenu.h"

namespace OVR {

class OvrGuiSys;

//==============================================================
// OvrVolumePopup
class OvrVolumePopup : public VRMenu
{
public:
	static const int		NumVolumeTics;

	static	VRMenuId_t		ID_BACKGROUND;
	static	VRMenuId_t		ID_VOLUME_ICON;
	static	VRMenuId_t		ID_VOLUME_TEXT;
	static	VRMenuId_t		ID_VOLUME_TICKS;

	static const char *		MENU_NAME;

	static const double		VolumeMenuFadeDelay;

	OvrVolumePopup();
	virtual ~OvrVolumePopup();

	// only one of these every needs to be created
	static  OvrVolumePopup * Create( OvrGuiSys & guiSys );

	void 					CheckForVolumeChange( OvrGuiSys & guiSys );
	void					Close( OvrGuiSys & guiSys );

	int						GetOSSoundVolume() const;
	double					GetOSSoundVolumeTimeSinceLastChangeInSeconds() const;

private:
	Vector3f				VolumeTextOffset;
	int						CurrentVolume;

private:
	virtual bool			Init_Impl( OvrGuiSys & guiSys, float const menuDistance, 
									VRMenuFlags_t const & flags, Array< VRMenuObjectParms const * > & itemParms ) OVR_OVERRIDE;
	virtual void			Shutdown_Impl( OvrGuiSys & guiSys ) OVR_OVERRIDE;

	void					ShowVolume( OvrGuiSys & guiSys, const int current );

	// overloads
	virtual void    		Frame_Impl( OvrGuiSys & guiSys, VrFrame const & vrFrame );
	virtual bool			OnKeyEvent_Impl( OvrGuiSys & guiSys, int const keyCode, const int repeatCount, 
									KeyEventType const eventType );

	void					CreateSubMenus( OvrGuiSys & guiSys );

#if defined( OVR_OS_ANDROID )
	jclass					VolumeReceiverClass;
#endif
};

} // namespace OVR

#endif // OVR_VolumePopup_h
