/************************************************************************************

Filename    :   UIButton.h
Content     :
Created     :	1/23/2015
Authors     :   Jim Dose

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( UIButton_h )
#define UIButton_h

#include "VRMenu.h"
#include "UI/UIObject.h"
#include "VRMenuComponent.h"
#include "UI/UITexture.h"

namespace OVR {

class VrAppInterface;
class UIButton;

//==============================================================
// UIButtonComponent
class UIButtonComponent : public VRMenuComponent
{
public:
	static const int TYPE_ID = 159493;

					UIButtonComponent( UIButton &button );

	virtual int		GetTypeId() const { return TYPE_ID; }

	bool			IsPressed() const { return TouchDown; }

private:
	UIButton &		Button;

    ovrSoundLimiter GazeOverSoundLimiter;
    ovrSoundLimiter DownSoundLimiter;
    ovrSoundLimiter UpSoundLimiter;

    bool			TouchDown;

private:
	// private assignment operator to prevent copying
	UIButtonComponent &	operator = ( UIButtonComponent & );

    virtual eMsgStatus      OnEvent_Impl( OvrGuiSys & guiSys, VrFrame const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );

    eMsgStatus              FocusGained( OvrGuiSys & guiSys, VrFrame const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );
    eMsgStatus              FocusLost( OvrGuiSys & guiSys, VrFrame const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );
};

//==============================================================
// UIButton

class UIButton : public UIObject
{
	friend class UIButtonComponent;

public:
										UIButton( OvrGuiSys &guiSys );
										~UIButton();

	void 								AddToMenu( UIMenu *menu, UIObject *parent = NULL );

	void 								SetButtonImages( const UITexture &normal, const UITexture &hover, const UITexture &pressed, const UIRectf &border = UIRectf() );
	void								SetButtonColors( const Vector4f &normal, const Vector4f &hover, const Vector4f &pressed );

	void								SetOnClick( void ( *callback )( UIButton *, void * ), void *object );
	void								SetOnFocusGained( void( *callback )( UIButton *, void * ), void *object );
	void								SetOnFocusLost( void( *callback )( UIButton *, void * ), void *object );

	void								UpdateButtonState();

private:
	UIButtonComponent *					ButtonComponent;

	UITexture 							NormalTexture;
	UITexture 							HoverTexture;
	UITexture 							PressedTexture;

	Vector4f							NormalColor;
	Vector4f 							HoverColor;
	Vector4f 							PressedColor;

	void 								( *OnClickFunction )( UIButton *button, void *object );
	void *								OnClickObject;

	void 								OnClick();

	void								( *OnFocusGainedFunction )( UIButton *button, void *object );
	void *								OnFocusGainedObject;

	void								FocusGained();

	void								( *OnFocusLostFunction )( UIButton *button, void *object );
	void *								OnFocusLostObject;

	void								FocusLost();

};

} // namespace OVR

#endif // UIButton_h
