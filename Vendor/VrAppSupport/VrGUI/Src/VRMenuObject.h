/************************************************************************************

Filename    :   VRMenuObject.h
Content     :   Menuing system for VR apps.
Created     :   May 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_VRMenuObject_h )
#define OVR_VRMenuObject_h

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_TypesafeNumber.h"
#include "Kernel/OVR_BitFlags.h"
#include "Kernel/OVR_GlUtils.h"	// GLuint
#include "Kernel/OVR_LogUtils.h"
#include "CollisionPrimitive.h"
#include "BitmapFont.h" // HorizontalJustification & VerticalJustification

namespace OVR {

class App;
class OvrVRMenuMgr;
class OvrGuiSys;

enum eGUIProgramType
{
	PROGRAM_DIFFUSE_ONLY,				// has a diffuse only
	PROGRAM_ADDITIVE_ONLY,
	PROGRAM_DIFFUSE_PLUS_ADDITIVE,		// has a diffuse and an additive
	PROGRAM_DIFFUSE_COLOR_RAMP,			// has a diffuse and color ramp, and color ramp target is the diffuse
	PROGRAM_DIFFUSE_COLOR_RAMP_TARGET,	// has diffuse, color ramp, and a separate color ramp target
	PROGRAM_DIFFUSE_COMPOSITE,			// has two diffuse, with the second composited onto the first using it's alpha mask
	PROGRAM_MAX							// some other combo not supported, or no texture maps at all
};

enum eVRMenuObjectType
{
	VRMENU_CONTAINER,	// a container for other controls
	VRMENU_STATIC,		// non-interactable item, may have text
	VRMENU_BUTTON,		// "clickable"
	VRMENU_MAX			// invalid
};

enum eVRMenuSurfaceImage
{
	VRMENUSURFACE_IMAGE_0,
	VRMENUSURFACE_IMAGE_1,
	VRMENUSURFACE_IMAGE_2,
	VRMENUSURFACE_IMAGE_MAX
};

enum eSurfaceTextureType
{
	SURFACE_TEXTURE_DIFFUSE,
	SURFACE_TEXTURE_ADDITIVE,
	SURFACE_TEXTURE_COLOR_RAMP,
	SURFACE_TEXTURE_COLOR_RAMP_TARGET,
	SURFACE_TEXTURE_MAX	// also use to indicate an uninitialized type
};

enum eVRMenuObjectFlags
{
	VRMENUOBJECT_FLAG_NO_FOCUS_GAINED,	// object will never get focus gained event
	VRMENUOBJECT_DONT_HIT_ALL,			// do not collide with object in hit testing
    VRMENUOBJECT_DONT_HIT_TEXT,         // do not collide with text in hit testing
    VRMENUOBJECT_HIT_ONLY_BOUNDS,       // only collide against the bounds, not the collision surface
    VRMENUOBJECT_BOUND_ALL,             // for hit testing, merge all bounds into a single AABB
	VRMENUOBJECT_FLAG_POLYGON_OFFSET,	// render with polygon offset
	VRMENUOBJECT_FLAG_NO_DEPTH,			// render without depth test
	VRMENUOBJECT_DONT_RENDER,			// don't render this object
	VRMENUOBJECT_DONT_RENDER_TEXT,		// don't render this object's text (useful for naming an object just for debugging)
	VRMENUOBJECT_NO_GAZE_HILIGHT,		// when hit this object won't change the cursor hilight state
	VRMENUOBJECT_RENDER_HIERARCHY_ORDER,	// this object and all its children will use this object's position for depth sorting, and sort vs. each other by submission index (i.e. parent's render first)
	VRMENUOBJECT_FLAG_BILLBOARD			// always face view plane normal
};

typedef BitFlagsT< eVRMenuObjectFlags > VRMenuObjectFlags_t;

enum eVRMenuObjectInitFlags
{
	VRMENUOBJECT_INIT_FORCE_POSITION	// use the position in the parms instead of an auto-calculated position
};

typedef BitFlagsT< eVRMenuObjectInitFlags > VRMenuObjectInitFlags_t;

enum eVRMenuId
{
	INVALID_MENU_ID = INT_MIN
};
typedef TypesafeNumberT< long long, eVRMenuId, INVALID_MENU_ID > VRMenuId_t;

// menu object handles
enum eMenuIdType
{
	INVALID_MENU_OBJECT_ID = 0
};
typedef OVR::TypesafeNumberT< uint64_t, eMenuIdType, INVALID_MENU_OBJECT_ID >	menuHandle_t;

// menu render flags
enum eVRMenuRenderFlags
{
	VRMENU_RENDER_NO_DEPTH,
	VRMENU_RENDER_NO_FONT_OUTLINE,
	VRMENU_RENDER_POLYGON_OFFSET,
	VRMENU_RENDER_BILLBOARD
};
typedef BitFlagsT< eVRMenuRenderFlags > VRMenuRenderFlags_t;

class VRMenuComponent;
class VRMenuComponent_OnFocusGained;
class VRMenuComponent_OnFocusLost;
class VRMenuComponent_OnDown;
class VRMenuComponent_OnUp;
class VRMenuComponent_OnSubmitForRendering;
class VRMenuComponent_OnRender;
class VRMenuComponent_OnTouchRelative;
class BitmapFont;
struct fontParms_t;

// border indices
enum eVRMenuBorder
{
	BORDER_LEFT,
	BORDER_BOTTOM,
	BORDER_RIGHT,
	BORDER_TOP
};

//==============================================================
// VRMenuSurfaceParms
class VRMenuSurfaceParms
{
public:
	VRMenuSurfaceParms() :
		SurfaceName( "" ),
		ImageNames(),
		Border( 0.0f, 0.0f, 0.0f, 0.0f ),
		Dims( 0.0f, 0.0f )

	{
		InitSurfaceTextureTypes();
	}

	explicit VRMenuSurfaceParms( char const * surfaceName ) :
		SurfaceName( surfaceName ),
		ImageNames(),
		Contents( CONTENT_SOLID ),
		Anchors( 0.5f, 0.5f ),
		Border( 0.0f, 0.0f, 0.0f, 0.0f ),
		Dims( 0.0f, 0.0f )

	{
		InitSurfaceTextureTypes();
	}
	VRMenuSurfaceParms( char const * surfaceName, 
			char const * imageName0,
			eSurfaceTextureType textureType0,
			char const * imageName1,
			eSurfaceTextureType textureType1,
			char const * imageName2,
			eSurfaceTextureType textureType2 ) :
		SurfaceName( surfaceName ),
		ImageNames(),
		Contents( CONTENT_SOLID ),
		Anchors( 0.5f, 0.5f ),
		Border( 0.0f, 0.0f, 0.0f, 0.0f ),
		Dims( 0.0f, 0.0f )

	{
		InitSurfaceTextureTypes();
		ImageNames[VRMENUSURFACE_IMAGE_0] = imageName0;
		TextureTypes[VRMENUSURFACE_IMAGE_0] = textureType0;

		ImageNames[VRMENUSURFACE_IMAGE_1] = imageName1;
		TextureTypes[VRMENUSURFACE_IMAGE_1] = textureType1;

		ImageNames[VRMENUSURFACE_IMAGE_2] = imageName2;
		TextureTypes[VRMENUSURFACE_IMAGE_2] = textureType2;		
	}
	VRMenuSurfaceParms( char const * surfaceName, 
			char const * imageName0,
			eSurfaceTextureType textureType0,
			char const * imageName1,
			eSurfaceTextureType textureType1,
			char const * imageName2,
			eSurfaceTextureType textureType2,
			Vector2f const & anchors ) :
		SurfaceName( surfaceName ),
		ImageNames(),
		Contents( CONTENT_SOLID ),
		Anchors( anchors ),
		Border( 0.0f, 0.0f, 0.0f, 0.0f ),
		Dims( 0.0f, 0.0f )

	{
		InitSurfaceTextureTypes();
		ImageNames[VRMENUSURFACE_IMAGE_0] = imageName0;
		TextureTypes[VRMENUSURFACE_IMAGE_0] = textureType0;

		ImageNames[VRMENUSURFACE_IMAGE_1] = imageName1;
		TextureTypes[VRMENUSURFACE_IMAGE_1] = textureType1;

		ImageNames[VRMENUSURFACE_IMAGE_2] = imageName2;
		TextureTypes[VRMENUSURFACE_IMAGE_2] = textureType2;		
	}
	VRMenuSurfaceParms( char const * surfaceName,
			char const * imageNames[],
			eSurfaceTextureType textureTypes[] ) :
		SurfaceName( surfaceName ),
		ImageNames(),
		TextureTypes(),
		Contents( CONTENT_SOLID ),
		Anchors( 0.5f, 0.5f ),
		Border( 0.0f, 0.0f, 0.0f, 0.0f ),
		Dims( 0.0f, 0.0f )

	{
		InitSurfaceTextureTypes();
		for ( int i = 0; i < VRMENUSURFACE_IMAGE_MAX && imageNames[i] != NULL; ++i )
		{
			ImageNames[i] = imageNames[i];
			TextureTypes[i] = textureTypes[i];
		}
	}
	VRMenuSurfaceParms( char const * surfaceName,
			GLuint imageTexId0,
			short width0,
			short height0,
			eSurfaceTextureType textureType0,
			GLuint imageTexId1,
			short width1,
			short height1,
			eSurfaceTextureType textureType1,
			GLuint imageTexId2,
			short width2,
			short height2,
			eSurfaceTextureType textureType2 ) :
		SurfaceName( surfaceName ),
		ImageNames(),
		Contents( CONTENT_SOLID ),
		Anchors( 0.5f, 0.5f ),
		Border( 0.0f, 0.0f, 0.0f, 0.0f ),
		Dims( 0.0f, 0.0f )

	{
		InitSurfaceTextureTypes();
		ImageTexId[VRMENUSURFACE_IMAGE_0] = imageTexId0;
		TextureTypes[VRMENUSURFACE_IMAGE_0] = textureType0;
		ImageWidth[VRMENUSURFACE_IMAGE_0] = width0;
		ImageHeight[VRMENUSURFACE_IMAGE_0] = height0;

		ImageTexId[VRMENUSURFACE_IMAGE_1] = imageTexId1;
		TextureTypes[VRMENUSURFACE_IMAGE_1] = textureType1;
		ImageWidth[VRMENUSURFACE_IMAGE_1] = width1;
		ImageHeight[VRMENUSURFACE_IMAGE_1] = height1;

		ImageTexId[VRMENUSURFACE_IMAGE_2] = imageTexId2;
		TextureTypes[VRMENUSURFACE_IMAGE_2] = textureType2;
		ImageWidth[VRMENUSURFACE_IMAGE_2] = width2;
		ImageHeight[VRMENUSURFACE_IMAGE_2] = height2;
    }

    String              SurfaceName;		// for debugging only
	String              ImageNames[VRMENUSURFACE_IMAGE_MAX];
	GLuint				ImageTexId[VRMENUSURFACE_IMAGE_MAX];
	int					ImageWidth[VRMENUSURFACE_IMAGE_MAX];
	int					ImageHeight[VRMENUSURFACE_IMAGE_MAX];
	eSurfaceTextureType	TextureTypes[VRMENUSURFACE_IMAGE_MAX];
	ContentFlags_t		Contents;
	Vector2f			Anchors;
	Vector4f			Border;						// if set to non-zero, sets the border on a sliced sprite
	Vector2f			Dims;						// if set to zero, use texture size, non-zero sets dims to absolute size

private:
	void InitSurfaceTextureTypes()
	{
		for ( int i = 0; i < VRMENUSURFACE_IMAGE_MAX; ++i )
		{
			TextureTypes[i] = SURFACE_TEXTURE_MAX;
			ImageTexId[i] = 0;
			ImageWidth[i] = 0;
			ImageHeight[i] = 0;
        }
    }
};

//==============================================================
// VRMenuFontParms
class VRMenuFontParms 
{
public:
	VRMenuFontParms() :
		AlignHoriz( HORIZONTAL_CENTER ),
		AlignVert( VERTICAL_CENTER ),
		Billboard( false ),
		TrackRoll( false ),
		Outline( true ),
		ColorCenter( 0.0f ),
		AlphaCenter( 0.5f ),
		Scale( 1.0f )
    {
    }

	VRMenuFontParms( bool const centerHoriz,
			bool const centerVert,
			bool const billboard,
			bool const trackRoll,
			bool const outline,
			float const colorCenter,
			float const alphaCenter,
			float const scale ) :
		AlignHoriz( centerHoriz ? HORIZONTAL_CENTER : HORIZONTAL_LEFT ),
		AlignVert( centerVert ? VERTICAL_CENTER : VERTICAL_BASELINE ),
		Billboard( billboard ),
		TrackRoll( trackRoll ),
		Outline( outline ),
		ColorCenter( colorCenter ),
		AlphaCenter( alphaCenter ),
		Scale( scale )
	{
	}

	VRMenuFontParms( bool const centerHoriz,
			bool const centerVert,
			bool const billboard,
			bool const trackRoll,
			bool const outline,
			float const scale ) :
		AlignHoriz( centerHoriz ? HORIZONTAL_CENTER : HORIZONTAL_LEFT ),
		AlignVert( centerVert ? VERTICAL_CENTER : VERTICAL_BASELINE ),
		Billboard( billboard ),
		TrackRoll( trackRoll ),
		Outline( outline ),
		ColorCenter( outline ? 0.5f : 0.0f ),
		AlphaCenter( outline ? 0.425f : 0.5f ),
		Scale( scale )
	{
	}

	VRMenuFontParms( HorizontalJustification const alignHoriz,
			VerticalJustification const alignVert,
			bool const billboard,
			bool const trackRoll,
			bool const outline,
			float const colorCenter,
			float const alphaCenter,
			float const scale ) :
		AlignHoriz( alignHoriz ),
		AlignVert( alignVert ),
		Billboard( billboard ),
		TrackRoll( trackRoll ),
		Outline( outline ),
		ColorCenter( colorCenter ),
		AlphaCenter( alphaCenter ),
		Scale( scale )
	{
	}

	VRMenuFontParms( HorizontalJustification const alignHoriz,
			VerticalJustification const alignVert,
			bool const billboard,
			bool const trackRoll,
			bool const outline,
			float const scale ) :
		AlignHoriz( alignHoriz ),
		AlignVert( alignVert ),
		Billboard( billboard ),
		TrackRoll( trackRoll ),
		Outline( outline ),
		ColorCenter( outline ? 0.5f : 0.0f ),
		AlphaCenter( outline ? 0.425f : 0.5f ),
		Scale( scale )
	{
	}

	HorizontalJustification	AlignHoriz;    	// horizontal justification around the specified x coordinate
    VerticalJustification	AlignVert;     	// vertical justification around the specified y coordinate
	bool	Billboard;		// true to always face the camera
	bool	TrackRoll;		// when billboarding, track with camera roll
	bool	Outline;		// true if font should rended with an outline
	float	ColorCenter;	// center value for color -- used for outlining
	float	AlphaCenter;	// center value for alpha -- used for outlined or non-outlined

	float	Scale;			// scale factor for the text
};

//==============================================================
// VRMenumObjectParms
//
// Parms passed to the VRMenuObject factory to create a VRMenuObject
class VRMenuObjectParms
{
public:
	VRMenuObjectParms(	eVRMenuObjectType const type, 
			Array< VRMenuComponent* > const & components,
			VRMenuSurfaceParms const & surfaceParms,
			char const * text,
			Posef const & localPose,
			Vector3f const & localScale,
			VRMenuFontParms const & fontParms,
			VRMenuId_t const id,
			VRMenuObjectFlags_t const flags,
			VRMenuObjectInitFlags_t const initFlags ) :
		Type( type ),
		Flags( flags ),
		InitFlags( initFlags ),
		Components( components ),
		SurfaceParms(),
		Text( text ),
		LocalPose( localPose ),
		LocalScale( localScale ),
        TextLocalPose( Quatf(), Vector3f( 0.0f ) ),
        TextLocalScale( 1.0f ),
		FontParms( fontParms ),
        Color( 1.0f ),
        TextColor( 1.0f ),
		Id( id ),
		Contents( CONTENT_SOLID )
	{
		SurfaceParms.PushBack( surfaceParms );
	}

    // same as above with additional text local parms
	VRMenuObjectParms(	eVRMenuObjectType const type, 
			Array< VRMenuComponent* > const & components,
			VRMenuSurfaceParms const & surfaceParms,
			char const * text,
			Posef const & localPose,
			Vector3f const & localScale,
            Posef const & textLocalPose,
            Vector3f const & textLocalScale,
			VRMenuFontParms const & fontParms,
			VRMenuId_t const id,
			VRMenuObjectFlags_t const flags,
			VRMenuObjectInitFlags_t const initFlags ) :
		Type( type ),
		Flags( flags ),
		InitFlags( initFlags ),
		Components( components ),
		SurfaceParms(),
		Text( text ),
		LocalPose( localPose ),
		LocalScale( localScale ),
        TextLocalPose( textLocalPose ),
        TextLocalScale( textLocalScale ),
		FontParms( fontParms ),
        Color( 1.0f ),
        TextColor( 1.0f ),
		Id( id ),
		Contents( CONTENT_SOLID )
    {
		SurfaceParms.PushBack( surfaceParms );
    }

    // takes an array of surface parms
	VRMenuObjectParms(	eVRMenuObjectType const type, 
			Array< VRMenuComponent* > const & components,
			Array< VRMenuSurfaceParms > const & surfaceParms,
			char const * text,
			Posef const & localPose,
			Vector3f const & localScale,
            Posef const & textLocalPose,
            Vector3f const & textLocalScale,
			VRMenuFontParms const & fontParms,
			VRMenuId_t const id,
			VRMenuObjectFlags_t const flags,
			VRMenuObjectInitFlags_t const initFlags ) :
		Type( type ),
		Flags( flags ),
		InitFlags( initFlags ),
		Components( components ),
		SurfaceParms( surfaceParms ),
		Text( text ),
		LocalPose( localPose ),
		LocalScale( localScale ),
        TextLocalPose( textLocalPose ),
        TextLocalScale( textLocalScale ),
		FontParms( fontParms ),
        Color( 1.0f ),
        TextColor( 1.0f ),
		Id( id ),
		Contents( CONTENT_SOLID )
    {
    }

	eVRMenuObjectType		    Type;							// type of menu object
	VRMenuObjectFlags_t		    Flags;							// bit flags
	VRMenuObjectInitFlags_t	    InitFlags;						// intialization-only flags (not referenced beyond object initialization)
    Array< VRMenuComponent* >   Components;						// list of pointers to components
	Array< VRMenuSurfaceParms >	SurfaceParms;					// list of surface parameters for the object. Each parm will result in one surface, and surfaces will render in the same order as this list.
	String      			    Text;							// text to display on this object (if any)
	Posef					    LocalPose;						// local-space position and orientation
	Vector3f				    LocalScale;						// local-space scale
    Posef                       TextLocalPose;                  // offset of text, local to surface
    Vector3f                    TextLocalScale;                 // scale of text, local to surface
	VRMenuFontParms			    FontParms;						// parameters for rendering the object's text
    Vector4f                    Color;                          // color modulation for surfaces
    Vector4f                    TextColor;                      // color of text
	VRMenuId_t				    Id;								// user identifier, so the client using a menu can find a particular object
	VRMenuId_t					ParentId;						// id of object that should be made this object's parent.
	ContentFlags_t				Contents;						// collision contents for the menu object
};

//==============================================================
// HitTestResult
// returns the results of a hit test vs menu objects
class HitTestResult : public OvrCollisionResult
{
public:
	HitTestResult &	operator=( OvrCollisionResult & rhs )
	{
		this->HitHandle.Release();
		this->RayStart = Vector3f::ZERO;
		this->RayDir = Vector3f::ZERO;
		this->t = rhs.t;
		this->uv = rhs.uv;
		return *this;
	}

	menuHandle_t	HitHandle;
	Vector3f		RayStart;
	Vector3f		RayDir;
};

typedef SInt16 guiIndex_t;

struct textMetrics_t {
	textMetrics_t() :
        w( 0.0f ),
        h( 0.0f ),
        ascent( 0.0f ),
		descent( 0.0f ),
		fontHeight( 0.0f )
	{
    }

	float w;
	float h;
	float ascent;
	float descent;
	float fontHeight;
};

class App;

//==============================================================
// VRMenuSurfaceTexture
class VRMenuSurfaceTexture
{
public:
	VRMenuSurfaceTexture();

	bool	LoadTexture( OvrGuiSys & guiSys, eSurfaceTextureType const type, char const * imageName, bool const allowDefault );
	void 	LoadTexture( eSurfaceTextureType const type, const GLuint texId, const int width, const int height );
	void	Free();
	void	SetOwnership( const bool isOwner )	{ OwnsTexture = isOwner; }

	GLuint				GetHandle() const { return Handle; }
	int					GetWidth() const { return Width; }
	int					GetHeight() const { return Height; }
	eSurfaceTextureType	GetType() const { return Type; }

private:
	GLuint				Handle;	// GL texture handle
	int					Width;	// width of the image
	int					Height;	// height of the image
	eSurfaceTextureType	Type;	// specifies how this image is used for rendering
    bool                OwnsTexture;    // if true, free texture on a reload or deconstruct
};

//==============================================================
// SubmittedMenuObject
class SubmittedMenuObject
{
public:
	SubmittedMenuObject() :
		SurfaceIndex( -1 ),
		DistanceIndex( -1 ),
		Scale( 1.0f ),
		Color( 1.0f ),
		ColorTableOffset( 0.0f ),
		SkipAdditivePass( false ),
		Offsets( 0.0f ),
		ClipUVs( 0.0f, 0.0f, 1.0f, 1.0f ),
		FadeDirection( 0.0f )
	{
	}

	menuHandle_t				Handle;				// handle of the object
	int							SurfaceIndex;		// surface of the object
	int							DistanceIndex;		// use the position at this index to calc sort distance
	Posef						Pose;				// pose in model space
	Vector3f					Scale;				// scale of the object
	Vector4f					Color;				// color of the object
	Vector2f					ColorTableOffset;	// color table offset for color ramp fx
	bool						SkipAdditivePass;	// true to skip any additive multi-texture pass
	VRMenuRenderFlags_t			Flags;				// various flags
	Vector2f					Offsets;			// offsets based on anchors (width / height * anchor.x / .y)
	Vector4f					ClipUVs;			// x,y are min clip uvs, z,w are max clip uvs
	Vector3f					FadeDirection;		// Fades vertices based on direction - default is zero vector which indicates off
#if defined( OVR_BUILD_DEBUG )
	String						SurfaceName;		// for debugging only
#endif
};


//==============================================================
// VRMenuSurface
class VRMenuSurface
{
public:
    // artificial bounds in Z, which is technically 0 for surfaces that are an image
    static const float Z_BOUNDS;

	VRMenuSurface();
	~VRMenuSurface();

	void							CreateFromSurfaceParms( OvrGuiSys & guiSys, VRMenuSurfaceParms const & parms );

	void							Free();

	void 							RegenerateSurfaceGeometry();

	void							Render( OvrVRMenuMgr const & menuMgr, Matrix4f const & mvp, SubmittedMenuObject const & sub ) const;

	Bounds3f const &				GetLocalBounds() const { return Tris.GetBounds(); }

	bool							IsRenderable() const { return ( SurfaceDef.geo.vertexCount > 0 && SurfaceDef.geo.indexCount > 0 ) && Visible; }

    bool							IntersectRay( Vector3f const & start, Vector3f const & dir, Posef const & pose,
									        Vector3f const & scale, ContentFlags_t const testContents, OvrCollisionResult & result ) const;
    // the ray should already be in local space
    bool							IntersectRay( Vector3f const & localStart, Vector3f const & localDir, 
									        Vector3f const & scale, ContentFlags_t const testContents, OvrCollisionResult & result ) const;
    void							LoadTexture( OvrGuiSys & guiSys, int const textureIndex, eSurfaceTextureType const type, 
									        const char * imageName );
    void							LoadTexture( int const textureIndex, eSurfaceTextureType const type, 
									        const GLuint texId, const int width, const int height );

	Vector4f const&					GetColor() const { return Color; }
	void							SetColor( Vector4f const & color ) { Color = color; }

	Vector2f const &				GetDims() const { return Dims; }
	void							SetDims( Vector2f const &dims ) { Dims = dims; } // requires call to CreateFromSurfaceParms or RegenerateSurfaceGeometry() to take effect

	Vector2f const &				GetAnchors() const { return Anchors; }
	void							SetAnchors( Vector2f const & a ) { Anchors = a; }

	Vector2f						GetAnchorOffsets() const;

	Vector4f const &				GetBorder() const { return Border; }
	void							SetBorder( Vector4f const & a ) { Border = a; }	// requires call to CreateFromSurfaceParms or RegenerateSurfaceGeometry() to take effect

	Vector4f const &				GetClipUVs() const { return ClipUVs; }
	void							SetClipUVs( Vector4f const & uvs ) { ClipUVs = uvs; }

	VRMenuSurfaceTexture const &	GetTexture( int const index )  const { return Textures[index]; }
	void							SetOwnership( int const index, bool const isOwner );
	
	bool							GetVisible() const { return Visible; }
	void							SetVisible( bool const v ) { Visible = v; }

	String const &					GetName() const { return SurfaceName; }

	void							BuildDrawSurface( OvrVRMenuMgr const & menuMgr, 
										Matrix4f const & modelMatrix,
										Matrix4f const & viewMatrix,
										Matrix4f const & projectionMatrix,
										Vector4f const & color, 
										Vector3f const & fadeDirection, 
										Vector2f const & colorTableOffset,
										Vector4f const & clipUVs,
										bool const skipAdditivePass, 
										VRMenuRenderFlags_t const & flags,
										ovrDrawSurface & outSurf ) const;

private:
	VRMenuSurfaceTexture			Textures[VRMENUSURFACE_IMAGE_MAX];
	//GlGeometry						Geo;				// VBO for this surface
	OvrTriCollisionPrimitive		Tris;				// per-poly collision object
	Vector4f						Color;				// Color, modulated with object color
	Vector2f						TextureDims;		// texture width and height
	Vector2f						Dims;				// width and height
	Vector2f						Anchors;			// anchors
	Vector4f						Border;				// border size for sliced sprite
	Vector4f						ClipUVs;			// UV boundaries for fragment clipping
	String      					SurfaceName;		// name of the surface for debugging
	ContentFlags_t					Contents;
	bool							Visible;			// must be true to render -- used to animate between different surfaces

	eGUIProgramType					ProgramType;

	mutable Matrix4f				ModelMatrix;		// transform of the current surface before applying view projection matrix
	mutable ovrSurfaceDef			SurfaceDef;

private:
	void							CreateImageGeometry(  int const textureWidth, int const textureHeight, const Vector2f &dims, const Vector4f &border, ContentFlags_t const content );
	// This searches the loaded textures for the specified number of matching types. For instance,
	// ( SURFACE_TEXTURE_DIFFUSE, 2 ) would only return true if two of the textures were of type
	// SURFACE_TEXTURE_DIFFUSE.  This is used to determine, based on surface types, which shaders
	// to use to render the surface, independent of how the texture maps are ordered.
	bool							HasTexturesOfType( eSurfaceTextureType const t, int const requiredCount ) const;
	// Returns the index in Textures[] of the n-th occurence of type t.
	int								IndexForTextureType( eSurfaceTextureType const t, int const occurenceCount ) const;
	void							SetTextureSampling( eGUIProgramType const pt );
};

//==============================================================
// VRMenuObject
// base class for all menu objects

class VRMenuObject
{
public:
	friend class VRMenuMgr;
	friend class VRMenuMgrLocal;

	static float const	TEXELS_PER_METER;
	static float const	DEFAULT_TEXEL_SCALE;

	// Initialize the object after creation
	void				Init( OvrGuiSys & guiSys, VRMenuObjectParms const & parms );

	// Frees all of this object's children
	void				FreeChildren( OvrVRMenuMgr & menuMgr );

	// Adds a child to this menu object
	void				AddChild( OvrVRMenuMgr & menuMgr, menuHandle_t const handle );
	// Removes a child from this menu object, but does not free the child object.
	void				RemoveChild( OvrVRMenuMgr & menuMgr, menuHandle_t const handle );
	// Removes a child from tis menu object and frees it, along with any children it may have.
	void				FreeChild( OvrVRMenuMgr & menuMgr, menuHandle_t const handle );
	// Returns true if the handle is in this menu's tree of children
	bool				IsDescendant( OvrVRMenuMgr & menuMgr, menuHandle_t const handle ) const;

	// Update this menu for a frame, including testing the gaze direction against the bounds
	// of this menu and all its children
	void				Frame( OvrVRMenuMgr & menuMgr, Matrix4f const & viewMatrix );

    // Tests a ray (in object's local space) for intersection.
    bool                IntersectRay( Vector3f const & localStart, Vector3f const & localDir, Vector3f const & parentScale, Bounds3f const & bounds,
								float & bounds_t0, float & bounds_t1, ContentFlags_t const testContents, 
								OvrCollisionResult & result ) const;

	// Test the ray against this object and all child objects, returning the first object that was 
	// hit by the ray. The ray should be in parent-local space - for the current root menu this is 
	// always world space.
	menuHandle_t		HitTest( OvrGuiSys & guiSys, Posef const & worldPose, 
								Vector3f const & rayStart, Vector3f const & rayDir, 
								ContentFlags_t const testContents, 
								HitTestResult & result ) const;

	//--------------------------------------------------------------
	// components
	//--------------------------------------------------------------
	void				AddComponent( VRMenuComponent * component );
	void				RemoveComponent( VRMenuComponent * component );

	Array< VRMenuComponent* > const & GetComponentList() const { return Components; }

	VRMenuComponent *	GetComponentById_Impl( int id ) const;
	VRMenuComponent *	GetComponentByName_Impl( const char * typeName ) const;

	// TODO We might want to refactor these into a single GetComponent which internally manages unique ids (using hashed class names for ex. ) 
	// Helper for getting component - returns NULL if it fails. Required Component class to overload GetTypeId and define unique TYPE_ID
	template< typename ComponentType >
	ComponentType*				GetComponentById() const
	{
		return static_cast< ComponentType* >( GetComponentById_Impl( ComponentType::TYPE_ID ) );
	}
	
	// Helper for getting component - returns NULL if it fails. Required Component class to overload GetTypeName and define unique TYPE_NAME
	template< typename ComponentType >
	ComponentType*				GetComponentByName() const
	{
		return static_cast< ComponentType* >( GetComponentByName_Impl( ComponentType::TYPE_NAME ) );
	}

	//--------------------------------------------------------------
	// accessors
	//--------------------------------------------------------------
	eVRMenuObjectType	GetType() const { return Type; }
	menuHandle_t		GetHandle() const { return Handle; }
	menuHandle_t		GetParentHandle() const { return ParentHandle; }
	void				SetParentHandle( menuHandle_t const h ) { ParentHandle = h; }

	VRMenuObjectFlags_t const &	GetFlags() const { return Flags; }
	void				SetFlags( VRMenuObjectFlags_t const & flags ) { Flags = flags; }
	void				AddFlags( VRMenuObjectFlags_t const & flags ) { Flags |= flags; }
	void				RemoveFlags( VRMenuObjectFlags_t const & flags ) { Flags &= ~flags; }

	OVR::String const &	GetText() const { return Text; }
	void				SetText( char const * text ) { Text = text; TextDirty = true; }
	void				SetTextWordWrapped( char const * text, class BitmapFont const & font, float const widthInMeters );

	bool				IsHilighted() const { return Hilighted; }
	void				SetHilighted( bool const b ) { Hilighted = b; }
	bool				IsSelected() const { return Selected; }
	void				SetSelected( bool const b ) { Selected = b; }
	int					NumChildren() const { return Children.GetSizeI(); }
	menuHandle_t		GetChildHandleForIndex( int const index ) const { return Children[index]; }

	Posef const &		GetLocalPose() const { return LocalPose; }
	void				SetLocalPose( Posef const & pose ) { LocalPose = pose; }
	Vector3f const &	GetLocalPosition() const { return LocalPose.Position; }
	void				SetLocalPosition( Vector3f const & pos ) { LocalPose.Position = pos; }
	Quatf const &		GetLocalRotation() const { return LocalPose.Orientation; }
	void				SetLocalRotation( Quatf const & rot ) { LocalPose.Orientation = rot; }
	Vector3f            GetLocalScale() const;
	void				SetLocalScale( Vector3f const & scale ) { LocalScale = scale; }

    Posef const &       GetHilightPose() const { return HilightPose; }
    void                SetHilightPose( Posef const & pose ) { HilightPose = pose; }
    float               GetHilightScale() const { return HilightScale; }
    void                SetHilightScale( float const s ) { HilightScale = s; }

    void                SetTextLocalPose( Posef const & pose ) { TextLocalPose = pose; }
    Posef const &       GetTextLocalPose() const { return TextLocalPose; }
    void                SetTextLocalPosition( Vector3f const & pos ) { TextLocalPose.Position = pos; }
    Vector3f const &    GetTextLocalPosition() const { return TextLocalPose.Position; }
    void                SetTextLocalRotation( Quatf const & rot ) { TextLocalPose.Orientation = rot; }
    Quatf const &       GetTextLocalRotation() const { return TextLocalPose.Orientation; }
    Vector3f            GetTextLocalScale() const;
    void                SetTextLocalScale( Vector3f const & scale ) { TextLocalScale = scale; }

	void				SetLocalBoundsExpand( Vector3f const mins, Vector3f const & maxs );

	Bounds3f			GetLocalBounds( BitmapFont const & font ) const;
    Bounds3f            GetTextLocalBounds( BitmapFont const & font ) const;

	Bounds3f const &	GetCullBounds() const { return CullBounds; }
	void				SetCullBounds( Bounds3f const & bounds ) const { CullBounds = bounds; }

	Vector2f const &	GetColorTableOffset() const;
	void				SetColorTableOffset( Vector2f const & ofs );

	Vector4f const &	GetColor() const;
	void				SetColor( Vector4f const & c );

    Vector4f const &	GetTextColor() const { return TextColor; }
    void				SetTextColor( Vector4f const & c ) { TextColor = c; }

	VRMenuId_t			GetId() const { return Id; }
	menuHandle_t		ChildHandleForId( OvrVRMenuMgr & menuMgr, VRMenuId_t const id ) const;

	void				SetFontParms( VRMenuFontParms const & fontParms ) { FontParms = fontParms; }
	VRMenuFontParms const & GetFontParms() const { return FontParms; }

	Vector3f const &	GetFadeDirection() const { return FadeDirection;  }
	void				SetFadeDirection( Vector3f const & dir ) { FadeDirection = dir;  }

	void				SetVisible( bool visible );

	// returns the index of the first surface with SURFACE_TEXTURE_ADDITIVE.
	// If singular is true, then the matching surface must have only one texture map and it must be of that type.
	int					FindSurfaceWithTextureType( eSurfaceTextureType const type, bool const singular ) const;
	int					FindSurfaceWithTextureType( eSurfaceTextureType const type, bool const singular, bool const visibleOnly ) const;	
	void				SetSurfaceColor( int const surfaceIndex, Vector4f const & color );
	Vector4f const &	GetSurfaceColor( int const surfaceIndex ) const;
	void				SetSurfaceVisible( int const surfaceIndex, bool const v );
	bool				GetSurfaceVisible( int const surfaceIndex ) const;
	int					NumSurfaces() const;
	int					AllocSurface();
	void 				CreateFromSurfaceParms( OvrGuiSys & guiSys, int const surfaceIndex, VRMenuSurfaceParms const & parms );

    void                SetSurfaceTexture( OvrGuiSys & guiSys, int const surfaceIndex, int const textureIndex, 
                                eSurfaceTextureType const type, char const * imageName );

    void                SetSurfaceTexture( int const surfaceIndex, int const textureIndex, 
                                eSurfaceTextureType const type, GLuint const texId, 
                                int const width, int const height );

	void                SetSurfaceTextureTakeOwnership( int const surfaceIndex, int const textureIndex,
								eSurfaceTextureType const type, GLuint const texId,
								int const width, int const height );

	void 				RegenerateSurfaceGeometry( int const surfaceIndex, const bool freeSurfaceGeometry );

	Vector2f const &	GetSurfaceDims( int const surfaceIndex ) const;
	void				SetSurfaceDims( int const surfaceIndex, Vector2f const &dims );

	Vector4f const &	GetSurfaceBorder( int const surfaceIndex );
	void				SetSurfaceBorder( int const surfaceIndex, Vector4f const & border );

	//--------------------------------------------------------------
	// collision
	//--------------------------------------------------------------
	void							SetCollisionPrimitive( OvrCollisionPrimitive * c );
	OvrCollisionPrimitive *			GetCollisionPrimitive() { return CollisionPrimitive; }
	OvrCollisionPrimitive const *	GetCollisionPrimitive() const { return CollisionPrimitive; }

	ContentFlags_t		GetContents() const { return Contents; }
	void				SetContents( ContentFlags_t const c ) { Contents = c; }

	//--------------------------------------------------------------
	// surfaces (non-virtual)
	//--------------------------------------------------------------
	VRMenuSurface const &			GetSurface( int const s ) const { return Surfaces[s]; }
	VRMenuSurface &					GetSurface( int const s ) { return Surfaces[s]; }
	Array< VRMenuSurface > const &	GetSurfaces() const { return Surfaces; }

	float							GetWrapWidth() const { return WrapWidth; }

	void							BuildDrawSurface( OvrVRMenuMgr const & menuMgr,
											Matrix4f const & modelMatrix, 
											Matrix4f const & viewMatrix,
											Matrix4f const & projectionMatrix,
											int const surfaceIndex,
											Vector4f const & color,
											Vector3f const & fadeDirection, 
											Vector2f const & colorTableOffset,
											Vector4f const & clipUVs,
											bool const skipAdditivePass, 
											VRMenuRenderFlags_t const & flags,
											Array< ovrDrawSurface > & surfaceList ) const;

private:
	eVRMenuObjectType			Type;			// type of this object
	menuHandle_t				Handle;			// handle of this object
	menuHandle_t				ParentHandle;	// handle of this object's parent
	VRMenuId_t					Id;				// opaque id that the creator of the menu can use to identify a menu object
	VRMenuObjectFlags_t			Flags;			// various bit flags
	Posef						LocalPose;		// local-space position and orientation
	Vector3f					LocalScale;		// local-space scale of this item
    Posef                       HilightPose;    // additional pose applied when hilighted
    float                       HilightScale;   // additional scale when hilighted
    Posef                       TextLocalPose;  // local-space position and orientation of text, local to this node (i.e. after LocalPose / LocalScale are applied)
    Vector3f                    TextLocalScale; // local-space scale of the text at this node
	OVR::String					Text;			// text to display on this object
	Array< menuHandle_t >		Children;		// array of direct children of this object
	Array< VRMenuComponent* >	Components;		// array of components on this object
	OvrCollisionPrimitive *		CollisionPrimitive;		// collision surface, if any
	ContentFlags_t				Contents;		// content flags for this object

	// may be cleaner to put texture and geometry in a separate surface structure
	Array< VRMenuSurface >		Surfaces;
	Vector4f					Color;				// color modulation
    Vector4f                    TextColor;          // color modulation for text
	Vector2f					ColorTableOffset;	// offset for color-ramp shader fx
	VRMenuFontParms				FontParms;			// parameters for text rendering
	Vector3f                    FadeDirection;		// Fades vertices based on direction - default is zero vector which indicates off

	bool						Hilighted;		// true if hilighted
	bool						Selected;		// true if selected
    mutable bool                TextDirty;      // if true, recalculate text bounds

    // cached state
	Vector3f					MinsBoundsExpand;	// amount to expand local bounds mins
	Vector3f					MaxsBoundsExpand;	// amount to expand local bounds maxs
	mutable Bounds3f			CullBounds;			// bounds of this object and all its children in the local space of its parent
    mutable textMetrics_t       TextMetrics;		// cached metrics for the text

	float						WrapWidth;

private:
	// only VRMenuMgrLocal static methods can construct and destruct a menu object.
	VRMenuObject( VRMenuObjectParms const & parms, menuHandle_t const handle );
	~VRMenuObject();

	// Render the specified surface.
	void						RenderSurface( OvrVRMenuMgr const & menuMgr, Matrix4f const & mvp, 
                                        SubmittedMenuObject const & sub ) const;

	bool						IntersectRayBounds( Vector3f const & start, Vector3f const & dir,
										Vector3f const & mins, Vector3f const & maxs,
										ContentFlags_t const testContents, float & t0, float & t1 ) const;

	// Test the ray against this object and all child objects, returning the first object that was 
	// hit by the ray. The ray should be in parent-local space - for the current root menu this is 
	// always world space.
	bool						HitTest_r( OvrGuiSys & guiSys, Posef const & parentPose, Vector3f const & parentScale,
                                        Vector3f const & rayStart, Vector3f const & rayDir,  ContentFlags_t const testContents, 
                                        HitTestResult & result ) const;

	int							GetComponentIndex( VRMenuComponent * component ) const;
};

} // namespace OVR

#endif // OVR_VRMenuObject_h
