/************************************************************************************

Filename    :   GlTexture.h
Content     :   OpenGL texture loading.
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVRGLTEXTURE_H
#define OVRGLTEXTURE_H

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_BitFlags.h"
#include "Kernel/OVR_MemBuffer.h"
#include "Kernel/OVR_GlUtils.h"

#include "VrApi_Types.h"

// Explicitly using unsigned instead of GLUint / GLenum to avoid including GL headers

namespace OVR {

enum eTextureFlags
{
	// Normally, a failure to load will create an 8x8 default texture, but
	// if you want to take explicit action, setting this flag will cause
	// it to return 0 for the texId.
	TEXTUREFLAG_NO_DEFAULT,

	// Use GL_SRGB8 / GL_SRGB8_ALPHA8 / GL_COMPRESSED_SRGB8_ETC2 formats instead
	// of GL_RGB / GL_RGBA / GL_ETC1_RGB8_OES
	TEXTUREFLAG_USE_SRGB,

	// No mip maps are loaded or generated when this flag is specified.
	TEXTUREFLAG_NO_MIPMAPS,

	// Forces a one pixel border around the texture to have
	// zero alpha, so a blended quad will be perfectly anti-aliased.
	// Will only work for uncompressed textures.
	// TODO: this only does the top mip level, since we use genMipmaps
	// to create the rest. Consider manually building the mip levels.
	TEXTUREFLAG_ALPHA_BORDER
};

typedef BitFlagsT< eTextureFlags > TextureFlags_t;

// From LibOVRKernel/CommonSrc/Render/Render_Device.h
enum eTextureFormat
{
	Texture_None			= 0x00000,
    Texture_R               = 0x00100,
    Texture_RGB				= 0x00200,
    Texture_RGBA            = 0x00300,
    Texture_DXT1            = 0x01100,
    Texture_DXT3            = 0x01200,
    Texture_DXT5            = 0x01300,
    Texture_PVR4bRGB        = 0x01400,
    Texture_PVR4bRGBA       = 0x01500,
    Texture_ATC_RGB         = 0x01600,
    Texture_ATC_RGBA        = 0x01700,
    Texture_ETC1			= 0x01800,
	Texture_ETC2_RGB		= 0x01900,
	Texture_ETC2_RGBA		= 0x01A00,
	Texture_ASTC_4x4		= 0x01B00,	// single channel, 4x4 block encoded ASTC
	Texture_ASTC_6x6		= 0x01C00,	// single channel, 6x6 block encoded ASTC
    Texture_Depth           = 0x08000,

	Texture_TypeMask        = 0x0ff00,
    Texture_Compressed      = 0x01000,
    Texture_SamplesMask     = 0x000ff,
    Texture_RenderTarget    = 0x10000,
    Texture_GenMipmaps      = 0x20000
};

// texture id/target pair
// the auto-casting should be removed but allows the target to be ignored by the code that does not care
struct GlTexture
{
	GlTexture() : texture( 0 ), target( 0 ) {}
	GlTexture( unsigned texture_ );
	GlTexture( unsigned texture_, unsigned target_ ) : texture( texture_ ), target( target_ ) {}
	operator unsigned() const { return texture; }

	unsigned	texture;
	unsigned	target;
};

bool TextureFormatToGlFormat( const int format, const bool useSrgbFormat, GLenum & glFormat, GLenum & glInternalFormat );
bool GlFormatToTextureFormat( int & format, const GLenum glFormat, const GLenum glInternalFormat );

// Allocates a GPU texture and uploads the raw data.
GlTexture	LoadRGBATextureFromMemory( const unsigned char * texture, const int width, const int height, const bool useSrgbFormat );
GlTexture	LoadRGBTextureFromMemory( const unsigned char * texture, const int width, const int height, const bool useSrgbFormat );
GlTexture	LoadRTextureFromMemory( const unsigned char * texture, const int width, const int height );
GlTexture	LoadASTCTextureFromMemory( const unsigned char * buffer, const size_t bufferSize, const int numPlanes );

void		MakeTextureClamped( GlTexture texid );
void		MakeTextureLodClamped( GlTexture texId, int maxLod );
void		MakeTextureTrilinear( GlTexture texid );
void		MakeTextureLinear( GlTexture texId );
void		MakeTextureAniso( GlTexture texId, float maxAniso );
void		BuildTextureMipmaps( GlTexture texid );

// FileName's extension determines the file type, but the data is taken from an
// already loaded buffer.
//
// The stb_image file formats are supported:
// .jpg .tga .png .bmp .psd .gif .hdr .pic
//
// Limited support for the PVR and KTX container formats.
//
// If TEXTUREFLAG_NO_DEFAULT, no default texture will be created.
// Otherwise a default square texture will be created on any failure.
//
// Uncompressed image formats will have mipmaps generated and trilinear filtering set.
GlTexture	LoadTextureFromBuffer( const char * fileName, const MemBuffer & buffer,
				const TextureFlags_t & flags, int & width, int & height );

// Returns 0 if the file is not found.
// For a file placed in the project assets folder, nameInZip would be
// something like "assets/cube.pvr".
// See GlTexture.h for supported formats.
GlTexture	LoadTextureFromOtherApplicationPackage( void * zipFile, const char * nameInZip,
									const TextureFlags_t & flags, int & width, int & height );
GlTexture	LoadTextureFromApplicationPackage( const char * nameInZip,
									const TextureFlags_t & flags, int & width, int & height );

unsigned char * LoadPVRBuffer( const char * fileName, int & width, int & height );

// glDeleteTextures()
// Can be safely called on a 0 texture without checking.
void		FreeTexture( GlTexture texId );


ovrTextureSwapChain * CreateTextureSwapChain( ovrTextureType type, ovrTextureFormat format, int width, int height, int levels, bool buffered );
void DestroyTextureSwapChain( ovrTextureSwapChain * chain );
int GetTextureSwapChainLength( ovrTextureSwapChain * chain );
unsigned int GetTextureSwapChainHandle( ovrTextureSwapChain * chain, int index );

}	// namespace OVR

#endif	// !OVRGLTEXTURE_H
