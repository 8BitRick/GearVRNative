/************************************************************************************

Filename    :   OVR_FileSys.cpp
Content     :   Abraction layer for file systems.
Created     :   July 1, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_FileSys.h"

#include "OVR_Stream_Impl.h"
#include "Kernel/OVR_UTF8Util.h"
#include "Kernel/OVR_Array.h"
#include <cctype>	// for isdigit, isalpha
#include "OVR_Uri.h"
#include "PathUtils.h"
#include "Kernel/OVR_LogUtils.h"

#if defined( OVR_OS_ANDROID )
#	include "Android/JniUtils.h"
#	include "SystemActivities.h"	// for PUI_PACKAGE_NAME
#elif defined( OVR_OS_WIN32 )
#include <direct.h>
#endif

namespace OVR {


//==============================================================
// ovrFileSysLocal
class ovrFileSysLocal : public ovrFileSys
{
public:
	// this is yucky right now because it's Java-specific, even though windows doesn't care about it.
							ovrFileSysLocal( ovrJava const & javaContext );	
	virtual					~ovrFileSysLocal();

	virtual ovrStream *		OpenStream( char const * uri, ovrStreamMode const mode );
	virtual void			CloseStream( ovrStream * & stream );
	virtual bool			ReadFile( char const * uri, MemBufferT< uint8_t > & outBuffer );

	virtual void			Shutdown();

private:
	Array< ovrUriScheme* >	Schemes;

private:
	int						FindSchemeIndexForName( char const * schemeName ) const;
	ovrUriScheme *			FindSchemeForName( char const * name ) const;
};

//==============================
// ovrFileSysLocal::ovrFileSysLocal
ovrFileSysLocal::ovrFileSysLocal( ovrJava const & javaContext )
{
	// always do unit tests on startup to assure nothing has been broken
	ovrUri::DoUnitTest();

#if defined( OVR_OS_ANDROID )
	// add the apk scheme 
	ovrUriScheme_Apk * scheme = new ovrUriScheme_Apk( "apk" );

	// add a host for the executing application's scheme
	char curPackageName[OVR_MAX_PATH_LEN];
	ovr_GetCurrentPackageName( javaContext.Env, javaContext.ActivityObject, curPackageName, sizeof( curPackageName ) );
	char curPackageCodePath[OVR_MAX_PATH_LEN];
	ovr_GetPackageCodePath( javaContext.Env, javaContext.ActivityObject, curPackageCodePath, sizeof( curPackageCodePath ) );

	// not sure if this is necessary... shouldn't the application always have permission to open its own scheme?
/*
	String outPath;
	const bool validCacheDir = StoragePaths->GetPathIfValidPermission(
			EST_INTERNAL_STORAGE, EFT_CACHE, "", permissionFlags_t( PERMISSION_WRITE ) | PERMISSION_READ, outPath );
	ovr_OpenApplicationPackage( temp, validCacheDir ? outPath.ToCStr() : NULL );
*/
	char curPackageUri[OVR_MAX_URI_LEN];
	OVR_sprintf( curPackageUri, sizeof( curPackageUri ), "file://%s", curPackageCodePath );
	if ( !scheme->OpenHost( "localhost", curPackageUri ) )
	{
		LOG( "Failed to OpenHost for host '%s', uri '%s'", "localhost", curPackageUri );
		OVR_ASSERT( false );
	}

	// add the hosts for the language scheme
	{
		for ( int i = 0; i < 2; ++i )
		{
			char const * packageName = ( i == 0 ) ? PUI_PACKAGE_NAME : curPackageName;
			char packagePath[OVR_MAX_PATH_LEN];
			packagePath[0] = '\0';
			if ( ovr_GetInstalledPackagePath( javaContext.Env, javaContext.ActivityObject, packageName, packagePath, sizeof( packagePath ) ) )
			{
				char packageUri[sizeof( packagePath ) + 7 ];
				OVR_sprintf( packageUri, sizeof( packageUri ), "file://%s", packagePath );

				scheme->OpenHost( packageName, packageUri );
				scheme->OpenHost( "lang", packageUri );
				break;
			}
		}
	}

	Schemes.PushBack( scheme );

	// add the host for font assets by opening a stream and trying to load res/raw/font_location.txt from the System Activites apk.
	// If this file exists then
	{
		MemBufferT< uint8_t > buffer;
		char fileName[256];
		OVR::OVR_sprintf( fileName, sizeof( fileName ), "apk://%s/res/raw/font_location.txt", PUI_PACKAGE_NAME );
		char fontPackageName[1024];		
		bool success = ReadFile( fileName, buffer );
		if ( success && buffer.GetSize() > 0 )
		{
			OVR::OVR_strncpy( fontPackageName, sizeof( fontPackageName ), ( char const * )( static_cast< uint8_t const * >( buffer ) ), buffer.GetSize() );
			LOG( "Found font package name '%s'", fontPackageName );
		} else {
			// default to the SystemActivities apk.
			OVR::OVR_strcpy( fontPackageName, sizeof( fontPackageName ), PUI_PACKAGE_NAME );
		}

		char packagePath[OVR_MAX_PATH_LEN];
		packagePath[0] = '\0';
		if ( ovr_GetInstalledPackagePath( javaContext.Env, javaContext.ActivityObject, fontPackageName, packagePath, sizeof( packagePath ) ) )
		{
			// add this package to our scheme as a host so that fonts can be loaded from it
			char packageUri[sizeof( packagePath ) + 7 ];
			OVR_sprintf( packageUri, sizeof( packageUri ), "file://%s", packagePath );				

			// add the package name as an explict host if it doesn't already exists -- it will already exist if the package name
			// is not overrloaded by font_location.txt (i.e. the fontPackageName will have defaulted to PUI_PACKAGE_NAME )
			if ( !scheme->HostExists( fontPackageName ) ) {
				scheme->OpenHost( fontPackageName, packageUri );
			}
			scheme->OpenHost( "font", packageUri );

			LOG( "Added host '%s' for fonts @'%s'", fontPackageName, packageUri );
		}
	}

