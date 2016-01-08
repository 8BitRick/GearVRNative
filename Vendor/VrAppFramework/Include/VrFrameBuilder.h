/************************************************************************************

Filename    :   VrFrameBuilder.h
Content     :   Builds the input for VrAppInterface::Frame()
Created     :   April 26, 2015
Authors     :   John Carmack

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_VrFrameBuilder_h
#define OVR_VrFrameBuilder_h

#include "Input.h"
#include "KeyState.h"

namespace OVR {

static const int MAX_INPUT_KEY_EVENTS = 16;

struct ovrInputEvents
{
	ovrInputEvents() :
		JoySticks(),
		TouchPosition(),
		TouchAction( -1 ),
		NumKeyEvents( 0 ),
		KeyEvents() {}

	float JoySticks[2][2];
	float TouchPosition[2];
	int TouchAction;
	int NumKeyEvents;
	struct ovrKeyEvents_t
	{
		ovrKeyEvents_t()
			: KeyCode( OVR_KEY_NONE )
			, RepeatCount( 0 )
			, Down( false ) 
			, IsJoypadButton( false )
		{
		}

		ovrKeyCode	KeyCode;
		int			RepeatCount;
		bool		Down : 1;
		bool		IsJoypadButton : 1;
	} KeyEvents[MAX_INPUT_KEY_EVENTS];
};

class VrFrameBuilder
{
public:
						VrFrameBuilder();

	void				UpdateNetworkState( JNIEnv * jni, jclass activityClass, jobject activityObject );

	void				AdvanceVrFrame( const ovrInputEvents & inputEvents, ovrMobile * ovr,
										const ovrFrameParms & frameParms,
										const ovrHeadModelParms & headModelParms,
										SystemActivitiesAppEventList_t * appEvents );
	const VrFrame &		Get() const { return vrFrame; }

private:
	VrFrame		vrFrame;
	KeyState	BackKeyState;

	double		lastTouchpadTime;
	double		touchpadTimer;
	bool		lastTouchDown;
	int			touchState;
	Vector2f	touchOrigin;

	void 		InterpretTouchpad( VrInput & input, const double currentTime );
};

}	// namespace OVR

#endif // OVR_VrFrameBuilder_h
