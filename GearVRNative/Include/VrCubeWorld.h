#pragma once
#include "App.h"

namespace OVR
{
	static const int NUM_INSTANCES = 1500;

	class VrCubeWorld
	{
	public:
		VrCubeWorld();
		~VrCubeWorld() {}

		void OneTimeInit();
		void OneTimeShutdown();
		void Frame(const VrFrame & vrFrame);
		void Draw(const Matrix4f &viewMat, const Matrix4f &projMat);

	private:
		unsigned int		Random;
		GlProgram			Program;
		GlGeometry			Cube;
		GLint				VertexTransformAttribute;
		GLuint				InstanceTransformBuffer;
		ovrVector3f			CubePositions[NUM_INSTANCES];
		ovrVector3f			CubeRotations[NUM_INSTANCES];
		float				RandomFloat();
	};
}