#elif defined( OVR_OS_WIN32 )
	ovrPathUtils::DoUnitTests();

	// add the apk scheme for the working path
	ovrUriScheme_File * scheme = new ovrUriScheme_File( "apk" );

	// Assume the working dir has an assets/ and res/ folder in it as is common on Android. Normally
	// the working folder would be Projects/Android.
	char curWorkingDir[MAX_PATH];
	_getcwd( curWorkingDir, sizeof( curWorkingDir ) );
	char uriWorkingDir[MAX_PATH];
	ovrPathUtils::FixSlashesForUri( curWorkingDir, uriWorkingDir, sizeof( uriWorkingDir ) );

	char dataUri[OVR_MAX_PATH_LEN];
	OVR_sprintf( dataUri, sizeof( dataUri ), "file:///%s", uriWorkingDir );

	// Also add a host path for the VR App Framework library.
	// HACK: this is currently relying on the relative path to VrAppFramework being the same for all projects, which it's not.
	// FIXME: change this to use command line parameters that specify additional paths.
	char frameworkPath[MAX_PATH];
	OVR_sprintf( frameworkPath, sizeof( frameworkPath ), "%s/%s", curWorkingDir, "../../../VrAppFramework" );
	// collapse the path -- fopen() was not working with relative paths, though it should?
	char frameworkPathCanonical[MAX_PATH];
	ovrPathUtils::CollapsePath( frameworkPath, frameworkPathCanonical, sizeof( frameworkPathCanonical ) );

	char frameworkUri[OVR_MAX_PATH_LEN];
	OVR_sprintf( frameworkUri, sizeof( frameworkUri ), "file:///%s", frameworkPathCanonical );

	scheme->OpenHost( "localhost", frameworkUri );
	scheme->AddHostSourceUri( "localhost", dataUri );

	static const char * PUI_PACKAGE_NAME = "com.oculus.systemactivities";
	scheme->OpenHost( PUI_PACKAGE_NAME, frameworkUri );
	scheme->AddHostSourceUri( PUI_PACKAGE_NAME, dataUri );

	scheme->OpenHost( "lang", frameworkUri );
	scheme->AddHostSourceUri( "lang", dataUri );

	scheme->OpenHost( "font", frameworkUri );
	scheme->AddHostSourceUri( "font", dataUri );	

	Schemes.PushBack( scheme );	
