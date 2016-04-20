/************************************************************************************

Filename    :   VRMenu.cpp
Content     :   Class that implements the basic framework for a VR menu, holds all the
				components for a single menu, and updates the VR menu event handler to
				process menu events for a single menu.
Created     :   June 30, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

// includes required for VRMenu.h

#include "VRMenu.h"

#include "Input.h"
#include "VRMenuMgr.h"
#include "VRMenuEventHandler.h"
#include "App.h"
#include "GuiSys.h"
#include "SystemActivities.h"

namespace OVR {

char const * VRMenu::MenuStateNames[MENUSTATE_MAX] =
{
	"MENUSTATE_OPENING",
	"MENUSTATE_OPEN",
	"MENUSTATE_CLOSING",
	"MENUSTATE_CLOSED"
};

// singleton so that other ids initialized during static initialization can be based off tis
VRMenuId_t VRMenu::GetRootId()
{
	VRMenuId_t ID_ROOT( -1 );
	return ID_ROOT;
}

//==============================
// VRMenu::VRMenu
VRMenu::VRMenu( char const * name ) :
	CurMenuState( MENUSTATE_CLOSED ),
	NextMenuState( MENUSTATE_CLOSED ),
	EventHandler( NULL ),
	Name( name ),
	MenuDistance( 1.45f ),
	IsInitialized( false ),
	ComponentsInitialized( false )
{
	EventHandler = new VRMenuEventHandler;
}

//==============================
// VRMenu::~VRMenu
VRMenu::~VRMenu()
{
	delete EventHandler;
	EventHandler = NULL;
}

//==============================
// VRMenu::Create
VRMenu * VRMenu::Create( char const * menuName )
{
	return new VRMenu( menuName );
}

//==============================
// VRMenu::Init
void VRMenu::Init( OvrGuiSys & guiSys, float const menuDistance, VRMenuFlags_t const & flags, Array< VRMenuComponent* > comps /*= Array< VRMenuComponent* >( )*/ )
{
	OVR_ASSERT( !RootHandle.IsValid() );
	OVR_ASSERT( !Name.IsEmpty() );

	Flags = flags;
	MenuDistance = menuDistance;

	// create an empty root item
	VRMenuObjectParms rootParms( VRMENU_CONTAINER, comps,
            VRMenuSurfaceParms( "root" ), "Root", 
			Posef(), Vector3f( 1.0f, 1.0f, 1.0f ), VRMenuFontParms(), 
			GetRootId(), VRMenuObjectFlags_t(), VRMenuObjectInitFlags_t() );
	RootHandle = guiSys.GetVRMenuMgr().CreateObject( rootParms );
	VRMenuObject * root = guiSys.GetVRMenuMgr().ToObject( RootHandle );
	if ( root == NULL )
	{
		WARN( "RootHandle (%llu) is invalid!", RootHandle.Get() );
		return;
	}

	IsInitialized = true;
	ComponentsInitialized = false;
}

void VRMenu::InitWithItems( OvrGuiSys & guiSys, float const menuDistance, VRMenuFlags_t const & flags, Array< VRMenuObjectParms const * > & itemParms )
{
	Init( guiSys, menuDistance, flags );

	if ( Init_Impl( guiSys, menuDistance, flags, itemParms ) )
	{
		AddItems( guiSys, itemParms, GetRootHandle(), true );
	}
}

//==============================================================
// ChildParmsPair
class ChildParmsPair
{
public:
	ChildParmsPair( menuHandle_t const handle, VRMenuObjectParms const * parms ) :
		Handle( handle ),
		Parms( parms )
	{
	}
	ChildParmsPair() : 
		Parms( NULL ) 
	{ 
	}

	menuHandle_t				Handle;
	VRMenuObjectParms const *	Parms;
};

