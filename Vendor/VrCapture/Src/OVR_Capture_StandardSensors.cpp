/************************************************************************************

PublicHeader:   OVR_Capture.h
Filename    :   OVR_Capture_StandardSensors.cpp
Content     :   Oculus performance capture library. Support for standard builtin device sensors.
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_Capture_StandardSensors.h"
#include "OVR_Capture_Local.h"
#include "OVR_Capture_FileIO.h"

#include <algorithm>

namespace OVR
{
namespace Capture
{

	static const Label g_gpuLabel("GPU Clocks");
	static const Label g_memLabel("Memory Bandwidth");

	// Container for label whose name is read from disk. Must be stored in
	// global memory (or local static) as name is assumed to be persistent.
	struct SensorLabelDesc
	{
		Label label;
		char  name[32];
	};

	StandardSensors::StandardSensors(void)
	{
	}

	StandardSensors::~StandardSensors(void)
	{
	}

	void StandardSensors::OnThreadExecute(void)
	{
		SetThreadName("CaptureSensors");

		// Pre-load what files we can... reduces open/close overhead (which is significant)

		// Setup CPU Clocks Support...
		static const UInt32 maxCpus = 8;
		static SensorLabelDesc cpuDescs[maxCpus];
		FileHandle cpuOnlineFiles[maxCpus];
		FileHandle cpuFreqFiles[maxCpus];
		for(UInt32 i=0; i<maxCpus; i++)
		{
			cpuOnlineFiles[i] = cpuFreqFiles[i] = NullFileHandle;
			SensorLabelDesc &desc = cpuDescs[i];
			FormatString(desc.name, sizeof(desc.name), "CPU%u Clocks", i);
			desc.label.ConditionalInit(desc.name);
		}
		if(CheckConnectionFlag(Enable_CPU_Zones))
		{
			int maxFreq = 0;
			for(UInt32 i=0; i<maxCpus; i++)
			{
				char onlinePath[64] = {0};
				FormatString(onlinePath, sizeof(onlinePath), "/sys/devices/system/cpu/cpu%u/online", i);
				cpuOnlineFiles[i] = OpenFile(onlinePath);
				cpuFreqFiles[i]   = NullFileHandle;
				if(cpuOnlineFiles[i] != NullFileHandle)
				{
					char maxFreqPath[64] = {0};
					FormatString(maxFreqPath, sizeof(maxFreqPath), "/sys/devices/system/cpu/cpu%u/cpufreq/cpuinfo_max_freq", i);
					maxFreq = std::max(maxFreq, ReadIntFile(maxFreqPath));
				}
			}
			for(UInt32 i=0; i<maxCpus; i++)
			{
				const SensorLabelDesc &desc = cpuDescs[i];
				SensorSetRange(desc.label, 0, (float)maxFreq, Sensor_Interp_Nearest, Sensor_Unit_KHz);
			}
		}

		// Setup GPU Clocks Support...
		FileHandle gpuFreqFile = NullFileHandle;
		if(CheckConnectionFlag(Enable_GPU_Clocks))
		{
			if(gpuFreqFile == NullFileHandle) // Adreno
			{
				gpuFreqFile = OpenFile("/sys/class/kgsl/kgsl-3d0/gpuclk");
				if(gpuFreqFile != NullFileHandle)
				{
					const int maxFreq = ReadIntFile("/sys/class/kgsl/kgsl-3d0/max_gpuclk");
					SensorSetRange(g_gpuLabel, 0, (float)maxFreq, Sensor_Interp_Nearest, Sensor_Unit_Hz);
				}
			}
			if(gpuFreqFile == NullFileHandle) // Mali
			{
				gpuFreqFile = OpenFile("/sys/devices/14ac0000.mali/clock");
				if(gpuFreqFile != NullFileHandle)
				{
					// TODO: query max GPU clocks on Mali, for now hacked to what we know the S6 is
					SensorSetRange(g_gpuLabel, 0, 0, Sensor_Interp_Nearest, Sensor_Unit_MHz);
				}
			}
		}

		// Setup Memory Clocks Support...
		FileHandle memFreqFile = NullFileHandle;
		//memFreqFile = OpenFile("/sys/class/devfreq/0.qcom,cpubw/cur_freq");
		if(memFreqFile != NullFileHandle)
		{
			const int maxFreq = ReadIntFile("/sys/class/devfreq/0.qcom,cpubw/max_freq");
			SensorSetRange(g_memLabel, 0, (float)maxFreq, Sensor_Interp_Nearest, Sensor_Unit_MByte_Second);
		}

		// Setup thermal sensors...
		static const UInt32 maxThermalSensors = 20;
		static SensorLabelDesc  thermalDescs[maxThermalSensors];
		FileHandle              thermalFiles[maxThermalSensors];
		for(UInt32 i=0; i<maxThermalSensors; i++)
		{
			thermalFiles[i] = NullFileHandle;
		}
		if(CheckConnectionFlag(Enable_Thermal_Sensors))
		{
			for(UInt32 i=0; i<maxThermalSensors; i++)
			{
				SensorLabelDesc &desc = thermalDescs[i];

				char typePath[64]      = {0};
				char tempPath[64]      = {0};
				char tripPointPath[64] = {0};
				FormatString(typePath,      sizeof(typePath),      "/sys/devices/virtual/thermal/thermal_zone%u/type", i);
				FormatString(tempPath,      sizeof(tempPath),      "/sys/devices/virtual/thermal/thermal_zone%u/temp", i);
				FormatString(tripPointPath, sizeof(tripPointPath), "/sys/devices/virtual/thermal/thermal_zone%u/trip_point_0_temp", i);

				// If either of these files don't exist, then we got to the end of the thermal zone list...
				if(!CheckFileExists(typePath) || !CheckFileExists(tempPath) || !CheckFileExists(tripPointPath))
					break;

				// Initialize the Label...
				if(ReadFileLine(typePath, desc.name, sizeof(desc.name)) <= 0)
					continue; // failed to read sensor name...
				desc.label.ConditionalInit(desc.name);

				char modePath[64] = {0};
				FormatString(modePath, sizeof(modePath), "/sys/devices/virtual/thermal/thermal_zone%d/mode", i);

				// check to see if the zone is disabled... its okay if there is no mode file...
				char mode[16] = {0};
				if(ReadFileLine(modePath, mode, sizeof(mode))>0 && !strcmp(mode, "disabled"))
					continue;

				// Finally... open the file.
				thermalFiles[i] = OpenFile(tempPath);

				// Check to see if the temperature file was found...
				if(thermalFiles[i] == NullFileHandle)
					continue;

				// Read in the critical temperature value.
				const int tripPoint = ReadIntFile(tripPointPath);
				if(tripPoint > 0)
				{
					SensorSetRange(desc.label, 0, (float)tripPoint, Sensor_Interp_Linear);
				}
			}
		}

		// For clocks, we store the last value and only send updates when it changes since we
		// use blocking chart rendering.
		int lastCpuFreq[maxCpus] = {0};
		int lastGpuFreq          = 0;
		int lastMemValue         = 0;

		UInt32 sampleCount = 0;

		while(!QuitSignaled() && IsConnected())
		{
			// Sample CPU Frequencies...
			for(UInt32 i=0; i<maxCpus; i++)
			{
				// If the 'online' file can't be found, then we just assume this CPU doesn't even exist
				if(cpuOnlineFiles[i] == NullFileHandle)
					continue;

				const SensorLabelDesc &desc   = cpuDescs[i];
				const bool             online = ReadIntFile(cpuOnlineFiles[i]) ? true : false;
				if(online && cpuFreqFiles[i]==NullFileHandle)
				{
					// Open the frequency file if we are online and its not already open...
					char freqPath[64] = {0};
					FormatString(freqPath, sizeof(freqPath), "/sys/devices/system/cpu/cpu%u/cpufreq/scaling_cur_freq", i);
					cpuFreqFiles[i] = OpenFile(freqPath);
				}
				else if(!online && cpuFreqFiles[i]!=NullFileHandle)
				{
					// close the frequency file if we are no longer online
					CloseFile(cpuFreqFiles[i]);
					cpuFreqFiles[i] = NullFileHandle;
				}
				const int freq = cpuFreqFiles[i]==NullFileHandle ? 0 : ReadIntFile(cpuFreqFiles[i]);
				if(freq != lastCpuFreq[i])
				{
					// Convert from KHz to Hz
					SensorSetValue(desc.label, (float)freq);
					lastCpuFreq[i] = freq;
				}
				ThreadYield();
			}

			// Sample GPU Frequency...
			if(gpuFreqFile != NullFileHandle)
			{
				const int freq = ReadIntFile(gpuFreqFile);
				if(freq != lastGpuFreq)
				{
					SensorSetValue(g_gpuLabel, (float)freq);
					lastGpuFreq = freq;
				}
			}

			// Sample Memory Bandwidth
			if(memFreqFile != NullFileHandle)
			{
				const int value = ReadIntFile(memFreqFile);
				if(value != lastMemValue)
				{
					SensorSetValue(g_memLabel, (float)value);
					lastMemValue = value;
				}
			}

			// Sample thermal sensors...
			if((sampleCount&15) == 0) // sample temperature at a much lower frequency as clocks... thermals don't change that fast.
			{
				for(UInt32 i=0; i<maxThermalSensors; i++)
				{
					FileHandle file = thermalFiles[i];
					if(file != NullFileHandle)
					{
						SensorSetValue(thermalDescs[i].label, (float)ReadIntFile(file));
					}
				}
				ThreadYield();
			}

			// Sleep 5ms between samples...
			ThreadSleepMicroseconds(5000);
			sampleCount++;
		}

		// Close down cached file handles...
		for(UInt32 i=0; i<maxCpus; i++)
		{
			if(cpuOnlineFiles[i] != NullFileHandle) CloseFile(cpuOnlineFiles[i]);
			if(cpuFreqFiles[i]   != NullFileHandle) CloseFile(cpuFreqFiles[i]);
		}
		if(gpuFreqFile != NullFileHandle) CloseFile(gpuFreqFile);
		if(memFreqFile != NullFileHandle) CloseFile(memFreqFile);
		for(UInt32 i=0; i<maxThermalSensors; i++)
		{
			if(thermalFiles[i] != NullFileHandle) CloseFile(thermalFiles[i]);
		}
	}

} // namespace Capture
} // namespace OVR

