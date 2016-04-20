#pragma once
// Manages 3D audio in Gear VR Native

#include "App.h"
#include "fmod.hpp"
#include "fmod_errors.h"

namespace GVR
{
	class AudioMgr
	{
	public:
		AudioMgr() {}
		~AudioMgr() {}

		void OneTimeInit();
		void OneTimeShutdown();
		void Frame(const OVR::VrFrame & vrFrame);

	private:
		FMOD::System     *system;
		FMOD::Sound      *sound1, *sound2, *sound3;
		FMOD::Channel    *channel1 = 0, *channel2 = 0, *channel3 = 0;
		FMOD::Channel    *channel = 0;
		FMOD_RESULT       result;
		unsigned int      version;
		void             *extradriverdata = 0;
		const int   INTERFACE_UPDATETIME = 16;      // 50ms update for interface
		const float DISTANCEFACTOR = 1.0f;          // Units per meter.  I.e feet would = 3.28.  centimeters would = 100.
		bool             listenerflag = true;
		const float LISTENDIST = 2.0f;
		FMOD_VECTOR      listenerpos = { LISTENDIST, 0.0f, 0.0f * DISTANCEFACTOR };
	};
}