//==============================
// VRMenu::AddItems
void VRMenu::AddItems( OvrGuiSys & guiSys,
        OVR::Array< VRMenuObjectParms const * > & itemParms,
        menuHandle_t parentHandle, bool const recenter ) 
{
	const Vector3f fwd( 0.0f, 0.0f, 1.0f );
	const Vector3f up( 0.0f, 1.0f, 0.0f );
	const Vector3f left( 1.0f, 0.0f, 0.0f );

	// create all items in the itemParms array, add each one to the parent, and position all items
	// without the INIT_FORCE_POSITION flag vertically, one on top of the other
	VRMenuObject * root = guiSys.GetVRMenuMgr().ToObject( parentHandle );
	OVR_ASSERT( root != NULL );

	Array< ChildParmsPair > pairs;

	Vector3f nextItemPos( 0.0f );
	int childIndex = 0;
	for ( int i = 0; i < itemParms.GetSizeI(); ++i )
	{
        VRMenuObjectParms const * parms = itemParms[i];
		// assure all ids are different
		for ( int j = 0; j < itemParms.GetSizeI(); ++j )
		{
			if ( j != i && parms->Id != VRMenuId_t() && parms->Id == itemParms[j]->Id )
			{
				LOG( "Duplicate menu object ids for '%s' and '%s'!",
						parms->Text.ToCStr(), itemParms[j]->Text.ToCStr() );
			}
		}
		menuHandle_t handle = guiSys.GetVRMenuMgr().CreateObject( *parms );
		if ( handle.IsValid() && root != NULL )
		{
			pairs.PushBack( ChildParmsPair( handle, parms ) );
			root->AddChild( guiSys.GetVRMenuMgr(), handle );
			VRMenuObject * obj = guiSys.GetVRMenuMgr().ToObject( root->GetChildHandleForIndex( childIndex++ ) );
			if ( obj != NULL && ( parms->InitFlags & VRMENUOBJECT_INIT_FORCE_POSITION ) == 0 )
			{
				Bounds3f const & lb = obj->GetLocalBounds( guiSys.GetDefaultFont() );
				Vector3f size = lb.GetSize() * obj->GetLocalScale();
				Vector3f centerOfs( left * ( size.x * -0.5f ) );
				if ( !parms->ParentId.IsValid() )	// only contribute to height if not being reparented
				{
					// stack the items
					obj->SetLocalPose( Posef( Quatf(), nextItemPos + centerOfs ) );
					// calculate the total height
					nextItemPos += up * size.y;
				}
				else // otherwise center local to parent
				{
					obj->SetLocalPose( Posef( Quatf(), centerOfs ) );
				}
			}
		}
	}

	// reparent
	Array< menuHandle_t > reparented;
	for ( int i = 0; i < pairs.GetSizeI(); ++i )
	{
		ChildParmsPair const & pair = pairs[i];
		if ( pair.Parms->ParentId.IsValid() )
		{
			menuHandle_t parentHandle = HandleForId( guiSys.GetVRMenuMgr(), pair.Parms->ParentId );
			VRMenuObject * parent = guiSys.GetVRMenuMgr().ToObject( parentHandle );
			if ( parent != NULL )
			{
				root->RemoveChild( guiSys.GetVRMenuMgr(), pair.Handle );
				parent->AddChild( guiSys.GetVRMenuMgr(), pair.Handle );
			}
		}
	}

    if ( recenter )
    {
	    // center the menu based on the height of the auto-placed children
	    float offset = nextItemPos.y * 0.5f;
	    Vector3f rootPos = root->GetLocalPosition();
	    rootPos -= offset * up;
	    root->SetLocalPosition( rootPos );
    }
}

//==============================
// VRMenu::Shutdown
void VRMenu::Shutdown( OvrGuiSys & guiSys )
{
	ASSERT_WITH_TAG( IsInitialized, "VrMenu" );

	Shutdown_Impl( guiSys );

	// this will free the root and all children
	if ( RootHandle.IsValid() )
	{
		guiSys.GetVRMenuMgr().FreeObject( RootHandle );
		RootHandle.Release();
	}
}

//==============================
// VRMenu::RepositionMenu
void VRMenu::RepositionMenu( Matrix4f const & viewMatrix )
{
	if ( Flags & VRMENU_FLAG_TRACK_GAZE )
	{
		MenuPose = CalcMenuPosition( viewMatrix );
	}
	else if ( Flags & VRMENU_FLAG_PLACE_ON_HORIZON )
	{
		MenuPose = CalcMenuPositionOnHorizon( viewMatrix );
	}
}

