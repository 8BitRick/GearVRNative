/************************************************************************************

Filename    :   SceneView.h
Content     :   Basic viewing and movement in a scene.
Created     :   December 19, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef SCENEVIEW_H
#define SCENEVIEW_H

#include "ModelFile.h"
#include "Input.h"		// VrFrame, etc

namespace OVR
{

//-----------------------------------------------------------------------------------
// ModelInScene
// 
class ModelInScene
{
public:
			ModelInScene() :
				Definition( NULL ) {}

	void	SetModelFile( const ModelFile * mf );
	void	AnimateJoints( const float timeInSeconds );

	ModelState			State;		// passed to rendering code
	const ModelFile	*	Definition;	// will not be freed by OvrSceneView
};

//-----------------------------------------------------------------------------------
// OvrSceneView
// 
class OvrSceneView
{
public:
							OvrSceneView();

	// The default view will be located at the origin, looking down the -Z axis,
	// with +X to the right and +Y up.
	// Increasing yaw looks to the left (rotation around Y axis).

	// loads the default GL shader programs
	ModelGlPrograms			GetDefaultGLPrograms();

	// Blocking load of a scene from the filesystem.
	// This model will be freed when a new world model is set.
    void					LoadWorldModel( const char * sceneFileName, const MaterialParms & materialParms );
    void					LoadWorldModelFromApplicationPackage( const char * sceneFileName, const MaterialParms & materialParms );

	// Set an already loaded scene, which will not be freed when a new
	// world model is set.
	void					SetWorldModel( ModelFile & model );
	ModelInScene			GetWorldModel() { return WorldModel; }

	// Passed on to world model
	ovrSurfaceDef *			FindNamedSurface( const char * name ) const;
	const ModelTexture *	FindNamedTexture( const char * name ) const;
	const ModelTag *		FindNamedTag( const char * name ) const;
	Bounds3f				GetBounds() const;

	// Returns the new modelIndex
	int						AddModel( ModelInScene * model );
	void					RemoveModelIndex( int index );

	void					PauseAnimations( bool pauseAnimations ) { Paused = pauseAnimations; }

	// Allow movement inside the scene based on the joypad.
	// Models that have DontRenderForClientUid == supressModelsWithClientId will be skipped
	// to prevent the client's own head model from drawing in their view.
	void					Frame(	const VrFrame & vrFrame,
									const ovrHeadModelParms & headModelParms_,
									const long long supressModelsWithClientId = -1 );

	// Issues GL calls and returns the view-projection matrix for the eye, as needed by AppInterface DrawEyeVIew
	Matrix4f				DrawEyeView( const int eye, const float fovDegreesX, const float fovDegreesY ) const;

	// Systems that want to manage individual surfaces instead of complete models
	// can add surfaces to this list during Frame().  They will be drawn for
	// both eyes, then the list will be cleared.
	Array<ovrDrawSurface> &	GetEmitList() { return EmitSurfaces; }

	float					GetEyeYaw() const { return EyeYaw; }
	float					GetEyePitch() const { return EyePitch; }
	float					GetEyeRoll() const { return EyeRoll; }

	float					GetYawOffset() const { return SceneYaw; }
	void					SetYawOffset( const float yaw ) { EyeYaw += (yaw-SceneYaw); SceneYaw = yaw; }

	float					GetZnear() const { return Znear; }

	void					SetMoveSpeed( const float speed ) { MoveSpeed = speed; }
	void					SetFreeMove( const bool allowFreeMovement ) { FreeMove = allowFreeMovement; }

	// Derived from state after last Frame()
	const Vector3f &		GetFootPos() const { return FootPos; }
	void					SetFootPos( const Vector3f & pos ) { FootPos = pos; UpdateCenterEye(); }

	Vector3f				GetNeutralHeadCenter() const;		// FootPos + HeadModelParms.EyeHeight
	Vector3f				GetCenterEyePosition() const;
	Vector3f				GetCenterEyeForward() const;
	Matrix4f				GetCenterEyeTransform() const;
	Matrix4f 				GetCenterEyeViewMatrix() const;

	Matrix4f 				GetEyeViewMatrix( const int eye ) const;
	Matrix4f 				GetEyeProjectionMatrix( const int eye, const float fovDegreesX, const float fovDegreesY ) const;
	Matrix4f 				GetEyeViewProjectionMatrix( const int eye, const float fovDegreesX, const float fovDegreesY ) const;

	ovrMatrix4f				GetExternalVelocity() const;

	// When head tracking is reset, any joystick offsets should be cleared
	// so the viewer is looking ehere the application wants.
	void					ClearStickAngles();

	void					UpdateCenterEye();

private:
    void                    LoadWorldModel( const char * sceneFileName, const MaterialParms & materialParms, const bool fromApk );

	// The only ModelInScene that OvrSceneView actually owns.
	bool					FreeWorldModelOnChange;
	ModelInScene			WorldModel;
	long long				SceneId;		// for network identification

	// Entries can be NULL.
	// None of these will be directly freed by OvrSceneView.
	Array<ModelInScene *>	Models;

	// Externally generated surfaces
	Array<ovrDrawSurface>		EmitSurfaces;

	// Rendered surfaces.
	mutable Array<ovrDrawSurface>	DrawSurfaceList;

	GlProgram				ProgVertexColor;
	GlProgram				ProgSingleTexture;
	GlProgram				ProgLightMapped;
	GlProgram				ProgReflectionMapped;
	GlProgram				ProgSkinnedVertexColor;
	GlProgram				ProgSkinnedSingleTexture;
	GlProgram				ProgSkinnedLightMapped;
	GlProgram				ProgSkinnedReflectionMapped;
	bool					LoadedPrograms;

	ModelGlPrograms			GlPrograms;

	// Don't animate if true.
	bool					Paused;

	// Updated each Frame()
	ovrHeadModelParms		HeadModelParms;
	long long				SupressModelsWithClientId;

	float					Znear;

	// Angle offsets in radians for joystick movement, which is
	// the moral equivalent of head tracking.  Reset head tracking
	// should also clear these.
	float					StickYaw;		// added on top of the sensor reading
	float					StickPitch;		// only applied if the tracking sensor isn't active

	// An application can turn the primary view direction, which is where
	// the view will go if head tracking is reset.  Should only be changed
	// at discrete transition points to avoid sickness.
	float					SceneYaw;

	// Applied one frame later to avoid bounce-back from async time warp yaw velocity prediction.
	float					YawVelocity;

	// 3.0 m/s by default.  Different apps may want different move speeds
	float					MoveSpeed;

	// Allows vertical movement when holding right shoulder button
	bool					FreeMove;

	// Modified by joypad movement and collision detection
	Vector3f				FootPos;

	// Calculated in Frame()
	ovrMatrix4f				CenterEyeTransform;
	ovrMatrix4f				CenterEyeViewMatrix;
	float					EyeYaw;				// Rotation around Y, CCW positive when looking at RHS (X,Z) plane.
	float					EyePitch;			// Pitch. If sensor is plugged in, only read from sensor.
	float					EyeRoll;			// Roll, only read from sensor.
	ovrTracking				CurrentTracking;
};

}	// namespace OVR

#endif // SCENEVIEW_H
