/************************************************************************************

Filename    :   VrFrameBuilder.cpp
Content     :   Builds the input for VrAppInterface::Frame()
Created     :   April 26, 2015
Authors     :   John Carmack

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "VrFrameBuilder.h"
#include "Kernel/OVR_LogUtils.h"
#include "Android/JniUtils.h"
#include "Kernel/OVR_Alg.h"
#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "Kernel/OVR_String.h"
#include "Input.h"
#include "SystemActivities.h"		// for BACK_BUTTON handling

#if defined ( OVR_OS_ANDROID )
#include <android/input.h>
#endif

namespace OVR
{

static struct
{
	ovrKeyCode	KeyCode;
	int			ButtonBit;
} buttonMappings[] = {
	{ OVR_KEY_BUTTON_A, 			BUTTON_A },
	{ OVR_KEY_BUTTON_B,				BUTTON_B },
	{ OVR_KEY_BUTTON_X, 			BUTTON_X },
	{ OVR_KEY_BUTTON_Y,				BUTTON_Y },
	{ OVR_KEY_BUTTON_START, 		BUTTON_START },
	{ OVR_KEY_ESCAPE,				BUTTON_BACK },
	{ OVR_KEY_BUTTON_SELECT, 		BUTTON_SELECT },
	{ OVR_KEY_MENU,					BUTTON_MENU },				// not really sure if left alt is the same as android OVR_KEY_MENU, but this is unused
	{ OVR_KEY_RIGHT_TRIGGER,		BUTTON_RIGHT_TRIGGER },
	{ OVR_KEY_LEFT_TRIGGER, 		BUTTON_LEFT_TRIGGER },
	{ OVR_KEY_DPAD_UP,				BUTTON_DPAD_UP },
	{ OVR_KEY_DPAD_DOWN,			BUTTON_DPAD_DOWN },
	{ OVR_KEY_DPAD_LEFT,			BUTTON_DPAD_LEFT },
	{ OVR_KEY_DPAD_RIGHT,			BUTTON_DPAD_RIGHT },
	{ OVR_KEY_LSTICK_UP,			BUTTON_LSTICK_UP },
	{ OVR_KEY_LSTICK_DOWN,			BUTTON_LSTICK_DOWN },
	{ OVR_KEY_LSTICK_LEFT,			BUTTON_LSTICK_LEFT },
	{ OVR_KEY_LSTICK_RIGHT,			BUTTON_LSTICK_RIGHT },
	{ OVR_KEY_RSTICK_UP,			BUTTON_RSTICK_UP },
	{ OVR_KEY_RSTICK_DOWN,			BUTTON_RSTICK_DOWN },
	{ OVR_KEY_RSTICK_LEFT,			BUTTON_RSTICK_LEFT },
	{ OVR_KEY_RSTICK_RIGHT,			BUTTON_RSTICK_RIGHT },
	/// FIXME: the following joypad buttons are not mapped yet because they would require extending the
	/// bit flags to 64 bits.
	{ OVR_KEY_BUTTON_C,				0 },
	{ OVR_KEY_BUTTON_Z,				0 },
	{ OVR_KEY_BUTTON_LEFT_SHOULDER, 0 },
	{ OVR_KEY_BUTTON_RIGHT_SHOULDER,0 },
	{ OVR_KEY_BUTTON_LEFT_THUMB,	0 },
	{ OVR_KEY_BUTTON_RIGHT_THUMB,	0 },

	{ OVR_KEY_MAX, 0 }
};

static ovrHeadSetPluggedState HeadPhonesPluggedState = OVR_HEADSET_PLUGGED_UNKNOWN;

VrFrameBuilder::VrFrameBuilder() :
	BackKeyState( BACK_BUTTON_DOUBLE_TAP_TIME_IN_SECONDS, BACK_BUTTON_LONG_PRESS_TIME_IN_SECONDS ),
	lastTouchpadTime( 0.0 ),
	touchpadTimer( 0.0 ),
	lastTouchDown( false ),
	touchState( 0 )
{
}

void VrFrameBuilder::UpdateNetworkState( JNIEnv * jni, jclass activityClass, jobject activityObject )
{
#if defined( OVR_OS_ANDROID )
	jmethodID isWififConnectedMethodId = ovr_GetStaticMethodID( jni, activityClass, "isWifiConnected", "(Landroid/app/Activity;)Z" );
	jmethodID isAirplaneModeEnabledMethodId = ovr_GetStaticMethodID( jni, activityClass, "isAirplaneModeEnabled", "(Landroid/app/Activity;)Z" );
	jmethodID isBluetoothEnabledMethodId = ovr_GetStaticMethodID( jni, activityClass, "getBluetoothEnabled", "(Landroid/app/Activity;)Z" );

	// NOTE: make sure android.permission.ACCESS_NETWORK_STATE is set in the manifest for isWifiConnected().
	vrFrame.DeviceStatus.WifiIsConnected		= jni->CallStaticBooleanMethod( activityClass, isWififConnectedMethodId, activityObject );
	vrFrame.DeviceStatus.AirplaneModeIsEnabled	= jni->CallStaticBooleanMethod( activityClass, isAirplaneModeEnabledMethodId, activityObject );
	vrFrame.DeviceStatus.BluetoothIsEnabled		= jni->CallStaticBooleanMethod( activityClass, isBluetoothEnabledMethodId, activityObject );

	if ( jni->ExceptionOccurred() )
	{
		jni->ExceptionClear();
		LOG( "VrFrameBuilder: Cleared JNI exception. Make sure android.permission.ACCESS_NETWORK_STATE is set in the manifest." );
	}
#else
	vrFrame.DeviceStatus.WifiIsConnected		= false;
	vrFrame.DeviceStatus.AirplaneModeIsEnabled	= false;
	vrFrame.DeviceStatus.BluetoothIsEnabled		= false;
#endif
}

void VrFrameBuilder::InterpretTouchpad( VrInput & input, const double currentTime )
{
	// 1) Down -> Up w/ Motion = Slide
	// 1) Down -> Timeout w/o Motion = Long Press
	// 2) Down -> Up w/out Motion -> Timeout = Single Tap
	// 3) Down -> Up w/out Motion -> Down -> Timeout = Nothing
	// 4) Down -> Up w/out Motion -> Down -> Up = Double Tap
	static const double timer_finger_down = 0.3;
	static const double timer_finger_up = 0.3;
	static const double timer_long_press = BACK_BUTTON_LONG_PRESS_TIME_IN_SECONDS;

	static const float min_swipe_distance = 100.0f;

	double deltaTime = currentTime - lastTouchpadTime;
	lastTouchpadTime = currentTime;
	touchpadTimer = touchpadTimer + deltaTime;

	bool down = false, up = false;
	bool currentTouchDown = ( input.buttonState & BUTTON_TOUCH ) != 0;

	if ( currentTouchDown && !lastTouchDown )
	{
		//LOG( "DOWN" );
		down = true;
		touchOrigin = input.touch;
	}

	if ( !currentTouchDown && lastTouchDown )
	{
		//LOG( "UP" );
		up = true;
	}

	lastTouchDown = currentTouchDown;

	input.touchRelative = input.touch - touchOrigin;
	float touchMagnitude = input.touchRelative.Length();
	input.swipeFraction = touchMagnitude / min_swipe_distance;

	switch ( touchState )
	{
	case 0:
		//CreateToast("0 - %f", touchpadTimer);
		if ( down )
		{
			touchState = 1;
			touchpadTimer = 0.0;
		}
		break;
	case 1:
		//CreateToast("1 - %f", touchpadTimer);
		//CreateToast("1 - %f", touchMagnitude);
		if ( touchMagnitude >= min_swipe_distance )
		{
			int dir = 0;
			if ( fabs( input.touchRelative[0] ) > fabs( input.touchRelative[1] ) )
			{
				if ( input.touchRelative[0] < 0 )
				{
					//LOG( "SWIPE FORWARD" );
					dir = BUTTON_SWIPE_FORWARD | BUTTON_TOUCH_WAS_SWIPE;
				}
				else
				{
					//LOG( "SWIPE BACK" );
					dir = BUTTON_SWIPE_BACK | BUTTON_TOUCH_WAS_SWIPE;
				}
			}
			else
			{
				if ( input.touchRelative[1] > 0 )
				{
					//LOG( "SWIPE DOWN" );
					dir = BUTTON_SWIPE_DOWN | BUTTON_TOUCH_WAS_SWIPE;
				}
				else
				{
					//LOG( "SWIPE UP" );
					dir = BUTTON_SWIPE_UP | BUTTON_TOUCH_WAS_SWIPE;
				}
			}
			input.buttonPressed |= dir;
			input.buttonReleased |= dir & ~BUTTON_TOUCH_WAS_SWIPE;
			input.buttonState |= dir;
			touchState = 0;
			touchpadTimer = 0.0;
		}
		else if ( up )
		{
			if ( touchpadTimer < timer_finger_down )
			{
				touchState = 2;
				touchpadTimer = 0.0;
			}
			else
			{
				//CreateToast("SINGLE TOUCH");
				input.buttonPressed |= BUTTON_TOUCH_SINGLE;
				input.buttonReleased |= BUTTON_TOUCH_SINGLE;
				input.buttonState |= BUTTON_TOUCH_SINGLE;
				touchState = 0;
				touchpadTimer = 0.0;
			}
		}
		else if ( touchpadTimer > timer_long_press )
		{
			//CreateToast( "1 - %f, longpress", touchpadTimer );
			input.buttonPressed |= BUTTON_TOUCH_LONGPRESS;
			input.buttonReleased |= BUTTON_TOUCH_LONGPRESS;
			input.buttonState |= BUTTON_TOUCH_LONGPRESS;
			touchState = 0;
			touchpadTimer = 0.0;
		}
		break;
	case 2:
		//CreateToast("2 - %f", touchpadTimer);
		if ( touchpadTimer >= timer_finger_up )
		{
			//CreateToast("SINGLE TOUCH");
			input.buttonPressed |= BUTTON_TOUCH_SINGLE;
			input.buttonReleased |= BUTTON_TOUCH_SINGLE;
			input.buttonState |= BUTTON_TOUCH_SINGLE;
			touchState = 0;
			touchpadTimer = 0.0;
		}
		else if ( down )
		{
			touchState = 3;
			touchpadTimer = 0.0;
		}
		break;
	case 3:
		//CreateToast("3 - %f", touchpadTimer);
		if ( touchpadTimer >= timer_finger_down )
		{
			touchState = 0;
			touchpadTimer = 0.0;
		}
		else if ( up )
		{
			//CreateToast("DOUBLE TOUCH");
			input.buttonPressed |= BUTTON_TOUCH_DOUBLE;
			input.buttonReleased |= BUTTON_TOUCH_DOUBLE;
			input.buttonState |= BUTTON_TOUCH_DOUBLE;
			touchState = 0;
			touchpadTimer = 0.0;
		}
		break;
	}
}

void VrFrameBuilder::AdvanceVrFrame( const ovrInputEvents & inputEvents, ovrMobile * ovr,
									const ovrFrameParms & frameParms,
									const ovrHeadModelParms & headModelParms,
									SystemActivitiesAppEventList_t * appEvents )
{
	const VrInput lastVrInput = vrFrame.Input;

	// Copy JoySticks and TouchPosition.
	for ( int i = 0; i < 2; i++ )
	{
		for ( int j = 0; j < 2; j++ )
		{
			vrFrame.Input.sticks[i][j] = inputEvents.JoySticks[i][j];
		}
		vrFrame.Input.touch[i] = inputEvents.TouchPosition[i];
	}

#if defined( OVR_OS_ANDROID )
	// Translate touch action into a touch button.
	if ( inputEvents.TouchAction == AMOTION_EVENT_ACTION_DOWN )
	{
		vrFrame.Input.buttonState |= BUTTON_TOUCH;
	}
	else if ( inputEvents.TouchAction == AMOTION_EVENT_ACTION_UP )
	{
		vrFrame.Input.buttonState &= ~BUTTON_TOUCH;
	}
#endif

	// Clear the key events.
	vrFrame.Input.NumKeyEvents = 0;

	// Handle the back key.
	const double currentTime = vrapi_GetTimeInSeconds();
	for ( int i = 0; i < inputEvents.NumKeyEvents; i++ )
	{
		// The back key is special because it has to handle short-press, long-press and double-tap.
		if ( inputEvents.KeyEvents[i].KeyCode == OVR_KEY_ESCAPE )
		{
			BackKeyState.HandleEvent( currentTime, inputEvents.KeyEvents[i].Down, inputEvents.KeyEvents[i].RepeatCount );
			continue;
		}
	}
	const KeyEventType eventType = BackKeyState.Update( currentTime );
	if ( eventType != KEY_EVENT_NONE )
	{
		vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].KeyCode = OVR_KEY_ESCAPE;
		vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].RepeatCount = 0;
		vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].EventType = eventType;
		vrFrame.Input.NumKeyEvents++;
	}

	// Copy the key events.
	for ( int i = 0; i < inputEvents.NumKeyEvents && vrFrame.Input.NumKeyEvents < MAX_KEY_EVENTS_PER_FRAME; i++ )
	{
		// The back key is already handled.
		if ( inputEvents.KeyEvents[i].KeyCode == OVR_KEY_ESCAPE )
		{
			continue;
		}
		vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].KeyCode = inputEvents.KeyEvents[i].KeyCode;
		vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].RepeatCount = inputEvents.KeyEvents[i].RepeatCount;
		vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].EventType = inputEvents.KeyEvents[i].Down ? KEY_EVENT_DOWN : KEY_EVENT_UP;
		vrFrame.Input.NumKeyEvents++;
	}

	// Clear previously set swipe buttons.
	vrFrame.Input.buttonState &= ~(	BUTTON_SWIPE_UP |
									BUTTON_SWIPE_DOWN |
									BUTTON_SWIPE_FORWARD |
									BUTTON_SWIPE_BACK |
									BUTTON_TOUCH_WAS_SWIPE |
									BUTTON_TOUCH_SINGLE |
									BUTTON_TOUCH_DOUBLE |
									BUTTON_TOUCH_LONGPRESS );

	// Update the joypad buttons using the key events.
	for ( int i = 0; i < inputEvents.NumKeyEvents; i++ )
	{
		const ovrKeyCode keyCode = inputEvents.KeyEvents[i].KeyCode;
		const bool down = inputEvents.KeyEvents[i].Down;

		// Keys always map to joystick buttons right now.
        for ( int j = 0; buttonMappings[j].KeyCode != OVR_KEY_MAX; j++ )
		{
			if ( keyCode == buttonMappings[j].KeyCode )
			{
				if ( down )
				{
					vrFrame.Input.buttonState |= buttonMappings[j].ButtonBit;
				}
				else
				{
					vrFrame.Input.buttonState &= ~buttonMappings[j].ButtonBit;
				}
				break;
			}
		}
		if ( down && 0 /* keyboard swipes */ )
		{
			if ( keyCode == OVR_KEY_CLOSE_BRACKET )
			{
				vrFrame.Input.buttonState |= BUTTON_SWIPE_FORWARD;
			} 
			else if ( keyCode == OVR_KEY_OPEN_BRACKET )
			{
				vrFrame.Input.buttonState |= BUTTON_SWIPE_BACK;
			}
		}
	}
	// Note joypad button transitions.
	vrFrame.Input.buttonPressed = vrFrame.Input.buttonState & ( ~lastVrInput.buttonState );
	vrFrame.Input.buttonReleased = ~vrFrame.Input.buttonState & ( lastVrInput.buttonState & ~BUTTON_TOUCH_WAS_SWIPE );

	if ( lastVrInput.buttonState & BUTTON_TOUCH_WAS_SWIPE )
	{
		if ( lastVrInput.buttonReleased & BUTTON_TOUCH )
		{
			vrFrame.Input.buttonReleased |= BUTTON_TOUCH_WAS_SWIPE;
		}
		else
		{
			// keep it around this frame
			vrFrame.Input.buttonState |= BUTTON_TOUCH_WAS_SWIPE;
		}
	}

	// Synthesize swipe gestures as buttons.
	InterpretTouchpad( vrFrame.Input, currentTime );

	// This is the only place FrameNumber gets incremented,
	// right before calling vrapi_GetPredictedDisplayTime().
	vrFrame.FrameNumber++;

	// Get the latest head tracking state, predicted ahead to the midpoint of the time
	// it will be displayed.  It will always be corrected to the real values by the
	// time warp, but the closer we get, the less black will be pulled in at the edges.
	double predictedDisplayTime = vrapi_GetPredictedDisplayTime( ovr, vrFrame.FrameNumber );

	// Make sure time always moves forward.
	if ( predictedDisplayTime <= vrFrame.PredictedDisplayTimeInSeconds )
	{
		predictedDisplayTime = vrFrame.PredictedDisplayTimeInSeconds + 0.001;
	}

	const ovrTracking baseTracking = vrapi_GetPredictedTracking( ovr, predictedDisplayTime );

	vrFrame.DeltaSeconds = Alg::Clamp( (float)( predictedDisplayTime - vrFrame.PredictedDisplayTimeInSeconds ), 0.0f, 0.1f );
	vrFrame.PredictedDisplayTimeInSeconds = predictedDisplayTime;
	vrFrame.Tracking = vrapi_ApplyHeadModel( &headModelParms, &baseTracking );

	// Update device status.
	vrFrame.DeviceStatus.HeadPhonesPluggedState		= HeadPhonesPluggedState;
	vrFrame.DeviceStatus.DeviceIsDocked				= ( vrapi_GetSystemStatusInt( &frameParms.Java, VRAPI_SYS_STATUS_DOCKED ) != VRAPI_FALSE );
	vrFrame.DeviceStatus.HeadsetIsMounted			= ( vrapi_GetSystemStatusInt( &frameParms.Java, VRAPI_SYS_STATUS_MOUNTED ) != VRAPI_FALSE );
	vrFrame.DeviceStatus.PowerLevelStateThrottled	= ( vrapi_GetSystemStatusInt( &frameParms.Java, VRAPI_SYS_STATUS_THROTTLED ) != VRAPI_FALSE );
	vrFrame.DeviceStatus.PowerLevelStateMinimum		= ( vrapi_GetSystemStatusInt( &frameParms.Java, VRAPI_SYS_STATUS_THROTTLED2 ) != VRAPI_FALSE );

	vrFrame.AppEvents = appEvents;
}

}	// namespace OVR

#if defined( OVR_OS_ANDROID )
extern "C"
{
JNIEXPORT void Java_com_oculus_vrappframework_HeadsetReceiver_stateChanged( JNIEnv * jni, jclass clazz, jint state )
{
	LOG( "nativeHeadsetEvent(%i)", state );
	OVR::HeadPhonesPluggedState = ( state == 1 ) ? OVR::OVR_HEADSET_PLUGGED : OVR::OVR_HEADSET_UNPLUGGED;
}
}	// extern "C"
#endif
