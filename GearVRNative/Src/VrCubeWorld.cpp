// VrCubeWorld - The setup and rendering code from example
// Extracted to get a clean starting project and to use as reference

#include "VrCubeWorld.h"
#include "PackageFiles.h"

#if 0
#define GL( func )		func; EglCheckErrors();
#else
#define GL( func )		func;
#endif

namespace OVR
{

	// setup Cube
	struct ovrCubeVertices
	{
		Vector3f positions[8];
		Vector4f colors[8];
	};

	static ovrCubeVertices cubeVertices =
	{
		// positions
		{
			Vector3f(-1.0f, +1.0f, -1.0f), Vector3f(+1.0f, +1.0f, -1.0f), Vector3f(+1.0f, +1.0f, +1.0f), Vector3f(-1.0f, +1.0f, +1.0f),	// top
			Vector3f(-1.0f, -1.0f, -1.0f), Vector3f(-1.0f, -1.0f, +1.0f), Vector3f(+1.0f, -1.0f, +1.0f), Vector3f(+1.0f, -1.0f, -1.0f)	// bottom
		},
		// colors
		{
			Vector4f(1.0f, 0.0f, 1.0f, 1.0f), Vector4f(0.0f, 1.0f, 0.0f, 1.0f), Vector4f(0.0f, 0.0f, 1.0f, 1.0f), Vector4f(1.0f, 0.0f, 0.0f, 1.0f),
			Vector4f(0.0f, 0.0f, 1.0f, 1.0f), Vector4f(0.0f, 1.0f, 0.0f, 1.0f), Vector4f(1.0f, 0.0f, 1.0f, 1.0f), Vector4f(1.0f, 0.0f, 0.0f, 1.0f)
		},
	};

	static const unsigned short cubeIndices[36] =
	{
		0, 1, 2, 2, 3, 0,	// top
		4, 5, 6, 6, 7, 4,	// bottom
		2, 6, 7, 7, 1, 2,	// right
		0, 4, 5, 5, 3, 0,	// left
		3, 5, 6, 6, 2, 3,	// front
		0, 1, 7, 7, 4, 0	// back
	};

	VrCubeWorld::VrCubeWorld() :Random(2) {}

	float VrCubeWorld::RandomFloat() {
		Random = 1664525L * Random + 1013904223L;
		unsigned int rf = 0x3F800000 | (Random & 0x007FFFFF);
		return (*(float *)&rf) - 1.0f;
	}

	void VrCubeWorld::OneTimeInit() {
		// Create the program.
		int fileLen = 0;
		void *fileContents = NULL;
		bool readOk = ovr_ReadFileFromOtherApplicationPackage(ovr_GetApplicationPackageFile(), "assets/shaders/basic.vert", fileLen, fileContents);
		void *fragShader = NULL;
		readOk = ovr_ReadFileFromOtherApplicationPackage(ovr_GetApplicationPackageFile(), "assets/shaders/basic.frag", fileLen, fragShader);

		//FILE *file = fopen("/sdcard/shaders/basic.vert", "r");
		if (readOk) {
			LOG("Opened file success!");
		}
		else {
			LOG("FAILED to open file!");
		}

		Program = BuildProgram((char*)fileContents, (char*)fragShader);
		free(fragShader);
		free(fileContents);

		VertexTransformAttribute = glGetAttribLocation(Program.program, "VertexTransform");

		// Create the cube.
		VertexAttribs attribs;
		attribs.position.Resize(8);
		attribs.color.Resize(8);
		for (int i = 0; i < 8; i++)
		{
			attribs.position[i] = cubeVertices.positions[i];
			attribs.color[i] = cubeVertices.colors[i];
		}

		Array< TriangleIndex > indices;
		indices.Resize(36);
		for (int i = 0; i < 36; i++)
		{
			indices[i] = cubeIndices[i];
		}

		Cube.Create(attribs, indices);

		// Setup the instance transform attributes.
		GL(glBindVertexArray(Cube.vertexArrayObject));
		GL(glGenBuffers(1, &InstanceTransformBuffer));
		GL(glBindBuffer(GL_ARRAY_BUFFER, InstanceTransformBuffer));
		GL(glBufferData(GL_ARRAY_BUFFER, NUM_INSTANCES * 4 * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW));
		for (int i = 0; i < 4; i++)
		{
			GL(glEnableVertexAttribArray(VertexTransformAttribute + i));
			GL(glVertexAttribPointer(VertexTransformAttribute + i, 4, GL_FLOAT,
				false, 4 * 4 * sizeof(float), (void *)(i * 4 * sizeof(float))));
			GL(glVertexAttribDivisor(VertexTransformAttribute + i, 1));
		}
		GL(glBindVertexArray(0));

		// Setup random cube positions and rotations.
		for (int i = 0; i < NUM_INSTANCES; i++)
		{
			volatile float rx, ry, rz;
			for (; ; )
			{
				rx = (RandomFloat() - 0.5f) * (50.0f + static_cast<float>(sqrt(NUM_INSTANCES)));
				ry = (RandomFloat() - 0.5f) * (50.0f + static_cast<float>(sqrt(NUM_INSTANCES)));
				rz = (RandomFloat() - 0.5f) * (50.0f + static_cast<float>(sqrt(NUM_INSTANCES)));

				// If too close to 0,0,0
				if (fabsf(rx) < 4.0f && fabsf(ry) < 4.0f && fabsf(rz) < 4.0f)
				{
					continue;
				}

				// Test for overlap with any of the existing cubes.
				bool overlap = false;
				for (int j = 0; j < i; j++)
				{
					if (fabsf(rx - CubePositions[j].x) < 4.0f &&
						fabsf(ry - CubePositions[j].y) < 4.0f &&
						fabsf(rz - CubePositions[j].z) < 4.0f)
					{
						overlap = true;
						break;
					}
				}

				if (!overlap)
				{
					break;
				}
			}

			// Insert into list sorted based on distance.
			int insert = 0;
			const float distSqr = rx * rx + ry * ry + rz * rz;
			for (int j = i; j > 0; j--)
			{
				const ovrVector3f * otherPos = &CubePositions[j - 1];
				const float otherDistSqr = otherPos->x * otherPos->x + otherPos->y * otherPos->y + otherPos->z * otherPos->z;
				if (distSqr > otherDistSqr)
				{
					insert = j;
					break;
				}
				CubePositions[j] = CubePositions[j - 1];
				CubeRotations[j] = CubeRotations[j - 1];
			}

			CubePositions[insert].x = rx;
			CubePositions[insert].y = ry;
			CubePositions[insert].z = rz;

			CubeRotations[insert].x = RandomFloat();
			CubeRotations[insert].y = RandomFloat();
			CubeRotations[insert].z = RandomFloat();
		}
	}

