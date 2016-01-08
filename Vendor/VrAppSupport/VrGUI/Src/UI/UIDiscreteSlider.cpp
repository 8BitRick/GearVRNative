/************************************************************************************

Filename    :   UIDiscreteSlider.cpp
Content     :
Created     :	10/04/2015
Authors     :   Warsam Osman

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#include "UIDiscreteSlider.h"
#include "UIMenu.h"


namespace OVR {

const char * UIDiscreteSliderComponent::TYPE_NAME = "UIDiscreteSliderComponent";

UIDiscreteSlider::UIDiscreteSlider( OvrGuiSys &guiSys )
	: UIContainer( guiSys )
	, MaxValue( 0 )
	, StartValue( 0 )
	, DiscreteSliderComponent( NULL )
	, CellOnTexture()
	, CellOffTexture()
	, CellOnColor( 1.0f )
	, CellOffColor( 1.0f )
	, OnReleaseFunction( NULL )
	, OnReleaseObject( NULL )
{

}

UIDiscreteSlider::~UIDiscreteSlider()
{

}

void UIDiscreteSlider::AddSliderToMenu( UIMenu *menu, UIObject *parent /*= NULL */ )
{
	AddToMenu( menu, parent );
}

void UIDiscreteSlider::AddCells( unsigned int maxValue, unsigned int startValue, float cellSpacing )
{
	MaxValue = maxValue;
	StartValue = startValue;

	DiscreteSliderComponent = new UIDiscreteSliderComponent( *this, StartValue );
	OVR_ASSERT( DiscreteSliderComponent );
	AddComponent( DiscreteSliderComponent );

	float cellOffset = 0.0f;
	const float pixelCellSpacing = cellSpacing * VRMenuObject::DEFAULT_TEXEL_SCALE;

	VRMenuFontParms fontParms( HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, false, 1.0f );
	Vector3f defaultScale( 1.0f );
	
	for ( unsigned int cellIndex = 0; cellIndex <= MaxValue; ++cellIndex )
	{
		const Posef pose( Quatf( Vector3f( 0.0f, 1.0f, 0.0f ), 0.0f ),
			Vector3f( cellOffset, 0.f, 0.0f ) );

		cellOffset += pixelCellSpacing;

		VRMenuObjectParms cellParms( VRMENU_BUTTON, Array< VRMenuComponent* >(), VRMenuSurfaceParms(),
			"", pose, defaultScale, fontParms, Menu->AllocId(),
			VRMenuObjectFlags_t(), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

		UICell * cellObject = new UICell( GuiSys );
		cellObject->AddToDiscreteSlider( Menu, this, cellParms );
		cellObject->SetImage( 0, SURFACE_TEXTURE_DIFFUSE, CellOffTexture );
		UICellComponent * cellComp = new UICellComponent( *DiscreteSliderComponent, cellIndex );

		VRMenuObject * object = cellObject->GetMenuObject();
		OVR_ASSERT( object );
		object->AddComponent( cellComp );

		DiscreteSliderComponent->AddCell( cellObject );
	}

	DiscreteSliderComponent->HighlightCells( StartValue );
}

void UIDiscreteSlider::ScaleCurrentValue( float scale )
{
	OVR_ASSERT( DiscreteSliderComponent );
	if ( scale >= 0.0f && scale <= 1.0f )
	{
		const unsigned int maxLevel = DiscreteSliderComponent->GetCellsCount();
		DiscreteSliderComponent->SetCurrentValue( static_cast<unsigned int>( maxLevel * scale ) );
		DiscreteSliderComponent->SetCurrentValue( maxLevel * scale );
	}
	else
	{
		WARN( "UIDiscreteSlider::SetCurrentValue passed non normal value: %f", scale );
	}
}

void UIDiscreteSlider::SetOnRelease( void( *callback )( UIDiscreteSlider *, void *, float ), void *object )
{
	OnReleaseFunction = callback;
	OnReleaseObject = object;
}

void UIDiscreteSlider::SetCellTextures( const UITexture & cellOnTexture, const UITexture & cellOffTexture )
{
	CellOnTexture = cellOnTexture;
	CellOffTexture = cellOffTexture;
}

void UIDiscreteSlider::SetCellColors( const Vector4f & cellOnColor, const Vector4f & cellOffColor )
{
	CellOnColor = cellOnColor;
	CellOffColor = cellOffColor;
}

void UIDiscreteSlider::OnRelease( unsigned int currentValue )
{
	if ( OnReleaseFunction && MaxValue > 0.0f )
	{
		( *OnReleaseFunction )( this, OnReleaseObject, static_cast< float >( currentValue ) / static_cast< float >( MaxValue ) );
	}
}

UICell::UICell( OvrGuiSys &guiSys )
	: UIObject( guiSys )
{
}

void UICell::AddToDiscreteSlider( UIMenu *menu, UIObject *parent, VRMenuObjectParms & cellParms )
{
	AddToMenuWithParms( menu, parent, cellParms );
}

void UIDiscreteSliderComponent::OnCellSelect( UICellComponent & cell )
{
	CurrentValue = cell.GetValue();
	DiscreteSlider.OnRelease( CurrentValue );
}

void UIDiscreteSliderComponent::OnCellFocusOn( UICellComponent & cell )
{
	HighlightCells( cell.GetValue() );
}

void UIDiscreteSliderComponent::OnCellFocusOff( UICellComponent & cell )
{
	HighlightCells( CurrentValue );
}

void UIDiscreteSliderComponent::HighlightCells( unsigned int stopIndex )
{
	for ( unsigned int cellIndex = 0; cellIndex < Cells.GetSize(); ++cellIndex )
	{
		UIObject * cell = Cells.At( cellIndex );
		
		if ( cellIndex <= stopIndex )
		{
			cell->SetImage( 0, SURFACE_TEXTURE_DIFFUSE, DiscreteSlider.CellOnTexture );
			cell->SetColor( DiscreteSlider.CellOnColor );
		}
		else
		{
			cell->SetImage( 0, SURFACE_TEXTURE_DIFFUSE, DiscreteSlider.CellOffTexture );
			cell->SetColor( DiscreteSlider.CellOffColor );
		}
	}
}

void UIDiscreteSliderComponent::SetCurrentValue( unsigned int value )
{
	if ( value >= 0 && value <= Cells.GetSize() )
	{
		CurrentValue = value;
		HighlightCells( value );
	}
	else
	{
		WARN( "UIDiscreteSliderComponent::SetCurrentValue - %d outside range %d -> %d", value, 0, Cells.GetSize() );
	}
}

UIDiscreteSliderComponent::UIDiscreteSliderComponent( UIDiscreteSlider & discreteSlider, unsigned int startValue )
	: VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_TOUCH_DOWN ) |
						VRMENU_EVENT_TOUCH_UP |
						VRMENU_EVENT_FOCUS_GAINED |
						VRMENU_EVENT_FOCUS_LOST )
	, DiscreteSlider( discreteSlider )
	, CurrentValue( startValue )
{
	
}

