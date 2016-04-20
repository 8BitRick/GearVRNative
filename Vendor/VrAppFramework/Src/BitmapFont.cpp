/************************************************************************************

Filename    :   BitmapFont.cpp
Content     :   Monospaced bitmap font rendering intended for debugging only.
Created     :   March 11, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

// TODO:
// - add support for multiple fonts per surface using texture arrays (store texture in 3rd texture coord)
// - in-world text really should sort with all other transparent surfaces
//

#include "BitmapFont.h"

#if defined( OVR_OS_WIN32 )
#include <intrin.h>
#endif

#include <errno.h>
#include <math.h>
#include <sys/stat.h>

#include "Kernel/OVR_UTF8Util.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_JSON.h"
#include "Kernel/OVR_GlUtils.h"
#include "Kernel/OVR_LogUtils.h"

#include "GlProgram.h"
#include "GlTexture.h"
#include "GlGeometry.h"
#include "VrCommon.h"
#include "PackageFiles.h"
#include "OVR_FileSys.h"
#include "OVR_Uri.h"


namespace OVR {

char const* FontSingleTextureVertexShaderSrc =
	"uniform mat4 Mvpm;\n"
	"uniform lowp vec4 UniformColor;\n"
	"attribute vec4 Position;\n"
	"attribute vec2 TexCoord;\n"
	"attribute vec4 VertexColor;\n"
	"attribute vec4 FontParms;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying lowp vec4 oColor;\n"
	"varying vec4 oFontParms;\n"
	"void main()\n"
	"{\n"
	"    gl_Position = Mvpm * Position;\n"
	"    oTexCoord = TexCoord;\n"
	"    oColor = UniformColor * VertexColor;\n"
	"    oFontParms = FontParms;\n"
	"}\n";

// Use derivatives to make the faded color and alpha boundaries a
// consistent thickness regardless of font scale.
char const* SDFFontFragmentShaderSrc =
	"#extension GL_OES_standard_derivatives : require\n"
	"uniform sampler2D Texture0;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying lowp vec4 oColor;\n"
	"varying mediump vec4 oFontParms;\n"
	"void main()\n"
	"{\n"
	"    mediump float distance = texture2D( Texture0, oTexCoord ).r;\n"
	"    mediump float ds = oFontParms.z * 255.0;\n"
	"	 mediump float dd = fwidth( oTexCoord.x ) * 8.0 * ds;\n"
	"    mediump float ALPHA_MIN = oFontParms.x - dd;\n"
	"    mediump float ALPHA_MAX = oFontParms.x + dd;\n"
	"    mediump float COLOR_MIN = oFontParms.y - dd;\n"
	"    mediump float COLOR_MAX = oFontParms.y + dd;\n"
	"	gl_FragColor.xyz = ( oColor * ( clamp( distance, COLOR_MIN, COLOR_MAX ) - COLOR_MIN ) / ( COLOR_MAX - COLOR_MIN ) ).xyz;\n"
	"	gl_FragColor.w = oColor.w * ( clamp( distance, ALPHA_MIN, ALPHA_MAX ) - ALPHA_MIN ) / ( ALPHA_MAX - ALPHA_MIN );\n"
	"}\n";

class FontGlyphType
{
public:
	FontGlyphType() :
		CharCode( 0 ),
		X( 0.0f ),
		Y( 0.0f ),
		Width( 0.0f ),
		Height( 0.0f ),
		AdvanceX( 0.0f ),
		AdvanceY( 0.0f ),
		BearingX( 0.0f ),
		BearingY( 0.0f )
	{
	}

	int32_t		CharCode;
	float		X;
	float		Y;
	float		Width;
	float		Height;
	float		AdvanceX;
	float		AdvanceY;
	float		BearingX;
	float		BearingY;
};


class FontInfoType
{
public:
	static const int FNT_FILE_VERSION;

	// This is used to scale the UVs to world units that work with the current scale values used throughout
	// the native code. Unfortunately the original code didn't account for the image size before factoring
	// in the user scale, so this keeps everything the same.
	static const float DEFAULT_SCALE_FACTOR;

	FontInfoType() :
		NaturalWidth( 0.0f ),
		NaturalHeight( 0.0f ),
		HorizontalPad( 0 ),
		VerticalPad( 0 ),
		FontHeight( 0 ),
		ScaleFactorX( 1.0f ),
		ScaleFactorY( 1.0f ),
		TweakScale( 1.0f ),
		CenterOffset( 0.0f ),
		MaxAscent( 0.0f ),
		MaxDescent( 0.0f )
	{
	}

	bool						Load( ovrFileSys & fileSys, char const * uri );
	bool						Load( OvrApkFile const & languagePackageFile, char const * fileName );
	FontGlyphType const &		GlyphForCharCode( uint32_t const charCode ) const;

	String						FontName;		// name of the font (not necessarily the file name)
	String						CommandLine;	// command line used to generate this font
	String						ImageFileName;	// the file name of the font image
	float						NaturalWidth;	// width of the font image before downsampling to SDF
	float						NaturalHeight;	// height of the font image before downsampling to SDF
	float						HorizontalPad;	// horizontal padding for all glyphs
	float						VerticalPad;	// vertical padding for all glyphs
	float						FontHeight;		// vertical distance between two baselines (i.e. two lines of text)
	float						ScaleFactorX;	// x-axis scale factor
	float						ScaleFactorY;	// y-axis scale factor
	float						TweakScale;		// additional scale factor used to tweak the size of other-language fonts
	float						CenterOffset;	// +/- value applied to "center" distance in the signed distance field. Range [-1,1]. A negative offset will make the font appear bolder.
	float						MaxAscent;		// maximum ascent of any character
	float						MaxDescent;		// maximum descent of any character
	OVR::Array< FontGlyphType >	Glyphs;			// info about each glyph in the font
	OVR::Array< int32_t >		CharCodeMap;	// index by character code to get the index of a glyph for the character

private:
	bool						LoadFromPackage( void* packageFile, char const * fileName );
	bool						LoadFromBuffer( void const * buffer, size_t const bufferSize );
};

const int FontInfoType::FNT_FILE_VERSION = 1;	// initial version storing pixel locations and scaling post/load to fix some precision loss
// for now, we're not going to increment this so that we're less likely to have dependency issues with loading the font from Home
// const int FontInfoType::FNT_FILE_VERSION = 2;		// added TweakScale for manual adjustment of other-language fonts
const float FontInfoType::DEFAULT_SCALE_FACTOR = 512.0f;

class BitmapFontLocal : public BitmapFont
{
public:
	BitmapFontLocal() :
		Texture( 0 ),
		ImageWidth( 0 ),
		ImageHeight( 0 ) {
	}
	~BitmapFontLocal() {
		if ( Texture != 0 ) {
			glDeleteTextures( 1, &Texture );
		}
		Texture = 0;
	}

	virtual bool   			Load( ovrFileSys & fileSys, const char * uri );
	virtual bool   			Load( const char * languagePackageFileName, char const * fontInfoFileName );

	// Calculates the native (unscaled) width of the text string. Line endings are ignored.
	virtual float			CalcTextWidth( char const * text ) const;

	// Calculates the native (unscaled) width of the text string. Each '\n' will start a new line
	// and will increase the height by FontInfo.FontHeight. For multi-line strings, lineWidths will
	// contain the width of each individual line of text and width will be the width of the widest
	// line of text.
	virtual void			CalcTextMetrics( char const * text, size_t & len,
									float & width, float & height,
									float & ascent, float & descent, float & fontHeight,
									float * lineWidths, int const maxLines, int & numLines ) const;


	void					WordWrapText( String & inOutText, const float widthMeters, const float fontScale = 1.0f ) const;
	void					WordWrapText( String & inOutText, const float widthMeters, OVR::Array< OVR::String > wholeStrsList, const float fontScale = 1.0f ) const;

	// Returns a newly allocated GlGeometry for the text, allowing it to be sorted
	// or transformed with more control than the global BitmapFontSurface.
	//
	// The GlGeometry must be freed, but the GlProgram is shared by all users of the font.
	virtual ovrSurfaceDef 	TextSurface( const char * text,
			float scale, const Vector4f & color, HorizontalJustification hjust,
			VerticalJustification vjust );

	FontGlyphType const &	GlyphForCharCode( uint32_t const charCode ) const { return FontInfo.GlyphForCharCode( charCode ); }
    virtual Vector2f        GetScaleFactor() const { return Vector2f( FontInfo.ScaleFactorX, FontInfo.ScaleFactorY ); }
    virtual void            GetGlyphMetrics( const uint32_t charCode, float &width,
                                             float &height, float &advancex,
                                             float &advancey ) const ;
	
	FontInfoType const &	GetFontInfo() const { return FontInfo; }
	const GlProgram &		GetFontProgram() const { return FontProgram; }
	int     				GetImageWidth() const { return ImageWidth; }
	int     				GetImageHeight() const { return ImageHeight; }
	GLuint  				GetTexture() const { return Texture; }

private:
	FontInfoType			FontInfo;
	GLuint      			Texture;
	int         			ImageWidth;
	int         			ImageHeight;

	GlProgram				FontProgram;

private:
	bool					LoadImage( ovrFileSys & fileSys, char const * uri );
	bool   					LoadImage( OvrApkFile const & languagePackageFile, char const * imageName );
	bool   					LoadImageFromBuffer( char const * imageName, unsigned char const * buffer,
									size_t const bufferSize, bool const isASTC );
	bool					LoadFontInfo( char const * glyphFileName );
	bool					LoadFontInfoFromBuffer( unsigned char const * buffer, size_t const bufferSize );
};

// We cast BitmapFont to BitmapFontLocal internally so that we do not have to expose
// a lot of BitmapFontLocal methods in the BitmapFont interface just so BitmapFontSurfaceLocal
// can use them. This problem comes up because BitmapFontSurface specifies the interface as
// taking BitmapFont as a parameter, not BitmapFontLocal. This is safe right now because
// we know that BitmapFont cannot be instantiated, nor is there any class derived from it other
// than BitmapFontLocal.
static BitmapFontLocal const &  AsLocal( BitmapFont const & font ) { return *static_cast< BitmapFontLocal const* >( &font ); }

struct fontVertex_t {
	fontVertex_t() :
		xyz( 0.0f ),
		s( 0.0f ),
		t( 0.0f ),
		rgba(),
		fontParms() {
	}

	Vector3f	xyz;
	float		s;
	float		t;
	UByte		rgba[4];
	UByte		fontParms[4];
};

typedef unsigned short fontIndex_t;

//==============================
// ftoi
#if defined( OVR_CPU_X86_64 )
inline int ftoi( float const f )
{
	return _mm_cvtt_ss2si( _mm_set_ss( f ) );
}
#elif defined( OVR_CPU_x86 )
inline int ftoi( float const f )
{
	int i;
	__asm
	{
		fld f
		fistp i
	}
	return i;
}
#else
inline int ftoi( float const f )
{
	return (int)f;
}
#endif

//==============================
// ColorToABGR
int32_t ColorToABGR( Vector4f const & color )
{
	// format is ABGR
	return  ( ftoi( color.w * 255.0f ) << 24 ) |
			( ftoi( color.z * 255.0f ) << 16 ) |
			( ftoi( color.y * 255.0f ) << 8 ) |
			ftoi( color.x * 255.0f );
}



// The vertices in a vertex block are in local space and pre-scaled.  They are transformed into
// world space and stuffed into the VBO before rendering (once the current MVP is known).
// The vertices can be pivoted around the Pivot point to face the camera, then an additional
// rotation applied.
class VertexBlockType
{
public:
	VertexBlockType() :
		Font( NULL ),
		Verts( NULL ),
		NumVerts( 0 ),
		Pivot( 0.0f ),
		Rotation(),
		Billboard( true ),
		TrackRoll( false )
	{
	}

	VertexBlockType( VertexBlockType const & other ) :
		Font( NULL ),
		Verts( NULL ),
		NumVerts( 0 ),
		Pivot( 0.0f ),
		Rotation(),
		Billboard( true ),
		TrackRoll( false )
	{
		Copy( other );
	}

	VertexBlockType & operator=( VertexBlockType const & other )
	{
		Copy( other );
		return *this;
	}

	void Copy( VertexBlockType const & other )
	{
		if ( &other == this )
		{
			return;
		}
		delete [] Verts;
		Font		= other.Font;
		Verts		= other.Verts;
		NumVerts	= other.NumVerts;
		Pivot		= other.Pivot;
		Rotation	= other.Rotation;
		Billboard	= other.Billboard;
		TrackRoll	= other.TrackRoll;

		other.Font = NULL;
		other.Verts = NULL;
		other.NumVerts = 0;
	}

	VertexBlockType( BitmapFont const & font, int const numVerts, Vector3f const & pivot,
			Quatf const & rot, bool const billboard, bool const trackRoll ) :
		Font( &font ),
		NumVerts( numVerts ),
		Pivot( pivot ),
		Rotation( rot ),
		Billboard( billboard ),
		TrackRoll( trackRoll )
	{
		Verts = new fontVertex_t[numVerts];
	}

	~VertexBlockType()
	{
		Free();
	}

	void Free()
	{
		Font = NULL;
		delete [] Verts;
		Verts = NULL;
		NumVerts = 0;
	}

	mutable BitmapFont const *	Font;		// the font used to render text into this vertex block
	mutable fontVertex_t *		Verts;		// the vertices
	mutable int					NumVerts;	// the number of vertices in the block
	Vector3f					Pivot;		// postion this vertex block can be rotated around
	Quatf						Rotation;	// additional rotation to apply
	bool						Billboard;	// true to always face the camera
	bool						TrackRoll;	// if true, when billboarded, roll with the camera
};

// Sets up VB and VAO for font drawing
GlGeometry	FontGeometry( int maxQuads )
{
	GlGeometry Geo;

	Geo.indexCount = maxQuads * 6;
	Geo.vertexCount = maxQuads * 4;

	// font VAO
	glGenVertexArrays( 1, &Geo.vertexArrayObject );
	glBindVertexArray( Geo.vertexArrayObject );

	// vertex buffer
	const int vertexByteCount = Geo.vertexCount * sizeof( fontVertex_t );
	glGenBuffers( 1, &Geo.vertexBuffer );
	glBindBuffer( GL_ARRAY_BUFFER, Geo.vertexBuffer );
	glBufferData( GL_ARRAY_BUFFER, vertexByteCount, NULL, GL_DYNAMIC_DRAW );

	glEnableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_POSITION ); // x, y and z
	glVertexAttribPointer( VERTEX_ATTRIBUTE_LOCATION_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( fontVertex_t ), (void*)0 );

	glEnableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_UV0 ); // s and t
	glVertexAttribPointer( VERTEX_ATTRIBUTE_LOCATION_UV0, 2, GL_FLOAT, GL_FALSE, sizeof( fontVertex_t ), (void*)offsetof( fontVertex_t, s ) );

	glEnableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_COLOR ); // color
	glVertexAttribPointer( VERTEX_ATTRIBUTE_LOCATION_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof( fontVertex_t ), (void*)offsetof( fontVertex_t, rgba ) );

	glDisableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_UV1 );

	glEnableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_FONT_PARMS );	// outline parms
	glVertexAttribPointer( VERTEX_ATTRIBUTE_LOCATION_FONT_PARMS, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof( fontVertex_t ), (void*)offsetof( fontVertex_t, fontParms ) );

	fontIndex_t * indices = new fontIndex_t[ Geo.indexCount ];
	const int indexByteCount = Geo.indexCount * sizeof( fontIndex_t );

	// indices never change
	fontIndex_t v = 0;
	for ( int i = 0; i < maxQuads; i++ )
	{
		indices[i * 6 + 0] = v + 2;
		indices[i * 6 + 1] = v + 1;
		indices[i * 6 + 2] = v + 0;
		indices[i * 6 + 3] = v + 3;
		indices[i * 6 + 4] = v + 2;
		indices[i * 6 + 5] = v + 0;
		v += 4;
	}

	glGenBuffers( 1, &Geo.indexBuffer );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, Geo.indexBuffer );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, indexByteCount, (void*)indices, GL_STATIC_DRAW );

	glBindVertexArray( 0 );

	delete [] indices;

	return Geo;
}

//==============================
// DrawText3D
VertexBlockType DrawTextToVertexBlock( BitmapFont const & font, fontParms_t const & parms,
		Vector3f const & pos, Vector3f const & normal, Vector3f const & up,
		float scale, Vector4f const & color, char const * text )
{
	if ( text == NULL || text[0] == '\0' )
	{
		return VertexBlockType();	// nothing to do here, move along
	}

	// TODO: multiple line support -- we would need to calculate the horizontal width
	// for each string ending in \n
	size_t len;
	float width;
	float height;
	float ascent;
	float descent;
	float fontHeight;
	int const MAX_LINES = 128;
	float lineWidths[MAX_LINES];
	int numLines;
	AsLocal( font ).CalcTextMetrics( text, len, width, height, ascent, descent, fontHeight, lineWidths, MAX_LINES, numLines );
//	LOG( "BitmapFontSurfaceLocal::DrawText3D( \"%s\" %s %s ) : width = %.2f, height = %.2f, numLines = %i, fh = %.2f",
//			text, ( parms.AlignVert == VERTICAL_CENTER ) ? "cv" : ( ( parms.AlignVert == VERTICAL_BOTTOM ) ? "bv" : "tv" ),
// 			( parms.AlignHoriz == HORIZONTAL_CENTER ) ? "ch" : ( ( parms.AlignVert == HORIZONTAL_LEFT ) ? "lh" : "rh" ),
//			width, height, numLines, AsLocal( font ).GetFontInfo().FontHeight );
	if ( len == 0 )
	{
		return VertexBlockType();
	}

	if ( !normal.IsNormalized() )
	{
		LOG( "DrawTextToVertexBlock: normal = ( %g, %g, %g ), text = '%s'", normal.x, normal.y, normal.z, text );
		ASSERT_WITH_TAG( normal.IsNormalized(), "BitmapFont" );
	}
	if ( !up.IsNormalized() )
	{
		LOG( "DrawTextToVertexBlock: up = ( %g, %g, %g ), text = '%s'", up.x, up.y, up.z, text );
		ASSERT_WITH_TAG( up.IsNormalized(), "BitmapFont" );
	}

	const FontInfoType & fontInfo = AsLocal( font ).GetFontInfo();

	float imageWidth = (float)AsLocal( font ).GetImageWidth();
	float const xScale = AsLocal( font ).GetFontInfo().ScaleFactorX * scale;
	float const yScale = AsLocal( font ).GetFontInfo().ScaleFactorY * scale;

	// allocate a vertex block
	size_t numVerts = 4 * len;
	VertexBlockType vb( font, numVerts, pos, Quatf(), parms.Billboard, parms.TrackRoll );

	Vector3f const right = up.Cross( normal );
	Vector3f const r = ( parms.Billboard ) ? Vector3f( 1.0f, 0.0f, 0.0f ) : right;
	Vector3f const u = ( parms.Billboard ) ? Vector3f( 0.0f, 1.0f, 0.0f ) : up;

	Vector3f curPos( 0.0f );
	switch( parms.AlignVert )
	{
		case VERTICAL_BASELINE :
			break;

		case VERTICAL_CENTER :
		{
			float const vofs = ( height * 0.5f ) - ascent;
			curPos += u * ( vofs * scale );
			break;
		}
		case VERTICAL_CENTER_FIXEDHEIGHT :
		{
			// for fixed height, we must adjust single-line text by the max ascent because fonts
			// are rendered according to their baseline. For multiline text, the first line
			// contributes max ascent only while the other lines are adjusted by font height.
			float const ma = AsLocal( font ).GetFontInfo().MaxAscent;
			float const md = AsLocal( font ).GetFontInfo().MaxDescent;
			float const fh = AsLocal( font ).GetFontInfo().FontHeight;
			float const adjust = ( ma - md ) * 0.5f;
			float const vofs = ( fh * ( numLines - 1 ) * 0.5f ) - adjust;
			curPos += u * ( vofs * yScale );
			break;
		}
		case VERTICAL_TOP :
		{
			float const vofs = height - ascent;
			curPos += u * ( vofs * scale );
			break;
		}
	}

	Vector3f basePos = curPos;
	switch( parms.AlignHoriz )
	{
		case HORIZONTAL_LEFT :
			break;

		case HORIZONTAL_CENTER :
		{
			curPos -= r * ( lineWidths[0] * 0.5f * scale );
			break;
		}
		case HORIZONTAL_RIGHT :
		{
			curPos -= r * ( lineWidths[0] * scale );
			break;
		}
	}

	Vector3f lineInc = u * ( fontInfo.FontHeight * yScale );
	float const distanceScale = imageWidth / FontInfoType::DEFAULT_SCALE_FACTOR;
	const uint8_t fontParms[4] =
	{
			(uint8_t)( OVR::Alg::Clamp( parms.AlphaCenter + fontInfo.CenterOffset, 0.0f, 1.0f ) * 255 ),
			(uint8_t)( OVR::Alg::Clamp( parms.ColorCenter + fontInfo.CenterOffset, 0.0f, 1.0f ) * 255 ),
			(uint8_t)( OVR::Alg::Clamp( distanceScale, 1.0f, 255.0f ) ),
			0
	};

    int iColor = ColorToABGR( color );

	int curLine = 0;
	fontVertex_t * v = vb.Verts;
	char const * p = text;
	size_t i = 0;
	uint32_t charCode = UTF8Util::DecodeNextChar( &p );
	for ( ; charCode != '\0'; i++, charCode = UTF8Util::DecodeNextChar( &p ) )
	{
		OVR_ASSERT( i < len );
		if ( charCode == '\n' && curLine < numLines && curLine < MAX_LINES )
		{
			// move to next line
			curLine++;
			basePos -= lineInc;
			curPos = basePos;
			switch( parms.AlignHoriz )
			{
				case HORIZONTAL_LEFT :
					break;

				case HORIZONTAL_CENTER :
				{
					curPos -= r * ( lineWidths[curLine] * 0.5f * scale );
					break;
				}
				case HORIZONTAL_RIGHT :
				{
					curPos -= r * ( lineWidths[curLine] * scale );
					break;
				}
			}
		}

		FontGlyphType const & g = AsLocal( font ).GlyphForCharCode( charCode );

		float s0 = g.X;
		float t0 = g.Y;
		float s1 = ( g.X + g.Width );
		float t1 = ( g.Y + g.Height );

		float bearingX = g.BearingX * xScale;
		float bearingY = g.BearingY * yScale ;

		float rw = ( g.Width + g.BearingX ) * xScale;
		float rh = ( g.Height - g.BearingY ) * yScale;

        // lower left
        v[i * 4 + 0].xyz = curPos + ( r * bearingX ) - ( u * rh );
        v[i * 4 + 0].s = s0;
        v[i * 4 + 0].t = t1;
        *(UInt32*)(&v[i * 4 + 0].rgba[0]) = iColor;
		*(UInt32*)(&v[i * 4 + 0].fontParms[0]) = *(UInt32*)(&fontParms[0]);
	    // upper left
        v[i * 4 + 1].xyz = curPos + ( r * bearingX ) + ( u * bearingY );
        v[i * 4 + 1].s = s0;
        v[i * 4 + 1].t = t0;
        *(UInt32*)(&v[i * 4 + 1].rgba[0]) = iColor;
		*(UInt32*)(&v[i * 4 + 1].fontParms[0]) = *(UInt32*)(&fontParms[0]);
        // upper right
        v[i * 4 + 2].xyz = curPos + ( r * rw ) + ( u * bearingY );
        v[i * 4 + 2].s = s1;
        v[i * 4 + 2].t = t0;
        *(UInt32*)(&v[i * 4 + 2].rgba[0]) = iColor;
		*(UInt32*)(&v[i * 4 + 2].fontParms[0]) = *(UInt32*)(&fontParms[0]);
        // lower right
        v[i * 4 + 3].xyz = curPos + ( r * rw ) - ( u * rh );
        v[i * 4 + 3].s = s1;
        v[i * 4 + 3].t = t1;
        *(UInt32*)(&v[i * 4 + 3].rgba[0]) = iColor;
		*(UInt32*)(&v[i * 4 + 3].fontParms[0]) = *(UInt32*)(&fontParms[0]);
		// advance to start of next char
		curPos += r * ( g.AdvanceX * xScale );
	}

	return vb;
}


ovrSurfaceDef BitmapFontLocal::TextSurface( const char * text,
		float scale, const Vector4f & color, HorizontalJustification hjust,
		VerticalJustification vjust )
{
	fontParms_t	fp;
	fp.AlignHoriz = hjust;
	fp.AlignVert = vjust;
	VertexBlockType	vb = DrawTextToVertexBlock( *this,
			fp,
			Vector3f( 0.0f ),				// origin
			Vector3f( 0.0f, 0.0f, 1.0f ),	// normal
			Vector3f( 0.0f, 1.0f, 0.0f ),	// up
			scale,
			color,
			text );

	ovrSurfaceDef s;

	s.cullingBounds.Clear();
	for ( int i = 0 ; i < vb.NumVerts ; i++ )
	{
		s.cullingBounds.AddPoint( vb.Verts[i].xyz );
	}
	s.geo = FontGeometry( vb.NumVerts / 4 );

	glBindVertexArray( s.geo.vertexArrayObject );
	glBindBuffer( GL_ARRAY_BUFFER, s.geo.vertexBuffer );
	glBufferSubData( GL_ARRAY_BUFFER, 0, vb.NumVerts * sizeof( fontVertex_t ), (void *)vb.Verts );
	glBindVertexArray( 0 );

	vb.Free();

	// Special blend mode to also work over underlay layers
	s.materialDef.gpuState.blendEnable = ovrGpuState::BLEND_ENABLE_SEPARATE;
	s.materialDef.gpuState.blendSrc = GL_SRC_ALPHA;
	s.materialDef.gpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
	s.materialDef.gpuState.blendSrcAlpha = GL_ONE;
	s.materialDef.gpuState.blendDstAlpha = GL_ONE_MINUS_SRC_ALPHA;
	s.materialDef.gpuState.depthMaskEnable = false;

	s.materialDef.programObject = FontProgram.program;
	s.materialDef.uniformMvp = FontProgram.uMvp;

	s.materialDef.uniformSlots[0] = FontProgram.uColor;
	for ( int i = 0 ; i < 4 ; i++ )
	{
		s.materialDef.uniformValues[0][i] = 1.0f;
	}

	s.materialDef.numTextures = 1;
	s.materialDef.textures[0].target = GL_TEXTURE_2D;
	s.materialDef.textures[0].texture = Texture;

	s.surfaceName = text;
	return s;
}

//==================================================================================================
// BitmapFontSurfaceLocal
//
class BitmapFontSurfaceLocal : public BitmapFontSurface
{
public:
						BitmapFontSurfaceLocal();
	virtual				~BitmapFontSurfaceLocal();

	virtual void		Init( const int maxVertices );
	void				Free();	

	// add text to the VBO that will render in a 2D pass. 
	virtual void		DrawText3D( BitmapFont const & font, const fontParms_t & flags,
			        			const Vector3f & pos, Vector3f const & normal, Vector3f const & up,
								float const scale, Vector4f const & color, char const * text );
	virtual void		DrawText3Df( BitmapFont const & font, const fontParms_t & flags,
								const Vector3f & pos, Vector3f const & normal, Vector3f const & up,
								float const scale, Vector4f const & color, char const * text, ... );

	virtual void		DrawTextBillboarded3D( BitmapFont const & font, fontParms_t const & flags,
			        			Vector3f const & pos, float const scale, Vector4f const & color, 
								char const * text );
	virtual void		DrawTextBillboarded3Df( BitmapFont const & font, fontParms_t const & flags,
			        			Vector3f const & pos, float const scale, Vector4f const & color, 
								char const * fmt, ... );

	// transform the billboarded font strings
	virtual void		Finish( Matrix4f const & viewMatrix );

	// render the VBO
	virtual void		Render3D( BitmapFont const & font, Matrix4f const & worldMVP ) const;

	virtual bool		IsInitialized() const { return Initialized; }

private:
	GlGeometry      Geo;		// font glyphs
	fontVertex_t *  Vertices;	// vertices that are written to the VBO
	int             MaxVertices;
	int             MaxIndices;
	int             CurVertex;  // reset every Render()
	int             CurIndex;   // reset every Render()
	bool			Initialized;

	Array< VertexBlockType >	    VertexBlocks;	// each pointer in the array points to an allocated block ov
};

//==============================
// FileSize
static size_t FileSize( FILE * f )
{
	if ( f == NULL )
	{
		return 0;
	}
#if defined( WIN32 )
	struct _stat64 stats;
	__int64 r = _fstati64( f->_file, &stats );
#else
	struct stat stats;
	int r = fstat( f->_file, &stats );
#endif
	if ( r < 0 )
	{
		return 0;
	}
	return static_cast< size_t >( stats.st_size );	// why st_size is signed I have no idea... negative file lengths?
}

//==================================================================================================
// FontInfoType
//==================================================================================================

//==============================
// FontInfoType::LoadFromPackage
bool FontInfoType::LoadFromPackage( void* packageFile, char const * fileName )
{
	int length = 0;
	void * packageBuffer = NULL;

	ovr_ReadFileFromOtherApplicationPackage( packageFile, fileName, length, packageBuffer );
	if ( packageBuffer == NULL ) 
	{
		return false;
	}

	size_t fsize;

	unsigned char * buffer = NULL;
	// copy to a zero-terminated buffer for JSON parser
	fsize = length + 1;
	buffer = new unsigned char[fsize];
	memcpy( buffer, packageBuffer, length );
	buffer[length] = '\0';
	free( packageBuffer );

	// try to load from the buffer -- this may fail due to an invalid version
	bool r = LoadFromBuffer( buffer, fsize );
	delete [] buffer;
	return r;
}


//==============================
// FontInfoType::Load
bool FontInfoType::Load( ovrFileSys & fileSys, char const * uri )
{
	MemBufferT< uint8_t > buffer;	
	if ( !fileSys.ReadFile( uri, buffer ) )
	{
		return false;
	}
	return LoadFromBuffer( buffer, buffer.GetSize() );
}

//==============================
// FontInfoType::Load
bool FontInfoType::Load( OvrApkFile const & languagePackageFile, char const * fileName )
{
	if ( languagePackageFile && LoadFromPackage( languagePackageFile, fileName ) )
	{
		return true;
	}
	
	// if it wasn't loaded from the language package, try again from the app package
	return LoadFromPackage( ovr_GetApplicationPackageFile(), fileName );
}

//==============================
// FontInfoType::LoadFromBuffer
bool FontInfoType::LoadFromBuffer( void const * buffer, size_t const bufferSize ) 
{
	char const * errorMsg = NULL;
	OVR::JSON * jsonRoot = OVR::JSON::Parse( reinterpret_cast< char const * >( buffer ), &errorMsg );
	if ( jsonRoot == NULL )
	{
		WARN( "JSON Error: %s", ( errorMsg != NULL ) ? errorMsg : "<NULL>" );
		return false;
	}

	int32_t maxCharCode = -1;
	// currently we're only supporting the first unicode plane up to 65k. If we were to support other planes
	// we could conceivably end up with a very sparse 1,114,111 byte type for mapping character codes to
	// glyphs and if that's the case we may just want to use a hash, or use a combination of tables for
	// the first 65K and hashes for the other, less-frequently-used characters.
	static const int MAX_GLYPHS = 0xffff;	

	// load the glyphs
	const JsonReader jsonGlyphs( jsonRoot );
	if ( !jsonGlyphs.IsObject() )
	{
		jsonRoot->Release();
		return false;
	}

	int Version = jsonGlyphs.GetChildFloatByName( "Version" );
	if ( Version != FNT_FILE_VERSION )
	{
		jsonRoot->Release();
		return false;
	}

	FontName = jsonGlyphs.GetChildStringByName( "FontName" );
	CommandLine = jsonGlyphs.GetChildStringByName( "CommandLine" );
	ImageFileName = jsonGlyphs.GetChildStringByName( "ImageFileName" );
	const int numGlyphs = jsonGlyphs.GetChildInt32ByName( "NumGlyphs" );
	if ( numGlyphs < 0 || numGlyphs > MAX_GLYPHS )
	{
		OVR_ASSERT( numGlyphs > 0 && numGlyphs <= MAX_GLYPHS );
		jsonRoot->Release();
		return false;
	}

	NaturalWidth = jsonGlyphs.GetChildFloatByName( "NaturalWidth" );
	NaturalHeight = jsonGlyphs.GetChildFloatByName( "NaturalHeight" );

	// we scale everything after loading integer values from the JSON file because the OVR JSON writer loses precision on floats
	float nwScale = 1.0f / NaturalWidth;
	float nhScale = 1.0f / NaturalHeight;

	HorizontalPad = jsonGlyphs.GetChildFloatByName( "HorizontalPad" ) * nwScale;
	VerticalPad = jsonGlyphs.GetChildFloatByName( "VerticalPad" ) * nhScale;
	FontHeight = jsonGlyphs.GetChildFloatByName( "FontHeight" ) * nhScale;
	CenterOffset = jsonGlyphs.GetChildFloatByName( "CenterOffset" );
	TweakScale = jsonGlyphs.GetChildFloatByName( "TweakScale", 1.0f );

	LOG( "FontName = %s", FontName.ToCStr() );
	LOG( "CommandLine = %s", CommandLine.ToCStr() );
	LOG( "HorizontalPad = %.4f", HorizontalPad );
	LOG( "VerticalPad = %.4f", VerticalPad );
	LOG( "FontHeight = %.4f", FontHeight );
	LOG( "CenterOffset = %.4f", CenterOffset );
	LOG( "TweakScale = %.4f", TweakScale );
	LOG( "ImageFileName = %s", ImageFileName.ToCStr() );
	LOG( "Loading %i glyphs.", numGlyphs );

/// HACK: this is hard-coded until we do not have a dependcy on reading the font from Home
	if ( OVR_stricmp( FontName.ToCStr(), "korean.fnt" ) == 0 )
	{
		TweakScale = 0.75f;
		CenterOffset = -0.02f;
	}
/// HACK: end hack

	Glyphs.Resize( numGlyphs );

	const JsonReader jsonGlyphArray( jsonGlyphs.GetChildByName( "Glyphs" ) );

	double oWidth = 0.0;
	double oHeight = 0.0;

	if ( jsonGlyphArray.IsArray() )
	{
		for ( int i = 0; i < Glyphs.GetSizeI() && !jsonGlyphArray.IsEndOfArray(); i++ )
		{
			const JsonReader jsonGlyph( jsonGlyphArray.GetNextArrayElement() );
			if ( jsonGlyph.IsObject() )
			{
				FontGlyphType & g = Glyphs[i];
				g.CharCode	= jsonGlyph.GetChildInt32ByName( "CharCode" );
				g.X			= jsonGlyph.GetChildFloatByName( "X" );
				g.Y			= jsonGlyph.GetChildFloatByName( "Y" );
				g.Width		= jsonGlyph.GetChildFloatByName( "Width" );
				g.Height	= jsonGlyph.GetChildFloatByName( "Height" );
				g.AdvanceX	= jsonGlyph.GetChildFloatByName( "AdvanceX" );
				g.AdvanceY	= jsonGlyph.GetChildFloatByName( "AdvanceY" );
				g.BearingX	= jsonGlyph.GetChildFloatByName( "BearingX" );
				g.BearingY	= jsonGlyph.GetChildFloatByName( "BearingY" );

				if ( g.CharCode == 'O' )
				{
					oWidth = g.Width;
					oHeight = g.Height;
				}

				g.X *= nwScale;
				g.Y *= nhScale;
				g.Width *= nwScale;
				g.Height *= nhScale;
				g.AdvanceX *= nwScale;
				g.AdvanceY *= nhScale;
				g.BearingX *= nwScale;
				g.BearingY *= nhScale;

				float const ascent = g.BearingY;
				float const descent = g.Height - g.BearingY;
				if ( ascent > MaxAscent )
				{
					MaxAscent = ascent;
				}
				if ( descent > MaxDescent )
				{
					MaxDescent = descent;
				}

				maxCharCode = Alg::Max( maxCharCode, g.CharCode );
			}
		}
	}

	float const DEFAULT_TEXT_SCALE = 0.0025f;

	double const NATURAL_WIDTH_SCALE = NaturalWidth / 4096.0;
	double const NATURAL_HEIGHT_SCALE = NaturalHeight / 3820.0;
	double const DEFAULT_O_WIDTH = 325.0;
	double const DEFAULT_O_HEIGHT = 322.0;
	double const OLD_WIDTH_FACTOR = 1.04240608;
	float const widthScaleFactor = static_cast< float >( DEFAULT_O_WIDTH / oWidth * OLD_WIDTH_FACTOR * NATURAL_WIDTH_SCALE );
	float const heightScaleFactor = static_cast< float >( DEFAULT_O_HEIGHT / oHeight * OLD_WIDTH_FACTOR * NATURAL_HEIGHT_SCALE );

	ScaleFactorX = DEFAULT_SCALE_FACTOR * DEFAULT_TEXT_SCALE * widthScaleFactor * TweakScale;
	ScaleFactorY = DEFAULT_SCALE_FACTOR * DEFAULT_TEXT_SCALE * heightScaleFactor * TweakScale;

	// This is not intended for wide or ucf character sets -- depending on the size range of
	// character codes lookups may need to be changed to use a hash.
	if ( maxCharCode >= MAX_GLYPHS )
	{
		OVR_ASSERT( maxCharCode <= MAX_GLYPHS );
		maxCharCode = MAX_GLYPHS;
	}
	
	// resize the array to the maximum glyph value
	CharCodeMap.Resize( maxCharCode + 1 );

	// init to empty value
	for ( int i = 0; i < CharCodeMap.GetSizeI(); ++i )
	{
		CharCodeMap[ i ] = -1;
	}

	for ( int i = 0; i < Glyphs.GetSizeI(); ++i )
	{
		FontGlyphType const & g = Glyphs[i];
		CharCodeMap[g.CharCode] = i;
	}

	jsonRoot->Release();

	return true;
}

//==============================
// FontInfoType::GlyphForCharCode
FontGlyphType const & FontInfoType::GlyphForCharCode( uint32_t const charCode ) const
{
	const int glyphIndex = charCode >= CharCodeMap.GetSize() ? -1 : CharCodeMap[charCode];

	if ( glyphIndex < 0 || glyphIndex >= Glyphs.GetSizeI() )
	{
		WARN( "FontInfoType::GlyphForCharCode FAILED TO FIND GLYPH FOR CHARACTER!" );
		WARN( "FontInfoType::GlyphForCharCode: charCode %u yielding %i", charCode, glyphIndex );
		WARN( "FontInfoType::GlyphForCharCode: CharCodeMap size %i Glyphs size %i", CharCodeMap.GetSize(), Glyphs.GetSizeI() );

		if ( charCode == '*' )
		{
			static FontGlyphType emptyGlyph;
			return emptyGlyph;
		}
		return GlyphForCharCode( '*' ); 
	}

	OVR_ASSERT( glyphIndex >= 0 && glyphIndex < Glyphs.GetSizeI() );
	return Glyphs[glyphIndex];
}

//==================================================================================================
// BitmapFontLocal
//==================================================================================================

#if defined( OVR_OS_WIN32 )
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"
#define PATH_SEPARATOR_NON_CANONICAL '/'
#else
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
#define PATH_SEPARATOR_NON_CANONICAL '\\'
#endif

// TODO: we really need a decent set of functions for path manipulation. OVR_String_PathUtil has
// some bugs and doesn't have functionality for cross-platform path conversion.
static void MakePathCanonical( char * path )
{
	int n = OVR_strlen( path );
	for ( int i = 0; i < n; ++i )
	{
		if ( path[i] == PATH_SEPARATOR_NON_CANONICAL )
		{
			path[i] = PATH_SEPARATOR;
		}
	}
}

#if 0	// unused?
static size_t MakePathCanonical( char const * inPath, char * outPath, size_t outSize )
{
	size_t i = 0;
	for ( ; outPath[i] != '\0' && i < outSize; ++i ) 
	{
		if ( inPath[i] == PATH_SEPARATOR_NON_CANONICAL )
		{
			outPath[i] = PATH_SEPARATOR;
		}
		else
		{
			outPath[i] = inPath[i];
		}
	}
	if ( i == outSize )
	{
		outPath[outSize - 1 ] = '\0';
	}
	else
	{
		outPath[i] = '\0';
	}
	return i;
}
#endif

static void AppendPath( char * path, size_t pathsize, char const * append )
{
	char appendCanonical[512];
	OVR_strcpy( appendCanonical, sizeof( appendCanonical ), append );
	MakePathCanonical( path );
	int n = OVR_strlen( path );
	if ( n > 0 && path[n - 1] != PATH_SEPARATOR && appendCanonical[0] != PATH_SEPARATOR )
	{
		OVR_strcat( path, pathsize, PATH_SEPARATOR_STR );
	}
	OVR_strcat( path, pathsize, appendCanonical );
}

static void StripPath( char const * path, char * outName, size_t const outSize )
{
	if ( path[0] == '\0' )
	{
		outName[0] = '\0';
		return;
	}
	size_t n = OVR_strlen( path );
	char const * fnameStart = NULL;
	for ( int i = n - 1; i >= 0; --i )
	{
		if ( path[i] == PATH_SEPARATOR )
		{
			fnameStart = &path[i];
			break;
		}
	}
	if ( fnameStart != NULL )
	{
		// this will copy 0 characters if the path separator was the last character
		OVR_strncpy( outName, outSize, fnameStart + 1, n - ( fnameStart - path ) );
	}
	else
	{
		OVR_strcpy( outName, outSize, path );
	}
}

static bool ExtensionMatches( char const * fileName, char const * ext )
{
	if ( fileName == NULL || ext == NULL )
	{
		return false;
	}
	size_t extLen = OVR_strlen( ext );
	size_t fileNameLen = OVR_strlen( fileName );
	if ( extLen > fileNameLen )
	{
		return false;
	}
	return OVR_stricmp( fileName + fileNameLen - extLen, ext ) == 0;
}

//==============================
// BitmapFontLocal::Load
bool BitmapFontLocal::Load( ovrFileSys & fileSys, char const * uri )
{
	char scheme[128];
	char host[128];
	int port;
	char path[1024];
	if ( !ovrUri::ParseUri( uri, scheme, sizeof( scheme ), NULL, 0, NULL, 0, host, sizeof( host ), port, path, sizeof( path ), NULL, 0, NULL, 0 ) )
	{
		return false;
	}

	if ( !FontInfo.Load( fileSys, uri ) )
	{
		return false;
	}

	char imagePath[1024];
	{
		LOG( "FontInfo file Uri = %s", uri );
		LOG( "FontInfo file path = %s", path );
		String imageBaseName = FontInfo.ImageFileName.GetFilename();
		LOG( "image base name = %s", imageBaseName.ToCStr() );

		ovrPathUtils::StripFilename( path, imagePath, sizeof( imagePath ) );
		LOG( "FontInfo base path = %s", imagePath );
	
		ovrPathUtils::AppendUriPath( imagePath, sizeof( imagePath ), imageBaseName.ToCStr() );
		LOG( "imagePath = %s", imagePath );
	}

	char imageUri[1024];
	OVR_sprintf( imageUri, sizeof( imageUri ), "%s://%s%s", scheme, host, imagePath );
	LOG( "imageUri = %s", imageUri );
	if ( !LoadImage( fileSys, imageUri ) )
	{
		return false;
	}

	// create the shaders for font rendering if not already created
	if ( FontProgram.vertexShader == 0 || FontProgram.fragmentShader == 0 )
	{
		FontProgram = BuildProgram( FontSingleTextureVertexShaderSrc, SDFFontFragmentShaderSrc );//SingleTextureFragmentShaderSrc );
	}

	return true;
}

//==============================
// BitmapFontLocal::Load
bool BitmapFontLocal::Load( char const * languagePackageName, char const * fontInfoFileName )
{
	OvrApkFile languagePackageFile( ovr_OpenOtherApplicationPackage( languagePackageName ) );
	if ( !FontInfo.Load( languagePackageFile, fontInfoFileName ) )
	{
		return false;
	}

	// strip any path from the image file name path and prepend the path from the .fnt file -- i.e. always
	// require them to be loaded from the same directory.
	String baseName = FontInfo.ImageFileName.GetFilename();
	LOG( "fontInfoFileName = %s", fontInfoFileName );
	LOG( "image baseName = %s", baseName.ToCStr() );
	
	char imagePath[512];
	ovrPathUtils::StripFilename( fontInfoFileName, imagePath, sizeof( imagePath ) );
	LOG( "imagePath = %s", imagePath );
	
	char imageFileName[512];
	StripPath( fontInfoFileName, imageFileName, sizeof( imageFileName ) );
	LOG( "imageFileName = %s", imageFileName );
	
	AppendPath( imagePath, sizeof( imagePath ), baseName.ToCStr() );
	if ( !LoadImage( languagePackageFile, imagePath ) )
	{
		return false;
	}

	// create the shaders for font rendering if not already created
	if ( FontProgram.vertexShader == 0 || FontProgram.fragmentShader == 0 )
	{
		FontProgram = BuildProgram( FontSingleTextureVertexShaderSrc, SDFFontFragmentShaderSrc );//SingleTextureFragmentShaderSrc );
	}

	return true;
}

//==============================
// BitmapFontLocal::Load
bool BitmapFontLocal::LoadImage( ovrFileSys & fileSys, char const * uri )
{
	MemBufferT< uint8_t > imageBuffer;
	if ( !fileSys.ReadFile( uri, imageBuffer ) )
	{
		return false;
	}
	bool success = LoadImageFromBuffer( uri, imageBuffer, imageBuffer.GetSize(), ExtensionMatches( uri, ".astc" ) );
	if ( !success )
	{
		LOG( "BitmapFontLocal::LoadImage: failed to load image '%s'", uri );
	}
	return success;
}

//==============================
// BitmapFontLocal::LoadImage
bool BitmapFontLocal::LoadImage( OvrApkFile const & languagePackageFile, char const * imageName )
{	
	// try to open the language pack apk
	int length = 0;
	void * packageBuffer = NULL;
	if ( languagePackageFile )
	{
		ovr_ReadFileFromOtherApplicationPackage( languagePackageFile, imageName, length, packageBuffer );
	}

	// one of the following conditions should be true here:
	// - we opened the language apk and read the texture file without error
	// - we opened the language apk and failed to open the texture file
	// - we failed to open the language apk
	if ( packageBuffer == NULL )
	{
		ovr_ReadFileFromApplicationPackage( imageName, length, packageBuffer );
	}

	bool result = false;
	if ( packageBuffer != NULL ) 
	{
		result = LoadImageFromBuffer( imageName, (unsigned char const*)packageBuffer, length, ExtensionMatches( imageName, ".astc" ) );
		free( packageBuffer );
	}
	else
	{
		FILE * f = fopen( imageName, "rb" );
		if ( f != NULL ) 
		{
			size_t fsize = FileSize( f );

			unsigned char * buffer = new unsigned char[fsize];

			size_t countRead = fread( buffer, fsize, 1, f );
			fclose( f );
			f = NULL;
			if ( countRead == 1 )
			{
				result = LoadImageFromBuffer( imageName, buffer, fsize, ExtensionMatches( imageName, ".astc" ) );
			}
			delete [] buffer;
		}
	}

	if ( !result ) {
		WARN( "BitmapFontLocal::LoadImage: failed to load image '%s'", imageName );
	}
	return result;
}

//==============================
// BitmapFontLocal::LoadImageFromBuffer
bool BitmapFontLocal::LoadImageFromBuffer( char const * imageName, unsigned char const * buffer, size_t bufferSize, bool const isASTC ) 
{
	if ( Texture != 0 ) 
	{
		glDeleteTextures( 1, &Texture );
		Texture = 0;
	}

	if ( isASTC )
	{
		Texture = LoadASTCTextureFromMemory( buffer, bufferSize, 1 );
	}
	else
	{
		Texture = LoadTextureFromBuffer( imageName, MemBuffer( (void *)buffer, bufferSize),
    			TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), ImageWidth, ImageHeight );
	}
	if ( Texture == 0 ) 
	{
		WARN( "BitmapFontLocal::Load: failed to load '%s'", imageName );
		return false;
	}

	LOG( "BitmapFontLocal::LoadImageFromBuffer: success" );
	return true;
}

//==============================
// BitmapFontLocal::GetGlyphMetrics
void BitmapFontLocal::GetGlyphMetrics( const uint32_t charCode, float &width,
                                       float &height, float &advanceX,
                                       float &advanceY ) const
{
    FontGlyphType const &glyph = GlyphForCharCode( charCode );
    width = glyph.Width;
    height = glyph.Height;
    advanceX = glyph.AdvanceX;
    advanceY = glyph.AdvanceY;
}

//==============================
// BitmapFontLocal::WordWrapText
void BitmapFontLocal::WordWrapText( String & inOutText, const float widthMeters, const float fontScale ) const
{
	WordWrapText( inOutText, widthMeters, OVR::Array< OVR::String >(), fontScale );
}

//==============================
// BitmapFontLocal::WordWrapText
void BitmapFontLocal::WordWrapText( String & inOutText, const float widthMeters, OVR::Array< OVR::String > wholeStrsList, const float fontScale ) const
{
	float const xScale = FontInfo.ScaleFactorX * fontScale;
	const int32_t totalLength = inOutText.GetLengthI();
	int32_t lastWhitespaceIndex = -1;
	double lineWidthAtLastWhitespace = 0.0f;
	double lineWidth = 0.0f;
	int dontSplitUntilIdx = -1;

	for ( int32_t pos = 0; pos < totalLength; ++pos )
	{
		uint32_t charCode = inOutText.GetCharAt( pos );

		// Replace any existing character escapes with space as we recompute where to insert line breaks
        bool forceBreak = false;
		if ( charCode == '\r' || charCode == '\n' || charCode == '\t' )
		{
            if ( charCode == '\r' || charCode == '\n' )
            {
                // We still want hard breaks in the wrapped-text.
                forceBreak = true;
            }

			inOutText.Remove( pos );
			inOutText.InsertCharAt( ' ', pos );
			charCode = ' ';
		}

		FontGlyphType const & g = GlyphForCharCode( charCode );
		lineWidth += g.AdvanceX * xScale;

		for ( int i = 0; i < wholeStrsList.GetSizeI(); ++i )
		{
			int curWholeStrLen = wholeStrsList[i].GetLengthI();
			int endPos = pos + curWholeStrLen;

			if ( endPos < totalLength )
			{
				String subInStr = inOutText.Substring( pos, endPos );
				if ( subInStr == wholeStrsList[i] )
				{
					dontSplitUntilIdx = Alg::Max(dontSplitUntilIdx, endPos);
				}
			}
		}

		if ( pos >= dontSplitUntilIdx )
		{
			if ( charCode == ' ' )
			{
				lastWhitespaceIndex = pos;
				lineWidthAtLastWhitespace = lineWidth;
			}

			// always check the line width and as soon as we exceed it, wrap the text at
			// the last whitespace. This ensure's the text always fits within the width.
			if ( forceBreak || ( lineWidth >= widthMeters && lastWhitespaceIndex >= 0 ) )
			{
				dontSplitUntilIdx = -1;
				inOutText.Remove( lastWhitespaceIndex );
				inOutText.InsertCharAt( '\n', lastWhitespaceIndex );
				// subtract the width after the last whitespace so that we don't lose any
				// of the accumulated width since then.
				lineWidth -= lineWidthAtLastWhitespace;
			}
		}
	}
}

//==============================
// BitmapFontLocal::CalcTextWidth
float BitmapFontLocal::CalcTextWidth( char const * text ) const
{
	float width = 0.0f;
	char const * p = text;
	for ( uint32_t charCode = UTF8Util::DecodeNextChar( &p ); charCode != '\0'; charCode = UTF8Util::DecodeNextChar( &p ) )
	{
		if ( charCode == '\r' || charCode == '\n' )
		{
			continue;	// skip line endings
		}

		FontGlyphType const & g = GlyphForCharCode( charCode );
		width += g.AdvanceX * FontInfo.ScaleFactorX;
	}
	return width;
}

//==============================
// BitmapFontLocal::CalcTextMetrics
void BitmapFontLocal::CalcTextMetrics( char const * text, size_t & len, float & width, float & height, 
		float & firstAscent, float & lastDescent, float & fontHeight, float * lineWidths, int const maxLines, int & numLines ) const
{
	len = 0;
	numLines = 0;
	width = 0.0f;
	height = 0.0f;

	if ( lineWidths == NULL || maxLines <= 0 )
	{
		return;
	}
	if ( text == NULL || text[0] == '\0' )
	{
		return;
	}

	float maxLineAscent = 0.0f;
	float maxLineDescent = 0.0f;
	firstAscent = 0.0f;
	lastDescent = 0.0f;
	fontHeight = FontInfo.FontHeight * FontInfo.ScaleFactorY;
	numLines = 0;
	int charsOnLine = 0;
	lineWidths[0] = 0.0f;

	char const * p = text;
	for ( ; ; len++ )
	{
		uint32_t charCode = UTF8Util::DecodeNextChar( &p );
		if ( charCode == '\r' )
		{
			continue;	// skip carriage returns
		}
		if ( charCode == '\n' || charCode == '\0' )
		{
			// keep track of the widest line, which will be the width of the entire text block
			if ( lineWidths[numLines] > width )
			{
				width = lineWidths[numLines];
			}
			
			firstAscent = ( numLines == 0 ) ? maxLineAscent : firstAscent;
			lastDescent = ( charsOnLine > 0 ) ? maxLineDescent : lastDescent;
			charsOnLine = 0;

			if ( numLines < maxLines - 1 )
			{
				// if we're not out of array space, advance and zero the width
				numLines++;
				lineWidths[numLines] = 0.0f;
				maxLineAscent = 0.0f;
				maxLineDescent = 0.0f;
			}
			if ( charCode == '\0' )
			{
				break;
			}
			continue;
		}

		charsOnLine++;

		FontGlyphType const & g = GlyphForCharCode( charCode );
		lineWidths[numLines] += g.AdvanceX * FontInfo.ScaleFactorX;

		if ( numLines == 0 )
		{
			if ( g.BearingY > maxLineAscent )
			{
				maxLineAscent = g.BearingY;
			}
		}
		else
		{
			// all lines after the first line are full height
			maxLineAscent = FontInfo.FontHeight;
		}
		float descent = g.Height - g.BearingY;
		if ( descent > maxLineDescent )
		{
			maxLineDescent = descent;
		}
	}

	OVR_ASSERT( numLines >= 1 );

	firstAscent *= FontInfo.ScaleFactorY;
	lastDescent *= FontInfo.ScaleFactorY;
	height = firstAscent;
	height += ( numLines - 1 ) * FontInfo.FontHeight * FontInfo.ScaleFactorY;
	height += lastDescent;

	OVR_ASSERT( numLines <= maxLines );
}

//==================================================================================================
// BitmapFontSurfaceLocal
//==================================================================================================

//==============================
// BitmapFontSurfaceLocal::BitmapFontSurface
BitmapFontSurfaceLocal::BitmapFontSurfaceLocal() :
	Vertices( NULL ),
	MaxVertices( 0 ),
	MaxIndices( 0 ),
	CurVertex( 0 ),
	CurIndex( 0 ),
	Initialized( false )
{
}

//==============================
// BitmapFontSurfaceLocal::~BitmapFontSurfaceLocal
BitmapFontSurfaceLocal::~BitmapFontSurfaceLocal()
{
	Geo.Free();
	delete [] Vertices;
	Vertices = NULL;
}

//==============================
// BitmapFontSurfaceLocal::Init
// Initializes the surface VBO
void BitmapFontSurfaceLocal::Init( const int maxVertices ) 
{
	assert( Geo.vertexBuffer == 0 && Geo.indexBuffer == 0 && Geo.vertexArrayObject == 0 );
	assert( Vertices == NULL );
	if ( Vertices != NULL ) 
	{
		delete [] Vertices;
		Vertices = NULL;
	}
	assert( maxVertices % 4 == 0 );

	MaxVertices = maxVertices;
	MaxIndices = ( maxVertices / 4 ) * 6;

	Vertices = new fontVertex_t[ maxVertices ];

	Geo = FontGeometry( MaxVertices / 4 );
	Geo.indexCount = 0; // if there's anything to render this will be modified
    
	CurVertex = 0;
	CurIndex = 0;

	Initialized = true;

	LOG( "BitmapFontSurfaceLocal::Init: success" );
}

//==============================
// BitmapFontSurfaceLocal::DrawText3D
void BitmapFontSurfaceLocal::DrawText3D( BitmapFont const & font, fontParms_t const & parms,
		Vector3f const & pos, Vector3f const & normal, Vector3f const & up,
		float scale, Vector4f const & color, char const * text )
{
	if ( text == NULL || text[0] == '\0' )
	{
		return;	// nothing to do here, move along
	}
	VertexBlockType vb = DrawTextToVertexBlock( font, parms, pos, normal, up, scale, color, text );

	// add the new vertex block to the array of vertex blocks
	VertexBlocks.PushBack( vb );
}

//==============================
// BitmapFontSurfaceLocal::DrawText3Df
void BitmapFontSurfaceLocal::DrawText3Df( BitmapFont const & font, fontParms_t const & parms,
		Vector3f const & pos, Vector3f const & normal, Vector3f const & up,
		float const scale, Vector4f const & color, char const * fmt, ... )
{
	char buffer[256];
	va_list args;
	va_start( args, fmt );
	vsnprintf( buffer, sizeof( buffer ), fmt, args );
	va_end( args );
	DrawText3D( font, parms, pos, normal, up, scale, color, buffer );
}

//==============================
// BitmapFontSurfaceLocal::DrawTextBillboarded3D
void BitmapFontSurfaceLocal::DrawTextBillboarded3D( BitmapFont const & font, fontParms_t const & parms,
		Vector3f const & pos, float const scale, Vector4f const & color, char const * text )
{
	fontParms_t billboardParms = parms;
	billboardParms.Billboard = true;
	DrawText3D( font, billboardParms, pos, Vector3f( 1.0f, 0.0f, 0.0f ), Vector3f( 0.0f, -1.0f, 0.0f ),
			scale, color, text );
}

//==============================
// BitmapFontSurfaceLocal::DrawTextBillboarded3Df
void BitmapFontSurfaceLocal::DrawTextBillboarded3Df( BitmapFont const & font, fontParms_t const & parms,
		Vector3f const & pos, float const scale, Vector4f const & color, char const * fmt, ... )
{
	char buffer[256];
	va_list args;
	va_start( args, fmt );
	vsnprintf( buffer, sizeof( buffer ), fmt, args );
	va_end( args );
	DrawTextBillboarded3D( font, parms, pos, scale, color, buffer );
}


//==============================================================
// vbSort_t
// small structure that is used to sort vertex blocks by their distance to the camera
//==============================================================
struct vbSort_t 
{
	int		VertexBlockIndex;
	float	DistanceSquared;
};

//==============================
// VertexBlockSortFn
// sort function for vertex blocks
int VertexBlockSortFn( void const * a, void const * b )
{
	return ftoi( ((vbSort_t const*)a)->DistanceSquared - ((vbSort_t const*)b)->DistanceSquared );
}

//==============================
// BitmapFontSurfaceLocal::Finish
// transform all vertex blocks into the vertices array so they're ready to be uploaded to the VBO
// We don't have to do this for each eye because the billboarded surfaces are sorted / aligned
// based on their distance from / direction to the camera view position and not the camera direction.
void BitmapFontSurfaceLocal::Finish( Matrix4f const & viewMatrix )
{
	ASSERT_WITH_TAG( this != NULL, "BitmapFont" );

	//SPAM( "BitmapFontSurfaceLocal::Finish" );

	Matrix4f invViewMatrix = viewMatrix.Inverted(); // if the view is never scaled or sheared we could use Transposed() here instead
	Vector3f viewPos = invViewMatrix.GetTranslation();

	// sort vertex blocks indices based on distance to pivot
	int const MAX_VERTEX_BLOCKS = 256;
	vbSort_t vbSort[MAX_VERTEX_BLOCKS];
	int const n = VertexBlocks.GetSizeI();
	for ( int i = 0; i < n; ++i )
	{
		vbSort[i].VertexBlockIndex = i;
		VertexBlockType & vb = VertexBlocks[i];
		vbSort[i].DistanceSquared = ( vb.Pivot - viewPos ).LengthSq();
	}

	qsort( vbSort, n, sizeof( vbSort[0] ), VertexBlockSortFn );

	// transform the vertex blocks into the vertices array
	CurIndex = 0;
	CurVertex = 0;

	// TODO:
	// To add multiple-font-per-surface support, we need to add a 3rd component to s and t, 
	// then get the font for each vertex block, and set the texture index on each vertex in 
	// the third texture coordinate.
	for ( int i = 0; i < VertexBlocks.GetSizeI(); ++i )
	{		
		VertexBlockType & vb = VertexBlocks[vbSort[i].VertexBlockIndex];
		Matrix4f transform;
		if ( vb.Billboard )
		{
			if ( vb.TrackRoll )
			{
				transform = invViewMatrix;
			}
			else
			{
                Vector3f textNormal = viewPos - vb.Pivot;
				float const len = textNormal.Length();
				if ( len < Mathf::SmallestNonDenormal )
				{
					vb.Free();
					continue;
				}
                textNormal *= 1.0f / len;
                transform = Matrix4f::CreateFromBasisVectors( textNormal, Vector3f( 0.0f, 1.0f, 0.0f ) );
			}
			transform.SetTranslation( vb.Pivot );
		}
		else
		{
			transform.SetIdentity();
			transform.SetTranslation( vb.Pivot );
		}

		for ( int j = 0; j < vb.NumVerts; j++ )
		{
			fontVertex_t const & v = vb.Verts[j];
			Vertices[CurVertex].xyz = transform.Transform( v.xyz );
			Vertices[CurVertex].s = v.s;
			Vertices[CurVertex].t = v.t;			
			*(UInt32*)(&Vertices[CurVertex].rgba[0]) = *(UInt32*)(&v.rgba[0]);
			*(UInt32*)(&Vertices[CurVertex].fontParms[0]) = *(UInt32*)(&v.fontParms[0]);
			CurVertex++;
		}
		CurIndex += ( vb.NumVerts / 2 ) * 3;
		// free this vertex block
		vb.Free();
	}
	// remove all elements from the vertex block (but don't free the memory since it's likely to be 
	// needed on the next frame.
	VertexBlocks.Clear();

	glBindVertexArray( Geo.vertexArrayObject );
	glBindBuffer( GL_ARRAY_BUFFER, Geo.vertexBuffer );
	glBufferSubData( GL_ARRAY_BUFFER, 0, CurVertex * sizeof( fontVertex_t ), (void *)Vertices );
	glBindVertexArray( 0 );	
	Geo.indexCount = CurIndex;
}

//==============================
// BitmapFontSurfaceLocal::Render3D
// render the font surface by transforming each vertex block and copying it into the VBO
// TODO: once we add support for multiple fonts per surface, this should not take a BitmapFont for input.
void BitmapFontSurfaceLocal::Render3D( BitmapFont const & font, Matrix4f const & worldMVP ) const
{
	GL_CheckErrors( "BitmapFontSurfaceLocal::Render3D - pre" );

	//SPAM( "BitmapFontSurfaceLocal::Render3D" );

	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glDepthMask( GL_FALSE );
	glDisable( GL_CULL_FACE );

	// Draw the text glyphs
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, AsLocal( font ).GetTexture() );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	glUseProgram( AsLocal( font ).GetFontProgram().program );

	glUniformMatrix4fv( AsLocal( font ).GetFontProgram().uMvp, 1, GL_TRUE, worldMVP.M[0] );

	float textColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };	
	glUniform4fv( AsLocal( font ).GetFontProgram().uColor, 1, textColor );

	// draw all font vertices
	glBindVertexArray( Geo.vertexArrayObject );
	glDrawElements( GL_TRIANGLES, Geo.indexCount, GL_UNSIGNED_SHORT, NULL );
	glBindVertexArray( 0 );

	glEnable( GL_CULL_FACE );

	glDisable( GL_BLEND );
	glDepthMask( GL_FALSE );

	GL_CheckErrors( "BitmapFontSurfaceLocal::Render3D - post" );
}

//==============================
// BitmapFont::Create
BitmapFont * BitmapFont::Create()
{
	return new BitmapFontLocal;
}
//==============================
// BitmapFont::Free
void BitmapFont::Free( BitmapFont * & font )
{
	if ( font != NULL )
	{
		delete font;
		font = NULL;
	}
}

//==============================
// BitmapFontSurface::Create
BitmapFontSurface * BitmapFontSurface::Create()
{
	return new BitmapFontSurfaceLocal();
}

//==============================
// BitmapFontSurface::Free
void BitmapFontSurface::Free( BitmapFontSurface * & fontSurface )
{
	if ( fontSurface != NULL )
	{
		delete fontSurface;
		fontSurface = NULL;
	}
}


} // namespace OVR
