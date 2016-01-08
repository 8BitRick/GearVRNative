/************************************************************************************

Filename    :   UINotification.cpp
Content     :   A pop up Notification object
Created     :   Apr 23, 2015
Authors     :   Clint Brewer

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "UI/UINotification.h"
#include "UI/UIMenu.h"
#include "VRMenuMgr.h"
#include "GuiSys.h"
#include "App.h"


namespace OVR {

static const float	BORDER_SPACING = 28.0f;
static const int	ICON_SIZE = 28;

static const float FADE_IN_CONTINUE_TIME = 0.25f;  //how long does it take to fade in if we are continuing from a previous visible message
static const float FADE_IN_FIRST_TIME = 0.75f; //how long does it take if this is the first message

static const float FADE_OUT_CONTINUE_TIME = 0.5f; //how long does it take to fade out if we are going to display another message
static const float FADE_OUT_LAST_TIME = 2.0f; //how long does it take to fade out the last message

static const float DURATION_CONTINUE_TIME = 5.0f;  //how long does a message stay visible if there are more in the queue
static const float DURATION_LAST_TIME = 10.0f; //how long does the last one stay visible for?


UINotification::UINotification( OvrGuiSys &guiSys ) :
			UIObject( guiSys ),
			NotificationComponent( NULL ),
			BackgroundTintTexture(),
			BackgroundImage( guiSys ),
			IconTexture(),
			IconImage( guiSys ),
			DescriptionLabel( guiSys ),
			VisibleDuration( DURATION_LAST_TIME ),
			FadeInDuration( FADE_IN_FIRST_TIME ),
			FadeOutDuration( 2.0f ),
			TimeLeft( 0 )

{
}

UINotification::~UINotification()
{
	BackgroundTintTexture.Free();
	IconTexture.Free();
}

void UINotification::AddToMenu( UIMenu *menu, const char* iconTextureName,  const char* backgroundTintTextureName, UIObject *parent )
{
	const Posef pose( Quatf( Vector3f( 0.0f, 1.0f, 0.0f ), 0.0f ), Vector3f( 0.0f, 0.0f, 0.0f ) );

	Vector3f defaultScale( 1.0f );

	VRMenuFontParms fontParms( HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, false, 1.0f );

	VRMenuObjectParms parms( VRMENU_STATIC, Array< VRMenuComponent* >(), VRMenuSurfaceParms(),
			"", pose, defaultScale, fontParms, menu->AllocId(),
			VRMenuObjectFlags_t(), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

	AddToMenuWithParms( menu, parent, parms );

	//textures
	IconTexture.LoadTextureFromApplicationPackage( iconTextureName );
	BackgroundTintTexture.LoadTextureFromApplicationPackage( backgroundTintTextureName );

	//The Container
	BackgroundImage.AddToMenu( menu, this );
	BackgroundImage.SetImage( 0, SURFACE_TEXTURE_DIFFUSE, BackgroundTintTexture, 300, 200, UIRectf( 2.0f, 2.0f, 2.0f, 2.0 ) );
	BackgroundImage.AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ) | VRMENUOBJECT_FLAG_NO_FOCUS_GAINED );
	BackgroundImage.SetMargin( UIRectf( BORDER_SPACING, BORDER_SPACING, BORDER_SPACING, BORDER_SPACING ) );

	//The Side Icon
	IconImage.AddToMenu( menu, &BackgroundImage );
	IconImage.SetImage( 0, SURFACE_TEXTURE_DIFFUSE, IconTexture, ICON_SIZE, ICON_SIZE );
	IconImage.AddFlags( VRMenuObjectFlags_t(VRMENUOBJECT_FLAG_NO_FOCUS_GAINED) | VRMENUOBJECT_DONT_HIT_ALL );
	IconImage.SetPadding( UIRectf( 0, 0, BORDER_SPACING * 0.5f + 2,0 ) );
	Vector3f iconPosition = IconImage.GetLocalPosition();
	iconPosition.z += 0.10f; //offset to prevent z fighting with BackgroundImage,
	IconImage.SetLocalPosition( iconPosition );

	// The Text...
	DescriptionLabel.AddToMenu( menu, &BackgroundImage );
	DescriptionLabel.SetText("");
	DescriptionLabel.SetFontScale( 0.5f );
	DescriptionLabel.SetPadding( UIRectf( BORDER_SPACING * 0.5f + 2, 0, 0, 0 ) );
	DescriptionLabel.AlignTo( CENTER, &IconImage, CENTER, 0.00f );

	VRMenuObject * object = GetMenuObject();
	OVR_ASSERT( object );

	NotificationComponent = new UINotificationComponent( *this );
	object->AddComponent( NotificationComponent );

	//make sure we start faded out
	Vector4f color = GetColor();
	color.w = 0.0f;
	SetColor( color );
	SetVisible(false);
}

void UINotification::QueueNotification( const String& description, bool showImmediately/* = false*/)
{
	if( showImmediately )
	{

		SetDescription( description );
	}
	else
	{
		if( GetVisible() && TimeLeft > DURATION_CONTINUE_TIME)
		{
			TimeLeft = DURATION_CONTINUE_TIME;
		}

		NotificationsQueue.PushBack(description);
	}
}


