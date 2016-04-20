/************************************************************************************

Filename    :   GlTexture_Windows.cpp
Content     :   OpenGL texture loading.
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "GlTexture.h"

#if defined( OVR_OS_WIN32 )

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
				glInternalFormat = GL_RGB8; 
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
				glInternalFormat = GL_RGBA8; 
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
			glFormat = glInternalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
//			LOG( "GL texture format is GL_COMPRESSED_RGBA_S3TC_DXT1_EXT" );
			return true;
		}
		case Texture_DXT3:
		{
			glFormat = glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
//			LOG( "GL texture format is GL_COMPRESSED_RGBA_S3TC_DXT1_EXT" );
			return true;
		}
	// unsupported on OpenGL ES:
	//    case Texture_DXT3:  glFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
	//    case Texture_DXT5:  glFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
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
	if ( glFormat == GL_RGB && ( glInternalFormat == GL_RGB || glInternalFormat == GL_RGB8 || glInternalFormat == GL_SRGB8 ) )
	{
		format = Texture_RGB;
		return true;
	}
	if ( glFormat == GL_RGBA && ( glInternalFormat == GL_RGBA || glInternalFormat == GL_RGBA8 || glInternalFormat == GL_SRGB8_ALPHA8 ) )
	{
		format = Texture_RGBA;
		return true;
	}
	if ( ( glFormat == 0 || glFormat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ) && glInternalFormat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT )
	{
		format = Texture_DXT1;
		return true;
	}
	if ( ( glFormat == 0 || glFormat == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT ) && glInternalFormat == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT )
	{
		format = Texture_DXT3;
		return true;
	}
	return false;
}

}	// namespace OVR

#endif