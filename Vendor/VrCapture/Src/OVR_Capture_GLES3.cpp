/************************************************************************************

PublicHeader:   OVR_Capture.h
Filename    :   OVR_Capture_GLES3.cpp
Content     :   Oculus performance capture library. OpenGL ES 3 interfaces.
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include <OVR_Capture_GLES3.h>

#if defined(OVR_CAPTURE_HAS_GLES3)

#include <OVR_Capture_Packets.h>
#include "OVR_Capture_Local.h"
#include "OVR_Capture_AsyncStream.h"

#include <string.h> // memset
#include <EGL/egl.h>

#if defined(OVR_CAPTURE_HAS_OPENGL_LOADER)
	#include <GLES3/gl3_loader.h>
	using namespace GLES3;
#else
	#include <GLES3/gl3.h>
#endif

// Experimental DXT1 encoding of framebuffer on the GPU to reduce network bandwidth
#define OVR_CAPTURE_GPU_DXT1_ENCODE

namespace OVR
{
namespace Capture
{

	struct PendingFrameBuffer
	{
		UInt64 timestamp;
		GLuint renderbuffer;
		GLuint fbo;
		GLuint pbo;
		bool   imageReady;
	};

	static bool                    g_requireCleanup         = false;

	static const unsigned int      g_maxPendingFramebuffers = 2;
	static unsigned int            g_nextPendingFramebuffer = 0;
	static PendingFrameBuffer      g_pendingFramebuffers[g_maxPendingFramebuffers];

#if defined(OVR_CAPTURE_GPU_DXT1_ENCODE) // 4bpp... 192x192@60 = 1.05MB/s
	static const unsigned int      g_imageWidth        = 192;
	static const unsigned int      g_imageHeight       = 192;
	static const unsigned int      g_blockByteSize     = 8;
	static const unsigned int      g_imageWidthBlocks  = g_imageWidth>>2;
	static const unsigned int      g_imageHeightBlocks = g_imageHeight>>2;
	static const FrameBufferFormat g_imageFormat       = FrameBuffer_DXT1;
	static const GLenum            g_imageFormatGL     = GL_RGBA16UI;
#elif 1 // 16bpp... 128x128@60 = 1.875MB/s
	static const unsigned int      g_imageWidth        = 128;
	static const unsigned int      g_imageHeight       = 128;
	static const unsigned int      g_blockByteSize     = 2;
	static const unsigned int      g_imageWidthBlocks  = g_imageWidth;
	static const unsigned int      g_imageHeightBlocks = g_imageHeight;
	static const FrameBufferFormat g_imageFormat       = FrameBuffer_RGB_565;
	static const GLenum            g_imageFormatGL     = GL_RGB565;
#else // 32bpp... 128x128@60 = 3.75MB/s... you probably shouldn't use this path outside of testing.
	static const unsigned int      g_imageWidth        = 128;
	static const unsigned int      g_imageHeight       = 128;
	static const unsigned int      g_blockByteSize     = 4;
	static const unsigned int      g_imageWidthBlocks  = g_imageWidth;
	static const unsigned int      g_imageHeightBlocks = g_imageHeight;
	static const FrameBufferFormat g_imageFormat       = FrameBuffer_RGBA_8888;
	static const GLenum            g_imageFormatGL     = GL_RGBA8;
#endif
	static const unsigned int      g_imageSize         = g_imageWidthBlocks * g_imageHeightBlocks * g_blockByteSize;

	static GLuint                  g_vertexShader   = 0;
	static GLuint                  g_fragmentShader = 0;
	static GLuint                  g_program        = 0;
	static GLint                   g_textureRectLoc = -1;

	static GLuint                  g_vertexBuffer      = 0;
	static GLuint                  g_vertexArrayObject = 0;

	enum ShaderAttributes
	{
		POSITION_ATTRIBUTE = 0,
		TEXCOORD_ATTRIBUTE,
	};

	static const char *g_vertexShaderSource = R"=====(
		attribute vec4 Position;
		attribute vec2 TexCoord;
		varying  highp vec2 oTexCoord;
		uniform   vec4 TextureRect;
		void main()
		{
			gl_Position = Position;
			vec2 uv = TexCoord.xy*TextureRect.zw + TextureRect.xy;
			oTexCoord = vec2(uv.x, 1.0 - uv.y);    // need to flip Y
		}
		)=====";

	static const char *g_fragmentShaderSource = R"=====(
		uniform sampler2D Texture0;
		varying highp vec2 oTexCoord;
		void main()
		{
			gl_FragColor = texture2D(Texture0, oTexCoord);
		}
		)=====";

	static const char *g_vertexShaderSourceDXT1 = R"=====(#version 300 es
		uniform   vec2 UVBlockScale;
		in        vec4 Position;
		in        vec2 TexCoord;
		out highp vec2 oTexCoord;
		void main()
		{
			gl_Position = Position;
			oTexCoord = TexCoord.xy * UVBlockScale.xy;  // don't flip Y here, we do it after applying block offsets. but clip to beginning of last block
		}
		)=====";

	// Based on http://www.nvidia.com/object/real-time-ycocg-dxt-compression.html
	static const char *g_fragmentShaderSourceDXT1 = R"=====(#version 300 es
		precision mediump float;

		uniform       sampler2D Texture0;
		uniform       vec2      TexelSize;
		uniform       vec4      TextureRect;
		in            vec2      oTexCoord;
		out   mediump uvec4     Output;

		// Convert to 565 and expand back into color
		mediump uint Encode565(inout vec3 color)
		{
			uvec3 c    = uvec3(round(color * vec3(31.0, 63.0, 31.0)));
			mediump uint  c565 = (c.r << 11) | (c.g << 5) | c.b;
			c.rb  = (c.rb << 3) | (c.rb >> 2);
			c.g   = (c.g << 2) | (c.g >> 4);
			color = vec3(c) * (1.0 / 255.0);
			return c565;
		}

		float ColorDistance(vec3 c0, vec3 c1)
		{
			vec3 d = c0-c1;
			return dot(d, d);
		}

		void main()
		{
			vec3 block[16];

			// Load block colors...
			for(int i=0; i<4; i++)
			{
				for(int j=0; j<4; j++)
				{
					vec2 uv = (oTexCoord.xy + vec2(j,i)*TexelSize);
					uv = uv * TextureRect.zw + TextureRect.xy; // clip to TextureRect
					uv.y = 1.0 - uv.y; // flip Y
					block[i*4+j] = texture(Texture0, uv).rgb;
				}
			}

			// Calculate bounding box...
			vec3 minblock = block[0];
			vec3 maxblock = block[0];
			for(int i=1; i<16; i++)
			{
				minblock = min(minblock, block[i]);
				maxblock = max(maxblock, block[i]);
			}

			// Inset bounding box...
			vec3 inset = (maxblock - minblock) / 16.0 - (8.0 / 255.0) / 16.0;
			minblock = clamp(minblock + inset, 0.0, 1.0);
			maxblock = clamp(maxblock - inset, 0.0, 1.0);

			// Convert to 565 colors...
			mediump uint c0 = Encode565(maxblock);
			mediump uint c1 = Encode565(minblock);

			// Make sure c0 has the largest integer value...
			if(c1>c0)
			{
				mediump uint uitmp=c0; c0=c1; c1=uitmp;
				vec3 v3tmp=maxblock; maxblock=minblock; minblock=v3tmp;
			}

			// Calculate indices...
			vec3 color0 = maxblock;
			vec3 color1 = minblock;
			vec3 color2 = (color0 + color0 + color1) * (1.0/3.0);
			vec3 color3 = (color0 + color1 + color1) * (1.0/3.0);

			mediump uint i0 = 0U;
			mediump uint i1 = 0U;
			for(int i=0; i<8; i++)
			{
				vec3 color = block[i];
				vec4 dist;
				dist.x = ColorDistance(color, color0);
				dist.y = ColorDistance(color, color1);
				dist.z = ColorDistance(color, color2);
				dist.w = ColorDistance(color, color3);
				mediump uvec4 b = uvec4(greaterThan(dist.xyxy, dist.wzzw));
				uint b4 = dist.z > dist.w ? 1U : 0U;
				uint index = (b.x & b4) | (((b.y & b.z) | (b.x & b.w)) << 1);
				i0 |= index << (i*2);
			}
			for(int i=0; i<8; i++)
			{
				vec3 color = block[i+8];
				vec4 dist;
				dist.x = ColorDistance(color, color0);
				dist.y = ColorDistance(color, color1);
				dist.z = ColorDistance(color, color2);
				dist.w = ColorDistance(color, color3);
				mediump uvec4 b = uvec4(greaterThan(dist.xyxy, dist.wzzw));
				uint b4 = dist.z > dist.w ? 1U : 0U;
				uint index = (b.x & b4) | (((b.y & b.z) | (b.x & b.w)) << 1);
				i1 |= index << (i*2);
			}

			// Write out final dxt1 block...
			Output = uvec4(c0, c1, i0, i1);
		}
		)=====";
	
	static const float g_vertices[] =
	{
	   -1.0f,-1.0f, 0.0f, 1.0f,  0.0f, 0.0f,
		3.0f,-1.0f, 0.0f, 1.0f,  2.0f, 0.0f,
	   -1.0f, 3.0f, 0.0f, 1.0f,  0.0f, 2.0f,
	};

	class GLES3ScopedState
	{
		public:
			enum State
			{
				DEPTH_TEST,
				SCISSOR_TEST,
				STENCIL_TEST,
				RASTERIZER_DISCARD,
				DITHER,
				CULL_FACE,
				BLEND,

				NUM_STATES
			};
			enum TextureUnit
			{
				TEXTURE0,

				NUM_TEXTURE_UNITS
			};
		public:
			GLES3ScopedState(void)
			{
				OVR_CAPTURE_CPU_ZONE(SaveState);

				memset(&m_previousViewport, 0, sizeof(m_previousViewport));
				glGetIntegerv(GL_VIEWPORT, &m_previousViewport.x);
				m_currentViewport = m_previousViewport;

				m_previousRBO = 0;
				glGetIntegerv(GL_RENDERBUFFER_BINDING, reinterpret_cast<GLint*>(&m_previousRBO));
				m_currentRBO = m_previousRBO;

				m_previousFBO = 0;
				glGetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&m_previousFBO));
				m_currentFBO = m_previousFBO;

				m_previousPBO = 0;
				glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, reinterpret_cast<GLint*>(&m_previousPBO));
				m_currentPBO = m_previousPBO;

				m_previousVAO = 0;
				glGetIntegerv(GL_VERTEX_ARRAY_BINDING, reinterpret_cast<GLint*>(&m_previousVAO));
				m_currentVAO = m_previousVAO;

				m_previousProgram = 0;
				glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint*>(&m_previousProgram));
				m_currentProgram = m_previousProgram;

				m_previousActiveTexture = GL_TEXTURE0;
				glGetIntegerv(GL_ACTIVE_TEXTURE, reinterpret_cast<GLint*>(&m_previousActiveTexture));
				m_currentActiveTexture = m_previousActiveTexture;

				for(GLuint i=0; i<NUM_TEXTURE_UNITS; i++)
				{
					ActiveTexture(GL_TEXTURE0 + i);
					m_previousBoundTexture[i] = 0;
					glGetIntegerv(GL_TEXTURE_BINDING_2D, reinterpret_cast<GLint*>(&m_previousBoundTexture[i]));
					m_currentBoundTexture[i] = m_previousBoundTexture[i];
				}

				memset(m_previousColorMask, 0, sizeof(m_previousColorMask));
				glGetBooleanv(GL_COLOR_WRITEMASK, m_previousColorMask);
				memcpy(m_currentColorMask, m_previousColorMask, sizeof(m_previousColorMask));

				memset(m_stateEnums, 0, sizeof(m_stateEnums));
				m_stateEnums[DEPTH_TEST]         = GL_DEPTH_TEST;
				m_stateEnums[SCISSOR_TEST]       = GL_SCISSOR_TEST;
				m_stateEnums[STENCIL_TEST]       = GL_STENCIL_TEST;
				m_stateEnums[RASTERIZER_DISCARD] = GL_RASTERIZER_DISCARD;
				m_stateEnums[DITHER]             = GL_DITHER;
				m_stateEnums[CULL_FACE]          = GL_CULL_FACE;
				m_stateEnums[BLEND]              = GL_BLEND;
				for(GLuint i=0; i<NUM_STATES; i++)
				{
					OVR_CAPTURE_ASSERT(m_stateEnums[i] != 0); // make sure the mapping is complete
					m_previousStates[i] = glIsEnabled(m_stateEnums[i]);
					m_currentStates[i] = m_previousStates[i];
				}
			}

			~GLES3ScopedState(void)
			{
				OVR_CAPTURE_CPU_ZONE(RestoreState);

				Viewport(m_previousViewport.x, m_previousViewport.y, m_previousViewport.width, m_previousViewport.height);
				BindRenderbuffer(m_previousRBO);
				BindFramebuffer(m_previousFBO);
				BindPixelBuffer(m_previousPBO);
				BindVertexArray(m_previousVAO);

				UseProgram(m_previousProgram);

				for(GLuint i=0; i<NUM_TEXTURE_UNITS; i++)
				{
					BindTexture((TextureUnit)i, m_previousBoundTexture[i]);
				}
				ActiveTexture(m_previousActiveTexture);

				ColorMask(m_previousColorMask[0], m_previousColorMask[1], m_previousColorMask[2], m_previousColorMask[3]);

				for(GLuint i=0; i<NUM_STATES; i++)
				{
					SetState((State)i, m_previousStates[i]);
				}
			}

			void Viewport(const Rect<GLint> &viewport)
			{
				if(m_currentViewport != viewport)
				{
					glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
					m_currentViewport = viewport;
				}
			}

			void Viewport(GLint x, GLint y, GLsizei width, GLsizei height)
			{
				const Rect<GLint> viewport = {x, y, width, height};
				Viewport(viewport);
			}

			void BindRenderbuffer(GLuint rbo)
			{
				if(m_currentRBO != rbo)
				{
					glBindRenderbuffer(GL_RENDERBUFFER, rbo);
					m_currentRBO = rbo;
				}
			}

			void BindFramebuffer(GLuint fbo)
			{
				if(m_currentFBO != fbo)
				{
					glBindFramebuffer(GL_FRAMEBUFFER, fbo);
					m_currentFBO = fbo;
				}
			}

			void BindPixelBuffer(GLuint pbo)
			{
				if(m_currentPBO != pbo)
				{
					glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
					m_currentPBO = pbo;
				}
			}

			void BindVertexArray(GLuint vao)
			{
				if(m_currentVAO != vao)
				{
					glBindVertexArray(vao);
					m_currentVAO = vao;
				}
			}

			void UseProgram(GLuint program)
			{
				if(m_currentProgram != program)
				{
					glUseProgram(program);
					m_currentProgram = program;
				}
			}

			void Enable(State state)
			{
				SetState(state, GL_TRUE);
			}

			void Disable(State state)
			{
				SetState(state, GL_FALSE);
			}

			void SetState(State state, GLboolean value)
			{
				OVR_CAPTURE_ASSERT(state >= 0 && state < NUM_STATES);
				if(m_currentStates[state] != value)
				{
					if(value) glEnable( m_stateEnums[state]);
					else      glDisable(m_stateEnums[state]);
					m_currentStates[state] = value;
				}
			}

			void ColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
			{
				if(m_currentColorMask[0]!=r || m_currentColorMask[1]!=g || m_currentColorMask[2]!=b || m_currentColorMask[3]!=a)
				{
					glColorMask(r, g, b, a);
					m_currentColorMask[0] = r;
					m_currentColorMask[1] = g;
					m_currentColorMask[2] = b;
					m_currentColorMask[3] = a;
				}
			}

			void BindTexture(TextureUnit textureUnit, GLuint textureid)
			{
				if(m_currentBoundTexture[textureUnit] != textureid)
				{
					ActiveTexture(textureUnit + GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, textureid);
					m_currentBoundTexture[textureUnit] = textureid;
				}
			}

		private:
			void ActiveTexture(GLenum textureUnit)
			{
				if(m_currentActiveTexture != textureUnit)
				{
					glActiveTexture(textureUnit);
					m_currentActiveTexture = textureUnit;
				}
			}
		private:
			Rect<GLint> m_previousViewport;
			Rect<GLint> m_currentViewport;

			GLuint      m_previousRBO;
			GLuint      m_currentRBO;

			GLuint      m_previousFBO;
			GLuint      m_currentFBO;

			GLuint      m_previousPBO;
			GLuint      m_currentPBO;

			GLuint      m_previousVAO;
			GLuint      m_currentVAO;

			GLuint      m_previousProgram;
			GLuint      m_currentProgram;

			GLenum      m_previousActiveTexture;
			GLenum      m_currentActiveTexture;

			GLuint      m_previousBoundTexture[NUM_TEXTURE_UNITS];
			GLuint      m_currentBoundTexture[NUM_TEXTURE_UNITS];

			GLboolean   m_previousColorMask[4];
			GLboolean   m_currentColorMask[4];

			GLenum      m_stateEnums[NUM_STATES];
			GLboolean   m_previousStates[NUM_STATES];
			GLboolean   m_currentStates[NUM_STATES];
	};

	static void InitGLES3(void)
	{
	#if defined(OVR_CAPTURE_HAS_OPENGL_LOADER)
		GLES3::LoadGLFunctions();
	#endif
	}


	// Clean up OpenGL state... MUST be called from the GL thread! Which is why Capture::Shutdown() can't safely call this.
	static void CleanupGLES3(void)
	{
		if(g_program)
		{
			glDeleteProgram(g_program);
			g_program = 0;
		}
		if(g_vertexShader)
		{
			glDeleteShader(g_vertexShader);
			g_vertexShader = 0;
		}
		if(g_fragmentShader)
		{
			glDeleteShader(g_fragmentShader);
			g_fragmentShader = 0;
		}
		if(g_vertexArrayObject)
		{
			glDeleteVertexArrays(1, &g_vertexArrayObject);
			g_vertexArrayObject = 0;
		}
		if(g_vertexBuffer)
		{
			glDeleteBuffers(1, &g_vertexBuffer);
			g_vertexBuffer = 0;
		}
		for(unsigned int i=0; i<g_maxPendingFramebuffers; i++)
		{
			PendingFrameBuffer &fb = g_pendingFramebuffers[i];
			if(fb.renderbuffer) glDeleteRenderbuffers(1, &fb.renderbuffer);
			if(fb.fbo)          glDeleteFramebuffers( 1, &fb.fbo);
			if(fb.pbo)          glDeleteBuffers(      1, &fb.pbo);
			memset(&fb, 0, sizeof(fb));
		}
		g_requireCleanup = false;
	}

	static void DumpShaderCompileLog(GLuint shader)
	{
		GLint msgLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &msgLength);
		if(msgLength > 0)
		{
			char *msg = new char[msgLength+1];
			glGetShaderInfoLog(shader, msgLength, 0, msg);
			msg[msgLength] = 0;
			while(*msg)
			{
				// retain the start of the line...
				char *line = msg;
				// Null terminate the line...
				for(; *msg; msg++)
				{
					if(*msg == '\r' || *msg == '\n')
					{
						*msg = 0;
						msg++; // goto next line...
						break;
					}
				}
				// print the line...
				if(*line) Logf(Log_Error, "GL: %s", line);
			}
			delete [] msg;
		}
	}

	// Captures the frame buffer from a Texture Object from an OpenGL ES 3.0 Context.
	// Must be called from a thread with a valid GLES3 context!
	void FrameBufferGLES3(unsigned int textureID, Rect<float> textureRect)
	{
		if(!CheckConnectionFlag(Enable_FrameBuffer_Capture))
		{
			if(g_requireCleanup)
				CleanupGLES3();
			return;
		}

		// Make sure GL entry points are initialized
		InitGLES3();

		OVR_CAPTURE_CPU_AND_GPU_ZONE(FrameBufferGLES3);

		// once a connection is stablished we should flag ourselves for cleanup...
		g_requireCleanup = true;

		// Basic Concept:
		//   0) Capture Time Stamp
		//   1) StretchBlit into lower resolution 565 texture
		//   2) Issue async ReadPixels into pixel buffer object
		//   3) Wait N Frames
		//   4) Map PBO memory and call FrameBuffer(g_imageFormat,g_imageWidth,g_imageHeight,imageData)

		// acquire current time before spending cycles mapping and copying the PBO...
		const UInt64 currentTime = GetNanoseconds();

		// Scoped Save/Restore GL state...
		GLES3ScopedState glstate;

		// Acquire a PendingFrameBuffer container...
		PendingFrameBuffer &fb = g_pendingFramebuffers[g_nextPendingFramebuffer];

		// If the pending framebuffer has valid data in it... lets send it over the network...
		if(fb.imageReady)
		{
			OVR_CAPTURE_CPU_ZONE(MapAndCopy);
			// 4) Map PBO memory and call FrameBuffer(g_imageFormat,g_imageWidth,g_imageHeight,imageData)
			glstate.BindPixelBuffer(fb.pbo);
			const void *mappedImage = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, g_imageSize, GL_MAP_READ_BIT);
			FrameBuffer(fb.timestamp, g_imageFormat, g_imageWidth, g_imageHeight, mappedImage);
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			fb.imageReady = false;
		}

		// 0) Capture Time Stamp
		fb.timestamp = currentTime;

		// Create GL objects if necessary...
		if(!fb.renderbuffer)
		{
			glGenRenderbuffers(1, &fb.renderbuffer);
			glstate.BindRenderbuffer(fb.renderbuffer);
			glRenderbufferStorage(GL_RENDERBUFFER, g_imageFormatGL, g_imageWidthBlocks, g_imageHeightBlocks);
		}
		if(!fb.fbo)
		{
			glGenFramebuffers(1, &fb.fbo);
			glstate.BindFramebuffer(fb.fbo);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fb.renderbuffer);
			const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if(status != GL_FRAMEBUFFER_COMPLETE)
			{
				Logf(Log_Error, "OVR::Capture::FrameBufferGLES3(): Failed to create valid FBO!");
				fb.fbo = 0;
			}
		}
		if(!fb.pbo)
		{
			glGenBuffers(1, &fb.pbo);
			glstate.BindPixelBuffer(fb.pbo);
			glBufferData(GL_PIXEL_PACK_BUFFER, g_imageSize, NULL, GL_DYNAMIC_READ);
		}

		// Create shader if necessary...
		if(!g_vertexShader)
		{
			const char *vertexShaderSource = g_imageFormat==FrameBuffer_DXT1 ? g_vertexShaderSourceDXT1 : g_vertexShaderSource;
			g_vertexShader = glCreateShader(GL_VERTEX_SHADER);
			glShaderSource(g_vertexShader, 1, &vertexShaderSource, 0);
			glCompileShader(g_vertexShader);
			GLint success = 0;
			glGetShaderiv(g_vertexShader, GL_COMPILE_STATUS, &success);
			if(success == GL_FALSE)
			{
				Logf(Log_Error, "OVR_Capture_GLES3: Failed to compile Vertex Shader!");
				DumpShaderCompileLog(g_vertexShader);
				glDeleteShader(g_vertexShader);
				g_vertexShader = 0;
			}
		}
		if(!g_fragmentShader)
		{
			const char *fragmentShaderSource = g_imageFormat==FrameBuffer_DXT1 ? g_fragmentShaderSourceDXT1 : g_fragmentShaderSource;
			g_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
			glShaderSource(g_fragmentShader, 1, &fragmentShaderSource, 0);
			glCompileShader(g_fragmentShader);
			GLint success = 0;
			glGetShaderiv(g_fragmentShader, GL_COMPILE_STATUS, &success);
			if(success == GL_FALSE)
			{
				Logf(Log_Error, "OVR_Capture_GLES3: Failed to compile Fragment Shader!");
				DumpShaderCompileLog(g_fragmentShader);
				glDeleteShader(g_fragmentShader);
				g_fragmentShader = 0;
			}
		}
		if(!g_program && g_vertexShader && g_fragmentShader)
		{
			g_program = glCreateProgram();
			glAttachShader(g_program, g_vertexShader);
			glAttachShader(g_program, g_fragmentShader);
			glBindAttribLocation(g_program, POSITION_ATTRIBUTE, "Position" );
			glBindAttribLocation(g_program, TEXCOORD_ATTRIBUTE, "TexCoord" );
			glLinkProgram(g_program);
			GLint success = 0;
			glGetProgramiv(g_program, GL_LINK_STATUS, &success);
			if(success == GL_FALSE)
			{
				Logf(Log_Error, "OVR_Capture_GLES3: Failed to link Program!");
				glDeleteProgram(g_program);
				g_program = 0;
			}
			else
			{
				glstate.UseProgram(g_program);
				glUniform1i(glGetUniformLocation(g_program, "Texture0"), 0);
				if(g_imageFormat == FrameBuffer_DXT1)
				{
					glUniform2f(glGetUniformLocation(g_program, "TexelSize"), 1.0f/(float)g_imageWidth, 1.0f/(float)g_imageHeight);
					glUniform2f(glGetUniformLocation(g_program, "UVBlockScale"), (g_imageWidth-3)/(float)g_imageWidth, (g_imageHeight-3)/(float)g_imageHeight);
				}
				g_textureRectLoc = glGetUniformLocation(g_program, "TextureRect");
			}
		}

		// Create Vertex Array...
		if(!g_vertexArrayObject)
		{
			glGenVertexArrays(1, &g_vertexArrayObject);
			glstate.BindVertexArray(g_vertexArrayObject);

			glGenBuffers(1, &g_vertexBuffer);
			glBindBuffer(GL_ARRAY_BUFFER, g_vertexBuffer);
			glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertices), g_vertices, GL_STATIC_DRAW);

			glEnableVertexAttribArray(POSITION_ATTRIBUTE);
			glEnableVertexAttribArray(TEXCOORD_ATTRIBUTE);
			glVertexAttribPointer(POSITION_ATTRIBUTE, 4, GL_FLOAT, GL_FALSE, sizeof(float)*6, ((const char*)NULL)+sizeof(float)*0);
			glVertexAttribPointer(TEXCOORD_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE, sizeof(float)*6, ((const char*)NULL)+sizeof(float)*4);
		}

		// Verify all the objects we need are allocated...
		if(!fb.renderbuffer || !fb.fbo || !fb.pbo || !g_program || !g_vertexArrayObject)
			return;

		// 1) StretchBlit into lower resolution 565 texture

		// Override OpenGL State...
		if(true)
		{
			OVR_CAPTURE_CPU_ZONE(BindState);

			glstate.BindFramebuffer(fb.fbo);

			glstate.Viewport(0, 0, g_imageWidthBlocks, g_imageHeightBlocks);

			glstate.Disable(glstate.DEPTH_TEST);
			glstate.Disable(glstate.SCISSOR_TEST);
			glstate.Disable(glstate.STENCIL_TEST);
			glstate.Disable(glstate.RASTERIZER_DISCARD);
			glstate.Disable(glstate.DITHER);
			glstate.Disable(glstate.CULL_FACE); // turning culling off entirely is one less state than setting winding order and front face.
			glstate.Disable(glstate.BLEND);

			glstate.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

			glstate.BindTexture(glstate.TEXTURE0, textureID);
			glstate.UseProgram(g_program);

			// TextureRect...
			if(g_textureRectLoc != -1)
			{
				glUniform4f(g_textureRectLoc, textureRect.x, textureRect.y, textureRect.width, textureRect.height);
			}

			glstate.BindVertexArray(g_vertexArrayObject);
		}

		const GLenum attachments[1] = { GL_COLOR_ATTACHMENT0 };
		glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, attachments);

		// Useful for detecting GL state leaks that kill drawing...
		// Because only a few state bits affect glClear (pixel ownership, dithering, color mask, and scissor), enabling this
		// helps narrow down if a the FB is not being updated because of further state leaks that affect rendering geometry,
		// or if its another issue.
		//glClearColor(0,0,1,1);
		//glClear(GL_COLOR_BUFFER_BIT);

		// Blit draw call...
		if(true)
		{
			OVR_CAPTURE_CPU_AND_GPU_ZONE(DrawArrays);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}

		// 2) Issue async ReadPixels into pixel buffer object
		if(true)
		{
			OVR_CAPTURE_CPU_ZONE(ReadPixels);
			glstate.BindFramebuffer(fb.fbo);
			glstate.BindPixelBuffer(fb.pbo);
			if(g_imageFormatGL == GL_RGB565)
				glReadPixels(0, 0, g_imageWidthBlocks, g_imageHeightBlocks, GL_RGB,          GL_UNSIGNED_SHORT_5_6_5, 0);
			else if(g_imageFormatGL == GL_RGBA8)
				glReadPixels(0, 0, g_imageWidthBlocks, g_imageHeightBlocks, GL_RGBA,         GL_UNSIGNED_BYTE,        0);
			else if(g_imageFormatGL == GL_RGBA16UI)
				glReadPixels(0, 0, g_imageWidthBlocks, g_imageHeightBlocks, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT,       0);
			else if(g_imageFormatGL == GL_RGBA16I)
				glReadPixels(0, 0, g_imageWidthBlocks, g_imageHeightBlocks, GL_RGBA_INTEGER, GL_SHORT,                0);
			fb.imageReady = true;
		}

		// Increment to the next PendingFrameBuffer...
		g_nextPendingFramebuffer = (g_nextPendingFramebuffer+1)&(g_maxPendingFramebuffers-1);
	}


	// We will have a circular buffer of GPU timer queries which will will take overship of inside Enter/Leave...
	// And every frame we will iterate through all the completed queries and dispatch the ones that are complete...
	// The circiular buffer size therefor should be at least big enough to hold all the outstanding queries for about 2 frames.
	// On Qualcomm chips we will need to check for disjoint events and send a different packet when that happens... as this indicates
	// that the timings are unreliable. This is probably also a worthwhile event to mark in the timeline as it probably will
	// coincide with a hitch or poor performance event.
	// EXT_disjoint_timer_query
	// https://www.khronos.org/registry/gles/extensions/EXT/EXT_disjoint_timer_query.txt

	#define GL_QUERY_COUNTER_BITS_EXT         0x8864
	#define GL_CURRENT_QUERY_EXT              0x8865
	#define GL_QUERY_RESULT_EXT               0x8866
	#define GL_QUERY_RESULT_AVAILABLE_EXT     0x8867
	#define GL_TIME_ELAPSED_EXT               0x88BF
	#define GL_TIMESTAMP_EXT                  0x8E28
	#define GL_GPU_DISJOINT_EXT               0x8FBB

	typedef void      (*PFNGLGENQUERIESEXTPROC)         (GLsizei n,       GLuint *ids);
	typedef void      (*PFNGLDELETEQUERIESEXTPROC)      (GLsizei n, const GLuint *ids);
	typedef GLboolean (*PFNGLISQUERYEXTPROC)            (GLuint id);
	typedef void      (*PFNGLBEGINQUERYEXTPROC)         (GLenum target, GLuint id);
	typedef void      (*PFNGLENDQUERYEXTPROC)           (GLenum target);
	typedef void      (*PFNGLQUERYCOUNTEREXTPROC)       (GLuint id, GLenum target);
	typedef void      (*PFNGLGETQUERYIVEXTPROC)         (GLenum target, GLenum pname, GLint    *params);
	typedef void      (*PFNGLGETQUERYOBJECTIVEXTPROC)   (GLuint id,     GLenum pname, GLint    *params);
	typedef void      (*PFNGLGETQUERYOBJECTUIVEXTPROC)  (GLuint id,     GLenum pname, GLuint   *params);
	typedef void      (*PFNGLGETQUERYOBJECTI64VEXTPROC) (GLuint id,     GLenum pname, GLint64  *params);
	typedef void      (*PFNGLGETQUERYOBJECTUI64VEXTPROC)(GLuint id,     GLenum pname, GLuint64 *params);
	typedef void      (*PFNGLGETINTEGER64VPROC)         (GLenum pname,                GLint64  *params);


	static PFNGLGENQUERIESEXTPROC          glGenQueriesEXT_          = NULL;
	static PFNGLDELETEQUERIESEXTPROC       glDeleteQueriesEXT_       = NULL;
	static PFNGLISQUERYEXTPROC             glIsQueryEXT_             = NULL;
	static PFNGLBEGINQUERYEXTPROC          glBeginQueryEXT_          = NULL;
	static PFNGLENDQUERYEXTPROC            glEndQueryEXT_            = NULL;
	static PFNGLQUERYCOUNTEREXTPROC        glQueryCounterEXT_        = NULL;
	static PFNGLGETQUERYIVEXTPROC          glGetQueryivEXT_          = NULL;
	static PFNGLGETQUERYOBJECTIVEXTPROC    glGetQueryObjectivEXT_    = NULL;
	static PFNGLGETQUERYOBJECTUIVEXTPROC   glGetQueryObjectuivEXT_   = NULL;
	static PFNGLGETQUERYOBJECTI64VEXTPROC  glGetQueryObjecti64vEXT_  = NULL;
	static PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64vEXT_ = NULL;
	static PFNGLGETINTEGER64VPROC          glGetInteger64v_          = NULL;

	static bool g_timerQueryInitialized        = false;
	static bool g_has_EXT_disjoint_timer_query = false;

	static void SendGPUSyncPacket(void)
	{
		// This is used to compare our own monotonic clock to the GL one...
		// OVRMonitor use the difference between these two values to sync up GPU and CPU events.
		GPUClockSyncPacket clockSyncPacket;
		clockSyncPacket.timestampCPU = GetNanoseconds();
		glGetInteger64v_(GL_TIMESTAMP_EXT, (GLint64*)&clockSyncPacket.timestampGPU);
		AsyncStream::Acquire()->WritePacket(clockSyncPacket);
	}

	struct TimerQuery
	{
		GLuint queryID;
		UInt32 labelID; // 0 for Leave event...
	};

	// We assume each thread has its own OpenGL Context and that GL contexts are not shared
	// between threads. This is of course not true 100% of the time but it should be for
	// the majority of applications and avoids us having to doing some sort of context->ring
	// hash map lookup + locks.
	// So currently we have a TimerQueryRing that is unique to each thread and acquired via
	// a TLS lookup.
	struct TimerQueryRing
	{
		static const UInt32 size  = 512; // TODO: dynamically resize as needed.
		UInt32              count;
		TimerQuery          ringBuffer[size];
		TimerQuery         *begin;
		TimerQuery         *end;
		TimerQuery         *head;
		TimerQuery         *tail;
	};

	static TimerQueryRing *CreateTimerQueryRing(void)
	{
		TimerQueryRing *ring = new TimerQueryRing;
		ring->count = 0;
		ring->begin = ring->ringBuffer;
		ring->end   = ring->begin + TimerQueryRing::size;
		ring->head  = ring->begin; // head = next element we read from
		ring->tail  = ring->begin; // tail = next element we write to
		for(GLuint i=0; i<TimerQueryRing::size; i++)
		{
			glGenQueriesEXT_(1, &ring->ringBuffer[i].queryID);
			OVR_CAPTURE_ASSERT(ring->ringBuffer[i].queryID);
		}
		SendGPUSyncPacket();
		return ring;
	}

	static void DestroyTimerQueryRing(void *value)
	{
		TimerQueryRing *ring = (TimerQueryRing*)value;
		// Note: we intentionally don't clean up the queryIDs because its almost certain that the
		//       OpenGL context has been destroyed before the thread is destroyed.
		delete ring;
	}

	static ThreadLocalKey g_timerQueryTlsKey = CreateThreadLocalKey(DestroyTimerQueryRing);

	static TimerQueryRing &AcquireTimerQueryRing(void)
	{
		TimerQueryRing *ring = (TimerQueryRing*)GetThreadLocalValue(g_timerQueryTlsKey);
		if(!ring)
		{
			ring = CreateTimerQueryRing();
			SetThreadLocalValue(g_timerQueryTlsKey, ring);
		}
		return *ring;
	}

	static TimerQuery *NextTimerQuery(TimerQuery *curr)
	{
		TimerQueryRing &ring = AcquireTimerQueryRing();
		curr++;
		if(curr == ring.end)
			curr = ring.begin;
		return curr;
	}

	static bool IsQueryRingEmpty(void)
	{
		TimerQueryRing &ring = AcquireTimerQueryRing();
		return ring.count==0;
	}

	static bool IsQueryRingFull(void)
	{
		TimerQueryRing &ring = AcquireTimerQueryRing();
		return ring.count==TimerQueryRing::size;
	}

	static TimerQuery *QueryRingAlloc(void)
	{
		TimerQueryRing &ring = AcquireTimerQueryRing();
		TimerQuery *query = NULL;
		if(!IsQueryRingFull())
		{
			query = ring.tail;
			ring.tail = NextTimerQuery(ring.tail);
			ring.count++;
		}
		return query;
	}

	static TimerQuery *QueryRingHead(void)
	{
		TimerQueryRing &ring = AcquireTimerQueryRing();
		return IsQueryRingEmpty() ? NULL : ring.head;
	}

	static void QueryRingPop(void)
	{
		TimerQueryRing &ring = AcquireTimerQueryRing();
		if(!IsQueryRingEmpty())
		{
			ring.head = NextTimerQuery(ring.head);
			ring.count--;
		}
	}

	static void InitTimerQuery(void)
	{
		// Make sure GL entry points are initialized
		InitGLES3();

		const char *extensions = (const char*)glGetString(GL_EXTENSIONS);
		if(strstr(extensions, "GL_EXT_disjoint_timer_query"))
		{
			g_has_EXT_disjoint_timer_query = true;
			glGenQueriesEXT_          = (PFNGLGENQUERIESEXTPROC)         eglGetProcAddress("glGenQueriesEXT");
			glDeleteQueriesEXT_       = (PFNGLDELETEQUERIESEXTPROC)      eglGetProcAddress("glDeleteQueriesEXT");
			glIsQueryEXT_             = (PFNGLISQUERYEXTPROC)            eglGetProcAddress("glIsQueryEXT");
			glBeginQueryEXT_          = (PFNGLBEGINQUERYEXTPROC)         eglGetProcAddress("glBeginQueryEXT");
			glEndQueryEXT_            = (PFNGLENDQUERYEXTPROC)           eglGetProcAddress("glEndQueryEXT");
			glQueryCounterEXT_        = (PFNGLQUERYCOUNTEREXTPROC)       eglGetProcAddress("glQueryCounterEXT");
			glGetQueryivEXT_          = (PFNGLGETQUERYIVEXTPROC)         eglGetProcAddress("glGetQueryivEXT");
			glGetQueryObjectivEXT_    = (PFNGLGETQUERYOBJECTIVEXTPROC)   eglGetProcAddress("glGetQueryObjectivEXT");
			glGetQueryObjectuivEXT_   = (PFNGLGETQUERYOBJECTUIVEXTPROC)  eglGetProcAddress("glGetQueryObjectuivEXT");
			glGetQueryObjecti64vEXT_  = (PFNGLGETQUERYOBJECTI64VEXTPROC) eglGetProcAddress("glGetQueryObjecti64vEXT");
			glGetQueryObjectui64vEXT_ = (PFNGLGETQUERYOBJECTUI64VEXTPROC)eglGetProcAddress("glGetQueryObjectui64vEXT");
			glGetInteger64v_          = (PFNGLGETINTEGER64VPROC)         eglGetProcAddress("glGetInteger64v");
		}
		g_timerQueryInitialized = true;
	}

	static void ProcessTimerQueries(void)
	{
		if(IsQueryRingEmpty())
			return;

		// if the ring buffer is full, force GL to flush pending commands to prevent a deadlock...
		//if(IsQueryRingFull())
		//    glFlush();

		do
		{
			while(true)
			{
				TimerQuery *query = QueryRingHead();
				// If no more queries are available, we are done...
				if(!query)
					break;
				// Check to see if results are ready...
				GLuint ready = GL_FALSE;
				glGetQueryObjectuivEXT_(query->queryID, GL_QUERY_RESULT_AVAILABLE_EXT, &ready);
				// If results not ready stop trying to flush results as any remaining queries will also fail... we will try again later...
				if(!ready)
					break;
				// Read timer value back...
				GLuint64 gputimestamp = 0;
				glGetQueryObjectui64vEXT_(query->queryID, GL_QUERY_RESULT, &gputimestamp);
				if(query->labelID)
				{
					// Send our enter packet...
					GPUZoneEnterPacket packet;
					packet.labelID   = query->labelID;
					packet.timestamp = gputimestamp;
					AsyncStream::Acquire()->WritePacket(packet);
				}
				else
				{
					// Send leave packet...
					GPUZoneLeavePacket packet;
					packet.timestamp = gputimestamp;
					AsyncStream::Acquire()->WritePacket(packet);
				}
				// Remove this query from the ring buffer...
				QueryRingPop();
			}
		} while(IsQueryRingFull()); // if the ring is full, we keep trying until we have room for more events.

		// Check for disjoint errors...
		GLint disjointOccured = GL_FALSE;
		glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjointOccured);
		if(disjointOccured)
		{
			// TODO: send some sort of disjoint error packet so that OVRMonitor can mark it on the timeline.
			//       because this error likely coicides with bad GPU data and/or a major performance event.
			SendGPUSyncPacket();
		}
	}

	void EnterGPUZoneGLES3(const LabelIdentifier label)
	{
		if(!TryLockConnection(Enable_GPU_Zones))
			return;

		if(!g_timerQueryInitialized)
			InitTimerQuery();

		ProcessTimerQueries();

		if(g_has_EXT_disjoint_timer_query)
		{
			TimerQuery *query = QueryRingAlloc();
			OVR_CAPTURE_ASSERT(query);
			if(query)
			{
				glQueryCounterEXT_(query->queryID, GL_TIMESTAMP_EXT);
				query->labelID = label.GetIdentifier();
			}
		}

		UnlockConnection();
	}

	void LeaveGPUZoneGLES3(void)
	{
		if(!TryLockConnection(Enable_GPU_Zones))
			return;

		OVR_CAPTURE_ASSERT(g_timerQueryInitialized);

		ProcessTimerQueries();
		
		if(g_has_EXT_disjoint_timer_query)
		{
			TimerQuery *query = QueryRingAlloc();
			OVR_CAPTURE_ASSERT(query);
			if(query)
			{
				glQueryCounterEXT_(query->queryID, GL_TIMESTAMP_EXT);
				query->labelID = 0;
			}
		}

		UnlockConnection();
	}

} // namespace Capture
} // namespace OVR

#endif // #if defined(OVR_CAPTURE_HAS_GLES3)
