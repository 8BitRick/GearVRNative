// Manages 3D audio in Gear VR Native
#include "GVRAudioMgr.h"

namespace GVR {
	void AudioMgr::OneTimeInit() {
		result = FMOD::System_Create(&system);      // Create the main system object.
		if (result != FMOD_OK)
		{
			printf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
			exit(-1);
		}

		result = system->init(512, FMOD_INIT_NORMAL, 0);    // Initialize FMOD.
		if (result != FMOD_OK)
		{
			printf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
			exit(-1);
		}
		result = system->set3DSettings(1.0, DISTANCEFACTOR, 1.0f); // FMOD3D

		result = system->createSound("file:///android_asset/singing.wav", FMOD_3D, 0, &sound1); // FMOD3D
		char blah[1024]; sprintf(blah, "drumloop load returned %d\n", result);
		LOG(blah);
		LOG(FMOD_ErrorString(result));
		result = sound1->set3DMinMaxDistance(0.5f * DISTANCEFACTOR, 5000.0f * DISTANCEFACTOR);
		LOG(FMOD_ErrorString(result));
		result = sound1->setMode(FMOD_LOOP_NORMAL);
		LOG(FMOD_ErrorString(result));

		// FMOD3D - Start singing!
		{
			FMOD_VECTOR pos = { 0.0f * DISTANCEFACTOR, 0.0f, 0.0f };
			FMOD_VECTOR vel = { 0.0f, 0.0f, 0.0f };

			result = system->playSound(sound1, 0, true, &channel1);
			LOG(FMOD_ErrorString(result));
			result = channel1->set3DAttributes(&pos, &vel);
			LOG(FMOD_ErrorString(result));
			result = channel1->setPaused(false);
			LOG(FMOD_ErrorString(result));
		}
	}

	void AudioMgr::OneTimeShutdown() {
	}

	void AudioMgr::Frame(const OVR::VrFrame & vrFrame) {
		// FMOD3D - update listener
		{
			static float t = 0.0f;
			static FMOD_VECTOR lastpos = { LISTENDIST, 0.0f, 0.0f };
			FMOD_VECTOR forward = { 0.0f, 0.0f, 1.0f };
			FMOD_VECTOR up = { 0.0f, 1.0f, 0.0f };
			FMOD_VECTOR vel;

			if (listenerflag)
			{
				//listenerpos.x = (float)sin(t * 0.05f) * 24.0f * DISTANCEFACTOR; // left right pingpong
				listenerpos.x = cos(t * 0.5f) * LISTENDIST;
				listenerpos.y = sin(t * 0.5f) * LISTENDIST;
				listenerpos.z = sin(t * 0.5f) * LISTENDIST;
			}

			// ********* NOTE ******* READ NEXT COMMENT!!!!!
			// vel = how far we moved last FRAME (m/f), then time compensate it to SECONDS (m/s).
			vel.x = (listenerpos.x - lastpos.x) * (1000 / INTERFACE_UPDATETIME);
			vel.y = (listenerpos.y - lastpos.y) * (1000 / INTERFACE_UPDATETIME);
			vel.z = (listenerpos.z - lastpos.z) * (1000 / INTERFACE_UPDATETIME);

			// store pos for next time
			lastpos = listenerpos;

			result = system->set3DListenerAttributes(0, &listenerpos, &vel, &forward, &up);
			//LOG(FMOD_ErrorString(result));

			t += 0.016666666f;
			//t += (30 * (1.0f / (float)INTERFACE_UPDATETIME));    // t is just a time value .. it increments in 30m/s steps in this example
		}

		// Tick FMOD
		system->update();
	}
}