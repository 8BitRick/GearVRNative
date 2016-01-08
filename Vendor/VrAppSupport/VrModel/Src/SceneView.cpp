/************************************************************************************

Filename    :   SceneView.cpp
Content     :   Basic viewing and movement in a scene.
Created     :   December 19, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "SceneView.h"

#include "Kernel/OVR_LogUtils.h"

#include "VrApi.h"
#include "VrApi_Helpers.h"

namespace OVR
{

void ModelInScene::SetModelFile( const ModelFile * mf ) 
{ 
	Definition = mf;
	State.modelDef = ( mf != NULL ) ? &mf->Def : NULL;
	State.Joints.Resize( ( mf != NULL ) ? mf->GetJointCount() : 0 );
};

void ModelInScene::AnimateJoints( const float timeInSeconds )
{
	if ( Definition == NULL )
	{
		return;
	}
	for ( int i = 0; i < Definition->GetJointCount(); i++ )
	{
		const ModelJoint * joint = Definition->GetJoint( i );
		if ( joint->animation == MODEL_JOINT_ANIMATION_NONE )
		{
			continue;
		}

		float time = ( timeInSeconds + joint->timeOffset ) * joint->timeScale;

		switch( joint->animation )
		{
			case MODEL_JOINT_ANIMATION_SWAY:
			{
				time = sinf( time * Math<float>::Pi );
				// NOTE: fall through
			}
			case MODEL_JOINT_ANIMATION_ROTATE:
			{
				const Vector3f angles = joint->parameters * ( Math<float>::DegreeToRadFactor * time );
				const Matrix4f matrix = joint->transform *
										Matrix4f::RotationY( angles.y ) *
										Matrix4f::RotationX( angles.x ) *
										Matrix4f::RotationZ( angles.z ) *
										joint->transform.Inverted();
				State.Joints[i] = matrix;
				break;
			}
			case MODEL_JOINT_ANIMATION_BOB:
			{
				const float frac = sinf( time * Math<float>::Pi );
				const Vector3f offset = joint->parameters * frac;
				const Matrix4f matrix = joint->transform *
										Matrix4f::Translation( offset ) *
										joint->transform.Inverted();
				State.Joints[i] = matrix;
				break;
			}
			case MODEL_JOINT_ANIMATION_NONE:
				break;
		}
	}
}

//-------------------------------------------------------------------------------------

OvrSceneView::OvrSceneView() :
	FreeWorldModelOnChange( false ),
	SceneId( 0 ),
	LoadedPrograms( false ),
	Paused( false ),
	SupressModelsWithClientId( -1 ),
	Znear( VRAPI_ZNEAR ),
	StickYaw( 0.0f ),
	StickPitch( 0.0f ),
	SceneYaw( 0.0f ),
	YawVelocity( 0.0f ),
	MoveSpeed( 3.0f ),
	FreeMove( false ),
	FootPos( 0.0f ),
	EyeYaw( 0.0f ),
	EyePitch( 0.0f ),
	EyeRoll( 0.0f )
{
	CenterEyeTransform = ovrMatrix4f_CreateIdentity();
	CenterEyeViewMatrix = ovrMatrix4f_CreateIdentity();
}

ModelGlPrograms OvrSceneView::GetDefaultGLPrograms()
{
	ModelGlPrograms programs;

	if ( !LoadedPrograms )
	{
		ProgVertexColor				= BuildProgram( VertexColorVertexShaderSrc, VertexColorFragmentShaderSrc );
		ProgSingleTexture			= BuildProgram( SingleTextureVertexShaderSrc, SingleTextureFragmentShaderSrc );
		ProgLightMapped				= BuildProgram( LightMappedVertexShaderSrc, LightMappedFragmentShaderSrc );
		ProgReflectionMapped		= BuildProgram( ReflectionMappedVertexShaderSrc, ReflectionMappedFragmentShaderSrc );
		ProgSkinnedVertexColor		= BuildProgram( VertexColorSkinned1VertexShaderSrc, VertexColorFragmentShaderSrc );
		ProgSkinnedSingleTexture	= BuildProgram( SingleTextureSkinned1VertexShaderSrc, SingleTextureFragmentShaderSrc );
		ProgSkinnedLightMapped		= BuildProgram( LightMappedSkinned1VertexShaderSrc, LightMappedFragmentShaderSrc );
		ProgSkinnedReflectionMapped	= BuildProgram( ReflectionMappedSkinned1VertexShaderSrc, ReflectionMappedFragmentShaderSrc );
		LoadedPrograms = true;
	}

	programs.ProgVertexColor				= & ProgVertexColor;
	programs.ProgSingleTexture				= & ProgSingleTexture;
	programs.ProgLightMapped				= & ProgLightMapped;
	programs.ProgReflectionMapped			= & ProgReflectionMapped;
	programs.ProgSkinnedVertexColor			= & ProgSkinnedVertexColor;
	programs.ProgSkinnedSingleTexture		= & ProgSkinnedSingleTexture;
	programs.ProgSkinnedLightMapped			= & ProgSkinnedLightMapped;
	programs.ProgSkinnedReflectionMapped	= & ProgSkinnedReflectionMapped;

	return programs;
}

void OvrSceneView::LoadWorldModel( const char * sceneFileName, const MaterialParms & materialParms, const bool fromApk )
{
    LOG( "OvrSceneView::LoadScene( %s )", sceneFileName );

    if ( GlPrograms.ProgSingleTexture == NULL )
    {
        GlPrograms = GetDefaultGLPrograms();
    }

    ModelFile * model = NULL;
    // Load the scene we are going to draw
    if ( fromApk )
    {
        model = LoadModelFileFromApplicationPackage( sceneFileName, GlPrograms, materialParms );
    }
    else
    {
        model = LoadModelFile( sceneFileName, GlPrograms, materialParms );
    }

    SetWorldModel( *model );

    FreeWorldModelOnChange = true;
}

void OvrSceneView::LoadWorldModelFromApplicationPackage( const char * sceneFileName, const MaterialParms & materialParms )
{
    LoadWorldModel( sceneFileName, materialParms, true );
}

void OvrSceneView::LoadWorldModel( const char * sceneFileName, const MaterialParms & materialParms )
{
    LoadWorldModel( sceneFileName, materialParms, false );
}

void OvrSceneView::SetWorldModel( ModelFile & world )
{
	LOG( "OvrSceneView::SetWorldModel( %s )", world.FileName.ToCStr() );

	if ( FreeWorldModelOnChange && Models.GetSizeI() > 0 )
	{
		delete WorldModel.Definition;
		FreeWorldModelOnChange = false;
	}
	Models.Clear();

	WorldModel.SetModelFile( &world );
	AddModel( &WorldModel );

	// Set the initial player position
	FootPos = Vector3f( 0.0f, 0.0f, 0.0f );
	StickYaw = 0.0f;
	StickPitch = 0.0f;
	SceneYaw = 0.0f;
}

void OvrSceneView::ClearStickAngles()
{
	StickYaw = 0.0f;
	StickPitch = 0.0f;
}

ovrSurfaceDef * OvrSceneView::FindNamedSurface( const char * name ) const
{
	return ( WorldModel.Definition == NULL ) ? NULL : WorldModel.Definition->FindNamedSurface( name );
}

const ModelTexture * OvrSceneView::FindNamedTexture( const char * name ) const
{
	return ( WorldModel.Definition == NULL ) ? NULL : WorldModel.Definition->FindNamedTexture( name );
}

const ModelTag * OvrSceneView::FindNamedTag( const char * name ) const
{
	return ( WorldModel.Definition == NULL ) ? NULL : WorldModel.Definition->FindNamedTag( name );
}

Bounds3f OvrSceneView::GetBounds() const
{
	return ( WorldModel.Definition == NULL ) ?
			Bounds3f( Vector3f( 0, 0, 0 ), Vector3f( 0, 0, 0 ) ) :
			WorldModel.Definition->GetBounds();
}

int OvrSceneView::AddModel( ModelInScene * model )
{
	const int modelsSize = Models.GetSizeI();

	// scan for a NULL entry
	for ( int i = 0; i < modelsSize; ++i )
	{
		if ( Models[i] == NULL )
		{
			Models[i] = model;
			return i;
		}
	}

	Models.PushBack( model );

	return Models.GetSizeI() - 1;
}

void OvrSceneView::RemoveModelIndex( int index )
{
	Models[index] = NULL;
}

Matrix4f OvrSceneView::DrawEyeView( const int eye, const float fovDegreesX, const float fovDegreesY ) const
{
	glEnable( GL_DEPTH_TEST );
	glEnable( GL_CULL_FACE );
	glFrontFace( GL_CCW );

	const Matrix4f centerEyeViewMatrix = GetCenterEyeViewMatrix();
	const Matrix4f viewMatrix = GetEyeViewMatrix( eye );
	const Matrix4f projectionMatrix = GetEyeProjectionMatrix( eye, fovDegreesX, fovDegreesY );

	// Cull the model surfaces using a view and projection matrix that contain both eyes
	// and add the surfaces to the sorted surface list.
	if ( eye == 0 )
	{
		Matrix4f symmetricEyeProjectionMatrix = projectionMatrix;
		symmetricEyeProjectionMatrix.M[0][0] = projectionMatrix.M[0][0] / ( fabs( projectionMatrix.M[0][2] ) + 1.0f );
		symmetricEyeProjectionMatrix.M[0][2] = 0.0f;
 
		const float moveBackDistance = 0.5f * HeadModelParms.InterpupillaryDistance * symmetricEyeProjectionMatrix.M[0][0];
		Matrix4f centerEyeCullViewMatrix = Matrix4f::Translation( 0, 0, -moveBackDistance ) * centerEyeViewMatrix;

		Array< ModelState * > emitModels;
		for ( int i = 0; i < Models.GetSizeI(); i++ )
		{
			emitModels.PushBack( &Models[i]->State );
		}

		BuildModelSurfaceList( DrawSurfaceList, SupressModelsWithClientId, emitModels, EmitSurfaces, centerEyeCullViewMatrix, symmetricEyeProjectionMatrix );
	}

	RenderSurfaceList( DrawSurfaceList, viewMatrix, projectionMatrix );

	return ( projectionMatrix * viewMatrix );
}

Vector3f OvrSceneView::GetNeutralHeadCenter() const
{
	return Vector3f( FootPos.x, FootPos.y + HeadModelParms.EyeHeight, FootPos.z );
}

Vector3f OvrSceneView::GetCenterEyePosition() const
{
	return Vector3f( CenterEyeTransform.M[0][3], CenterEyeTransform.M[1][3], CenterEyeTransform.M[2][3] );
}

Vector3f OvrSceneView::GetCenterEyeForward() const
{
	return Vector3f( -CenterEyeViewMatrix.M[2][0], -CenterEyeViewMatrix.M[2][1], -CenterEyeViewMatrix.M[2][2] );
}

Matrix4f OvrSceneView::GetCenterEyeTransform() const
{
	return CenterEyeTransform;
}

Matrix4f OvrSceneView::GetCenterEyeViewMatrix() const
{
	return CenterEyeViewMatrix;
}

Matrix4f OvrSceneView::GetEyeViewMatrix( const int eye ) const
{
	return vrapi_GetEyeViewMatrix( &HeadModelParms, &CenterEyeViewMatrix, eye );
}

Matrix4f OvrSceneView::GetEyeProjectionMatrix( const int eye, const float fovDegreesX, const float fovDegreesY ) const
{
	OVR_UNUSED( eye );

	// We may want to make per-eye projection matrices if we move away from nearly-centered lenses.
	// Use an infinite projection matrix because, except for things right up against the near plane,
	// it provides better precision:
	//		"Tightening the Precision of Perspective Rendering"
	//		Paul Upchurch, Mathieu Desbrun
	//		Journal of Graphics Tools, Volume 16, Issue 1, 2012
	return ovrMatrix4f_CreateProjectionFov( fovDegreesX, fovDegreesY, 0.0f, 0.0f, Znear, 0.0f );
}

Matrix4f OvrSceneView::GetEyeViewProjectionMatrix( const int eye, const float fovDegreesX, const float fovDegreesY ) const
{
	return GetEyeProjectionMatrix( eye, fovDegreesX, fovDegreesY ) * GetEyeViewMatrix( eye );
}

ovrMatrix4f OvrSceneView::GetExternalVelocity() const
{
	return ovrMatrix4f_CalculateExternalVelocity( &CenterEyeViewMatrix, YawVelocity );
}

// This is called by Frame(), but it must be explicitly called when FootPos is
// updated, or calls to GetCenterEyePosition() won't reflect changes until the
// following frame.
void OvrSceneView::UpdateCenterEye()
{
	const ovrMatrix4f input = Matrix4f::Translation( GetNeutralHeadCenter() ) *
								Matrix4f::RotationY( StickYaw + SceneYaw ) *
								Matrix4f::RotationX( StickPitch );

	CenterEyeTransform = vrapi_GetCenterEyeTransform( &HeadModelParms, &CurrentTracking, &input );
	CenterEyeViewMatrix = ovrMatrix4f_Inverse( &CenterEyeTransform );
}

void OvrSceneView::Frame( const VrFrame & vrFrame,
							const ovrHeadModelParms & headModelParms_,
							const long long supressModelsWithClientId_ )
{
	HeadModelParms = headModelParms_;
	SupressModelsWithClientId = supressModelsWithClientId_;

	CurrentTracking = vrFrame.Tracking;

	// Delta time in seconds since last frame.
	const float dt = vrFrame.DeltaSeconds;
	const float angleSpeed = 1.5f;

	//
	// Player view angles
	//

	// Turn based on the look stick
	// Because this can be predicted ahead by async TimeWarp, we apply
	// the yaw from the previous frame's controls, trading a frame of
	// latency on stick controls to avoid a bounce-back.
	StickYaw -= YawVelocity * dt;
	YawVelocity = angleSpeed * vrFrame.Input.sticks[1][0];

	// Only if there is no head tracking, allow right stick up/down to adjust pitch,
	// which can be useful for debugging without having to dock the device.
	if ( ( vrFrame.Tracking.Status & VRAPI_TRACKING_STATUS_ORIENTATION_TRACKED ) == 0 )
	{
		StickPitch -= angleSpeed * vrFrame.Input.sticks[1][1] * dt;
	}
	else
	{
		StickPitch = 0.0f;
	}

	// We extract Yaw, Pitch, Roll instead of directly using the orientation
	// to allow "additional" yaw manipulation with mouse/controller and scene offsets.
	const Quatf quat = vrFrame.Tracking.HeadPose.Pose.Orientation;

	quat.GetEulerAngles<Axis_Y, Axis_X, Axis_Z>( &EyeYaw, &EyePitch, &EyeRoll );

	// Yaw is modified by both joystick and application-set scene yaw.
	// Pitch is only modified by joystick when no head tracking sensor is active.
	EyeYaw += StickYaw + SceneYaw;
	EyePitch += StickPitch;

	//
	// Player movement
	//

	// Allow up / down movement if there is no floor collision model or in 'free move' mode.
	const bool upDown = ( WorldModel.Definition == NULL || FreeMove ) && ( ( vrFrame.Input.buttonState & BUTTON_RIGHT_TRIGGER ) != 0 );
	const Vector3f gamepadMove(
			vrFrame.Input.sticks[0][0],
			upDown ? -vrFrame.Input.sticks[0][1] : 0.0f,
			upDown ? 0.0f : vrFrame.Input.sticks[0][1] );

	// Perform player movement if there is input.
	if ( gamepadMove.LengthSq() > 0.0f )
	{
		const Matrix4f yawRotate = Matrix4f::RotationY( EyeYaw );
		const Vector3f orientationVector = yawRotate.Transform( gamepadMove );

		// Don't let move get too crazy fast
		const float moveDistance = OVR::Alg::Min<float>( MoveSpeed * (float)dt, 1.0f );
		if ( WorldModel.Definition != NULL && !FreeMove )
		{
			FootPos = SlideMove( FootPos, HeadModelParms.EyeHeight, orientationVector, moveDistance,
						WorldModel.Definition->Collisions, WorldModel.Definition->GroundCollisions );
		}
		else
		{	// no scene loaded, walk without any collisions
			ModelCollision collisionModel;
			ModelCollision groundCollisionModel;
			FootPos = SlideMove( FootPos, HeadModelParms.EyeHeight, orientationVector, moveDistance,
						collisionModel, groundCollisionModel );
		}
	}

	//
	// Center eye transform
	//
	UpdateCenterEye();

	//
	// Model animations
	//

	if ( !Paused )
	{
		for ( int i = 0; i < Models.GetSizeI(); i++ )
		{
			if ( Models[i] != NULL )
			{
				Models[i]->AnimateJoints( static_cast<float>( vrFrame.PredictedDisplayTimeInSeconds ) );
			}
		}
	}

	// External systems can add surfaces to this list before drawing.
	EmitSurfaces.Resize( 0 );
}

}	// namespace OVR
