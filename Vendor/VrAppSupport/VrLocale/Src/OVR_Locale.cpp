/************************************************************************************

Filename    :   OVR_Locale.cpp
Content     :   Implementation of string localization for strings loaded at run-time.
Created     :   April 6, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/

#include "OVR_Locale.h"

#include <sys/stat.h>

#include "tinyxml2.h"
#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_Hash.h"
#include "Kernel/OVR_MemBuffer.h"
#include "Kernel/OVR_JSON.h"
#include "Kernel/OVR_LogUtils.h"
#include "Android/JniUtils.h"
// Not including App.h means we would need to pass the JNIEnv, the ActivityObject 
// and the VrActivityClass to several functions any time they're called.
#include "App.h"	
#include "PackageFiles.h"

namespace OVR {

char const *	ovrLocale::LOCALIZED_KEY_PREFIX = "@string/";
size_t const	ovrLocale::LOCALIZED_KEY_PREFIX_LEN = OVR_strlen( LOCALIZED_KEY_PREFIX );

// DJB2 string hash function
static unsigned long DJB2Hash( char const * str )
{
    unsigned long hash = 5381;
    int c;

	for ( c = *str; c != 0; str++, c = *str )
    //while ( c = *str++ )
	{
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}
    return hash;
}

//==============================================================
// Hash functor for OVR::String

template< class C >
class OvrStringHash
{
public:
	UPInt operator()( const C & data ) const 
	{
		return DJB2Hash( data.ToCStr() );
	}
};

//==============================================================
// ovrLocaleInternal
class ovrLocaleInternal : public ovrLocale
{
public:
	static char const *	LOCALIZED_KEY_PREFIX;
	static OVR::UPInt	LOCALIZED_KEY_PREFIX_LEN;

#if defined( OVR_OS_ANDROID )
	ovrLocaleInternal( App & app_, char const * name, char const * languageCode );
#else
	ovrLocaleInternal( char const * name, char const * languageCode );
#endif
	virtual ~ovrLocaleInternal();

	// returns the language code for this locale object
	virtual char const *	GetName() const { return Name.ToCStr(); }
	
	virtual char const *	GetLanguageCode() const { return LanguageCode.ToCStr(); }
	
	virtual bool			IsSystemDefaultLocale() const;
	
	virtual bool			LoadStringsFromAndroidFormatXMLFile( char const * fileName );

	virtual bool			AddStringsFromAndroidFormatXMLBuffer( char const * name, char const * buffer, size_t const size );

	virtual bool			GetString( char const * key, char const * defaultStr, String & out ) const;

private:
#if defined( OVR_OS_ANDROID )
	App &									app;			// stupid non-prefixed class name... cascade of fail.
#endif

	typedef OvrStringHash< String >			HashFunctor;

	String									Name;			// user-specified locale name
	String									LanguageCode;	// system-specific locale name
	OVR::Hash< String, int, HashFunctor >	StringHash;
	Array< String	>						Strings;

private:
#if defined( OVR_OS_ANDROID )
	bool					GetStringJNI( char const * key, char const * defaultOut, String & out ) const;
#endif
};

char const *	ovrLocaleInternal::LOCALIZED_KEY_PREFIX = "@string/";
OVR::UPInt		ovrLocaleInternal::LOCALIZED_KEY_PREFIX_LEN = OVR_strlen( LOCALIZED_KEY_PREFIX );

//==============================
// ovrLocaleInternal::ovrLocaleInternal
#if defined( OVR_OS_ANDROID )
ovrLocaleInternal::ovrLocaleInternal( App & app_, char const * name, char const * languageCode )
	: app( app_ )
	, Name( name )
	, LanguageCode( languageCode )
{
}
#else
ovrLocaleInternal::ovrLocaleInternal( char const * name, char const * languageCode )
	: Name( name )
	, LanguageCode( languageCode )
{
}
#endif

//==============================
// ovrLocaleInternal::~ovrLocaleInternal
ovrLocaleInternal::~ovrLocaleInternal()
{
}

//==============================
// ovrLocaleInternal::IsSystemDefaultLocale
bool ovrLocaleInternal::IsSystemDefaultLocale() const 
{
#if defined( OVR_OS_ANDROID )
	return OVR_stricmp( LanguageCode.ToCStr(), "en" ) == 0;
#else
	// FIXME: Implement
	return true;
#endif
}

//==============================
// ovrLocaleInternal::AddStringsFromAndroidFormatXMLBuffer
bool ovrLocaleInternal::AddStringsFromAndroidFormatXMLBuffer( char const * name, char const * buffer, size_t const size )
{
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLError error = doc.Parse( buffer, size );
	if ( error != tinyxml2::XML_NO_ERROR )
	{
		LOG( "ERROR: XML parse error %i parsing '%s'!", error, name );
		return false;
	}

	tinyxml2::XMLElement * root = doc.RootElement();
	if ( OVR_stricmp( root->Value(), "resources" ) != 0 )
	{
		LOG( "ERROR: Expected root value of 'resources', found '%s'!\n", root->Value() );
		return false;
	}

	tinyxml2::XMLElement const * curElement = root->FirstChildElement();
	for ( ; curElement != NULL; curElement = curElement->NextSiblingElement() )
	{
		if ( OVR_stricmp( curElement->Value(), "string" ) != 0 )
		{
			LOG( "WARNING: Expected element value 'string', found '%s'!\n", curElement->Value() );
			continue;
		}

		tinyxml2::XMLAttribute const * nameAttr = curElement->FindAttribute( "name" );

		String key = nameAttr->Value();
		String value = curElement->GetText();
		String decodedValue;
		// fix special encodings. Use GetFirstCharAt() and GetNextChar() to handle UTF-8.
		const char * ofs = NULL;
		uint32_t curChar = value.GetFirstCharAt( 0, &ofs );
		while( curChar != 0 )
		{
			if ( curChar == '\\' )
			{
				uint32_t nextChar = value.GetNextChar( &ofs );
				if ( nextChar == 0 )
				{
					break;
				}
				if ( nextChar != '<' &&
					 nextChar != '>' &&
					 nextChar != '"' &&
					 nextChar != '\'' &&
					 nextChar != '&' )
				{
					LOG( "Unknown escape sequence '\\%x'", nextChar );
					decodedValue.AppendChar( curChar );
				}
				curChar = nextChar;
			}

			decodedValue.AppendChar( curChar );

			curChar = value.GetNextChar( &ofs );
		}

		LOG( "Name: '%s' = '%s'\n", key.ToCStr(), value.ToCStr() );

		int index = -1;
		if ( !StringHash.Get( key, &index ) )
		{
			StringHash.Add( key, Strings.GetSizeI() );
			Strings.PushBack( decodedValue );
		}
	}

	LOG( "Added %i strings from '%s'", Strings.GetSizeI(), name );

	return true;
}

//==============================
// FileSize
namespace {

size_t FileSize( FILE * f )
{
	if ( f == NULL )
	{
		return 0;
	}
#if defined( WIN32 )
	struct _stat64 stats;
	__int64 r = _fstati64( f->_file, &stats );
#else
	struct stat stats;
	int r = fstat( f->_file, &stats );
#endif
	if ( r < 0 )
	{
		return 0;
	}
	return static_cast< size_t >( stats.st_size );	// why st_size is signed I have no idea... negative file lengths?
}

}	// empty namespace

//==============================
// ovrLocaleInternal::LoadStringsFromAndroidFormatXMLFile
bool ovrLocaleInternal::LoadStringsFromAndroidFormatXMLFile( char const * fileName ) 
{
	FILE * f = fopen( fileName, "rb" );
	if ( f == NULL )
	{
		return false;
	}

	size_t fsize = FileSize( f );

	MemBufferT< char > buffer( fsize );

	size_t numRead = fread( buffer, fsize, 1, f );

	fclose( f );

	if ( numRead != 1 )
	{
		return false;
	}

	return AddStringsFromAndroidFormatXMLBuffer( fileName, buffer, buffer.GetSize() );
}

#if defined( OVR_OS_ANDROID )
//==============================
// ovrLocale::GetStringJNI
// Get's a localized UTF-8-encoded string from the Android application's string table.
bool ovrLocaleInternal::GetStringJNI( char const * key, char const * defaultOut, String & out ) const
{
	if ( app.GetJava()->Env == NULL )
	{
		WARN( "ovrLocale: app.GetJava()->Env = NULL!" );
		out = defaultOut;
		return false;
	}

	if ( app.GetJava()->ActivityObject == NULL )
	{
		WARN( "ovrLocale: activityObject = NULL!" );
		out = defaultOut;
		return false;
	}

	//LOG( "Localizing key '%s'", key );
	// if the key doesn't start with KEY_PREFIX then it's not a valid key, just return
	// the key itself as the output text.
	if ( strstr( key, LOCALIZED_KEY_PREFIX ) != key )
	{
		out = defaultOut;
		LOG( "no prefix, localized to '%s'", out.ToCStr() );
		return true;
	}

	char const * realKey = key + LOCALIZED_KEY_PREFIX_LEN;
	//LOG( "realKey = %s", realKey );

	JavaClass vrLocaleClass( app.GetJava()->Env, ovr_GetLocalClassReference( app.GetJava()->Env, app.GetJava()->ActivityObject, 
			"com/oculus/vrlocale/VrLocale" ) );
	jmethodID const getLocalizedStringId = ovr_GetStaticMethodID( app.GetJava()->Env, vrLocaleClass.GetJClass(),
		"getLocalizedString", "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;" );
	if ( getLocalizedStringId != NULL )
	{
		JavaString keyObj( app.GetJava()->Env, realKey );
		JavaUTFChars resultStr( app.GetJava()->Env, static_cast< jstring >( app.GetJava()->Env->CallStaticObjectMethod( vrLocaleClass.GetJClass(), 
				getLocalizedStringId, app.GetJava()->ActivityObject, keyObj.GetJString() ) ) );
		if ( !app.GetJava()->Env->ExceptionOccurred() )
		{
			out = resultStr;
			if ( out.IsEmpty() )
			{
				out = defaultOut;
				LOG( "key not found, localized to '%s'", out.ToCStr() );
				return false;
			}

			//LOG( "localized to '%s'", out.ToCStr() );
			return true;
		}
		WARN( "Exception calling VrLocale.getLocalizedString" );
	}
	else
	{
		WARN( "Could not find VrLocale.getLocalizedString()" );
	}

	out = "JAVAERROR";
	OVR_ASSERT( false );	// the java code is missing getLocalizedString or an exception occured while calling it
	return false;
}
#endif

//==============================
// ovrLocaleInternal::GetString
bool ovrLocaleInternal::GetString( char const * key, char const * defaultStr, String & out ) const 
{
	if ( key == NULL )
	{
		return false;
	}

	if ( strstr( key, LOCALIZED_KEY_PREFIX ) == key )
	{
		if ( Strings.GetSizeI() > 0 )
		{
			String realKey( key + LOCALIZED_KEY_PREFIX_LEN );
			int index = -1;
			if ( StringHash.Get( realKey, &index ) )
			{
				out = Strings[index];
				return true;
			}
		}
	}
#if defined( OVR_OS_ANDROID )
	// try instead to find the string via Android's resources. Ideally, we'd have combined these all
	// into our own hash, but enumerating application resources from library code on is problematic
	// on android
	if ( GetStringJNI( key, defaultStr, out ) )
	{
		return true;
	}
#endif
	out = defaultStr != NULL ? defaultStr : "";
	return false;
}



//==============================================================================================
// ovrLocale		
// static functions for managing the global instance to a ovrLocaleInternal object
//==============================================================================================

//==============================
// ovrLocale::Create
ovrLocale * ovrLocale::Create( App & app, char const * name )
{
	LOG( "ovrLocale::Create - entered" );

	ovrLocale * localePtr = NULL;

#if defined( OVR_OS_ANDROID )
	// add the strings from the Android resource file
	JavaClass vrLocaleClass( app.GetJava()->Env, ovr_GetLocalClassReference( app.GetJava()->Env, app.GetJava()->ActivityObject, 
			"com/oculus/vrlocale/VrLocale" ) );
	if ( vrLocaleClass.GetJClass() == NULL )
	{
		LOG( "Couldn't find VrLocale class." );
	}
	jmethodID getCurrentLanguageMethodId = ovr_GetStaticMethodID( app.GetJava()->Env, vrLocaleClass.GetJClass(),
			"getCurrentLanguage", "()Ljava/lang/String;" );
	if ( getCurrentLanguageMethodId != NULL )
	{
		char const * languageCode = "en";
		JavaUTFChars utfCurrentLanguage( app.GetJava()->Env, (jstring)app.GetJava()->Env->CallStaticObjectMethod( vrLocaleClass.GetJClass(), getCurrentLanguageMethodId ) );
		if ( app.GetJava()->Env->ExceptionOccurred() )
		{
			WARN( "Exception occurred when calling getCurrentLanguage" );
			app.GetJava()->Env->ExceptionClear();
		}
		else
		{
			languageCode = utfCurrentLanguage.ToStr();
		}
		localePtr = new ovrLocaleInternal( app, name, languageCode );
	}
	else
	{
		WARN( "Could not find VrLocale.getCurrentLanguage" );
	}
#else
	OVR_UNUSED( app );
	localePtr = new ovrLocaleInternal( name, "en" );
#endif

	LOG( "ovrLocale::Create - exited" );
	return localePtr;
}

//==============================
// ovrLocale::Destroy
void ovrLocale::Destroy( ovrLocale * & localePtr )
{
	delete localePtr;
	localePtr = NULL;
}

//==============================
// ovrLocale::MakeStringIdFromUTF8
// Turns an arbitray ansi string into a string id.
// - Deletes any character that is not a space, letter or number.
// - Turn spaces into underscores.
// - Ignore contiguous spaces.
String ovrLocale::MakeStringIdFromUTF8( char const * str )
{
	enum eLastOutputType
	{
		LO_LETTER,
		LO_DIGIT,
		LO_SPACE,
		LO_MAX
	};
	eLastOutputType lastOutputType = LO_MAX;
	String out = LOCALIZED_KEY_PREFIX;
	char const * ptr = str;
	if ( strstr( str, LOCALIZED_KEY_PREFIX ) == str )
	{
		// skip UTF-8 chars... technically could just += LOCALIZED_KEY_PREFIX_LEN if the key prefix is only ANSI chars...
		for ( size_t i = 0; i < LOCALIZED_KEY_PREFIX_LEN; ++i )
		{
			UTF8Util::DecodeNextChar( &ptr );
		}
	}
	int n = static_cast< int >( UTF8Util::GetLength( ptr ) );
	for ( int i = 0; i < n; ++i )
	{
		uint32_t c = UTF8Util::DecodeNextChar( &ptr );
		if ( ( c >= '0' && c <= '9' ) ) 
		{
			if ( i == 0 )
			{
				// string identifiers in Android cannot start with a number because they
				// are also encoded as Java identifiers, so output an underscore first.
				out.AppendChar( '_' );
			}
			out.AppendChar( c );
			lastOutputType = LO_DIGIT;
		}
		else if ( ( c >= 'a' && c <= 'z' ) )
		{
			// just output the character
			out.AppendChar( c );
			lastOutputType = LO_LETTER;
		}
		else if ( ( c >= 'A' && c <= 'Z' ) )
		{
			// just output the character as lowercase
			out.AppendChar( c + 32 );
			lastOutputType = LO_LETTER;
		}
		else if ( c == 0x20 )
		{
			if ( lastOutputType != LO_SPACE )
			{
				out.AppendChar( '_' );
				lastOutputType = LO_SPACE;
			}
			continue;
		}
		// ignore everything else
	}
	return out;
}

//==============================
// ovrLocale::MakeStringIdFromANSI
// Turns an arbitray ansi string into a string id.
// - Deletes any character that is not a space, letter or number.
// - Turn spaces into underscores.
// - Ignore contiguous spaces.
String ovrLocale::MakeStringIdFromANSI( char const * str )
{
	enum eLastOutputType
	{
		LO_LETTER,
		LO_DIGIT,
		LO_SPACE,
		LO_PUNCTUATION,
		LO_MAX
	};
	eLastOutputType lastOutputType = LO_MAX;
	String out = LOCALIZED_KEY_PREFIX;
	char const * ptr = strstr( str, LOCALIZED_KEY_PREFIX ) == str ? str + LOCALIZED_KEY_PREFIX_LEN : str;
	OVR::UPInt n = OVR_strlen( ptr );
	for ( unsigned int i = 0; i < n; ++i )
	{
		unsigned char c = ptr[i];
		if ( ( c >= '0' && c <= '9' ) ) 
		{
			if ( i == 0 )
			{
				// string identifiers in Android cannot start with a number because they
				// are also encoded as Java identifiers, so output an underscore first.
				out.AppendChar( '_' );
			}
			out.AppendChar( c );
			lastOutputType = LO_DIGIT;
		}
		else if ( ( c >= 'a' && c <= 'z' ) )
		{
			// just output the character
			out.AppendChar( c );
			lastOutputType = LO_LETTER;
		}
		else if ( ( c >= 'A' && c <= 'Z' ) )
		{
			// just output the character as lowercase
			out.AppendChar( c + 32 );
			lastOutputType = LO_LETTER;
		}
		else if ( c == 0x20 )
		{
			if ( lastOutputType != LO_SPACE )
			{
				out.AppendChar( '_' );
				lastOutputType = LO_SPACE;
			}
			continue;
		}
		// ignore everything else
	}
	return out;
}

//==============================
// private_GetXliffFormattedString
// Supports up to 9 arguments and %s format only
// inXliffStr is intentionally not passed by reference because "va_start has undefined behavior with reference types"
static String private_GetXliffFormattedString( const StringDataPtr inXliffStr, ... )
{
	// format spec looks like: %1$s - we expect at least 3 chars after %
	const int MIN_NUM_EXPECTED_FORMAT_CHARS = 3;

	// If the passed in string is shorter than minimum expected xliff formatting, just return it
	if ( static_cast< int >( inXliffStr.GetSize() ) <= MIN_NUM_EXPECTED_FORMAT_CHARS )
	{
		return inXliffStr.ToCStr();
	}

	// Buffer that holds formatted return string
	StringBuffer retStrBuffer;

	char const * p = inXliffStr.ToCStr();
	for ( ; ; )
	{
		uint32_t charCode = UTF8Util::DecodeNextChar( &p );
		if ( charCode == '\0' )
		{
			break;
		}
		else if( charCode == '%' )
		{
			// We found the start of the format specifier
			// Now check that there are at least three more characters which contain the format specification
			Array< uint32_t > formatSpec;
			for ( int count = 0; count < MIN_NUM_EXPECTED_FORMAT_CHARS; ++count )
			{
				uint32_t formatCharCode = UTF8Util::DecodeNextChar( &p );
				formatSpec.PushBack( formatCharCode );
			}

			OVR_ASSERT( formatSpec.GetSizeI() >= MIN_NUM_EXPECTED_FORMAT_CHARS );
			
			uint32_t desiredArgIdxChar = formatSpec.At( 0 );
			uint32_t dollarThing = formatSpec.At( 1 );
			uint32_t specifier = formatSpec.At( 2 );

			// Checking if it has supported xliff format specifier
			if( ( desiredArgIdxChar >= '1' && desiredArgIdxChar <= '9' ) &&
				( dollarThing == '$' ) &&
				( specifier == 's' ) )
			{
				// Found format valid specifier, so processing entire format specifier.
				int desiredArgIdxint = desiredArgIdxChar - '0';

				va_list args;
				va_start( args, inXliffStr );

				// Loop till desired argument is found.
				for( int j = 0; ; ++j )
				{
					const char* tempArg = va_arg( args, const char* );
					if( j == ( desiredArgIdxint - 1 ) ) // found desired argument
					{
						retStrBuffer.AppendFormat( "%s", tempArg );
						break;
					}
				}

				va_end(args);
			}
			else
			{
				LOG( "%s has invalid xliff format - has unsupported format specifier.", inXliffStr.ToCStr() );
				return inXliffStr.ToCStr();
			}
		}
		else
		{
			retStrBuffer.AppendChar( charCode );
		}
	}

	return String(retStrBuffer);
}

//==============================
// ovrLocale::GetXliffFormattedString
String ovrLocale::GetXliffFormattedString( const String & inXliffStr, const char * arg1 )
{
	return private_GetXliffFormattedString( StringDataPtr(inXliffStr), arg1 );
}

//==============================
// ovrLocale::GetXliffFormattedString
String ovrLocale::GetXliffFormattedString( const String & inXliffStr, const char * arg1, const char * arg2 )
{
	return private_GetXliffFormattedString( StringDataPtr(inXliffStr), arg1, arg2 );
}

//==============================
// ovrLocale::GetXliffFormattedString
OVR::String ovrLocale::GetXliffFormattedString( const String & inXliffStr, const char * arg1, const char * arg2, const char * arg3 )
{
	return private_GetXliffFormattedString( StringDataPtr(inXliffStr), arg1, arg2, arg3 );
}

//==============================
// ovrLocale::ToString
String ovrLocale::ToString( char const * fmt, float const f )
{
	char buffer[128];
	OVR_sprintf( buffer, 128, fmt, f );
	return String( buffer );
}

//==============================
// ovrLocale::ToString
String ovrLocale::ToString( char const * fmt, int const i )
{
	char buffer[128];
	OVR_sprintf( buffer, 128, fmt, i );
	return String( buffer );
}

} // namespace OVR