void UINotification::SetDescription( const String &description )
{
	if ( DescriptionLabel.GetMenuObject() )
	{
		DescriptionLabel.SetTextWordWrapped( description.ToCStr(), GuiSys.GetDefaultFont(), 1.0f );
		DescriptionLabel.CalculateTextDimensions();
		Bounds3f textBounds = DescriptionLabel.GetTextLocalBounds( GuiSys.GetDefaultFont() ) * VRMenuObject::TEXELS_PER_METER;
		BackgroundImage.SetDimensions( Vector2f( textBounds.GetSize().x, textBounds.GetSize().y + BORDER_SPACING * 2.0f ) ); //y is only what matters on this line
		BackgroundImage.WrapChildrenHorizontal();

		bool noMoreInQueue = NotificationsQueue.GetSize() == 0;
		FadeInDuration = GetVisible() ? FADE_IN_CONTINUE_TIME : FADE_IN_FIRST_TIME;
		FadeOutDuration = noMoreInQueue ? FADE_OUT_LAST_TIME  : FADE_OUT_CONTINUE_TIME;
		VisibleDuration = noMoreInQueue ? DURATION_LAST_TIME  : DURATION_CONTINUE_TIME;
		TimeLeft = VisibleDuration;

		SetVisible(true);
	}
}

void UINotification::Update( float deltaSeconds )
{
	TimeLeft -= deltaSeconds;

	if( TimeLeft <= 0 )
	{
		//done, hide or show next one
		TimeLeft = 0;

		if( NotificationsQueue.GetSize() > 0 )
		{
			SetDescription( NotificationsQueue.PopFront() );
		}
		else
		{
			SetVisible( false );
		}
	}
	else if( TimeLeft < FadeOutDuration )
	{
		//fading out
		float alpha = ( TimeLeft / FadeOutDuration );
		Vector4f color = GetColor();
		color.w = Alg::Clamp( alpha, 0.0f, 1.0f );
		SetColor( color );
	}
	else if( ( VisibleDuration - TimeLeft) < FadeInDuration  )
	{
		//fade in
		float alpha = ( VisibleDuration - TimeLeft ) / FadeInDuration;
		Vector4f color = GetColor();
		color.w = Alg::Clamp( alpha, 0.0f, 1.0f);
		SetColor( color );
	}
	else
	{
		Vector4f color = GetColor();
		color.w = 1.0f;
		SetColor( color );
	}
}


//===========================================================================================
//  UINotificationComponent::
UINotificationComponent::UINotificationComponent( UINotification &notification ) :
    VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) ),
    Notification( notification )
{
}

eMsgStatus UINotificationComponent::OnEvent_Impl( OvrGuiSys & guiSys, VrFrame const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
    switch( event.EventType )
    {
        case VRMENU_EVENT_FRAME_UPDATE:
            return Frame( guiSys, vrFrame, self, event );
        default:
            OVR_ASSERT( !"Event flags mismatch!" );
            return MSG_STATUS_ALIVE;
    }
}

eMsgStatus UINotificationComponent::Frame( OvrGuiSys & guiSys, VrFrame const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event )
{
	Notification.Update( vrFrame.DeltaSeconds );
	return MSG_STATUS_ALIVE;
}


} // namespace OVR
