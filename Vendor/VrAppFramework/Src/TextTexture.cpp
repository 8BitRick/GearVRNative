/************************************************************************************

Filename    :   TextTexture.h
Content     :   Text texture.
Created     :   April 24, 2015
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "TextTexture.h"
#include "Kernel/OVR_LogUtils.h"
#include "VrCommon.h"

namespace OVR
{

void TextTexture::Create( BitmapFont & bf, const char * text,
		const float fontTexelHeight, const float fontWorldHeight,
		HorizontalJustification hjust, VerticalJustification vjust )
{
	ovrSurfaceDef	surf = bf.TextSurface( text, 1.0f, Vector4f( 1.0f, 1.0f, 1.0f, 1.0f ),
			hjust, vjust );

LOG( "Surf bounds: %f %f to %f %f",
		surf.cullingBounds.b[0][0], surf.cullingBounds.b[0][1],
		surf.cullingBounds.b[1][0], surf.cullingBounds.b[1][1] );

	GL_CheckErrors( "Before TextTexture");

	// Calculate texture size proportional to fontTexelHeight
	// and the surface bounds.
	// FIXME: multi-line, proper handling of ascenders / descenders
	const Vector3f geoSize = surf.cullingBounds.GetSize();
	Size.x = ceil( fontTexelHeight * geoSize.x / geoSize.y );
	Size.y = ceil( fontTexelHeight );

LOG( "Size: %i %i", Size.x, Size.y );

	// Origin offset inside the -1 to 1 quad that should map to
	// the origin point for surface drawing.
	Origin.x = -1.0f + 2.0f * -surf.cullingBounds.b[0][0] / geoSize.x;
	Origin.y = -1.0f + 2.0f * -surf.cullingBounds.b[0][1] / geoSize.y;

LOG( "Origin: %f %f", Origin.x, Origin.y );

	glGenTextures( 1, &TexId );
	glBindTexture( GL_TEXTURE_2D, TexId );
	glTexStorage2D( GL_TEXTURE_2D, 7, GL_RGBA8, Size.x, Size.y );

	GLuint	fbo;

	glGenFramebuffers( 1, &fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, TexId, 0 );

	glDisable( GL_CULL_FACE );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_SCISSOR_TEST );
	glViewport( 0, 0, Size.x, Size.y );
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear( GL_COLOR_BUFFER_BIT );

	glBindTexture( GL_TEXTURE_2D, surf.materialDef.textures[0] );
	glUseProgram( surf.materialDef.programObject );

	// Flip vertically
	Matrix4f	m;
	m.M[0][0] = 2.0f / geoSize.x;
	m.M[0][3] = -surf.cullingBounds.b[0][0] * m.M[0][0] - 1.0f;
	m.M[1][1] = -2.0f / geoSize.y;
	m.M[1][3] = -surf.cullingBounds.b[0][1] * m.M[1][1] + 1.0f;

LogMatrix( "DrawMatrix", m );
	glUniformMatrix4fv( surf.materialDef.uniformMvp, 1, GL_TRUE, m.M[0] );

	glUniform4f( surf.materialDef.uniformSlots[0], 1.0f, 1.0f, 1.0f, 1.0f );


	// Draw fuzzy expanded outlines with blend mode MAX
	glEnable( GL_BLEND );
	glBlendFunc( GL_ONE, GL_ONE );
	glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE );
	surf.geo.Draw();

	// Draw glyph bodies with blend mode blend
	glBlendFunc( GL_ONE, GL_ONE );
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE );
	surf.geo.Draw();

	glDisable( GL_BLEND );
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );


	// TODO: draw all outlines first, then all foregrounds

	glUseProgram( 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glDeleteFramebuffers( 1, &fbo );

	// Don't need the TextSurf now that we have rendered it to the texture.
	surf.geo.Free();

	// Build mipmaps and set filter parms
	glBindTexture( GL_TEXTURE_2D, TexId );
	glGenerateMipmap( GL_TEXTURE_2D );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glBindTexture( GL_TEXTURE_2D, 0 );

	// Setup surface for drawing
	SimpleProg = BuildProgram(
		"uniform highp mat4 Mvpm;\n"
		"attribute highp vec4 Position;\n"
		"attribute highp vec2 TexCoord;\n"
		"varying lowp vec4 oColor;\n"
		"varying highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   oTexCoord = TexCoord;\n"
		"}\n"
		,
		"uniform sampler2D Texture0;\n"
		"uniform lowp vec4 UniformColor;\n"
		"varying highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_FragColor = /* UniformColor * */ texture2D( Texture0, oTexCoord );\n"
//		"   gl_FragColor = vec4( 1.0, 1.0, 1.0, 1.0 );\n"
		"}\n"
	);

	// Special blend mode to also work over underlay layers
	Def.materialDef.gpuState.blendEnable = ovrGpuState::BLEND_ENABLE_SEPARATE;
	Def.materialDef.gpuState.blendSrc = GL_SRC_ALPHA;
	Def.materialDef.gpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
	Def.materialDef.gpuState.blendSrcAlpha = GL_ONE;
	Def.materialDef.gpuState.blendDstAlpha = GL_ONE_MINUS_SRC_ALPHA;

	Def.materialDef.programObject = SimpleProg.program;
	Def.materialDef.uniformMvp = SimpleProg.uMvp;

	Def.materialDef.uniformSlots[0] = SimpleProg.uColor;

	Def.materialDef.numTextures = 1;
	Def.materialDef.textures[0].target = GL_TEXTURE_2D;
	Def.materialDef.textures[0].texture = TexId;
	Def.geo = BuildTesselatedQuad( 1, 1 );

	Def.surfaceName = text;

	DrawSurf.Clear();
	DrawSurf.surface = &Def;
	DrawSurf.modelMatrix = &ModelMatrix;

	// TODO: multi-line text
	LocalMatrix =
		Matrix4f::Scaling( geoSize.x / geoSize.y * fontWorldHeight*0.5f, fontWorldHeight*0.5f, 1.0f )
		* Matrix4f::Translation( -Origin.x, -Origin.y );

	GL_CheckErrors( "After TextTexture");
}

}	// namespace OVR
