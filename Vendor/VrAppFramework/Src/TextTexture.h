/************************************************************************************

Filename    :   TextTexture.h
Content     :   Text texture.
Created     :   April 24, 2015
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_TEX_TEXTURE_H
#define OVR_TEX_TEXTURE_H

#include "BitmapFont.h"
#include "SurfaceRender.h"
#include "GlProgram.h"

namespace OVR {

// Signed distance field text rendering works poorly when the text shrinks
// to the point that the border is less than one pixel wide, and our default
// rendering also overlaps the border and foreground improperly when characters
// are close together.
//
// Generates a RG8 texture with the given text.
//
// The texture will be mipmapped with trilinear filtering, but the mip chain
// will be truncated.
//
// The texture will have sufficient alpha 0 border that it will not bleed on
// the lowest mip level provided.

struct TextTexture
{
	TextTexture() : TexId( 0 ) {}

	// This disturbs various GL state to render to an FBO
	void 	Create( BitmapFont & bf, const char * text,
			const float fontTexelHeight, const float fontWorldHeight,
			HorizontalJustification hjust, VerticalJustification vjust );

	void	Free()
	{
		DeleteProgram( SimpleProg );
		FreeTexture( TexId );
		Def.geo.Free();
	}

	// Draw so that it matches the origin of a TextSurface with the given transform.
	// Scale will be the world units of the text line height.
	void	Emit( const Matrix4f & transform, Array<ovrDrawSurface> & emitList )
	{
		ModelMatrix = transform * LocalMatrix;
		emitList.PushBack( DrawSurf );
	}

	unsigned		TexId;
	Vector2i		Size;		// integer pixel dimensions
	Vector2f		Origin;		// 0.0 - 1.0 inside texture based on hjust / vjust

	Matrix4f 		LocalMatrix;

	GlGeometry		Quad;
	GlProgram		SimpleProg;
	ovrSurfaceDef	Def;
	ovrDrawSurface	DrawSurf;
	Matrix4f		ModelMatrix;
};

}

#endif // OVR_TEX_TEXTURE_H
