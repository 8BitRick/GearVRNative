/************************************************************************************

Filename    :   EyeBuffers.cpp
Content     :   Handling of different eye buffer formats
Created     :   March 8, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "EyeBuffers.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "VrApi.h"

#include "Framebuffer.h"

namespace OVR
{

ovrEyeBuffers::ovrEyeBuffers() :
	LogEyeSceneGpuTime(),
	DiscardInsteadOfClear( true ),
	Framebuffers()
{
}

ovrEyeBuffers::~ovrEyeBuffers()
{
	for ( int eye = 0; eye < 2; eye++ )
	{
		delete Framebuffers[eye];
	}
}

void ovrEyeBuffers::Initialize( const ovrEyeBufferParms & bufferParms_ )
{
	// Update the buffers if parameters have changed
	if ( Framebuffers[0] != NULL &&
			BufferParms.resolutionWidth == bufferParms_.resolutionWidth &&
			BufferParms.resolutionHeight == bufferParms_.resolutionHeight &&
			BufferParms.multisamples == bufferParms_.multisamples &&
			BufferParms.colorFormat == bufferParms_.colorFormat &&
			BufferParms.depthFormat == bufferParms_.depthFormat )
	{
		return;
	}

	/*
	* Changes a buffer set from one (possibly uninitialized) state to a new
	* resolution / multisample / color depth state.
	*
	* Various GL binding points are modified, as well as the clear color and scissor rect.
	*
	* Note that in OpenGL 3.0 and ARB_framebuffer_object, frame buffer objects are NOT
	* shared across context share groups, so this work must be done in the rendering thread,
	* not the timewarp thread.  The texture that is bound to the FBO is properly shared.
	*
	* TODO: fall back to simpler cases on failure and mark a flag in bufferData?
	*/
	LOG( "Reallocating buffers" );

	LOG( "Allocate FBO: res=%ix%i color=%i depth=%i ms=%i",
			bufferParms_.resolutionWidth, bufferParms_.resolutionHeight, 
			bufferParms_.colorFormat, bufferParms_.depthFormat, bufferParms_.multisamples );

	GL_CheckErrors( "Before framebuffer creation" );

	ovrTextureFormat colorFormat = VRAPI_TEXTURE_FORMAT_8888;
	switch ( bufferParms_.colorFormat )
	{
		case COLOR_565: 		colorFormat = VRAPI_TEXTURE_FORMAT_565; break;
		case COLOR_5551:		colorFormat = VRAPI_TEXTURE_FORMAT_5551; break;
		case COLOR_4444:		colorFormat = VRAPI_TEXTURE_FORMAT_4444; break;
		case COLOR_8888:		colorFormat = VRAPI_TEXTURE_FORMAT_8888; break;
		case COLOR_8888_sRGB:	colorFormat = VRAPI_TEXTURE_FORMAT_8888_sRGB; break;
		case COLOR_RGBA16F:		colorFormat = VRAPI_TEXTURE_FORMAT_RGBA16F; break;
		default: 				FAIL( "Unknown colorFormat %i", bufferParms_.colorFormat );
	}

	ovrTextureFormat depthFormat = VRAPI_TEXTURE_FORMAT_NONE;
	switch ( bufferParms_.depthFormat )
	{
		case DEPTH_0:				depthFormat = VRAPI_TEXTURE_FORMAT_NONE; break;
		case DEPTH_16:				depthFormat = VRAPI_TEXTURE_FORMAT_DEPTH_16; break;
		case DEPTH_24:				depthFormat = VRAPI_TEXTURE_FORMAT_DEPTH_24; break;
		case DEPTH_24_STENCIL_8:	depthFormat = VRAPI_TEXTURE_FORMAT_DEPTH_24_STENCIL_8; break;
	}

	for ( int eye = 0; eye < 2; eye++ )
	{
		delete Framebuffers[eye];
		Framebuffers[eye] = new ovrFramebuffer( colorFormat, depthFormat,
												bufferParms_.resolutionWidth, bufferParms_.resolutionHeight,
												bufferParms_.multisamples, bufferParms_.resolveDepth );
	}

	GL_CheckErrors( "after framebuffer creation" );

	// Save the current buffer parms
	BufferParms = bufferParms_;
}

void ovrEyeBuffers::BeginFrame()
{
	for ( int eye = 0; eye < 2; eye++ )
	{
		Framebuffers[eye]->Advance();
	}
}

void ovrEyeBuffers::BeginRenderingEye( const int eyeNum )
{
	LogEyeSceneGpuTime.Begin( eyeNum );
	LogEyeSceneGpuTime.PrintTime( eyeNum, "GPU time for eye render" );

	Framebuffers[eyeNum]->Bind();

	glViewport( 0, 0, BufferParms.resolutionWidth, BufferParms.resolutionHeight );
	glScissor( 0, 0, BufferParms.resolutionWidth, BufferParms.resolutionHeight );
	glDepthMask( GL_TRUE );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );

	if ( DiscardInsteadOfClear )
	{
		GL_InvalidateFramebuffer( INV_FBO, true, true );
		glClear( GL_DEPTH_BUFFER_BIT );
	}
	else
	{
		glClearColor( 0, 0, 0, 1 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}
}

void ovrEyeBuffers::EndRenderingEye( const int eyeNum )
{
	Framebuffers[eyeNum]->Resolve();

	LogEyeSceneGpuTime.End( eyeNum );

	// Left to themselves, tiled GPU drivers will avoid starting rendering
	// until they are absolutely forced to by a swap or read of a buffer,
	// because poorly implemented applications may switch away from an
	// FBO (say, to render a shadow buffer), then switch back to it and
	// continue rendering, which would cause a wasted resolve and unresolve
	// from real memory if drawing started automatically on leaving an FBO.
	//
	// We are not going to do any such thing, and we want the drawing of the
	// first eye to overlap with the command generation for the second eye
	// if the GPU has idled, so explicitly flush the pipeline.
	//
	// Adreno and Mali Do The Right Thing on glFlush, but PVR is
	// notorious for ignoring flushes to optimize naive apps.  The preferred
	// solution there is to use EGL_SYNC_FLUSH_COMMANDS_BIT_KHR on a
	// KHR sync object, but several versions of the Adreno driver have
	// had broken implementations of this that performed a full finish
	// instead of just a flush.
	glFlush();
}

ovrFrameTextureSwapChains ovrEyeBuffers::GetCurrentFrameTextureSwapChains()
{
	ovrFrameTextureSwapChains eyes = {};

	for ( int eye = 0; eye < 2; eye++ )
	{
		eyes.ColorTextureSwapChain[eye] = Framebuffers[eye]->GetColorTextureSwapChain();
		eyes.DepthTextureSwapChain[eye] = Framebuffers[eye]->GetDepthTextureSwapChain();
	}
	eyes.TextureSwapChainIndex = Framebuffers[0]->GetTextureSwapChainIndex();

	return eyes;
}

void ovrEyeBuffers::ScreenShot()
{
	Framebuffers[0]->ScreenShot();
}

} // namespace OVR