//==============================
// VRMenu::Frame
void VRMenu::Frame( OvrGuiSys & guiSys, VrFrame const & vrFrame, Matrix4f const & centerViewMatrix )
{
	const Vector3f viewPos( GetViewMatrixPosition( centerViewMatrix ) );
	const Vector3f viewFwd( GetViewMatrixForward( centerViewMatrix ) );

	Array< VRMenuEvent > events;

	if ( !ComponentsInitialized )
	{
		EventHandler->InitComponents( events );
		ComponentsInitialized = true;
	}

	// note we never enter this block unless our next state is different -- the switch statement
	// inside this block is dependent on this.
	if ( NextMenuState != CurMenuState )
	{
		LOG( "NextMenuState for '%s': %s", GetName(), MenuStateNames[NextMenuState] );
		switch( NextMenuState )
		{
            case MENUSTATE_OPENING:
				OVR_ASSERT( CurMenuState != NextMenuState ); // logic below is dependent on this!!
				if ( CurMenuState == MENUSTATE_OPEN )
				{
					// we're already open, so just set next to OPEN, too so we don't do any transition
					NextMenuState = MENUSTATE_OPEN;	
				}
				else
				{
					RepositionMenu( centerViewMatrix );
					EventHandler->Opening( events );
					Open_Impl( guiSys );
				}
                break;
			case MENUSTATE_OPEN:
				{
					OVR_ASSERT( CurMenuState != NextMenuState ); // logic below is dependent on this!!
					if ( CurMenuState != MENUSTATE_OPENING ) { LOG( "Instant open" ); }
					OpenSoundLimiter.PlayMenuSound( guiSys, Name.ToCStr(), "sv_release_active", 0.1 );
					EventHandler->Opened( events );				
				}
				break;
            case MENUSTATE_CLOSING:
				OVR_ASSERT( CurMenuState != NextMenuState ); // logic below is dependent on this!!
				if ( CurMenuState == MENUSTATE_CLOSED )
				{
					// we're already closed, so just set next to CLOSED, too so we don't do any transition
					NextMenuState = MENUSTATE_CLOSED;	
				}
				else
				{
				EventHandler->Closing( events );
				}
                break;
			case MENUSTATE_CLOSED:
				{
					OVR_ASSERT( CurMenuState != NextMenuState ); // logic below is dependent on this!!
					if ( CurMenuState != MENUSTATE_CLOSING ) { LOG( "Instant close" ); }
					CloseSoundLimiter.PlayMenuSound( guiSys, Name.ToCStr(), "sv_deselect", 0.1 );
					EventHandler->Closed( events );
					Close_Impl( guiSys );
				}
				break;
            default:
                OVR_ASSERT( !"Unhandled menu state!" );
                break;
		}
		CurMenuState = NextMenuState;
	}

    switch( CurMenuState )
    {
        case MENUSTATE_OPENING:
			if ( IsFinishedOpening() )
			{
				NextMenuState = MENUSTATE_OPEN;
			}
            break;
        case MENUSTATE_OPEN:
            break;
        case MENUSTATE_CLOSING:
			if ( IsFinishedClosing() )
			{
				NextMenuState = MENUSTATE_CLOSED;
			}
            break;
        case MENUSTATE_CLOSED:
	        // handle remaining events -- note focus path is empty right now, but this may still broadcast messages to controls
		    EventHandler->HandleEvents( guiSys, vrFrame, RootHandle, events );	
		    return;
        default:
            OVR_ASSERT( !"Unhandled menu state!" );
            break;
	}

	if ( Flags & VRMENU_FLAG_TRACK_GAZE )
	{
		MenuPose = CalcMenuPosition( centerViewMatrix );
	}
	else if ( Flags & VRMENU_FLAG_TRACK_GAZE_HORIZONTAL )
	{
		MenuPose = CalcMenuPositionOnHorizon( centerViewMatrix );
	}

	Frame_Impl( guiSys, vrFrame );

	EventHandler->Frame( guiSys, vrFrame, RootHandle, MenuPose, events );

	EventHandler->HandleEvents( guiSys, vrFrame, RootHandle, events );

	VRMenuObject * root = guiSys.GetVRMenuMgr().ToObject( RootHandle );		
	if ( root != NULL )
	{
		VRMenuRenderFlags_t renderFlags;
		guiSys.GetVRMenuMgr().SubmitForRendering( guiSys, centerViewMatrix, RootHandle, MenuPose, renderFlags );
	}

}

