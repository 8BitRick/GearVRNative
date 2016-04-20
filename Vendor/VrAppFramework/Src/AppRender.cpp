/************************************************************************************

Filename    :   AppRender.cpp
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#include <math.h>

#include "Kernel/OVR_GlUtils.h"
#include "VrApi.h"
#include "VrApi_Helpers.h"

#include "App.h"
#include "AppLocal.h"
#include "VrCommon.h"
#include "BitmapFont.h"
#include "DebugLines.h"

namespace OVR
{

// Debug tool to draw outlines of a 3D bounds
void AppLocal::DrawBounds( const Vector3f & mins, const Vector3f & maxs, const Matrix4f & mvp, const Vector3f & color )
{
	const Matrix4f scaled = mvp * Matrix4f::Translation( mins ) * Matrix4f::Scaling( maxs - mins );
	const GlProgram & prog = untexturedMvpProgram;
	glUseProgram( prog.program );
	glLineWidth( 1.0f );
	glUniform4f( prog.uColor, color.x, color.y, color.z, 1 );
	glUniformMatrix4fv( prog.uMvp, 1, GL_TRUE, scaled.M[0] );
	unitCubeLines.Draw();
	glBindVertexArray( 0 );
}

void AppLocal::DrawDialog( const Matrix4f & mvp )
{
	// draw the pop-up dialog
	const float now = vrapi_GetTimeInSeconds();
	if ( now >= dialogStopSeconds )
	{
		return;
	}
	const Matrix4f dialogMvp = mvp * dialogMatrix;

	const float fadeSeconds = 0.5f;
	const float f = now - ( dialogStopSeconds - fadeSeconds );
	const float clampF = f < 0.0f ? 0.0f : f;
	const float alpha = 1.0f - clampF;

	DrawPanel( dialogTexture->GetTextureId(), dialogMvp, alpha );
}

void AppLocal::DrawPanel( const GLuint externalTextureId, const Matrix4f & dialogMvp, const float alpha )
{
#if defined( OVR_OS_ANDROID )
	const GlProgram & prog = externalTextureProgram2;
	glUseProgram( prog.program );
	glUniform4f( prog.uColor, 1, 1, 1, alpha );

	glUniformMatrix4fv( prog.uTexm, 1, GL_TRUE, Matrix4f::Identity().M[0] );
	glUniformMatrix4fv( prog.uMvp, 1, GL_TRUE, dialogMvp.M[0] );

	// It is important that panels write to destination alpha, or they
	// might get covered by an overlay plane/cube in TimeWarp.
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_EXTERNAL_OES, externalTextureId );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	panelGeometry.Draw();
	glDisable( GL_BLEND );
	glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );	// don't leave it bound
#endif
}

void AppLocal::DrawEyeViews( Matrix4f const & centerViewMatrix )
{
	GetDebugFontSurface().Finish( centerViewMatrix );

	// Increase the fov by about 10 degrees if we are not holding 60 fps so
	// there is less black pull-in at the edges.
	//
	// Doing this dynamically based just on time causes visible flickering at the
	// periphery when the fov is increased, so only do it if minimumVsyncs is set.
	const float fovIncrease = ( ( FrameParms.MinimumVsyncs > 1 ) || TheVrFrame.Get().DeviceStatus.PowerLevelStateThrottled ) ? 10.0f : 0.0f;
	const float fovDegreesX = SuggestedEyeFovDegreesX + fovIncrease;
	const float fovDegreesY = SuggestedEyeFovDegreesY + fovIncrease;

	// DisplayMonoMode uses a single eye rendering for speed improvement
	// and / or high refresh rate double-scan hardware modes.
	const int numEyes = VrSettings.RenderMonoMode ? 1 : 2;

	// Flush out and report any errors
	GL_CheckErrors( "FrameStart" );

	{
		EyeBuffers->BeginFrame();

		// Setup the default frame parms first so they can be modified in DrawEyeView().
		const ovrFrameTextureSwapChains eyes = EyeBuffers->GetCurrentFrameTextureSwapChains();
		const ovrMatrix4f projectionMatrix = ovrMatrix4f_CreateProjectionFov( fovDegreesX, fovDegreesY, 0.0f, 0.0f, VRAPI_ZNEAR, 0.0f );
		const ovrMatrix4f texCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection( &projectionMatrix );

		//FrameParms = vrapi_DefaultFrameParms( GetJava(), VRAPI_FRAME_INIT_DEFAULT, vrapi_GetTimeInSeconds(), NULL );
		for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
		{
			FrameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[eye].ColorTextureSwapChain = eyes.ColorTextureSwapChain[renderMonoMode ? 0 : eye ];
			FrameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[eye].DepthTextureSwapChain = eyes.DepthTextureSwapChain[renderMonoMode ? 0 : eye ];
			FrameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[eye].TextureSwapChainIndex = eyes.TextureSwapChainIndex;

			for ( int layer = 0; layer < VRAPI_FRAME_LAYER_TYPE_MAX; layer++ )
			{
				FrameParms.Layers[layer].Textures[eye].TexCoordsFromTanAngles = texCoordsFromTanAngles;
				FrameParms.Layers[layer].Textures[eye].HeadPose = TheVrFrame.Get().Tracking.HeadPose;
			}
		}

		// Render each eye.
		for ( int eye = 0; eye < numEyes; eye++ )
		{
			EyeBuffers->BeginRenderingEye( eye );

			// Call back to the app for drawing.
			const Matrix4f eyeViewProjection = appInterface->DrawEyeView( eye, fovDegreesX, fovDegreesY, FrameParms );

			GetDebugFontSurface().Render3D( GetDebugFont(), eyeViewProjection );

			glDisable( GL_DEPTH_TEST );
			glDisable( GL_CULL_FACE );

			// Optionally draw thick calibration lines into the texture,
			// which will be overlayed by the thinner origin cross when
			// distorted to the window.
			if ( drawCalibrationLines )
			{
				EyeDecorations.DrawEyeCalibrationLines( fovDegreesX, eye );
				calibrationLinesDrawn = true;
			}
			else
			{
				calibrationLinesDrawn = false;
			}

			DrawDialog( eyeViewProjection );

			GetDebugLines().Render( eyeViewProjection );

			// Draw a thin vignette at the edges of the view so clamping will give black
			// This will not be reflected correctly in overlay planes.
			// EyeDecorations.DrawEyeVignette();
			EyeDecorations.FillEdge( VrSettings.EyeBufferParms.resolutionWidth, VrSettings.EyeBufferParms.resolutionHeight );

			EyeBuffers->EndRenderingEye( eye );
		}
	}
}

// Draw a screen to an eye buffer the same way it would be drawn as a
// time warp overlay.
void AppLocal::DrawScreenDirect( const ovrMatrix4f & mvp, const GLuint texid )
{
	const Matrix4f mvpMatrix( mvp );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, texid );

	glUseProgram( OverlayScreenDirectProgram.program );

	glUniformMatrix4fv( OverlayScreenDirectProgram.uMvp, 1, GL_TRUE, mvpMatrix.M[0] );

	glBindVertexArray( unitSquare.vertexArrayObject );
	glDrawElements( GL_TRIANGLES, unitSquare.indexCount, GL_UNSIGNED_SHORT, NULL );

	glBindTexture( GL_TEXTURE_2D, 0 );	// don't leave it bound
}

// draw a zero to destination alpha
void AppLocal::DrawScreenMask( const ovrMatrix4f & mvp, const float fadeFracX, const float fadeFracY )
{
	Matrix4f mvpMatrix( mvp );

	glUseProgram( OverlayScreenFadeMaskProgram.program );

	glUniformMatrix4fv( OverlayScreenFadeMaskProgram.uMvp, 1, GL_TRUE, mvpMatrix.M[0] );

	if ( FadedScreenMaskSquare.vertexArrayObject == 0 )
	{
		FadedScreenMaskSquare = BuildFadedScreenMask( fadeFracX, fadeFracY );
	}

	glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE );
	FadedScreenMaskSquare.Draw();
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
}

}	// namespace OVR
