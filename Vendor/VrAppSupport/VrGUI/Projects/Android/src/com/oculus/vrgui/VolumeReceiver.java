// Copyright 2014 Oculus VR, LLC. All Rights reserved.
package com.oculus.vrgui;

import android.util.Log;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.media.AudioManager;

class VolumeReceiver extends BroadcastReceiver {
	private static final String TAG = "VolumeReceiver";
	private static final boolean DEBUG = false;

	private static native void nativeVolumeChanged( int state );

	private static IntentFilter filter;
	private static VolumeReceiver receiver;

	private static String VOLUME_CHANGED_ACTION = "android.media.VOLUME_CHANGED_ACTION";
	private static String STREAM_TYPE = "android.media.EXTRA_VOLUME_STREAM_TYPE";
	private static String STREAM_VALUE = "android.media.EXTRA_VOLUME_STREAM_VALUE";

	private static void startReceiver( Context context )
	{
		Log.d( TAG, "Registering volume receiver" );
		if ( filter == null ) {
			filter = new IntentFilter();
			filter.addAction( VOLUME_CHANGED_ACTION );
		}
		if ( receiver == null ) {
			receiver = new VolumeReceiver();
		}

		context.registerReceiver( receiver, filter );

		AudioManager audio = (AudioManager)context.getSystemService( Context.AUDIO_SERVICE );
		int volume = audio.getStreamVolume( AudioManager.STREAM_MUSIC );
		Log.d( TAG, "startVolumeReceiver: " + volume );
	}

	private static void stopReceiver( Context context )
	{
		Log.d( TAG, "Unregistering volume receiver" );
		context.unregisterReceiver( receiver );
	}

	@Override
	public void onReceive( final Context context, final Intent intent ) {
		Log.d(TAG, "OnReceive VOLUME_CHANGED_ACTION" );
		int stream = ( Integer )intent.getExtras().get( STREAM_TYPE );
		int volume = ( Integer )intent.getExtras().get( STREAM_VALUE );
		if ( stream == AudioManager.STREAM_MUSIC )
		{
			nativeVolumeChanged( volume );
		}
		else
		{
			Log.d(TAG, "skipping volume change from stream " + stream );
		}
	}
}
