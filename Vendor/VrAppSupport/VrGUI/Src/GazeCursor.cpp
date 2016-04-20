/************************************************************************************

Filename    :   GazeCursor.cpp
Content     :   Global gaze cursor.
Created     :   June 6, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "GazeCursorLocal.h"

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String_Utils.h"
#include "Kernel/OVR_LogUtils.h"
#include "VrApi.h"
#include "GlTexture.h"
#include "PackageFiles.h"			// for loading images from the assets folder
#include "VrCommon.h"

namespace OVR {

static const char* GazeCursorVertexSrc =
	"uniform mat4 Mvpm;\n"
	"uniform vec4 UniformColor;\n"
	"attribute vec4 Position;\n"
	"attribute vec2 TexCoord;\n"
	"attribute vec4 VertexColor;\n"
//	"attribute vec2 TexCoord1;\n"
	"varying  highp vec2 oTexCoord;\n"
//	"varying  highp vec2 oTexCoord1;\n"
	"varying  lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = Mvpm * Position;\n"
	"   oTexCoord = TexCoord;\n"
//	"   oTexCoord1 = TexCoord1;\n"
	"	oColor = VertexColor * UniformColor;\n"
	"}\n";

static const char* GazeCursorFragmentSrc =
	"uniform sampler2D Texture0;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"	gl_FragColor = oColor * texture2D( Texture0, oTexCoord );\n"
	"}\n";

static const char* GazeCursorTimerVertexSrc =
	"uniform mat4 Mvpm;\n"
	"uniform vec4 UniformColor;\n"
	"attribute vec4 Position;\n"
	"attribute vec2 TexCoord;\n"
//	"attribute vec2 TexCoord1;\n"
	"varying  highp vec2 oTexCoord;\n"
//	"varying  highp vec2 oTexCoord1;\n"
	"varying  lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = Mvpm * Position;\n"
	"   oTexCoord = TexCoord;\n"
//	"   oTexCoord1 = TexCoord1;\n"
	"	oColor = UniformColor;\n"
	"}\n";


static const char * GazeCursorColorTableFragmentSrc =
	"uniform sampler2D Texture0;\n"
	"uniform sampler2D Texture1;\n"
	"uniform mediump vec2 ColorTableOffset;\n"
	"varying mediump vec2 oTexCoord;\n"
	"varying lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"    lowp vec4 texel = texture2D( Texture0, oTexCoord );\n"
	"    mediump vec2 colorIndex = vec2( texel.x, ColorTableOffset.y );\n"
	"    lowp vec4 outColor = texture2D( Texture1, colorIndex.xy );\n"
	"    gl_FragColor = vec4( outColor.xyz * oColor.xyz, texel.a );\n"
	"}\n";

static const Vector4f GazeCursorPositions[4] =
{
	Vector4f( -1.0f,-1.0f, 0.0f, 1.0f ),
	Vector4f(  1.0f,-1.0f, 0.0f, 1.0f ),
	Vector4f( -1.0f, 1.0f, 0.0f, 1.0f ),
	Vector4f(  1.0f, 1.0f, 0.0f, 1.0f ),
};

static const Vector2f GazeCursorUV0s[4] =
{
	Vector2f( 0.0f, 0.0f ),
	Vector2f( 1.0f, 0.0f ),
	Vector2f( 0.0f, 1.0f ),
	Vector2f( 1.0f, 1.0f ),
};

static const GLushort GazeCursorIndices[6] = 
{
	0, 1, 2,
	3, 2, 1,
};

//==============================
// OvrGazeCursor::Create
OvrGazeCursor * OvrGazeCursor::Create()
{
	OvrGazeCursorLocal * gc = new OvrGazeCursorLocal();
	gc->Init();
	return gc;
}

//==============================
// OvrGazeCursor::Destroy
void OvrGazeCursor::Destroy( OvrGazeCursor * & gazeCursor )
{
	if ( gazeCursor != NULL )
	{
		static_cast< OvrGazeCursorLocal* >( gazeCursor )->Shutdown();
		delete gazeCursor;
		gazeCursor = NULL;
	}
}

//==============================
// OvrGazeCursorLocal::OvrGazeCursorLocal
OvrGazeCursorLocal::OvrGazeCursorLocal() :
	CursorRotation( 0.0f ),
	RotationRateRadians( 0.0f ),
	CursorScale( 0.0125f ),
	DistanceOffset( 0.05f ),
	HiddenFrames( 0 ),
	CurrentTransform( 0 ),
	ColorTableOffset( 0.0f ),
	TimerShowTime( -1.0 ),
	TimerEndTime( -1.0 ),
	CursorDynamicVBO( 0 ),
	CursorStaticVBO( 0 ),
	CursorIBO( 0 ),
	CursorVAO( 0 ),
	CursorTextureHandle(),
	TimerTextureHandle( 0 ),
	ColorTableHandle( 0 ),
	Initialized( false ),
	Hidden( true )
{
}

//==============================
// OvrGazeCursorLocal::OvrGazeCursorLocal
OvrGazeCursorLocal::~OvrGazeCursorLocal()
{
}

//==============================
// OvrGazeCursorLocal::
void OvrGazeCursorLocal::Init()
{
	LOG( "OvrGazeCursorLocal::Init" );
	ASSERT_WITH_TAG( Initialized == false, "GazeCursor" );

	if ( Initialized )
	{
		LOG( "OvrGazeCursorLocal::Init - already initialized!" );
		return;
	}

	CreateCursorGeometry();
	TimerGeometry = BuildTesselatedQuad( 1, 1 );

	int w = 0;
	int h = 0;
	char const * const cursorStateNames[ CURSOR_STATE_MAX ] =
	{
		//"res/raw/color_ramp_test.tga",
		//"res/raw/color_ramp_test.tga",

		//"res/raw/gaze_cursor_dot.tga",
		//"res/raw/gaze_cursor_dot_hi.tga"
		
		//"res/raw/gaze_cursor_cross.tga",
		//"res/raw/gaze_cursor_cross_hi.tga"
		
		"res/raw/gaze_cursor_cross.tga",
		"res/raw/gaze_cursor_cross.tga",	// for now, hilight is the same because the graphic needs work
		"res/raw/gaze_cursor_cross.tga",
		"res/raw/gaze_cursor_hand.tga"
	};

	for ( int i = 0; i < CURSOR_STATE_MAX; ++i )
	{
		CursorTextureHandle[i] = LoadTextureFromApplicationPackage( cursorStateNames[i], TextureFlags_t(), w, h );
	}

	TimerTextureHandle = LoadTextureFromApplicationPackage( "res/raw/gaze_cursor_timer.tga", TextureFlags_t(), w, h );

	ColorTableHandle = LoadTextureFromApplicationPackage( "res/raw/color_ramp_timer.tga", TextureFlags_t(), w, h );

	CursorProgram = BuildProgram( GazeCursorVertexSrc, GazeCursorFragmentSrc );
	TimerProgram = BuildProgram( GazeCursorTimerVertexSrc, GazeCursorColorTableFragmentSrc );//GazeCursorFragmentSrc );

	Initialized = true;
}

//==============================
// OvrGazeCursorLocal::
void OvrGazeCursorLocal::Shutdown()
{
	LOG( "OvrGazeCursorLocal::Shutdown" );
	ASSERT_WITH_TAG( Initialized == true, "GazeCursor" );

	ReleaseCursorGeometry();

	for ( int i = 0; i < CURSOR_STATE_MAX; ++i )
	{
		if ( CursorTextureHandle[i] != 0 )
		{
			glDeleteTextures( 1, &CursorTextureHandle[i] );
			CursorTextureHandle[i] = 0;
		}
	}

	if ( TimerTextureHandle != 0 )
	{
		glDeleteTextures( 1, & TimerTextureHandle );
		TimerTextureHandle = 0;
	}

	if ( ColorTableHandle != 0 )
	{
		glDeleteTextures( 1, &ColorTableHandle );
		ColorTableHandle = 0;
	}

	DeleteProgram( CursorProgram );
	DeleteProgram( TimerProgram );

	Initialized = false;
}

//==============================
// OvrGazeCursorLocal::UpdateDistance
void OvrGazeCursorLocal::UpdateDistance( float const d, eGazeCursorStateType const state )
{
	//LOG( "OvrGazeCursorLocal::UpdateDistance %.4f", d );
	if ( d < Info.Distance )
	{
		//LOG( "OvrGazeCursorLocal::UpdateDistance - new closest distace %.2f", d );
		Info.Distance = d;
		Info.State = state;
	}
}
//==============================
// OvrGazeCursorLocal::ForceDistance
void OvrGazeCursorLocal::ForceDistance( float const d, eGazeCursorStateType const state )
{
	Info.Distance = d;
	Info.State = state;
}

//==============================
// OvrGazeCursorLocal::ClearGhosts
void OvrGazeCursorLocal::ClearGhosts()
{
	CurrentTransform = 0;
}

static float frand()
{
	return ( rand() & 65535 ) / (65535.0f / 2.0f) - 1.0f;
}

//==============================
// OvrGazeCursorLocal::Frame
void OvrGazeCursorLocal::Frame( Matrix4f const & viewMatrix, float const deltaTime )
{
	//LOG( "OvrGazeCursorLocal::Frame" );
	HiddenFrames -= 1;

	if ( RotationRateRadians != 0.0f )	// comparison to exactly 0 is intentional
	{
		CursorRotation += deltaTime * RotationRateRadians;
		if ( CursorRotation > Mathf::TwoPi )
		{
			CursorRotation -= Mathf::TwoPi;
		}
		else if ( CursorRotation < 0.0f )
		{
			CursorRotation += Mathf::TwoPi;
		}
	}
#if 1
	if ( TimerEndTime > 0.0 )
	{
		double TimeRemaining = TimerEndTime - vrapi_GetTimeInSeconds();
		if ( TimeRemaining <= 0.0 )
		{
			TimerEndTime = -1.0;
			TimerShowTime = -1.0;
			ColorTableOffset = Vector2f( 0.0f );
		}
		else
		{
			double duration = TimerEndTime - TimerShowTime;
			double ratio = 1.0f - ( TimeRemaining / duration );
			//SPAM( "TimerEnd = %.2f, TimeRemaining = %.2f, Ratio = %.2f", TimerEndTime, TimeRemaining, ratio );
			ColorTableOffset.x = 0.0f;
			ColorTableOffset.y = float( ratio );
		}
	}
	else
	{
		ColorTableOffset = Vector2f( 0.0f );
	}
#else
	// cycling
	float COLOR_TABLE_CYCLE_RATE = 0.25f;
	ColorTableOffset.x = 0.0f;
	ColorTableOffset.y += COLOR_TABLE_CYCLE_RATE * deltaTime;
	if ( ColorTableOffset.y > 1.0f )
	{
		ColorTableOffset.y -= floorf( ColorTableOffset.y );
	}
	else if ( ColorTableOffset.y < 0.0f )
	{
		ColorTableOffset.y += ceilf( ColorTableOffset.y );
	}
#endif

	const Vector3f viewPos( GetViewMatrixPosition( viewMatrix ) );
	const Vector3f viewFwd( GetViewMatrixForward( viewMatrix ) );

	Vector3f position = viewPos + viewFwd * ( Info.Distance - DistanceOffset );

	Matrix4f viewRot = viewMatrix;
	viewRot.SetTranslation( Vector3f( 0.0f ) );

	// Add one ghost for every four milliseconds.
	// Assume we are going to be at even multiples of vsync, so we don't need to bother
	// keeping an accurate roundoff count.
	const int lerps = static_cast<int>( deltaTime / 0.004f );

	const Matrix4f & prev = CursorTransform[ CurrentTransform % TRAIL_GHOSTS ];
	Matrix4f & now = CursorTransform[ ( CurrentTransform + lerps ) % TRAIL_GHOSTS ];

	now = Matrix4f::Translation( position ) * viewRot.Inverted() * Matrix4f::RotationZ( CursorRotation )
		* Matrix4f::Scaling( CursorScale, CursorScale, 1.0f );

	if ( CurrentTransform > 0 )
	{
		for ( int i = 1 ; i <= lerps ; i++ )
		{
			const float f = (float)i / lerps;
			Matrix4f & tween = CursorTransform[ ( CurrentTransform + i) % TRAIL_GHOSTS ];

			// We only need to build a scatter on the final point that is already set by now
			if ( i != lerps )
			{
				tween = ( ( now * f ) + ( prev * ( 1.0f - f ) ) );
			}

			// When the cursor depth fails, draw a scattered set of ghosts
			Matrix4f & scatter = CursorScatterTransform[ ( CurrentTransform + i) % TRAIL_GHOSTS ];

			// random point in circle
			float	rx, ry;
			while( 1 )
			{
				rx = frand();
				ry = frand();
				if ( (rx*rx + ry*ry < 1.0f ))
				{
					break;
				}
			}
			scatter = tween * Matrix4f::Translation( rx, ry, 0.0f );
		}
	}
	else
	{
		// When CurrentTransform is 0, reset "lerp" cursors to the now transform as these will be drawn in the next frame.
		// If this is not done, only the cursor at pos 0 will have the now orientation, while the others up to lerps will have old data
		// causing a brief "duplicate" to be on screen.
		for ( int i = 1 ; i < lerps ; i++ )
		{
			Matrix4f & tween = CursorTransform[ ( CurrentTransform + i) % TRAIL_GHOSTS ];
			tween = now;
		}
	}
	CurrentTransform += lerps;

	position -= viewFwd * 0.025f; // to avoid z-fight with the translucent portion of the crosshair image
	TimerTransform = Matrix4f::Translation( position ) * viewRot.Inverted() * Matrix4f::RotationZ( CursorRotation ) * Matrix4f::Scaling( CursorScale * 4.0f, CursorScale * 4.0f, 1.0f );

	RenderInfo = Info;

	// Update cursor geometry...
	UpdateCursorGeometry();
}

//==============================
// OvrGazeCursorLocal::RenderForEye
void OvrGazeCursorLocal::RenderForEye( Matrix4f const & mvp ) const
{
	GL_CheckErrors( "OvrGazeCursorLocal::Render - pre" );

	//LOG( "OvrGazeCursorLocal::Render" );

	if ( HiddenFrames >= 0 )
	{
		return;
	}

	if ( Hidden && !TimerActive() )
	{
		return;
	}

	if ( CursorScale <= 0.0f )
	{
		LOG( "OvrGazeCursorLocal::Render - scale 0" );
		return;
	}

	// It is important that glBlendFuncSeparate be used so that destination alpha
	// correctly holds the opacity over the overlay plane.  If normal blending is
	// used, the cursor ghosts will "punch holes" in things through to the overlay plane.
	glEnable( GL_BLEND );
	glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

	// If the cursor is not active, allow it to depth sort... if it is active we know it should
	// be on top, so don't bother depth testing. However, this assumes that the cursor is placed 
	// correctly. If it isn't, then it could still appear stereoscopically behind the object it's
	// on top of, which would be bad. The reason for not using depth testing when active is to
	// solve any z-fighting issues, particularly with SwipeView, where the panel distance isn't
	// entirely accurate right now. It can also be solved by pushing the cursor in a little bit
	// from the panel, but in stereo vision it looks like it's floating further above the panel
	// than it does with other systems like the VRMenu.

	glDepthMask( GL_FALSE );	// don't write to depth, or ghost trails wouldn't work

	// We always want depth test enabled so we can see if any GUI panels have
	// been placed too close and obscured the cursor.
	glEnable( GL_DEPTH_TEST );

	glUseProgram( CursorProgram.program );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, CursorTextureHandle[RenderInfo.State] );
	glUniformMatrix4fv( CursorProgram.uMvp, 1, GL_FALSE, mvp.Transposed().M[0] );

	// Draw with default z-func...
	const Vector4f cursorColor( 1.0f, 1.0f, 1.0f, 0.5f );
	glUniform4fv( CursorProgram.uColor, 1, &cursorColor.x );
	DrawCursorWithTrail( 0 );

	// Reverse depth test and draw the scattered ghosts where they are occluded
	const Vector4f cursorColorOccluded( 1.0f, 0.0f, 0.0f, 0.15f );
	glUniform4fv( CursorProgram.uColor, 1, &cursorColorOccluded.x );
	glDepthFunc( GL_GREATER );
	DrawCursorWithTrail( 1 );
	glDepthFunc( GL_LEQUAL );

	// draw the timer if it's enabled
	if ( TimerEndTime > 0.0 && vrapi_GetTimeInSeconds() >= TimerShowTime )
	{
		glUseProgram( TimerProgram.program );
		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_2D, TimerTextureHandle );

		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_2D, ColorTableHandle );
		// do not do any filtering on the "palette" texture
#if defined( OVR_OS_ANDROID )
		if ( EXT_texture_filter_anisotropic )
#endif
		{
			glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f );
		}
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

		Matrix4f timerMVP = mvp * TimerTransform;
		glUniformMatrix4fv( TimerProgram.uMvp, 1, GL_FALSE, timerMVP.Transposed().M[0] );

		Vector4f cursorColor( 0.0f, 0.643f, 1.0f, 1.0f );
		glUniform4fv( TimerProgram.uColor, 1, &cursorColor.x );
		glUniform2fv( TimerProgram.uColorTableOffset, 1, &ColorTableOffset.x );

		TimerGeometry.Draw();
	}

	glDepthMask( GL_TRUE );
	glDisable( GL_BLEND );

	GL_CheckErrors( "OvrGazeCursorLocal::Render - post" );
}

//==============================
// OvrGazeCursorLocal::GetInfo
OvrGazeCursorInfo OvrGazeCursorLocal::GetInfo() const
{
	return Info;
}


//==============================
// OvrGazeCursorLocal::SetRotationRate
void OvrGazeCursorLocal::SetRotationRate( float const degreesPerSec )
{
	RotationRateRadians = degreesPerSec * Mathf::DegreeToRadFactor;
}

//==============================
// OvrGazeCursorLocal::SetCursorScale
void OvrGazeCursorLocal::SetCursorScale( float const scale )
{
	CursorScale = scale;
}

//==============================
// OvrGazeCursorLocal::StartTimer
// Returns whether the gaze cursor will be drawn this frame
bool OvrGazeCursorLocal::IsVisible() const
{
	if ( HiddenFrames >= 0 )
	{
		return false;
	}

	if ( Hidden && !TimerActive() )
	{
		return false;
	}

	if ( CursorScale <= 0.0f )
	{
		return false;
	}
	
	return true;
}

//==============================
// OvrGazeCursorLocal::StartTimer
void OvrGazeCursorLocal::StartTimer( float const durationSeconds, float const timeBeforeShowingTimer )
{
	double curTime = vrapi_GetTimeInSeconds();
	LOG( "(%.4f) StartTimer = %.2f", curTime, durationSeconds );
	TimerShowTime =  curTime + (double)timeBeforeShowingTimer; 
	TimerEndTime = curTime + (double)durationSeconds;
}

//==============================
// OvrGazeCursorLocal::CancelTimer
void OvrGazeCursorLocal::CancelTimer()
{
	double curTime = vrapi_GetTimeInSeconds();
	LOG( "(%.4f) Cancel Timer", curTime );
	TimerShowTime = -1.0;
	TimerEndTime = -1.0;
}

//==============================
// OvrGazeCursorLocal::TimerActive
bool OvrGazeCursorLocal::TimerActive() const
{ 
    return TimerEndTime > vrapi_GetTimeInSeconds();
}

void OvrGazeCursorLocal::CreateCursorGeometry()
{
	const size_t vertexCount = TRAIL_GHOSTS * 4;
	const size_t indexCount  = TRAIL_GHOSTS * 6;

	// We pack two buffers into one... one for scattered trails, one for normal.
	const int numBuffers = 2;

	glGenBuffers( 1, &CursorDynamicVBO );
	glGenBuffers( 1, &CursorStaticVBO );
	glGenBuffers( 1, &CursorIBO );
	glGenVertexArrays( 1, &CursorVAO );

	glBindVertexArray( CursorVAO );

	// allocate dynamic vertex buffer...
	glBindBuffer( GL_ARRAY_BUFFER, CursorDynamicVBO );
	glBufferData( GL_ARRAY_BUFFER, vertexCount * numBuffers * sizeof( Vector4f ), NULL, GL_DYNAMIC_DRAW );
	glEnableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_POSITION );
	glVertexAttribPointer( VERTEX_ATTRIBUTE_LOCATION_POSITION, 4, GL_FLOAT, false, sizeof( float ) * 4, NULL );

	// Generate static vertex buffer...
	struct GazeCusorStaticVertex
	{
		Vector2f uv0;
		Vector4f color;
	};
	GazeCusorStaticVertex * staticData = new GazeCusorStaticVertex[ vertexCount * numBuffers ];
	GazeCusorStaticVertex * currStaticData = staticData;
	for ( int bufferIndex = 0; bufferIndex < numBuffers; bufferIndex++ )
	{
		for ( int slice = 0; slice < TRAIL_GHOSTS; slice++ )
		{
			const float alpha = ( slice + 1 ) / (float)TRAIL_GHOSTS;
			for ( int vertexIndex = 0; vertexIndex < 4; vertexIndex++ )
			{
				GazeCusorStaticVertex &staticVertex = *( currStaticData++ );
				staticVertex.uv0   = GazeCursorUV0s[ vertexIndex ];
				staticVertex.color = Vector4f( 1.0f, 1.0f, 1.0f, alpha );
			}
		}
	}
	glBindBuffer( GL_ARRAY_BUFFER, CursorStaticVBO );
	glBufferData( GL_ARRAY_BUFFER, vertexCount * numBuffers * sizeof( GazeCusorStaticVertex ), staticData, GL_STATIC_DRAW );
	glEnableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_UV0 );
	glEnableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_COLOR );
	glVertexAttribPointer( VERTEX_ATTRIBUTE_LOCATION_UV0,   2, GL_FLOAT, false, sizeof( GazeCusorStaticVertex ), ( void * )offsetof( GazeCusorStaticVertex, uv0 ) );
	glVertexAttribPointer( VERTEX_ATTRIBUTE_LOCATION_COLOR, 4, GL_FLOAT, false, sizeof( GazeCusorStaticVertex ), ( void * )offsetof( GazeCusorStaticVertex, color ) );
	delete [] staticData;

	// Generate static index buffer...
	GLushort * indexData = new GLushort[ indexCount * numBuffers ];
	for ( GLushort slice = 0; slice < TRAIL_GHOSTS * numBuffers; slice++ )
	{
		for ( int i = 0; i < 6; i++ )
		{
			indexData[ slice * 6 + i ] = slice * 4 + GazeCursorIndices[ i ];
		}
	}
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, CursorIBO );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, indexCount * numBuffers * sizeof( GLushort ), indexData, GL_STATIC_DRAW );
	delete [] indexData;

	glBindVertexArray( 0 );
}

void OvrGazeCursorLocal::ReleaseCursorGeometry()
{
	if( CursorVAO )
	{
		glDeleteVertexArrays( 1, &CursorVAO );
		CursorVAO = 0;
	}
	if( CursorDynamicVBO )
	{
		glDeleteBuffers( 1, &CursorDynamicVBO );
		CursorDynamicVBO = 0;
	}
	if( CursorStaticVBO )
	{
		glDeleteBuffers( 1, &CursorStaticVBO );
		CursorStaticVBO = 0;
	}
	if( CursorIBO )
	{
		glDeleteBuffers( 1, &CursorIBO );
		CursorIBO = 0;
	}
}

void OvrGazeCursorLocal::UpdateCursorGeometry() const
{
	ASSERT_WITH_TAG( CursorDynamicVBO != 0, "DrawCursorGeometry" );

	const size_t numQuadsPerDraw = TRAIL_GHOSTS;
	const size_t numVertsPerQuad = 4;
	const size_t bufferSize      = numQuadsPerDraw * numVertsPerQuad * sizeof( Vector4f );

	// We pack two buffers into one... one for scattered trails, one for normal.
	const size_t numBuffers = 2;

	glBindBuffer( GL_ARRAY_BUFFER, CursorDynamicVBO );
	Vector4f * positions = ( Vector4f * )glMapBufferRange( GL_ARRAY_BUFFER, 0, bufferSize * numBuffers, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT );
	if ( positions == NULL )
	{
		// we have logs of glMapBufferRange() apparently returning NULL here after the GPU resets.
		OVR_ASSERT( positions != NULL );
		return;
	}

	// Z-pass positions...
	UpdateCursorPositions( &positions[ 0 ], CursorTransform );

	// Z-fail positions...
	UpdateCursorPositions( &positions[ numQuadsPerDraw * numVertsPerQuad ], CursorScatterTransform );

	glUnmapBuffer( GL_ARRAY_BUFFER );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
}

void OvrGazeCursorLocal::UpdateCursorPositions( Vector4f * positions, Matrix4f const * transforms ) const
{
	OVR_ASSERT( positions != NULL );	

	const int numTrails = TRAIL_GHOSTS<CurrentTransform ? TRAIL_GHOSTS : CurrentTransform;

	// For missing trail transforms, draw degenerate triangles...
	for ( int slice = numTrails; slice < TRAIL_GHOSTS; slice++ )
	{
		for ( int i = 0; i < 4; i++ )
		{
			*( positions++ ) = Vector4f( 0.0f, 0.0f, 0.0f, 0.0f );
		}
	}

	// Transforming on the CPU shouldn't be too painful in this scenario since the vertex count is low
	// and we would be uploading the same amount of data in the form of transforms if we used instancing
	// anyways. So this costs us a few extra ops on the CPU, but allows us to bake the color fade into
	// a static VBO and avoids instancing which may or maynot be fast on all hardware.
	for ( int slice = numTrails - 1; slice >= 0; slice-- )
	{
		const int index = ( CurrentTransform - slice ) % TRAIL_GHOSTS;
		const Matrix4f transform = transforms[ index ];
		for ( int vertexIndex = 0; vertexIndex < 4; vertexIndex++ )
		{
			*( positions++ ) = transform.Transform( GazeCursorPositions[ vertexIndex ] );
		}
	}
}

void OvrGazeCursorLocal::DrawCursorWithTrail( unsigned int bufferIndex ) const
{
	ASSERT_WITH_TAG( CursorVAO != 0, "DrawCursorWithTrail" );

	const GLvoid * offset = reinterpret_cast<const GLvoid *>( bufferIndex * TRAIL_GHOSTS * 6 * sizeof( GLushort ) );

	glBindVertexArray( CursorVAO );
	glDrawElements( GL_TRIANGLES, TRAIL_GHOSTS * 6, GL_UNSIGNED_SHORT, offset );
}


} // namespace OVR
