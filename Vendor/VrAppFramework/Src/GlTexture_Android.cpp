/************************************************************************************

Filename    :   GlTexture_Android.cpp
Content     :   OpenGL texture loading.
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "GlTexture.h"

#if defined( OVR_OS_ANDROID )

#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR            0x93B0
#define GL_COMPRESSED_RGBA_ASTC_5x4_KHR            0x93B1
#define GL_COMPRESSED_RGBA_ASTC_5x5_KHR            0x93B2
#define GL_COMPRESSED_RGBA_ASTC_6x5_KHR            0x93B3
#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR            0x93B4
#define GL_COMPRESSED_RGBA_ASTC_8x5_KHR            0x93B5
#define GL_COMPRESSED_RGBA_ASTC_8x6_KHR            0x93B6
#define GL_COMPRESSED_RGBA_ASTC_8x8_KHR            0x93B7
#define GL_COMPRESSED_RGBA_ASTC_10x5_KHR           0x93B8
#define GL_COMPRESSED_RGBA_ASTC_10x6_KHR           0x93B9
#define GL_COMPRESSED_RGBA_ASTC_10x8_KHR           0x93BA
#define GL_COMPRESSED_RGBA_ASTC_10x10_KHR          0x93BB
#define GL_COMPRESSED_RGBA_ASTC_12x10_KHR          0x93BC
#define GL_COMPRESSED_RGBA_ASTC_12x12_KHR          0x93BD
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR    0x93D0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR    0x93D1
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR    0x93D2
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR    0x93D3
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR    0x93D4
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR    0x93D5
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR    0x93D6
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR    0x93D7
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR   0x93D8
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR   0x93D9
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR   0x93DA
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR  0x93DB
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR  0x93DC
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR  0x93DD

/*
struct TextureFormatMap
{
	eTextureFormat	Format;
	const char *	Name;
};

static TextureFormatMap TextureFormatNames[] =
{
    { Texture_R,			"Texture_R" },
    { Texture_RGB,			"Texture_RGB" },
	{ Texture_RGBA,			"Texture_RGBA" },
    { Texture_DXT1,			"Texture_DXT1" },
    { Texture_DXT3,			"Texture_DXT3" },
    { Texture_DXT5,			"Texture_DXT5" },
    { Texture_PVR4bRGB,		"Texture_PVR4bRGB" },
    { Texture_PVR4bRGBA,	"Texture_PVR4bRGBA" },
    { Texture_ATC_RGB,		"Texture_ATC_RGB" },
    { Texture_ATC_RGBA,		"Texture_ATC_RGBA" },
    { Texture_ETC1,			"Texture_ETC1" },
	{ Texture_ETC2_RGB,		"Texture_ETC2_RGB" },
	{ Texture_ETC2_RGBA,	"Texture_ETC2_RGBA" },
	{ Texture_ASTC_4x4,		"Texture_ASTC_4x4" },
	{ Texture_ASTC_6x6,		"Texture_ASTC_6x6" },
    { Texture_Depth,		"Texture_Depth" },
    { Texture_TypeMask,		"Texture_TypeMask" },
    { Texture_Compressed,	"Texture_Compressed" },
    { Texture_SamplesMask,	"Texture_SamplesMask" },
    { Texture_RenderTarget,	"Texture_RenderTarget" },
	{ Texture_GenMipmaps,	"Texture_GenMipmaps" },
	{ Texture_None,			NULL }
};

static const char * NameForTextureFormat( eTextureFormat const format )
{
	for ( int i = 0; TextureFormatNames[i].Name != NULL; ++i )
	{
		if ( TextureFormatNames[i].Format == format )
		{
			return TextureFormatNames[i].Name;
		}
	}
	return "<unknown>";
}
*/


