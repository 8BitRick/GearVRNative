// Copyright 2014 Oculus VR, LLC. All Rights reserved.
package com.oculus.sound;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.content.res.Resources;
import android.media.AudioManager;
import android.media.SoundPool;
import android.util.Log;

// This class is entirely accessed through JNI
class SoundPooler {
	private static final String TAG = "SoundPooler";
    private static final boolean DEBUG = false;

    private final Context		context;
	private final SoundPool		pool = new SoundPool(3 /* voices */, AudioManager.STREAM_MUSIC, 100);
	private final List<Integer>	soundIds = new ArrayList<Integer>();
	private final List<String>	soundNames = new ArrayList<String>();

    private SoundPooler( Context context ) {
        this.context = context;

        if ( DEBUG )
        {
            final AudioManager audioMgr =
                (AudioManager)context.getSystemService(Context.AUDIO_SERVICE);
            final String rate = audioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            final String size = audioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
            Log.d( TAG, "rate = " + rate );
            Log.d( TAG, "size = " + size );
        }
    }

    private void release() {
		pool.release();
		soundIds.clear();
		soundNames.clear();
    }

	private void play( String name ) {
		for ( int i = 0 ; i < soundNames.size() ; i++ ) {
			if ( soundNames.get(i).equals( name ) ) {
				pool.play( soundIds.get( i ), 1.0f, 1.0f, 1, 0, 1 );
				return;
			}
		}

        if ( DEBUG )
        {
            Log.d(TAG, "play: loading "+name);
        }

		// check first if this is a raw resource
		int soundId = 0;
		if ( name.indexOf( "res/raw/" ) == 0 ) {
			String resourceName = name.substring( 4, name.length() - 4 );
            final Resources resources = context.getResources();
			int id = resources.getIdentifier( resourceName, "raw", context.getPackageName() );
			if ( id == 0 ) {
                if ( DEBUG )
                {
                    Log.e( TAG, "No resource named " + resourceName );
                }
			} else {
				AssetFileDescriptor afd = resources.openRawResourceFd( id );
				soundId = pool.load( afd, 1 );
			}
		} else {
			try {
				AssetFileDescriptor afd = context.getAssets().openFd( name );
				soundId = pool.load( afd, 1 );
			} catch ( IOException t ) {
                if ( DEBUG )
                {
                    Log.e( TAG, "Couldn't open " + name + " because " + t.getMessage() );
                }
			}
		}

		if ( soundId == 0 )
		{
			// Try to load the sound directly - works for absolute path - for wav files for sdcard for ex.
			soundId = pool.load( name, 1 );
		}

		soundNames.add( name );
		soundIds.add( soundId );

		pool.play( soundIds.get( soundNames.size() - 1 ), 1.0f, 1.0f, 1, 0, 1 );
	}
}