#else
#error Unsupported platform!
#endif
}

//==============================
// ovrFileSysLocal::ovrFileSysLocal
ovrFileSysLocal::~ovrFileSysLocal()
{
}

//==============================
// ovrFileSysLocal::OpenStream
ovrStream *	ovrFileSysLocal::OpenStream( char const * uri, ovrStreamMode const mode )
{
	// parse the Uri to find the scheme
	char scheme[OVR_MAX_SCHEME_LEN];
	char host[OVR_MAX_HOST_NAME_LEN];
	char path[OVR_MAX_PATH_LEN];
	int port = 0;
	ovrUri::ParseUri( uri, scheme, sizeof( scheme ), NULL, 0, NULL, 0, host, sizeof( host ), 
			port, path, sizeof( path ), NULL, 0, NULL, 0 );

	ovrUriScheme * uriScheme = FindSchemeForName( scheme );
	if ( uriScheme == NULL )
	{
		LOG( "Uri '%s' missing scheme! Assuming apk scheme!", uri );
		uriScheme = FindSchemeForName( "apk" );
		if ( uriScheme == NULL )
		{
			return NULL;
		}
	}

	ovrStream * stream = uriScheme->AllocStream();
	if ( stream == NULL )
	{
		OVR_ASSERT( stream != NULL );
		return NULL;
	}
	if ( !stream->Open( uri, mode ) )
	{
		delete stream;
		return NULL;
	}
	return stream;
}

//==============================
// ovrFileSysLocal::CloseStream
void ovrFileSysLocal::CloseStream( ovrStream * & stream )
{
	if ( stream != NULL )
	{
		stream->Close();
		delete stream;
		stream = NULL;
	}
}

//==============================
// ovrFileSysLocal::ReadFile
bool ovrFileSysLocal::ReadFile( char const * uri, MemBufferT< uint8_t > & outBuffer )
{
	ovrStream * stream = OpenStream( uri, OVR_STREAM_MODE_READ );
	if ( stream == NULL )
	{
		return false;
	}
	bool success = stream->ReadFile( uri, outBuffer );
	CloseStream( stream );
	return success;
}

//==============================
// ovrFileSysLocal::FindSchemeIndexForName
int ovrFileSysLocal::FindSchemeIndexForName( char const * schemeName ) const
{
	for ( int i = 0; i < Schemes.GetSizeI(); ++i )
	{
		if ( OVR_stricmp( Schemes[i]->GetSchemeName(), schemeName ) == 0 )
		{
			return i;
		}
	}
	return -1;
}

//==============================
// ovrFileSysLocal::FindSchemeForName
ovrUriScheme * ovrFileSysLocal::FindSchemeForName( char const * name ) const
{
	int index = FindSchemeIndexForName( name );
	return index < 0 ? NULL : Schemes[index];
}

//==============================
// ovrFileSysLocal::Shutdown
void ovrFileSysLocal::Shutdown()
{
	for ( int i = 0; i < Schemes.GetSizeI(); ++i )
	{
		Schemes[i]->Shutdown();
		delete Schemes[i];
		Schemes[i] = NULL;
	}
	Schemes.Clear();
}

//==============================================================================================
// ovrFileSys
//==============================================================================================

//==============================
// ovrFileSys::Create
ovrFileSys *	ovrFileSys::Create( ovrJava const & javaContext )
{
	ovrFileSys * fs = new ovrFileSysLocal( javaContext );
	return fs;
}

//==============================
// ovrFileSys::Destroy
void ovrFileSys::Destroy( ovrFileSys * & fs )
{
	if ( fs != NULL )
	{
		ovrFileSysLocal * fsl = static_cast< ovrFileSysLocal* >( fs );
		fsl->Shutdown();
		delete fs;
		fs = NULL;
	}
}

} // namespace OVR