namespace OVR {

bool TextureFormatToGlFormat( const int format, const bool useSrgbFormat, GLenum & glFormat, GLenum & glInternalFormat )
{
    switch ( format & Texture_TypeMask )
    {
		case Texture_RGB:
		{
			glFormat = GL_RGB; 
			if ( useSrgbFormat )
			{
				glInternalFormat = GL_SRGB8; 
//				LOG( "GL texture format is GL_RGB / GL_SRGB8" );
			}
			else
			{
				glInternalFormat = GL_RGB; 
//				LOG( "GL texture format is GL_RGB / GL_RGB" );
			}
			return true;
		}
		case Texture_RGBA:
		{
			glFormat = GL_RGBA; 
			if ( useSrgbFormat )
			{
				glInternalFormat = GL_SRGB8_ALPHA8; 
//				LOG( "GL texture format is GL_RGBA / GL_SRGB8_ALPHA8" );
			}
			else
			{
				glInternalFormat = GL_RGBA; 
//				LOG( "GL texture format is GL_RGBA / GL_RGBA" );
			}
			return true;
		}
		case Texture_R:
		{
			glInternalFormat = GL_R8;
			glFormat = GL_RED;
//			LOG( "GL texture format is GL_R8" );
			return true;
		}
		case Texture_DXT1:
		{
			glFormat = glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
//			LOG( "GL texture format is GL_COMPRESSED_RGBA_S3TC_DXT1_EXT" );
			return true;
		}
	// unsupported on OpenGL ES:
	//    case Texture_DXT3:  glFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
	//    case Texture_DXT5:  glFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
		case Texture_PVR4bRGB:
		{
			glFormat = GL_RGB;
			glInternalFormat = GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG; 
//			LOG( "GL texture format is GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG" );
			return true;
		}
		case Texture_PVR4bRGBA:
		{
			glFormat = GL_RGBA;
			glInternalFormat = GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG; 
//			LOG( "GL texture format is GL_RGBA / GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG" );
			return true;
		}
		case Texture_ETC1:
		{
			glFormat = GL_RGB;
			if ( useSrgbFormat )
			{
				// Note that ETC2 is backwards compatible with ETC1.
				glInternalFormat = GL_COMPRESSED_SRGB8_ETC2; 
//				LOG( "GL texture format is GL_RGB / GL_COMPRESSED_SRGB8_ETC2 " );
			}
			else
			{
				glInternalFormat = GL_ETC1_RGB8_OES; 
//				LOG( "GL texture format is GL_RGB / GL_ETC1_RGB8_OES" );
			}
			return true;
		}
		case Texture_ETC2_RGB:
		{
			glFormat = GL_RGB;
			if ( useSrgbFormat )
			{
				glInternalFormat = GL_COMPRESSED_SRGB8_ETC2;
//				LOG( "GL texture format is GL_RGB / GL_COMPRESSED_SRGB8_ETC2 " );
			}
			else
			{
				glInternalFormat = GL_COMPRESSED_RGB8_ETC2;
//				LOG( "GL texture format is GL_RGB / GL_COMPRESSED_RGB8_ETC2 " );
			}
			return true;
		}
		case Texture_ETC2_RGBA:
		{
			glFormat = GL_RGBA;
			if ( useSrgbFormat )
			{
				glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC;
//				LOG( "GL texture format is GL_RGBA / GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC " );
			}
			else
			{
				glInternalFormat = GL_COMPRESSED_RGBA8_ETC2_EAC;
//				LOG( "GL texture format is GL_RGBA / GL_COMPRESSED_RGBA8_ETC2_EAC " );
			}
			return true;
		}
		case Texture_ASTC_4x4:
		{
			glFormat = GL_RGBA;
			glInternalFormat = GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
			return true;
		}
		case Texture_ASTC_6x6:
		{
			glFormat = GL_RGBA;
			glInternalFormat = GL_COMPRESSED_RGBA_ASTC_6x6_KHR;
			return true;
		}
		case Texture_ATC_RGB:
		{
			glFormat = GL_RGB;
			glInternalFormat = GL_ATC_RGB_AMD; 
//			LOG( "GL texture format is GL_RGB / GL_ATC_RGB_AMD" );
			return true;
		}
		case Texture_ATC_RGBA:
		{
			glFormat = GL_RGBA; 
			glInternalFormat = GL_ATC_RGBA_EXPLICIT_ALPHA_AMD; 
//			LOG( "GL texture format is GL_RGBA / GL_ATC_RGBA_EXPLICIT_ALPHA_AMD" );
			return true;
		}
    }
	return false;
}

bool GlFormatToTextureFormat( int & format, const GLenum glFormat, const GLenum glInternalFormat )
{
	if ( glFormat == GL_RED && glInternalFormat == GL_R8 )
	{
		format = Texture_R;
		return true;
	}
	if ( glFormat == GL_RGB && ( glInternalFormat == GL_RGB || glInternalFormat == GL_SRGB8 ) )
	{
		format = Texture_RGB;
		return true;
	}
	if ( glFormat == GL_RGBA && ( glInternalFormat == GL_RGBA || glInternalFormat == GL_SRGB8_ALPHA8 ) )
	{
		format = Texture_RGBA;
		return true;
	}
	if ( ( glFormat == 0 || glFormat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ) && glInternalFormat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT )
	{
		format = Texture_DXT1;
		return true;
	}
	if ( ( glFormat == 0 || glFormat == GL_RGB ) && glInternalFormat == GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG )
	{
		format = Texture_PVR4bRGB;
		return true;
	}
	if ( ( glFormat == 0 || glFormat == GL_RGBA ) && glInternalFormat == GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG )
	{
		format = Texture_PVR4bRGBA;
		return true;
	}
	if ( ( glFormat == 0 || glFormat == GL_RGB ) && ( glInternalFormat == GL_ETC1_RGB8_OES || glInternalFormat == GL_COMPRESSED_SRGB8_ETC2 ) )
	{
		format = Texture_ETC1;
		return true;
	}
	if ( ( glFormat == 0 || glFormat == GL_RGB ) && ( glInternalFormat == GL_COMPRESSED_RGB8_ETC2 || glInternalFormat == GL_COMPRESSED_SRGB8_ETC2 ) )
	{
		format = Texture_ETC2_RGB;
		return true;
	}
	if ( ( glFormat == 0 || glFormat == GL_RGBA ) && ( glInternalFormat == GL_COMPRESSED_RGBA8_ETC2_EAC || glInternalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC ) )
	{
		format = Texture_ETC2_RGBA;
		return true;
	}
	if ( ( glFormat == 0 || glFormat == GL_RGB ) && glInternalFormat == GL_ATC_RGB_AMD )
	{
		format = Texture_ATC_RGB;
		return true;
	}
	if ( ( glFormat == 0 || glFormat == GL_RGBA ) && glInternalFormat == GL_ATC_RGBA_EXPLICIT_ALPHA_AMD )
	{
		format = Texture_ATC_RGBA;
		return true;
    }
	return false;
}

}	// namespace OVR

#endif
