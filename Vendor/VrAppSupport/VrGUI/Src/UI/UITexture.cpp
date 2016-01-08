/************************************************************************************

Filename    :   UITexture.cpp
Content     :
Created     :	1/8/2015
Authors     :   Jim Dose

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "UI/UITexture.h"
#include "App.h"
#include "PackageFiles.h"

namespace OVR {

UITexture::UITexture() :
	Width( 0 ),
	Height( 0 ),
	Texture( 0 ),
	FreeTextureOfDestruct( false )

{
}

UITexture::~UITexture()
{
	Free();
}

void UITexture::Free()
{
	if ( FreeTextureOfDestruct )
	{
		glDeleteTextures( 1, &Texture );
	}
	Texture = 0;
	Width = 0;
	Height = 0;
	FreeTextureOfDestruct = false;
}

void UITexture::LoadTextureFromApplicationPackage( const char *assetPath )
{
#if defined( OVR_OS_ANDROID )
	Free();
	Texture = OVR::LoadTextureFromApplicationPackage( assetPath, TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), Width, Height );
	FreeTextureOfDestruct = true;
#else
	MemBufferFile mbf(MemBufferFile::NoInit);
	if (mbf.LoadFile(assetPath))
	{
		LoadTextureFromBuffer(assetPath, mbf);
		mbf.FreeData();
	}
#endif
}

void UITexture::LoadTextureFromBuffer( const char * fileName, const MemBuffer & buffer )
{
	Free();
	Texture = OVR::LoadTextureFromBuffer( fileName, buffer, TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), Width, Height );
	FreeTextureOfDestruct = true;
}

void UITexture::LoadTextureFromMemory( const uint8_t * data, const float width, const float height )
{
	LOG( "UITexture::LoadTextureFromMemory" );
	Free();
	Width = width;
	Height = height;
	Texture = OVR::LoadRGBATextureFromMemory( data, width, height, true );
	FreeTextureOfDestruct = true;
}

void UITexture::SetTexture( const GLuint texture, const int width, const int height, const bool freeTextureOnDestruct )
{
	Free();
	FreeTextureOfDestruct = freeTextureOnDestruct;
	Texture = texture;
	Width = width;
	Height = height;
}

} // namespace OVR
