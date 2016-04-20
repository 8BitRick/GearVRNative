/************************************************************************************

Filename    :   VRMenuMgr.h
Content     :   Menuing system for VR apps.
Created     :   May 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_VRMenuMgr_h )
#define OVR_VRMenuMgr_h

#include "VRMenuObject.h"

namespace OVR {

class BitmapFont;
class BitmapFontSurface;
struct GlProgram;
class OvrDebugLines;

//==============================================================
// OvrVRMenuMgr
class OvrVRMenuMgr
{
public:
	virtual						~OvrVRMenuMgr() { }

	static OvrVRMenuMgr *		Create( OvrGuiSys & guiSys );
	static void					Destroy( OvrVRMenuMgr * & mgr );

	// Initialize the VRMenu system
	virtual void				Init( OvrGuiSys & guiSys ) = 0;
	// Shutdown the VRMenu syatem
	virtual void				Shutdown() = 0;

	// creates a new menu object
	virtual menuHandle_t		CreateObject( VRMenuObjectParms const & parms ) = 0;
	// Frees a menu object.  If the object is a child of a parent object, this will
	// also remove the child from the parent.
	virtual void				FreeObject( menuHandle_t const handle ) = 0;
	// Returns true if the handle is valid.
	virtual bool				IsValid( menuHandle_t const handle ) const = 0;
	// Return the object for a menu handle or NULL if the object does not exist or the
	// handle is invalid;
	virtual VRMenuObject	*	ToObject( menuHandle_t const handle ) const = 0;

	// Submits the specified menu object and its children
	virtual void				SubmitForRendering( OvrGuiSys & guiSys, Matrix4f const & centerViewMatrix, 
										menuHandle_t const handle, Posef const & worldPose, 
										VRMenuRenderFlags_t const & flags ) = 0;

	// Call once per frame before rendering to sort surfaces.
	virtual void				Finish( Matrix4f const & viewMatrix ) = 0;
#if 1
	virtual void 				RenderEyeView( Matrix4f const & centerViewMatrix, 
										Matrix4f const & viewMatrix, 
										Matrix4f const & projectionMatrix, 
										Array< ovrDrawSurface > & surfaceList ) const = 0;
#else
	// Render's all objects that have been submitted on the current frame.
	virtual void				RenderSubmitted( Matrix4f const & worldMVP, Matrix4f const & viewMatrix, 
										Array< ovrDrawSurface > & surfaceList ) const = 0;
#endif
    virtual GlProgram const *   GetGUIGlProgram( eGUIProgramType const programType ) const = 0;
};

} // namespace OVR

#endif // OVR_VRMenuMgr_h