void UIDiscreteSliderComponent::AddCell( UIObject * cellObject )
{
	Cells.PushBack( cellObject );
}

OVR::eMsgStatus UIDiscreteSliderComponent::OnEvent_Impl( OvrGuiSys & guiSys, VrFrame const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
{
	return MSG_STATUS_ALIVE;
}

const char * UICellComponent::TYPE_NAME = "UICellComponent";

UICellComponent::UICellComponent( UIDiscreteSliderComponent & sliderComponent, unsigned int val )
: VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_TOUCH_UP ) |
						VRMENU_EVENT_FOCUS_GAINED |
						VRMENU_EVENT_FOCUS_LOST )
	, SliderComponent( sliderComponent ) 
	, Value( val )
	, OnClickFunction( &UIDiscreteSliderComponent::OnCellSelect )
	, OnFocusGainedFunction( &UIDiscreteSliderComponent::OnCellFocusOn )
	, OnFocusLostFunction( &UIDiscreteSliderComponent::OnCellFocusOff )
{

}

OVR::eMsgStatus UICellComponent::OnEvent_Impl( OvrGuiSys & guiSys, VrFrame const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
{
	switch ( event.EventType )
	{
	case VRMENU_EVENT_FOCUS_GAINED:
		( SliderComponent.*OnFocusGainedFunction )( *this );
		return MSG_STATUS_ALIVE;
	case VRMENU_EVENT_FOCUS_LOST:
		( SliderComponent.*OnFocusLostFunction )( *this );
		return MSG_STATUS_ALIVE;
	case VRMENU_EVENT_TOUCH_UP:
		( SliderComponent.*OnClickFunction )( *this );
		return MSG_STATUS_CONSUMED;
	default:
		OVR_ASSERT( !"Event flags mismatch!" );
		return MSG_STATUS_ALIVE;
	}
}

}