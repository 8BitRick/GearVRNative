/************************************************************************************

Filename    :   MemBuffer.cpp
Content     :	Memory buffer.
Created     :	May 13, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_MemBuffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined( OVR_OS_ANDROID )
#include <unistd.h>
#endif

#include "OVR_Log.h"

namespace OVR
{

bool MemBuffer::WriteToFile( const char * filename )
{
	LogText( "Writing %i bytes to %s", Length, filename );
	FILE * f = fopen( filename, "wb" );
	if ( f != NULL )
	{
		fwrite( Buffer, Length, 1, f );
		fclose( f );
		return true;
	}
	else
	{
		LogText( "MemBuffer::WriteToFile failed to write to %s", filename );
	}
	return false;
}

MemBuffer::MemBuffer( int length ) : Buffer( malloc( length ) ), Length( length )
{
}

MemBuffer::~MemBuffer()
{
}

void MemBuffer::FreeData()
{
	if ( Buffer != NULL )
	{
		free( (void *)Buffer );
		Buffer = NULL;
	}
	Length = 0;
}

MemBufferFile::MemBufferFile( const char * filename )
{
	LoadFile( filename );
}

bool MemBufferFile::LoadFile( const char * filename )
{
	FreeData();
#if !defined( OVR_OS_ANDROID )
	FILE * f = fopen( filename, "rb" );
	if ( !f )
	{
		LogText( "Couldn't open %s", filename );
		return false;
	}
	fseek( f, 0, SEEK_END );
	Length = ftell( f );
	fseek( f, 0, SEEK_SET );
	Buffer = malloc( Length );
	const size_t readRet = fread( (unsigned char *)Buffer, Length, 1, f );
	fclose( f );
	if ( readRet != 1 )
	{
		LogText( "Only read %zu of %i bytes in %s", readRet, Length, filename );
		FreeData();
		return false;
	}
	return true;
#else
	// Using direct IO gives read speeds of 200 - 290 MB/s,
	// versus 130 - 170 MB/s with buffered stdio on a background thread.
	const int fd = open( filename, O_RDONLY, 0 );
	if ( fd < 0 )
	{
		LogText( "Couldn't open %s", filename );
		return false;
	}
	struct stat buf;
	if ( -1 == fstat( fd, &buf ) )
	{
		close( fd );
		LogText( "Couldn't fstat %s", filename );
		return false;
	}
	Length = (int)buf.st_size;
	Buffer = malloc( Length );
	const size_t readRet = read( fd, (unsigned char *)Buffer, Length );
	close( fd );
	if ( readRet != (size_t)Length )
	{
		LogText( "Only read %zu of %i bytes in %s", readRet, Length, filename );
		FreeData();
		return false;
	}
	return true;
#endif
}

MemBufferFile::MemBufferFile( eNoInit const noInit )
{
	OVR_UNUSED( noInit );
}

MemBuffer MemBufferFile::ToMemBuffer()
{
	MemBuffer	mb( Buffer, Length );
	Buffer = NULL;
	Length = 0;
	return mb;
}

MemBufferFile::~MemBufferFile()
{
	FreeData();
}

}	// namespace OVR
