/************************************************************************************

Filename    :   SurfaceRender.cpp
Content     :   Optimized OpenGL rendering path
Created     :   August 9, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "SurfaceRender.h"

#include <stdlib.h>

#include "Kernel/OVR_GlUtils.h"
#include "Kernel/OVR_LogUtils.h"

#include "GlTexture.h"
#include "GlProgram.h"
#include "VrCommon.h"

namespace OVR
{

bool LogRenderSurfaces = false;	// Do not check in set to true!

void ChangeGpuState( const ovrGpuState oldState, const ovrGpuState newState, bool force = false )
{
	if ( force || newState.blendEnable != oldState.blendEnable )
	{
		if ( newState.blendEnable )
		{
			glEnable( GL_BLEND );
		}
		else
		{
			glDisable( GL_BLEND );
		}
	}
	if ( force || newState.blendEnable != oldState.blendEnable
			|| newState.blendSrc != oldState.blendSrc
			|| newState.blendDst != oldState.blendDst
			|| newState.blendSrcAlpha != oldState.blendSrcAlpha
			|| newState.blendDstAlpha != oldState.blendDstAlpha
			|| newState.blendMode != oldState.blendMode
			|| newState.blendModeAlpha != oldState.blendModeAlpha
			)
	{
		if ( newState.blendEnable == ovrGpuState::BLEND_ENABLE_SEPARATE )
		{
			glBlendFuncSeparate( newState.blendSrc, newState.blendDst,
					newState.blendSrcAlpha, newState.blendDstAlpha );
			glBlendEquationSeparate( newState.blendMode, newState.blendModeAlpha );
		}
		else
		{
			glBlendFunc( newState.blendSrc, newState.blendDst );
			glBlendEquation( newState.blendMode );
		}
	}
	if ( force || newState.depthFunc != oldState.depthFunc )
	{
		glDepthFunc( newState.depthFunc );
	}
	if ( force || newState.frontFace != oldState.frontFace )
	{
		glFrontFace( newState.frontFace );
	}
	if ( force || newState.depthEnable != oldState.depthEnable )
	{
		if ( newState.depthEnable )
		{
			glEnable( GL_DEPTH_TEST );
		}
		else
		{
			glDisable( GL_DEPTH_TEST );
		}
	}
	if ( force || newState.depthMaskEnable != oldState.depthMaskEnable )
	{
		if ( newState.depthMaskEnable )
		{
			glDepthMask( GL_TRUE );
		}
		else
		{
			glDepthMask( GL_FALSE );
		}
	}
	if ( force || newState.polygonOffsetEnable != oldState.polygonOffsetEnable )
	{
		if ( newState.polygonOffsetEnable )
		{
			glEnable( GL_POLYGON_OFFSET_FILL );
			glPolygonOffset( 1.0f, 1.0f );
		}
		else
		{
			glDisable( GL_POLYGON_OFFSET_FILL );
		}
	}
	if ( force || newState.cullEnable != oldState.cullEnable )
	{
		if ( newState.cullEnable )
		{
			glEnable( GL_CULL_FACE );
		}
		else
		{
			glDisable( GL_CULL_FACE );
		}
	}
	// extend as needed
}

// Renders a list of pointers to models in order.
ovrDrawCounters RenderSurfaceList( const Array<ovrDrawSurface> & surfaceList,
								const Matrix4f & viewMatrix,
								const Matrix4f & projectionMatrix )
{
	// Force the GPU state to a known value, then only set on changes
	ovrGpuState			currentGpuState;
	ChangeGpuState( currentGpuState, currentGpuState, true /* force */ );

	GLuint				currentTextures[ MAX_PROGRAM_TEXTURES ] = {};	// TODO: This should be a range checked container.
	const Matrix4f *	currentModelMatrix = NULL;
	GLuint				currentProgramObject = -1;

	const Matrix4f vpMatrix = projectionMatrix * viewMatrix;

	// default joints if no joints are specified
	static const Matrix4f defaultJoints[MAX_JOINTS];

	// counters
	ovrDrawCounters counters;

	// Loop through all the surfaces
	for ( int surfaceNum = 0; surfaceNum < surfaceList.GetSizeI(); surfaceNum++ )
	{
		const ovrDrawSurface & drawSurface = surfaceList[ surfaceNum ];
		const ovrSurfaceDef & surfaceDef = *drawSurface.surface;
		const ovrMaterialDef & materialDef = surfaceDef.materialDef;

		// Update GPU state -- blending, etc
		ChangeGpuState( currentGpuState, materialDef.gpuState );
		currentGpuState = materialDef.gpuState;

		// Update texture bindings
		assert( materialDef.numTextures <= MAX_PROGRAM_TEXTURES );
		for ( int textureNum = 0; textureNum < materialDef.numTextures; textureNum++ ) 
		{
			const GLuint texNObj = materialDef.textures[textureNum].texture;
			if ( currentTextures[textureNum] != texNObj )
			{
				counters.numTextureBinds++;
				currentTextures[textureNum] = texNObj;
				glActiveTexture( GL_TEXTURE0 + textureNum );
				// Something is leaving target set to 0; assume GL_TEXTURE_2D
				glBindTexture( materialDef.textures[textureNum].target ?
						materialDef.textures[textureNum].target : GL_TEXTURE_2D, texNObj );
			}
		}

		// Update program object
		assert( materialDef.programObject != 0 );
		if ( materialDef.programObject != currentProgramObject ) 
		{
			counters.numProgramBinds++;

			currentProgramObject = materialDef.programObject;
			glUseProgram( currentProgramObject );

			// It is possible the program still has the correct MVP,
			// but we don't want to track it, so reset it anyway.
			currentModelMatrix = NULL;
		}

		// Update the program parameters
		if ( drawSurface.modelMatrix != currentModelMatrix )
		{
			counters.numParameterUpdates++;

			currentModelMatrix = drawSurface.modelMatrix;
			// FIXME: get rid of the MVP and transform vertices with the individial model/view/projection matrices for improved precision
			const Matrix4f mvp = vpMatrix * (*currentModelMatrix );
			glUniformMatrix4fv( materialDef.uniformMvp, 1, GL_TRUE, &mvp.M[0][0] );

			// set the model matrix
			if ( materialDef.uniformModel != -1 )
			{
				glUniformMatrix4fv( materialDef.uniformModel, 1, GL_TRUE, &currentModelMatrix->M[0][0] );
			}

			// set the view matrix
			if ( materialDef.uniformView != -1 )
			{
				glUniformMatrix4fv( materialDef.uniformView, 1, GL_TRUE, &viewMatrix.M[0][0] );
			}

			// set the projection matrix
			if ( materialDef.uniformProjection != -1 )
			{
				glUniformMatrix4fv( materialDef.uniformProjection, 1, GL_TRUE, &projectionMatrix.M[0][0] );
			}

			// set the joint matrices
			if ( materialDef.uniformJoints != -1 )
			{
				if ( drawSurface.joints != NULL && drawSurface.joints->GetSize() > 0 )
				{
#if 1	// FIXME: setting glUniformMatrix4fv transpose to GL_TRUE for an array of matrices produces garbage using the Adreno 420 OpenGL ES 3.0 driver.
					static Matrix4f transposedJoints[MAX_JOINTS];
					for ( int i = 0; i < drawSurface.joints->GetSizeI(); i++ )
					{
						transposedJoints[i] = drawSurface.joints->At( i ).Transposed();
					}
					glUniformMatrix4fv( materialDef.uniformJoints, Alg::Min( drawSurface.joints->GetSizeI(), MAX_JOINTS ), GL_FALSE, &transposedJoints[0].M[0][0] );
#else
					glUniformMatrix4fv( materialDef.uniformJoints, Alg::Min( drawSurface.joints->GetSizeI(), MAX_JOINTS ), GL_TRUE, &drawSurface.joints->At( 0 ).M[0][0] );
#endif
				}
				else
				{
					glUniformMatrix4fv( materialDef.uniformJoints, MAX_JOINTS, GL_TRUE, &defaultJoints[0].M[0][0] );
				}
			}
		}

		for ( int unif = 0; unif < MAX_PROGRAM_UNIFORMS; unif++ )
		{
			const int slot = materialDef.uniformSlots[unif];
			if ( slot == -1 )
			{
				break;
			}
			counters.numParameterUpdates++;
			glUniform4fv( slot, 1, materialDef.uniformValues[unif] );
		}

		counters.numDrawCalls++;

		if ( LogRenderSurfaces )
		{
			LOG( "Drawing %s", surfaceDef.surfaceName.ToCStr() );
		}

		// Bind all the vertex and element arrays
		surfaceDef.geo.Draw();
	}

	// set the gpu state back to the default
	ChangeGpuState( currentGpuState, ovrGpuState() );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, 0 );
	glUseProgram( 0 );
	glBindVertexArray( 0 );

	return counters;
}

}	// namespace OVR