//==============================
// VRMenu::OnKeyEvent
bool VRMenu::OnKeyEvent( OvrGuiSys & guiSys, int const keyCode, const int repeatCount, KeyEventType const eventType )
{
    if ( OnKeyEvent_Impl( guiSys, keyCode, repeatCount, eventType ) )
    {
        // consumed by sub class
        return true;
    }

	if ( keyCode == OVR_KEY_BACK )
	{
		LOG( "VRMenu '%s' Back key event: %s", GetName(), KeyEventNames[eventType] );

		switch( eventType )
		{
			case KEY_EVENT_LONG_PRESS:
				return false;
			case KEY_EVENT_DOWN:
				return false;
			case KEY_EVENT_SHORT_PRESS:
				if ( IsOpenOrOpening() )
				{
					if ( Flags & VRMENU_FLAG_BACK_KEY_EXITS_APP )
					{
						// FIXME: we REALLY should require clients of VrGUI to do this themselves and not make this a default behavior
						// in part because it's a hidden behavior switched on by confusing flags and also because it creates a 
						// dependency in VrGUI now on the SystemUtils library.
						guiSys.GetApp()->StartSystemActivity( PUI_CONFIRM_QUIT );
						return true;
					}
					else if ( Flags & VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP )
					{
						return false;
					}
					else if ( !( Flags & VRMENU_FLAG_BACK_KEY_DOESNT_EXIT ) )
					{
						Close( guiSys );
						return true;
					}
				}
				return false;
			case KEY_EVENT_DOUBLE_TAP:
			default:
				return false;
		}
	}
	return false;
}

//==============================
// VRMenu::Open
void VRMenu::Open( OvrGuiSys & guiSys )
{
	LOG( "VRMenu::Open - '%s', pre - c: %s n: %s", GetName(), MenuStateNames[CurMenuState], MenuStateNames[NextMenuState] );
	if ( CurMenuState == MENUSTATE_OPENING ) 
	{ 
		// this is a NOP, never allow transitioning back to the same state
		return; 
	}
	NextMenuState = MENUSTATE_OPENING;
	guiSys.MakeActive( this );
	LOG( "VRMenu::Open - %s, post - c: %s n: %s", GetName(), MenuStateNames[CurMenuState], MenuStateNames[NextMenuState] );
}

//==============================
// VRMenu::Close
void VRMenu::Close( OvrGuiSys & guiSys, bool const instant )
{
	LOG( "VRMenu::Close - %s, pre - c: %s n: %s", GetName(), MenuStateNames[CurMenuState], MenuStateNames[NextMenuState] );
	if ( CurMenuState == MENUSTATE_CLOSING )
	{ 
		// this is a NOP, never allow transitioning back to the same state
		return; 
	}
	NextMenuState = instant ? MENUSTATE_CLOSED : MENUSTATE_CLOSING;
	LOG( "VRMenu::Close - %s, post - c: %s n: %s", GetName(), MenuStateNames[CurMenuState], MenuStateNames[NextMenuState] );
}

//==============================
// VRMenu::HandleForId
menuHandle_t VRMenu::HandleForId( OvrVRMenuMgr & menuMgr, VRMenuId_t const id ) const
{
	VRMenuObject * root = menuMgr.ToObject( RootHandle );
	return root->ChildHandleForId( menuMgr, id );
}

