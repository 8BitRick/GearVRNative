/************************************************************************************

Filename    :   VrApi_Types.h
Content     :   Types for minimum necessary API for mobile VR
Created     :   April 30, 2015
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_VrApi_Types_h
#define OVR_VrApi_Types_h

#include <stdbool.h>
#include "VrApi_Config.h"   // needed for VRAPI_EXPORT

//-----------------------------------------------------------------
// Java
//-----------------------------------------------------------------

#if defined( ANDROID )
#include <jni.h>
#elif defined( __cplusplus )
typedef struct _JNIEnv JNIEnv;
typedef struct _JavaVM JavaVM;
typedef class _jobject * jobject;
#else
typedef const struct JNINativeInterface * JNIEnv;
typedef const struct JNIInvokeInterface * JavaVM;
void * jobject;
#endif

typedef struct
{
	JavaVM *	Vm;					// Java Virtual Machine
	JNIEnv *	Env;				// Thread specific environment
	jobject		ActivityObject;		// Java activity object
} ovrJava;

//-----------------------------------------------------------------
// Basic Types
//-----------------------------------------------------------------

typedef struct ovrVector3f_
{
	float x, y, z;
} ovrVector3f;

// Quaternion.
typedef struct ovrQuatf_
{
	float x, y, z, w;
} ovrQuatf;

// Row-major 4x4 matrix.
typedef struct ovrMatrix4f_
{
	float M[4][4];
} ovrMatrix4f;

// Position and orientation together.
typedef struct ovrPosef_
{
	ovrQuatf	Orientation;
	ovrVector3f	Position;
} ovrPosef;

typedef struct ovrRectf_
{
	float x;
	float y;
	float width;
	float height;
} ovrRectf;

typedef enum
{
	VRAPI_FALSE = 0,
	VRAPI_TRUE
} ovrBooleanResult;

//-----------------------------------------------------------------
// Structure Types
//-----------------------------------------------------------------

typedef enum
{
	VRAPI_STRUCTURE_TYPE_INIT_PARMS		= 1,
	VRAPI_STRUCTURE_TYPE_MODE_PARMS		= 2,
	VRAPI_STRUCTURE_TYPE_FRAME_PARMS	= 3,
} ovrStructureType;

//-----------------------------------------------------------------
// System Properties and Status
//-----------------------------------------------------------------

typedef enum
{
	VRAPI_DEVICE_TYPE_NOTE4,
	VRAPI_DEVICE_TYPE_NOTE5,
	VRAPI_DEVICE_TYPE_S6,
	VRAPI_MAX_DEVICE_TYPES
} ovrDeviceType;

typedef enum
{
	VRAPI_SYS_PROP_DEVICE_TYPE,
	VRAPI_SYS_PROP_MAX_FULLSPEED_FRAMEBUFFER_SAMPLES,
	// Physical width and height of the display in pixels.
	VRAPI_SYS_PROP_DISPLAY_PIXELS_WIDE,
	VRAPI_SYS_PROP_DISPLAY_PIXELS_HIGH,
	// Refresh rate of the display in cycles per second.
	// Currently 60Hz.
	VRAPI_SYS_PROP_DISPLAY_REFRESH_RATE,
	// With a display resolution of 2560x1440, the pixels at the center
	// of each eye cover about 0.06 degrees of visual arc. To wrap a
	// full 360 degrees, about 6000 pixels would be needed and about one
	// quarter of that would be needed for ~90 degrees FOV. As such, Eye
	// images with a resolution of 1536x1536 result in a good 1:1 mapping
	// in the center, but they need mip-maps for off center pixels. To
	// avoid the need for mip-maps and for significantly improved rendering
	// performance this currently returns a conservative 1024x1024.
	VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH,
	VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT,
	// This is a product of the lens distortion and the screen size,
	// but there is no truly correct answer.
	// There is a tradeoff in resolution and coverage.
	// Too small of an FOV will leave unrendered pixels visible, but too
	// large wastes resolution or fill rate.  It is unreasonable to
	// increase it until the corners are completely covered, but we do
	// want most of the outside edges completely covered.
	// Applications might choose to render a larger FOV when angular
	// acceleration is high to reduce black pull in at the edges by
	// the time warp.
	// Currently symmetric 90.0 degrees.
	VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_X,		// Horizontal field of view in degrees
	VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_Y,		// Vertical field of view in degrees
} ovrSystemProperty;

typedef enum
{
	VRAPI_SYS_STATUS_DOCKED,						// Device is docked.
	VRAPI_SYS_STATUS_MOUNTED,						// Device is mounted.
	VRAPI_SYS_STATUS_THROTTLED,						// Device is in powersave mode.
	VRAPI_SYS_STATUS_THROTTLED2,					// Device is in extreme powersave mode.
	VRAPI_SYS_STATUS_THROTTLED_WARNING_LEVEL,		// Powersave mode warning required.
	VRAPI_SYS_STATUS_RENDER_LATENCY_MILLISECONDS,	// Average time between render tracking sample and scanout.
	VRAPI_SYS_STATUS_TIMEWARP_LATENCY_MILLISECONDS,	// Average time between timewarp tracking sample and scanout.
	VRAPI_SYS_STATUS_SCANOUT_LATENCY_MILLISECONDS,	// Average time between Vsync and scanout.
	VRAPI_SYS_STATUS_APP_FRAMES_PER_SECOND,			// Number of frames per second delivered through vrapi_SubmitFrame.
	VRAPI_SYS_STATUS_SCREEN_TEARS_PER_SECOND,		// Number of screen tears per second (per eye).
	VRAPI_SYS_STATUS_EARLY_FRAMES_PER_SECOND,		// Number of frames per second delivered a whole display refresh early.
	VRAPI_SYS_STATUS_STALE_FRAMES_PER_SECOND,		// Number of frames per second delivered late.
} ovrSystemStatus;

//-----------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------

typedef enum
{
	VRAPI_INITIALIZE_SUCCESS			=  0,
	VRAPI_INITIALIZE_UNKNOWN_ERROR		= -1,
	VRAPI_INITIALIZE_PERMISSIONS_ERROR	= -2,
} ovrInitializeStatus;

typedef enum
{
	VRAPI_GRAPHICS_API_OPENGL_ES_2   = ( 0x10000 | 0x0200 ), // OpenGL ES 2.x context
	VRAPI_GRAPHICS_API_OPENGL_ES_3   = ( 0x10000 | 0x0300 ), // OpenGL ES 3.x context
	VRAPI_GRAPHICS_API_OPENGL_COMPAT = ( 0x20000 | 0x0100 ), // OpenGL Compatibility Profile
	VRAPI_GRAPHICS_API_OPENGL_CORE_3 = ( 0x20000 | 0x0300 ), // OpenGL Core Profile 3.x
	VRAPI_GRAPHICS_API_OPENGL_CORE_4 = ( 0x20000 | 0x0400 ), // OpenGL Core Profile 4.x
} ovrGraphicsAPI;

typedef struct
{
	ovrStructureType	Type;
	int					ProductVersion;
	int					MajorVersion;
	int					MinorVersion;
	int					PatchVersion;
	ovrGraphicsAPI		GraphicsAPI;
	ovrJava				Java;
} ovrInitParms;

//-----------------------------------------------------------------
// VR Mode
//-----------------------------------------------------------------

typedef struct
{
	ovrStructureType	Type;

	// If true, warn and allow the app to continue at 30fps when
	// throttling occurs.
	// If false, display the level 2 error message which requires
	// the user to undock.
	bool				AllowPowerSave;

	// When an application with multiple activities moves backwards on
	// the activity stack, the activity window it returns to is no longer
	// flagged as fullscreen. As a result, Android will also render
	// the decor view, which wastes a significant amount of bandwidth.
	// By setting this flag, the fullscreen flag is reset on the window.
	// Unfortunately, this causes Android life cycle events that mess up
	// several NativeActivity codebases like Stratum and UE4, so this
	// flag should only be set for select applications with multiple
	// activities. Use "adb shell dumpsys SurfaceFlinger" to verify
	// that there is only one HWC next to the FB_TARGET.
	bool				ResetWindowFullscreen;

	// The Java VM is needed for the time warp thread to create a Java environment.
	// A Java environment is needed to access various system services. The thread
	// that enters VR mode is responsible for attaching and detaching the Java
	// environment. The Java Activity object is needed to get the windowManager,
	// packageName, systemService, etc.
	ovrJava				Java;

	// If not zero, then use this display for asynchronous time warp rendering.
	// Using EGL this is an EGLDisplay.
	unsigned long long	Display;

	// If not zero, then use this window surface for asynchronous time warp rendering
	// This is expected to be the front buffer.
	// Using EGL this is an EGLSurface.
	unsigned long long	WindowSurface;

	// If not zero, then resources from this context will be shared
	// with the asynchronous time warp.
	// Using EGL this is an EGLContext.
	unsigned long long	ShareContext;
} ovrModeParms;

// VR context
// To allow multiple Android activities that live in the same address space
// to cooperatively use the VrApi, each activity needs to maintain its own
// separate contexts for a lot of the video related systems.
typedef struct ovrMobile ovrMobile;

//-----------------------------------------------------------------
// Tracking
//-----------------------------------------------------------------

// Full rigid body pose with first and second derivatives.
typedef struct ovrRigidBodyPosef_
{
	ovrPosef	Pose;
	ovrVector3f	AngularVelocity;
	ovrVector3f	LinearVelocity;
	ovrVector3f	AngularAcceleration;
	ovrVector3f	LinearAcceleration;
	double		TimeInSeconds;			// Absolute time of this pose.
	double		PredictionInSeconds;	// Seconds this pose was predicted ahead.
} ovrRigidBodyPosef;

// Bit flags describing the current status of sensor tracking.
typedef enum
{
	VRAPI_TRACKING_STATUS_ORIENTATION_TRACKED	= 0x0001,	// Orientation is currently tracked.
	VRAPI_TRACKING_STATUS_POSITION_TRACKED		= 0x0002,	// Position is currently tracked.
	VRAPI_TRACKING_STATUS_HMD_CONNECTED			= 0x0080	// HMD is available & connected.
} ovrTrackingStatus;

// Tracking state at a given absolute time.
typedef struct ovrTracking_
{
	// Sensor status described by ovrTrackingStatus flags.
	unsigned int		Status;
	// Predicted head configuration at the requested absolute time.
	// The pose describes the head orientation and center eye position.
	ovrRigidBodyPosef	HeadPose;
} ovrTracking;

//-----------------------------------------------------------------
// Texture Swap Chain
//-----------------------------------------------------------------

typedef enum
{
	VRAPI_TEXTURE_TYPE_2D,				// 2D textures.
	VRAPI_TEXTURE_TYPE_2D_EXTERNAL,		// External 2D texture.
	VRAPI_TEXTURE_TYPE_2D_ARRAY,		// Texture array.
	VRAPI_TEXTURE_TYPE_CUBE,			// Cube maps.
	VRAPI_TEXTURE_TYPE_MAX
} ovrTextureType;

typedef enum
{
	VRAPI_TEXTURE_FORMAT_NONE,
	VRAPI_TEXTURE_FORMAT_565,
	VRAPI_TEXTURE_FORMAT_5551,
	VRAPI_TEXTURE_FORMAT_4444,
	VRAPI_TEXTURE_FORMAT_8888,
	VRAPI_TEXTURE_FORMAT_8888_sRGB,
	VRAPI_TEXTURE_FORMAT_RGBA16F,
	VRAPI_TEXTURE_FORMAT_DEPTH_16,
	VRAPI_TEXTURE_FORMAT_DEPTH_24,
	VRAPI_TEXTURE_FORMAT_DEPTH_24_STENCIL_8,
} ovrTextureFormat;

typedef enum
{
	VRAPI_DEFAULT_TEXTURE_SWAPCHAIN_BLACK			= 0x1,
	VRAPI_DEFAULT_TEXTURE_SWAPCHAIN_LOADING_ICON	= 0x2
} ovrDefaultTextureSwapChain;

typedef enum
{
	VRAPI_TEXTURE_SWAPCHAIN_FULL_MIP_CHAIN		= -1
} ovrTextureSwapChainSettings;

typedef struct ovrTextureSwapChain ovrTextureSwapChain;

//-----------------------------------------------------------------
// Frame Submission
//-----------------------------------------------------------------

typedef enum
{
	// To get gamma correct sRGB filtering of the eye textures, the textures must be
	// allocated with GL_SRGB8_ALPHA8 format and the window surface must be allocated
	// with these attributes:
	// EGL_GL_COLORSPACE_KHR,  EGL_GL_COLORSPACE_SRGB_KHR
	//
	// While we can reallocate textures easily enough, we can't change the window
	// colorspace without relaunching the entire application, so if you want to
	// be able to toggle between gamma correct and incorrect, you must allocate
	// the framebuffer as sRGB, then inhibit that processing when using normal
	// textures.
	VRAPI_FRAME_FLAG_INHIBIT_SRGB_FRAMEBUFFER					= 1,
	// Flush the warp swap pipeline so the images show up immediately.
	// This is expensive and should only be used when an immediate transition
	// is needed like displaying black when resetting the HMD orientation.
	VRAPI_FRAME_FLAG_FLUSH										= 2,
	// This is the final frame. Do not accept any more frames after this.
	VRAPI_FRAME_FLAG_FINAL										= 4,
	// Display continuously changing graph of TimeWarp timing data. By default,
	// this will display the start and end times of the draw.
	VRAPI_FRAME_FLAG_TIMEWARP_DEBUG_GRAPH_SHOW					= 8,
	// Continue to display the timing data, but no new data is collected and displayed.
	VRAPI_FRAME_FLAG_TIMEWARP_DEBUG_GRAPH_FREEZE				= 16,
	// Change the TimeWarp graph to display the latency (seconds from eye buffer
	// orientation time) instead of the draw times.
	VRAPI_FRAME_FLAG_TIMEWARP_DEBUG_GRAPH_LATENCY_MODE			= 32,
} ovrFrameFlags;

typedef enum
{
	// Enable writing to the alpha channel
	VRAPI_FRAME_LAYER_FLAG_WRITE_ALPHA								= 1,
	// Correct for chromatic aberration. Quality/perf trade-off.
	VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION	= 2,
	// Used for some HUDs, but generally considered bad practice.
	VRAPI_FRAME_LAYER_FLAG_FIXED_TO_VIEW							= 4,
	// Spin the layer - for loading icons
	VRAPI_FRAME_LAYER_FLAG_SPIN										= 8,
} ovrFrameLayerFlags;

typedef enum
{
	VRAPI_FRAME_LAYER_EYE_LEFT,
	VRAPI_FRAME_LAYER_EYE_RIGHT,
	VRAPI_FRAME_LAYER_EYE_MAX
} ovrFrameLayerEye;

typedef enum
{
	VRAPI_FRAME_LAYER_BLEND_ZERO,
	VRAPI_FRAME_LAYER_BLEND_ONE,
	VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA,
	VRAPI_FRAME_LAYER_BLEND_DST_ALPHA,
	VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_DST_ALPHA,
	VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA
} ovrFrameLayerBlend;

typedef enum
{
	VRAPI_FRAME_LAYER_TYPE_WORLD,
	VRAPI_FRAME_LAYER_TYPE_OVERLAY,
	VRAPI_FRAME_LAYER_TYPE_CURSOR,
	VRAPI_FRAME_LAYER_TYPE_USER,
	VRAPI_FRAME_LAYER_TYPE_MAX
} ovrFrameLayerType;

typedef enum
{
	VRAPI_EXTRA_LATENCY_MODE_OFF,
	VRAPI_EXTRA_LATENCY_MODE_ON,
	VRAPI_EXTRA_LATENCY_MODE_DYNAMIC
} ovrExtraLatencyMode;

// Note that any layer textures that are dynamic must be triple buffered.
typedef struct
{
	// Because OpenGL ES does not support clampToBorder, it is the
	// application's responsibility to make sure that all mip levels
	// of the primary eye texture have a black border that will show
	// up when time warp pushes the texture partially off screen.
	ovrTextureSwapChain *	ColorTextureSwapChain;

	// The depth texture is optional for positional time warp.
	ovrTextureSwapChain *	DepthTextureSwapChain;

	// Index to the texture from the set that should be displayed.
	int						TextureSwapChainIndex;

	// Points on the screen are mapped by a distortion correction
	// function into ( TanX, TanY, -1, 1 ) vectors that are transformed
	// by this matrix to get ( S, T, Q, _ ) vectors that are looked
	// up with texture2dproj() to get texels.
	ovrMatrix4f				TexCoordsFromTanAngles;

	// Only texels within this range should be drawn.
	// This is a sub-rectangle of the [(0,0)-(1,1)] texture coordinate range.
	ovrRectf				TextureRect;

	// The tracking state for which ModelViewMatrix is correct.
	// It is ok to update the orientation for each eye, which
	// can help minimize black edge pull-in, but the position
	// must remain the same for both eyes, or the position would
	// seem to judder "backwards in time" if a frame is dropped.
	ovrRigidBodyPosef		HeadPose;

	// If not zero, this fence will be used to determine whether or not
	// rendering to the color and depth texture swap chains has completed.
	unsigned long long		CompletionFence;
} ovrFrameLayerTexture;

typedef struct
{
	// Image used for each eye.
	ovrFrameLayerTexture	Textures[VRAPI_FRAME_LAYER_EYE_MAX];

	// Program-specific tuning values.
	float					ProgramParms[4];

	// Layer blend function.
	ovrFrameLayerBlend		SrcBlend;
	ovrFrameLayerBlend		DstBlend;

	// Combination of ovrFrameLayerFlags flags.
	int						Flags;
} ovrFrameLayer;

typedef struct
{
	// These are fixed clock levels in the range [0, 3].
	int						CpuLevel;
	int						GpuLevel;

	// These threads will get SCHED_FIFO.
	int						MainThreadTid;
	int						RenderThreadTid;
} ovrPerformanceParms;

typedef struct
{
	ovrStructureType		Type;

	// Layers composited in the time warp.
	ovrFrameLayer	 		Layers[VRAPI_FRAME_LAYER_TYPE_MAX];
	int						LayerCount;

	// Combination of ovrFrameFlags flags.
	int 					Flags;

	// Application controlled frame index that uniquely identifies this particular frame.
	// This must be the same frame index that was passed to vrapi_GetPredictedDisplayTime()
	// when synthesis of this frame started.
	long long				FrameIndex;

	// WarpSwap will not return until at least this many V-syncs have
	// passed since the previous WarpSwap returned.
	// Setting to 2 will reduce power consumption and may make animation
	// more regular for applications that can't hold full frame rate.
	int						MinimumVsyncs;

	// Latency Mode.
	ovrExtraLatencyMode		ExtraLatencyMode;

	// Rotation from a joypad can be added on generated frames to reduce
	// judder in FPS style experiences when the application framerate is
	// lower than the V-sync rate.
	// This will be applied to the view space distorted
	// eye vectors before applying the rest of the time warp.
	// This will only be added when the same ovrFrameParms is used for
	// more than one V-sync.
	ovrMatrix4f				ExternalVelocity;

	// jobject that will be updated before each eye for minimal
	// latency.
	// IMPORTANT: This should be a JNI weak reference to the object.
	// The system will try to convert it into a global reference before
	// calling SurfaceTexture->Update, which allows it to be safely
	// freed by the application.
	jobject					SurfaceTextureObject;

	// CPU/GPU performance parameters.
	ovrPerformanceParms		PerformanceParms;

	// For handling HMD events and power level state changes.
	ovrJava					Java;
} ovrFrameParms;

//-----------------------------------------------------------------
// Head Model
//-----------------------------------------------------------------

typedef struct
{
	float	InterpupillaryDistance;	// Distance between eyes.
	float	EyeHeight;				// Eye height relative to the ground.
	float	HeadModelDepth;			// Eye offset forward from the head center at EyeHeight.
	float	HeadModelHeight;		// Neck joint offset down from the head center at EyeHeight.
} ovrHeadModelParms;

//-----------------------------------------------------------------
// FIXME:VRAPI remove this once all simulation code uses VrFrame::PredictedDisplayTimeInSeconds and perf timing uses LOGCPUTIME
//-----------------------------------------------------------------

#if defined( __cplusplus )
extern "C" {
#endif
OVR_VRAPI_EXPORT double vrapi_GetTimeInSeconds();
#if defined( __cplusplus )
}	// extern "C"
#endif

#endif	// OVR_VrApi_Types_h