	void VrCubeWorld::OneTimeShutdown() {
		DeleteProgram(Program);
		Cube.Free();
		GL(glDeleteBuffers(1, &InstanceTransformBuffer));
	}

	void VrCubeWorld::Frame(const VrFrame & vrFrame) {
		Vector3f currentRotation;
		currentRotation.x = (float)(vrFrame.PredictedDisplayTimeInSeconds);
		currentRotation.y = (float)(vrFrame.PredictedDisplayTimeInSeconds);
		currentRotation.z = (float)(vrFrame.PredictedDisplayTimeInSeconds);

		// Update the instance transform attributes.
		GL(glBindBuffer(GL_ARRAY_BUFFER, InstanceTransformBuffer));
		GL(Matrix4f * cubeTransforms = (Matrix4f *)glMapBufferRange(GL_ARRAY_BUFFER, 0,
			NUM_INSTANCES * sizeof(Matrix4f), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
		for (int i = 0; i < NUM_INSTANCES; i++)
		{
			const Matrix4f rotation = Matrix4f::RotationX(CubeRotations[i].x * currentRotation.x) *
				Matrix4f::RotationY(CubeRotations[i].y * currentRotation.y) *
				Matrix4f::RotationZ(CubeRotations[i].z * currentRotation.z);
			const Matrix4f translation = Matrix4f::Translation(
				CubePositions[i].x,
				CubePositions[i].y,
				CubePositions[i].z);
			const Matrix4f transform = translation * rotation;
			cubeTransforms[i] = transform.Transposed();
		}
		GL(glUnmapBuffer(GL_ARRAY_BUFFER));
		GL(glBindBuffer(GL_ARRAY_BUFFER, 0));
	}

	void VrCubeWorld::Draw(const Matrix4f &viewMat, const Matrix4f &projMat)	{
		GL(glClearColor(0.125f, 0.0f, 0.125f, 1.0f));
		GL(glClear(GL_COLOR_BUFFER_BIT));
		GL(glUseProgram(Program.program));
		GL(glUniformMatrix4fv(Program.uView, 1, GL_TRUE, viewMat.M[0]));
		GL(glUniformMatrix4fv(Program.uProjection, 1, GL_TRUE, projMat.M[0]));
		GL(glBindVertexArray(Cube.vertexArrayObject));
		GL(glDrawElementsInstanced(GL_TRIANGLES, Cube.indexCount, GL_UNSIGNED_SHORT, NULL, NUM_INSTANCES));
		GL(glBindVertexArray(0));
		GL(glUseProgram(0));
	}
}
