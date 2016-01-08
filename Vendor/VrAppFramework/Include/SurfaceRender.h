/************************************************************************************

Filename    :   SurfaceRender.h
Content     :   Optimized OpenGL rendering path
Created     :   August 9, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/
#ifndef OVR_SurfaceRender_h
#define OVR_SurfaceRender_h

#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_GlUtils.h"

#include "GlTexture.h"
#include "GlGeometry.h"

namespace OVR
{

// can be made as high as 16
static const int MAX_PROGRAM_TEXTURES = 5;

// number of vec4
enum {
	 MAX_PROGRAM_UNIFORMS = 4
};

struct ovrGpuState
{
	ovrGpuState()
	{
		blendMode = GL_FUNC_ADD;
		blendModeAlpha = GL_FUNC_ADD;
		blendSrc = GL_ONE;
		blendDst = GL_ZERO;
		blendSrcAlpha = GL_ONE;
		blendDstAlpha = GL_ZERO;
		depthFunc = GL_LEQUAL;
		frontFace = GL_CCW;
		blendEnable = false;
		depthEnable = true;
		depthMaskEnable = true;
		polygonOffsetEnable = false;
		cullEnable = true;
	}

	GLenum	blendSrc;
	GLenum	blendDst;
	GLenum	blendMode;		// GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX
	GLenum	blendSrcAlpha;
	GLenum	blendDstAlpha;
	GLenum	blendModeAlpha;
	GLenum	depthFunc;
	GLenum	frontFace;		// GL_CW, GL_CCW
	static const int BLEND_DISABLE = 0;
	static const int BLEND_ENABLE = 1;
	static const int BLEND_ENABLE_SEPARATE = 2;
	unsigned char	blendEnable;	// off, normal, separate
	bool	depthEnable;
	bool	depthMaskEnable;
	bool	polygonOffsetEnable;
	bool	cullEnable;
};

struct ovrMaterialDef
{
	ovrMaterialDef() :
		programObject( 0 ),
		uniformMvp( -1 ),
		uniformModel( -1 ),
		uniformView( -1 ),
		uniformProjection( -1 ),
		uniformJoints( -1 ),
		uniformValues(),
		numTextures( 0 ) {
		for ( int i = 0; i < MAX_PROGRAM_UNIFORMS; ++i )
		{
			uniformSlots[i] = -1;
		}
	}

	ovrGpuState	gpuState;				// blending, depth testing, etc

	// We might want to reference a gpuProgram_t object instead of copying these.
	GLuint		programObject;
	GLint		uniformMvp;
	GLint		uniformModel;
	GLint		uniformView;
	GLint		uniformProjection;
	GLint		uniformJoints;

	// Parameter setting stops when uniformSlots[x] == -1
	GLint		uniformSlots[MAX_PROGRAM_UNIFORMS];
	GLfloat		uniformValues[MAX_PROGRAM_UNIFORMS][4];

	// Additional uniforms for lighting and so on will need to be added.

	// Currently assumes GL_TEXTURE_2D for all; will need to be
	// extended for GL_TEXTURE_CUBE and GL_TEXTURE_EXTERNAL_OES.
	//
	// There should never be any 0 textures in the active elements.
	//
	// This should be a range checked container.
	int			numTextures;
	GlTexture	textures[MAX_PROGRAM_TEXTURES];
};

struct ovrSurfaceDef
{
						ovrSurfaceDef() {}

	// Name from the model file, can be used to control surfaces with code.
	// May be multiple semi-colon separated names if multiple source meshes
	// were merged into one surface.
	String				surfaceName;

	// We may want a do-not-cull flag for trivial quads and
	// skybox sorts of geometry.
	Bounds3f			cullingBounds;

	// There is a space savings to be had with triangle strips
	// if primitive restart is supported, but it is a net speed
	// loss on most architectures.  Adreno docs still recommend,
	// so it might be worth trying.
	GlGeometry			geo;

	// This could be a constant reference, but inline has some
	// advantages for now while the definition is small.
	ovrMaterialDef		materialDef;
};

struct ovrDrawCounters
{
			ovrDrawCounters() :
				numElements( 0 ),
				numDrawCalls( 0 ),
				numProgramBinds( 0 ),
				numParameterUpdates( 0 ),
				numTextureBinds( 0 ) {}

	int		numElements;
	int		numDrawCalls;
	int		numProgramBinds;
	int		numParameterUpdates;		// MVP, etc
	int		numTextureBinds;
};

struct ovrDrawSurface
{
								void Clear()
								{
									modelMatrix = NULL;
									joints = NULL;
									surface = NULL;
								}

	const Matrix4f *			modelMatrix;
	const Array< Matrix4f > *	joints;
	const ovrSurfaceDef *		surface;
};

// Draws a list of surfaces in order.
// Any sorting or culling should be performed before calling.
ovrDrawCounters RenderSurfaceList( const Array<ovrDrawSurface> & surfaceList,
								const Matrix4f & viewMatrix,
								const Matrix4f & projectionMatrix );

// Set this true for log spew from BuildDrawSurfaceList and RenderSurfaceList.
extern bool LogRenderSurfaces;


} // namespace OVR

#endif	// OVR_SurfaceRender_h
