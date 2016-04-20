/************************************************************************************

Filename    :   MainActivity.java
Content     :   
Created     :   
Authors     :   

Copyright   :   None.

*************************************************************************************/
package com.yourcomp.gearvrnative;

import android.content.Context;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.content.Intent;
import com.oculus.vrappframework.VrActivity;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.CharBuffer;
import java.util.logging.Logger;

public class MainActivity extends VrActivity {

	public static final String TAG = "GearVR";
	
	/** Load jni .so on initialization */
	static {
		Log.d( TAG, "LoadLibrary" );
		System.loadLibrary( "fmodL" );
		System.loadLibrary( "gearvrnative" );
	}

	public static native long nativeSetAppInterface( VrActivity act, String fromPackageNameString, String commandString, String uriString );

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		Log.d(TAG, "onCreate");
		super.onCreate(savedInstanceState);

		org.fmod.FMOD.init(this);

		Intent intent = getIntent();
		String commandString = VrActivity.getCommandStringFromIntent( intent );
		String fromPackageNameString = VrActivity.getPackageStringFromIntent( intent );
		String uriString = VrActivity.getUriStringFromIntent( intent );

		setAppPtr( nativeSetAppInterface( this, fromPackageNameString, commandString, uriString ) );
	}
}
