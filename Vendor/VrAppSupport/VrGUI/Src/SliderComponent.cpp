/************************************************************************************

Filename    :   Slider_Component.cpp
Content     :   A reusable component implementing a slider bar.
Created     :   Sept 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "SliderComponent.h"

#include "VRMenu.h"
#include "VRMenuMgr.h"
#include "GuiSys.h"
#include "Input.h"
#include "App.h"

namespace OVR {

char const * OvrSliderComponent::TYPE_NAME = "OvrSliderComponent";

//==============================
// OvrSliderComponent::OvrSliderComponent
OvrSliderComponent::OvrSliderComponent( VRMenu & menu, float const sliderFrac, Vector3f const & localSlideDelta, 
		float const minValue, float const maxValue, float const sensitivityScale, VRMenuId_t const rootId, 
		VRMenuId_t const scrubberId, VRMenuId_t const textId, VRMenuId_t const bubbleId, VRMenuId_t const fillId )
		: VRMenuComponent( 	VRMenuEventFlags_t( VRMENU_EVENT_TOUCH_DOWN ) | VRMENU_EVENT_TOUCH_UP |
							VRMENU_EVENT_TOUCH_RELATIVE | VRMENU_EVENT_INIT | VRMENU_EVENT_FRAME_UPDATE )
		, TouchDown( false )
		, SliderFrac( sliderFrac )
		, MinValue( minValue )
		, MaxValue( maxValue )
		, SensitivityScale( sensitivityScale )
		, LocalSlideDelta( localSlideDelta )
		, Menu( menu )
		, RootId( rootId )
		, ScrubberId( scrubberId )
		, TextId( textId )		
		, BubbleId( bubbleId )
		, FillId( fillId )
		, CaretBasePose()
		, BubbleFader( 0.0f )
		, BubbleFadeOutTime( -1.0 )
		, LastDot( 0.0f )
		, OnReleaseFunction( NULL )
		, OnReleaseObject( NULL )
{
}

//==============================
// OvrSliderComponent::OnEvent_Impl
eMsgStatus OvrSliderComponent::OnEvent_Impl( OvrGuiSys & guiSys, VrFrame const & vrFrame, 
		VRMenuObject * self, VRMenuEvent const & event )
{
	switch ( event.EventType )
	{
		case VRMENU_EVENT_INIT:
			return OnInit( guiSys, vrFrame, self, event );
		case VRMENU_EVENT_FRAME_UPDATE:
			return OnFrameUpdate( guiSys, vrFrame, self, event );
		case VRMENU_EVENT_TOUCH_DOWN:
			return OnTouchDown( guiSys, vrFrame, self, event );
		case VRMENU_EVENT_TOUCH_UP:
			if ( OnReleaseFunction )
			{
				( *OnReleaseFunction )( this, OnReleaseObject, SliderFrac );
			}
			TouchDown = false;
			return OnTouchUp( guiSys, vrFrame, self, event );
		case VRMENU_EVENT_TOUCH_RELATIVE:
			return OnTouchRelative( guiSys, vrFrame, self, event );
		default:
			OVR_ASSERT( false );
			return MSG_STATUS_ALIVE;
	}
    return MSG_STATUS_CONSUMED;
}

//==============================
// OvrSliderComponent::OnInit
eMsgStatus OvrSliderComponent::OnInit( OvrGuiSys & guiSys, VrFrame const & vrFrame, 
		VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_UNUSED( vrFrame );
	OVR_UNUSED( event );

	// find the starting offset of the caret
	//LOG( "OvrSliderComponent - VRMENU_EVENT_INIT" );
	VRMenuObject * caret = guiSys.GetVRMenuMgr().ToObject( self->ChildHandleForId( guiSys.GetVRMenuMgr(), ScrubberId ) );
	if ( caret != NULL )
	{
		CaretBasePose = caret->GetLocalPose();
	}
	SetCaretPoseFromFrac( guiSys.GetVRMenuMgr(), self, SliderFrac );
	UpdateText( guiSys.GetVRMenuMgr(), self, BubbleId );
	return MSG_STATUS_ALIVE;
}

//==============================
// OvrSliderComponent::OnFrameUpdate
eMsgStatus OvrSliderComponent::OnFrameUpdate( OvrGuiSys & guiSys, VrFrame const & vrFrame, 
		VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_UNUSED( event );

	if ( TouchDown )
	{
		UpdateText( guiSys.GetVRMenuMgr(), self, BubbleId );
		UpdateText( guiSys.GetVRMenuMgr(), self, TextId );
	}

	if ( BubbleFadeOutTime > 0.0 )
	{
		if ( vrapi_GetTimeInSeconds() >= BubbleFadeOutTime )
		{
			BubbleFadeOutTime = -1.0;
			BubbleFader.StartFadeOut();
		}
	}

	VRMenuObject * bubble = guiSys.GetVRMenuMgr().ToObject( self->ChildHandleForId( guiSys.GetVRMenuMgr(), BubbleId ) );
	if ( bubble != NULL )
	{
		float const fadeTime = 0.5f;
		float const fadeRate = 1.0 / fadeTime;
		BubbleFader.Update( fadeRate, vrFrame.DeltaSeconds );

		Vector4f color = bubble->GetColor();
		color.w = BubbleFader.GetFinalAlpha();
		bubble->SetColor( color );
		Vector4f textColor = bubble->GetTextColor();
		textColor.w = color.w;
		bubble->SetTextColor( textColor );
	}

	return MSG_STATUS_ALIVE;
}

//==============================
// OvrSliderComponent::OnTouchRelative
eMsgStatus OvrSliderComponent::OnTouchRelative( OvrGuiSys & guiSys, VrFrame const & vrFrame,
		VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_UNUSED( vrFrame );

	return MSG_STATUS_CONSUMED;
}

//==============================
// OvrSliderComponent::OnTouchDown
eMsgStatus OvrSliderComponent::OnTouchDown( OvrGuiSys & guiSys, VrFrame const & vrFrame, 
		VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_UNUSED( guiSys );
	OVR_UNUSED( vrFrame );
	OVR_UNUSED( self );
	OVR_UNUSED( event );

	BubbleFader.StartFadeIn();
	BubbleFadeOutTime = -1.0;
	LastDot = 0.0f;

	return MSG_STATUS_CONSUMED;
}

//==============================
// OvrSliderComponent::OnTouchUp
eMsgStatus OvrSliderComponent::OnTouchUp( OvrGuiSys & guiSys, VrFrame const & vrFrame, 
		VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_UNUSED( guiSys );
	OVR_UNUSED( vrFrame );
	OVR_UNUSED( self );
	OVR_UNUSED( event );

	BubbleFadeOutTime = vrapi_GetTimeInSeconds() + 1.5;

	//LOG( "event.FloatValue = ( %.8f, %.8f )", event.FloatValue.x, event.FloatValue.y );
	// project on to the normalized slide delta so that the movement on the pad matches the orientation of the slider
	Vector2f slideDelta( LocalSlideDelta.x, LocalSlideDelta.y );
	slideDelta.Normalize();
	Vector2f touchXY( event.FloatValue.x, event.FloatValue.y );
	
	float dot = slideDelta.Dot( touchXY );
	if ( fabsf( dot ) < 0.7071f )
	{
		// if we're more than 45 degrees off axis, don't move at all
		dot = 0.0f;
	}
	else
	{
		// move as if we were perfectly aligned with the slider direction
		if ( dot < 0.0f )
		{
			dot = -1.0f;
		}
		else
		{
			dot = 1.0f;
		}
	}
	LastDot = dot;

	if ( LastDot != 0.0f )
	{
		float range = MaxValue - MinValue;
		float cur = floor( SliderFrac * range ) + MinValue;
		if ( LastDot < 0.0f )
		{
			SliderFrac = ( cur + 1.0f ) * ( 1.0f / range );
		}
		else 
		{
			SliderFrac = ( cur - 1.0f ) * ( 1.0f / range );
		}
		SliderFrac = Alg::Clamp( SliderFrac, 0.0f, 1.0f );

		SetCaretPoseFromFrac( guiSys.GetVRMenuMgr(), self, SliderFrac );
	
		Menu.OnItemEvent( guiSys, RootId, event );

		// update the bubble text
		UpdateText( guiSys.GetVRMenuMgr(), self, BubbleId );
	}
	LastDot = 0.0f;

	return MSG_STATUS_CONSUMED;
}

//==============================
// OvrSliderComponent::SetCaretPoseFromFrac
void OvrSliderComponent::SetCaretPoseFromFrac( OvrVRMenuMgr & menuMgr, VRMenuObject * self, float const frac )
{
	OVR_UNUSED( frac );

	VRMenuObject * caret = menuMgr.ToObject( self->ChildHandleForId( menuMgr, ScrubberId ) );
	if ( caret != NULL )
	{
		Posef curPose = CaretBasePose;
		float range = MaxValue - MinValue;
		float frac = floor( SliderFrac * range ) / range;
		curPose.Position += ( LocalSlideDelta * -0.5f ) + LocalSlideDelta * frac;
		caret->SetLocalPose( curPose );
	}

	// find the fill object and scale it
	menuHandle_t fillHandle = Menu.HandleForId( menuMgr, FillId );
	VRMenuObject * fillObj = menuMgr.ToObject( fillHandle );
	if ( fillObj != NULL )
	{
		Vector4f clipUVs( 0.0f, 1.0f - SliderFrac, 1.0f, 1.0f );
		VRMenuSurface & surf = fillObj->GetSurface( 0 );
		surf.SetClipUVs( clipUVs );
		//LOG( "SliderFrac = %.2f", SliderFrac );
		//LOG( "Setting clip UVs to ( %.2f, %.2f, %.2f, %.2f )", clipUVs.x, clipUVs.y, clipUVs.z, clipUVs.w );
	}	
}

//==============================
// OvrSliderComponent::UpdateText
void OvrSliderComponent::UpdateText( OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuId_t const id )
{
	if ( !id.IsValid() )
	{
		return;
	}

	VRMenuObject * obj = menuMgr.ToObject( self->ChildHandleForId( menuMgr, id ) );
	if ( obj != NULL )
	{
		char valueStr[127];
		GetStringValue( valueStr, sizeof( valueStr ) );
		obj->SetText( valueStr );
	}
}

//==============================
// OvrSliderComponent::GetStringValue
void OvrSliderComponent::GetStringValue( char * valueStr, int maxLen ) const
{
	int curValue = (int)floor( ( MaxValue - MinValue ) * SliderFrac + MinValue );
	OVR_sprintf( valueStr, maxLen, "%i", curValue );
}

String GetSliderImage( OvrSliderComponent::imageInfo_t const & info, bool vertical )
{
	char buff[256];
	OVR_sprintf( buff, sizeof( buff ), info.ImageName, vertical ? "vert" : "horz" );
	return String( buff );
}

Vector3f MakePosition( float const fwdDist, float const upDist, float const leftDist )
{
	static const Vector3f FWD( 0.0f, 0.0f, -1.0f );
	static const Vector3f UP( 0.0f, 1.0f, 0.0f );
	static const Vector3f LEFT( FWD.Cross( UP ) );

	// we only scale left and up by texel scale because those correspond to width / height
	return FWD * fwdDist + ( UP * upDist + LEFT * leftDist ) * VRMenuObject::DEFAULT_TEXEL_SCALE;
}

Posef MakePose( Quatf const & q, float const fwdDist, float const upDist, float const leftDist )
{
	return Posef( q, MakePosition( fwdDist, upDist, leftDist ) );
}

void OvrSliderComponent::GetVerticalSliderParms( VRMenu & menu, 
		VRMenuId_t const parentId, VRMenuId_t const rootId, 
		imageInfo_t const & iconImage,
		imageInfo_t const & baseImage,
		imageInfo_t const & trackImage, 
		imageInfo_t const & trackFullImage,
		imageInfo_t const & scrubberImage,
		imageInfo_t const & bubbleImage,
		VRMenuId_t const scrubberId, VRMenuId_t const bubbleId, VRMenuId_t const fillId,
		float const sliderFrac, float const minValue, float const maxValue, 
		float const sensitivityScale, 
		Array< VRMenuObjectParms const* > & parms )
{
	Vector3f const fwd( 0.0f, 0.0f, -1.0f );
	Vector3f const right( 1.0f, 0.0f, 0.0f );
	Vector3f const up( 0.0f, 1.0f, 0.0f );

	float const BASE_FWD_OFFSET = 0.033f;
	float const FWD_INC = 0.01f;
	//float TRACK_UP_OFFSET = ( baseImage.h - trackImage.h - iconImage.h ) * 0.5f;	// offset track just above the button image
	float TRACK_UP_OFFSET = ( baseImage.h * 0.5f ) - ( trackImage.h * 0.5f  + iconImage.h );	// offset track just above the button image
	Vector3f const localSlideDelta( MakePosition( 0.0f, trackImage.h, 0.0f ) );

	bool const vertical = true;

	// add parms for the root object that holds all the slider components
	{
		Array< VRMenuComponent* > comps;
		comps.PushBack( new OvrSliderComponent( menu, sliderFrac, localSlideDelta, minValue, maxValue, sensitivityScale, rootId, scrubberId, VRMenuId_t(), bubbleId, fillId ) );
		Array< VRMenuSurfaceParms > surfParms;
		char const * text = "slider_root";
		Vector3f scale( 1.0f );
		Posef pose( MakePose( Quatf(), 0.0f, ( baseImage.h * 0.5f ) - ( iconImage.h * 0.5f ), 0.0f ) );
		Posef textPose( Quatf(), Vector3f( 0.0f ) );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms;
		VRMenuObjectFlags_t objectFlags( VRMENUOBJECT_RENDER_HIERARCHY_ORDER );
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_CONTAINER, comps,
				surfParms, text, pose, scale, textPose, textScale, fontParms, rootId, 
				objectFlags, initFlags );
		itemParms->ParentId = parentId;
		parms.PushBack( itemParms );
	}

	// add parms for the base image that underlays the whole slider
	{
		Array< VRMenuComponent* > comps;
		Array< VRMenuSurfaceParms > surfParms;
		VRMenuSurfaceParms baseParms( "base", GetSliderImage( baseImage, vertical ).ToCStr(), SURFACE_TEXTURE_DIFFUSE,
				NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
		surfParms.PushBack( baseParms );
		char const * text = "base";
		Vector3f scale( 1.0f );
		Posef pose( MakePose( Quatf(), BASE_FWD_OFFSET, 0.0f, 0.0f ) );
		Posef textPose( Quatf(), Vector3f( 0.0f ) );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms;
		VRMenuObjectFlags_t objectFlags( VRMENUOBJECT_DONT_RENDER_TEXT );
		objectFlags |= VRMENUOBJECT_FLAG_NO_DEPTH;
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_BUTTON, comps,
				surfParms, text, pose, scale, textPose, textScale, fontParms, VRMenuId_t(), 
				objectFlags, initFlags );
		itemParms->ParentId = rootId;
		parms.PushBack( itemParms );
	}

	// add parms for the track image
	{
		Array< VRMenuComponent* > comps;
		Array< VRMenuSurfaceParms > surfParms;
		VRMenuSurfaceParms baseParms( "track", GetSliderImage( trackImage, vertical ).ToCStr(), SURFACE_TEXTURE_DIFFUSE,
				NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
		surfParms.PushBack( baseParms );
		char const * text = "track";
		Vector3f scale( 1.0f );
		Posef pose( MakePose( Quatf(), BASE_FWD_OFFSET - FWD_INC * 1, -TRACK_UP_OFFSET, 0.0f ) );
		Posef textPose( Quatf(), Vector3f( 0.0f ) );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms;
		VRMenuObjectFlags_t objectFlags( VRMENUOBJECT_DONT_RENDER_TEXT );
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_BUTTON, comps,
				surfParms, text, pose, scale, textPose, textScale, fontParms, VRMenuId_t(), 
				objectFlags, initFlags );
		itemParms->ParentId = rootId;
		parms.PushBack( itemParms );
	}

	// add parms for the filled track image
	{
		Array< VRMenuComponent* > comps;
		Array< VRMenuSurfaceParms > surfParms;
		VRMenuSurfaceParms baseParms( "track_full", GetSliderImage( trackFullImage, vertical ).ToCStr(), SURFACE_TEXTURE_DIFFUSE,
				NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
		surfParms.PushBack( baseParms );
		char const * text = "track_full";
		Vector3f scale( 1.0f );
		Posef pose( MakePose( Quatf(), BASE_FWD_OFFSET - FWD_INC * 2, -TRACK_UP_OFFSET, 0.0f ) );
		Posef textPose( Quatf(), Vector3f( 0.0f ) );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms;
		VRMenuObjectFlags_t objectFlags( VRMENUOBJECT_DONT_RENDER_TEXT );
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_BUTTON, comps,
				surfParms, text, pose, scale, textPose, textScale, fontParms, fillId, 
				objectFlags, initFlags );
		itemParms->ParentId = rootId;
		parms.PushBack( itemParms );
	}

	// add parms for the scrubber
	{
		Array< VRMenuComponent* > comps;
		Array< VRMenuSurfaceParms > surfParms;
		VRMenuSurfaceParms baseParms( "scrubber", GetSliderImage( scrubberImage, vertical ).ToCStr(), SURFACE_TEXTURE_DIFFUSE,
				NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
		surfParms.PushBack( baseParms );
		char const * text = "scrubber";
		Vector3f scale( 1.0f );
		Posef pose( MakePose( Quatf(), BASE_FWD_OFFSET - FWD_INC * 3, -TRACK_UP_OFFSET, 0.0f ) );
		Posef textPose( Quatf(), Vector3f( 0.0f ) );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms;
		VRMenuObjectFlags_t objectFlags( VRMENUOBJECT_DONT_RENDER_TEXT );
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_BUTTON, comps,
				surfParms, text, pose, scale, textPose, textScale, fontParms, scrubberId, 
				objectFlags, initFlags );
		itemParms->ParentId = rootId;
		parms.PushBack( itemParms );
	}

	// add parms for the bubble
	{
		Array< VRMenuComponent* > comps;
		Array< VRMenuSurfaceParms > surfParms;
		VRMenuSurfaceParms baseParms( "bubble", GetSliderImage( bubbleImage, vertical ).ToCStr(), SURFACE_TEXTURE_DIFFUSE,
				NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
		surfParms.PushBack( baseParms );
		char const * text = NULL;
		Vector3f scale( 1.0f );
		//Posef pose( Quatf(), right * ( trackImage.w + SLIDER_BUBBLE_CENTER ) + fwd * ( BASE_FWD_OFFSET - ( FWD_INC * 4 ) ) );
		float const SLIDER_BUBBLE_CENTER = bubbleImage.w * 0.5f;
		Posef pose( MakePose( Quatf(), BASE_FWD_OFFSET - FWD_INC * 4, 0.0f , trackImage.w + bubbleImage.w * 0.5593f ) );
		const float bubbleTextScale = 0.66f;
		const float bubbleTextCenterOffset = SLIDER_BUBBLE_CENTER - ( bubbleImage.w * 0.5f );
		const Vector3f textPosition = right * bubbleTextCenterOffset;
		Posef textPose( Quatf(), textPosition );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms( HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, true, bubbleTextScale );
		VRMenuObjectFlags_t objectFlags;
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_BUTTON, comps,
				surfParms, text, pose, scale, textPose, textScale, fontParms, bubbleId, 
				objectFlags, initFlags );
		itemParms->ParentId = scrubberId;
		parms.PushBack( itemParms );
	}
}

void OvrSliderComponent::SetOnRelease( void( *callback )( OvrSliderComponent *, void *, float ), void *object )
{
	OnReleaseFunction = callback;
	OnReleaseObject = object;
}

} // namespace OVR
