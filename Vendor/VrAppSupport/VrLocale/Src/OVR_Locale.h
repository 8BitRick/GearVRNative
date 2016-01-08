/************************************************************************************

Filename    :   OVR_Locale.h
Content     :   Header file for string localization interface.
Created     :   April 6, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/

#if !defined( OVR_LOCALE_H_ )
#define OVR_LOCALE_H_

#include <stdint.h>
#include "Kernel/OVR_String.h"

// TODO: remove String from this interface to reduce dependencies on LibOVRKernel.

namespace OVR {

class App;

class ovrLocale 
{
public:
	static char const *	LOCALIZED_KEY_PREFIX;
	static size_t const	LOCALIZED_KEY_PREFIX_LEN;

	//----------------------------------------------------------
	// static methods
	//----------------------------------------------------------
	// creates a locale object for the system's current locale.
	static ovrLocale *	Create( App & app, char const * name );

	// frees the local object
	static void			Destroy( ovrLocale * & localePtr );

	// Takes a UTF8 string and returns an identifier that can be used as an Android string id.
	static String		MakeStringIdFromUTF8( char const * str );

	// Takes an ANSI string and returns an identifier that can be used as an Android string id. 
	static String		MakeStringIdFromANSI( char const * str );

	// Localization : Returns xliff formatted string
	// These are set to const char * to make sure that's all that's passed in - we support up to 9, add more functions as needed
	static String		GetXliffFormattedString( const String & inXliffStr, const char * arg1 );
	static String		GetXliffFormattedString( const String & inXliffStr, const char * arg1, const char * arg2 );
	static String		GetXliffFormattedString( const String & inXliffStr, const char * arg1, const char * arg2, const char * arg3 );

	static String		ToString( char const * fmt, float const f );
	static String		ToString( char const * fmt, int const i );

	//----------------------------------------------------------
	// public non-virtual interface methods
	//----------------------------------------------------------
	virtual ~ovrLocale() { }

	virtual char const *	GetName() const = 0;
	virtual char const *	GetLanguageCode() const = 0;

	// returns true if this locale is the system's default locale (on Android this
	// means the string resources are loaded from res/values/ vs. res/values-*/
	virtual bool			IsSystemDefaultLocale() const = 0;

	virtual bool			LoadStringsFromAndroidFormatXMLFile( char const * fileName ) = 0;

	// takes a file name, a buffer and a size in bytes. The buffer must already have
	// been loaded. The name is only an identifier used for error reporting.
	virtual bool			AddStringsFromAndroidFormatXMLBuffer( char const * name, char const * buffer, size_t const size ) = 0;

	// returns the localized string associated with the passed key. Returns false if the
	// key was not found. If the key was not found, out will be set to the defaultStr.
	virtual bool			GetString( char const * key, char const * defaultStr, String & out ) const = 0;
};

} // namespace OVR

#endif // OVR_LOCALE_H_