//==============================
// VRMenu::CalcMenuPosition
Posef VRMenu::CalcMenuPosition( Matrix4f const & viewMatrix ) const
{
	const Matrix4f invViewMatrix = viewMatrix.Inverted();
	const Vector3f viewPos( GetViewMatrixPosition( viewMatrix ) );
	const Vector3f viewFwd( GetViewMatrixForward( viewMatrix ) );

	// spawn directly in front 
	Quatf rotation( -viewFwd, 0.0f );
	Quatf viewRot( invViewMatrix );
	Quatf fullRotation = rotation * viewRot;
	fullRotation.Normalize();

	Vector3f position( viewPos + viewFwd * MenuDistance );

	return Posef( fullRotation, position );
}

//==============================
// VRMenu::CalcMenuPositionOnHorizon
Posef VRMenu::CalcMenuPositionOnHorizon( Matrix4f const & viewMatrix ) const
{
	const Vector3f viewPos( GetViewMatrixPosition( viewMatrix ) );
	const Vector3f viewFwd( GetViewMatrixForward( viewMatrix ) );

	// project the forward view onto the horizontal plane
	Vector3f const up( 0.0f, 1.0f, 0.0f );
	float dot = viewFwd.Dot( up );
	Vector3f horizontalFwd = ( dot < -0.99999f || dot > 0.99999f ) ? Vector3f( 1.0f, 0.0f, 0.0f ) : viewFwd - ( up * dot );
	horizontalFwd.Normalize();

	Matrix4f horizontalViewMatrix = Matrix4f::LookAtRH( Vector3f( 0 ), horizontalFwd, up );
	horizontalViewMatrix.Transpose();	// transpose because we want the rotation opposite of where we're looking

	// this was only here to test rotation about the local axis
	//Quatf rotation( -horizontalFwd, 0.0f );

	Quatf viewRot( horizontalViewMatrix );
	Quatf fullRotation = /*rotation * */viewRot;
	fullRotation.Normalize();

	Vector3f position( viewPos + horizontalFwd * MenuDistance );

	return Posef( fullRotation, position );
}

//==============================
// VRMenu::OnItemEvent
void VRMenu::OnItemEvent( OvrGuiSys & guiSys, VRMenuId_t const itemId, VRMenuEvent const & event )
{
	OnItemEvent_Impl( guiSys, itemId, event );
}

//==============================
// VRMenu::Init_Impl
bool VRMenu::Init_Impl( OvrGuiSys & guiSys, float const menuDistance, 
                VRMenuFlags_t const & flags, Array< VRMenuObjectParms const * > & itemParms )
{
	return true;
}

//==============================
// VRMenu::Shutdown_Impl
void VRMenu::Shutdown_Impl( OvrGuiSys & guiSys )
{
}

//==============================
// VRMenu::Frame_Impl
void VRMenu::Frame_Impl( OvrGuiSys & guiSys, VrFrame const & vrFrame )
{
}

//==============================
// VRMenu::OnKeyEvent_Impl
bool VRMenu::OnKeyEvent_Impl( OvrGuiSys & guiSys, int const keyCode, const int repeatCount, KeyEventType const eventType )
{
	return false;
}

//==============================
// VRMenu::Open_Impl
void VRMenu::Open_Impl( OvrGuiSys & guiSys )
{
}

//==============================
// VRMenu::Close_Impl
void VRMenu::Close_Impl( OvrGuiSys & guiSys )
{
}

//==============================
// VRMenu::OnItemEvent_Impl
void VRMenu::OnItemEvent_Impl( OvrGuiSys & guiSys, VRMenuId_t const itemId, VRMenuEvent const & event )
{
}

//==============================
// VRMenu::GetFocusedHandle()
menuHandle_t VRMenu::GetFocusedHandle() const
{
	if ( EventHandler != NULL )
	{
		return EventHandler->GetFocusedHandle(); 
	}
	return menuHandle_t();
}

//==============================
// VRMenu::ResetMenuOrientation
void VRMenu::ResetMenuOrientation( Matrix4f const & viewMatrix )
{
	LOG( "ResetMenuOrientation for '%s'", GetName() );
	ResetMenuOrientation_Impl( viewMatrix );
}

//==============================
// VRMenuResetMenuOrientation_Impl
void VRMenu::ResetMenuOrientation_Impl( Matrix4f const & viewMatrix )
{
	RepositionMenu( viewMatrix );
}

} // namespace OVR

