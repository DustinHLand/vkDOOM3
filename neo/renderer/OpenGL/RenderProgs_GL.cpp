/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company. 
Copyright (C) 2016-2017 Dustin Land

This file is part of the Doom 3 BFG Edition GPL Source Code ("Doom 3 BFG Edition Source Code").  

Doom 3 BFG Edition Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 BFG Edition Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 BFG Edition Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 BFG Edition Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 BFG Edition Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#pragma hdrstop
#include "../../idlib/precompiled.h"
#include "../RenderBackend.h"
#include "../RenderProgs.h"
#include "../RenderLog.h"

void RpPrintState( uint64 stateBits, uint64 * stencilBits );

static const int AT_VS_IN  = BIT( 1 );
static const int AT_VS_OUT = BIT( 2 );
static const int AT_PS_IN  = BIT( 3 );
static const int AT_PS_OUT = BIT( 4 );

/*
================================================
attribInfo_t
================================================
*/
struct attribInfo_t {
	const char *	type;
	const char *	name;
	const char *	semantic;
	const char *	glsl;
	int				bind;
	int				flags;
	int				vertexMask;
};

attribInfo_t attribsPC[] = {
	// vertex attributes
	{ "float4",		"position",		"POSITION",		"in_Position",			PC_ATTRIB_INDEX_VERTEX,			AT_VS_IN,		VERTEX_MASK_XYZ },
	{ "float2",		"texcoord",		"TEXCOORD0",	"in_TexCoord",			PC_ATTRIB_INDEX_ST,				AT_VS_IN,		VERTEX_MASK_ST },
	{ "float4",		"normal",		"NORMAL",		"in_Normal",			PC_ATTRIB_INDEX_NORMAL,			AT_VS_IN,		VERTEX_MASK_NORMAL },
	{ "float4",		"tangent",		"TANGENT",		"in_Tangent",			PC_ATTRIB_INDEX_TANGENT,		AT_VS_IN,		VERTEX_MASK_TANGENT },
	{ "float4",		"color",		"COLOR0",		"in_Color",				PC_ATTRIB_INDEX_COLOR,			AT_VS_IN,		VERTEX_MASK_COLOR },
	{ "float4",		"color2",		"COLOR1",		"in_Color2",			PC_ATTRIB_INDEX_COLOR2,			AT_VS_IN,		VERTEX_MASK_COLOR2 },

	// pre-defined vertex program output
	{ "float4",		"position",		"POSITION",		"gl_Position",			0,	AT_VS_OUT,		0 },
	{ "float",		"clip0",		"CLP0",			"gl_ClipDistance[0]",	0,	AT_VS_OUT,		0 },
	{ "float",		"clip1",		"CLP1",			"gl_ClipDistance[1]",	0,	AT_VS_OUT,		0 },
	{ "float",		"clip2",		"CLP2",			"gl_ClipDistance[2]",	0,	AT_VS_OUT,		0 },
	{ "float",		"clip3",		"CLP3",			"gl_ClipDistance[3]",	0,	AT_VS_OUT,		0 },
	{ "float",		"clip4",		"CLP4",			"gl_ClipDistance[4]",	0,	AT_VS_OUT,		0 },
	{ "float",		"clip5",		"CLP5",			"gl_ClipDistance[5]",	0,	AT_VS_OUT,		0 },

	// pre-defined fragment program input
	{ "float4",		"position",		"WPOS",			"gl_FragCoord",			0,	AT_PS_IN,		0 },
	{ "half4",		"hposition",	"WPOS",			"gl_FragCoord",			0,	AT_PS_IN,		0 },
	{ "float",		"facing",		"FACE",			"gl_FrontFacing",		0,	AT_PS_IN,		0 },

	// fragment program output
	{ "float4",		"color",		"COLOR",		"gl_FragColor",		0,	AT_PS_OUT,		0 }, // GLSL version 1.2 doesn't allow for custom color name mappings
	{ "half4",		"hcolor",		"COLOR",		"gl_FragColor",		0,	AT_PS_OUT,		0 },
	{ "float4",		"color0",		"COLOR0",		"gl_FragColor",		0,	AT_PS_OUT,		0 },
	{ "float4",		"color1",		"COLOR1",		"gl_FragColor",		1,	AT_PS_OUT,		0 },
	{ "float4",		"color2",		"COLOR2",		"gl_FragColor",		2,	AT_PS_OUT,		0 },
	{ "float4",		"color3",		"COLOR3",		"gl_FragColor",		3,	AT_PS_OUT,		0 },
	{ "float",		"depth",		"DEPTH",		"gl_FragDepth",		4,	AT_PS_OUT,		0 },

	// vertex to fragment program pass through
	{ "float4",		"color",		"COLOR",		"gl_FrontColor",			0,	AT_VS_OUT,	0 },
	{ "float4",		"color0",		"COLOR0",		"gl_FrontColor",			0,	AT_VS_OUT,	0 },
	{ "float4",		"color1",		"COLOR1",		"gl_FrontSecondaryColor",	0,	AT_VS_OUT,	0 },


	{ "float4",		"color",		"COLOR",		"gl_Color",				0,	AT_PS_IN,	0 },
	{ "float4",		"color0",		"COLOR0",		"gl_Color",				0,	AT_PS_IN,	0 },
	{ "float4",		"color1",		"COLOR1",		"gl_SecondaryColor",	0,	AT_PS_IN,	0 },

	{ "half4",		"hcolor",		"COLOR",		"gl_Color",				0,	AT_PS_IN,		0 },
	{ "half4",		"hcolor0",		"COLOR0",		"gl_Color",				0,	AT_PS_IN,		0 },
	{ "half4",		"hcolor1",		"COLOR1",		"gl_SecondaryColor",	0,	AT_PS_IN,		0 },

	{ "float4",		"texcoord0",	"TEXCOORD0_centroid",	"vofi_TexCoord0",	0,	AT_PS_IN,	0 },
	{ "float4",		"texcoord1",	"TEXCOORD1_centroid",	"vofi_TexCoord1",	0,	AT_PS_IN,	0 },
	{ "float4",		"texcoord2",	"TEXCOORD2_centroid",	"vofi_TexCoord2",	0,	AT_PS_IN,	0 },
	{ "float4",		"texcoord3",	"TEXCOORD3_centroid",	"vofi_TexCoord3",	0,	AT_PS_IN,	0 },
	{ "float4",		"texcoord4",	"TEXCOORD4_centroid",	"vofi_TexCoord4",	0,	AT_PS_IN,	0 },
	{ "float4",		"texcoord5",	"TEXCOORD5_centroid",	"vofi_TexCoord5",	0,	AT_PS_IN,	0 },
	{ "float4",		"texcoord6",	"TEXCOORD6_centroid",	"vofi_TexCoord6",	0,	AT_PS_IN,	0 },
	{ "float4",		"texcoord7",	"TEXCOORD7_centroid",	"vofi_TexCoord7",	0,	AT_PS_IN,	0 },
	{ "float4",		"texcoord8",	"TEXCOORD8_centroid",	"vofi_TexCoord8",	0,	AT_PS_IN,	0 },
	{ "float4",		"texcoord9",	"TEXCOORD9_centroid",	"vofi_TexCoord9",	0,	AT_PS_IN,	0 },

	{ "float4",		"texcoord0",	"TEXCOORD0",	"vofi_TexCoord0",		0,	AT_VS_OUT | AT_PS_IN,	0 },
	{ "float4",		"texcoord1",	"TEXCOORD1",	"vofi_TexCoord1",		0,	AT_VS_OUT | AT_PS_IN,	0 },
	{ "float4",		"texcoord2",	"TEXCOORD2",	"vofi_TexCoord2",		0,	AT_VS_OUT | AT_PS_IN,	0 },
	{ "float4",		"texcoord3",	"TEXCOORD3",	"vofi_TexCoord3",		0,	AT_VS_OUT | AT_PS_IN,	0 },
	{ "float4",		"texcoord4",	"TEXCOORD4",	"vofi_TexCoord4",		0,	AT_VS_OUT | AT_PS_IN,	0 },
	{ "float4",		"texcoord5",	"TEXCOORD5",	"vofi_TexCoord5",		0,	AT_VS_OUT | AT_PS_IN,	0 },
	{ "float4",		"texcoord6",	"TEXCOORD6",	"vofi_TexCoord6",		0,	AT_VS_OUT | AT_PS_IN,	0 },
	{ "float4",		"texcoord7",	"TEXCOORD7",	"vofi_TexCoord7",		0,	AT_VS_OUT | AT_PS_IN,	0 },
	{ "float4",		"texcoord8",	"TEXCOORD8",	"vofi_TexCoord8",		0,	AT_VS_OUT | AT_PS_IN,	0 },
	{ "float4",		"texcoord9",	"TEXCOORD9",	"vofi_TexCoord9",		0,	AT_VS_OUT | AT_PS_IN,	0 },

	{ "half4",		"htexcoord0",	"TEXCOORD0",	"vofi_TexCoord0",		0,	AT_PS_IN,		0 },
	{ "half4",		"htexcoord1",	"TEXCOORD1",	"vofi_TexCoord1",		0,	AT_PS_IN,		0 },
	{ "half4",		"htexcoord2",	"TEXCOORD2",	"vofi_TexCoord2",		0,	AT_PS_IN,		0 },
	{ "half4",		"htexcoord3",	"TEXCOORD3",	"vofi_TexCoord3",		0,	AT_PS_IN,		0 },
	{ "half4",		"htexcoord4",	"TEXCOORD4",	"vofi_TexCoord4",		0,	AT_PS_IN,		0 },
	{ "half4",		"htexcoord5",	"TEXCOORD5",	"vofi_TexCoord5",		0,	AT_PS_IN,		0 },
	{ "half4",		"htexcoord6",	"TEXCOORD6",	"vofi_TexCoord6",		0,	AT_PS_IN,		0 },
	{ "half4",		"htexcoord7",	"TEXCOORD7",	"vofi_TexCoord7",		0,	AT_PS_IN,		0 },
	{ "half4",		"htexcoord8",	"TEXCOORD8",	"vofi_TexCoord8",		0,	AT_PS_IN,		0 },
	{ "half4",		"htexcoord9",	"TEXCOORD9",	"vofi_TexCoord9",		0,	AT_PS_IN,		0 },
	{ "float",		"fog",			"FOG",			"gl_FogFragCoord",		0,	AT_VS_OUT,		0 },
	{ "float4",		"fog",			"FOG",			"gl_FogFragCoord",		0,	AT_PS_IN,		0 },
	{ NULL,			NULL,			NULL,			NULL,					0,	0,				0 }
};

const char * types[] = {
	"int",
	"float",
	"half",
	"fixed",
	"bool",
	"cint",
	"cfloat",
	"void"
};
static const int numTypes = sizeof( types ) / sizeof( types[0] );

const char * typePosts[] = {
	"1", "2", "3", "4",
	"1x1", "1x2", "1x3", "1x4",
	"2x1", "2x2", "2x3", "2x4",
	"3x1", "3x2", "3x3", "3x4",
	"4x1", "4x2", "4x3", "4x4"
};
static const int numTypePosts = sizeof( typePosts ) / sizeof( typePosts[0] );

const char * prefixes[] = {
	"static",
	"const",
	"uniform",
	"struct",

	"sampler",

	"sampler1D",
	"sampler2D",
	"sampler3D",
	"samplerCUBE",

	"sampler1DShadow",		// GLSL
	"sampler2DShadow",		// GLSL
	"sampler3DShadow",		// GLSL
	"samplerCubeShadow",	// GLSL

	"sampler2DMS",			// GLSL
};
static const int numPrefixes = sizeof( prefixes ) / sizeof( prefixes[0] );

struct typeConversion_t {
	const char * typeCG;
	const char * typeGLSL;
} typeConversion[] = {
	{ "void",				"void" },

	{ "fixed",				"float" },

	{ "float",				"float" },
	{ "float2",				"vec2" },
	{ "float3",				"vec3" },
	{ "float4",				"vec4" },

	{ "half",				"float" },
	{ "half2",				"vec2" },
	{ "half3",				"vec3" },
	{ "half4",				"vec4" },

	{ "int",				"int" },
	{ "int2",				"ivec2" },
	{ "int3",				"ivec3" },
	{ "int4",				"ivec4" },

	{ "bool",				"bool" },
	{ "bool2",				"bvec2" },
	{ "bool3",				"bvec3" },
	{ "bool4",				"bvec4" },

	{ "float2x2",			"mat2x2" },
	{ "float2x3",			"mat2x3" },
	{ "float2x4",			"mat2x4" },

	{ "float3x2",			"mat3x2" },
	{ "float3x3",			"mat3x3" },
	{ "float3x4",			"mat3x4" },

	{ "float4x2",			"mat4x2" },
	{ "float4x3",			"mat4x3" },
	{ "float4x4",			"mat4x4" },

	{ "sampler1D",			"sampler1D" },
	{ "sampler2D",			"sampler2D" },
	{ "sampler3D",			"sampler3D" },
	{ "samplerCUBE",		"samplerCube" },

	{ "sampler1DShadow",	"sampler1DShadow" },
	{ "sampler2DShadow",	"sampler2DShadow" },
	{ "sampler3DShadow",	"sampler3DShadow" },
	{ "samplerCubeShadow",	"samplerCubeShadow" },

	{ "sampler2DMS",		"sampler2DMS" },

	{ NULL, NULL }
};

const char * vertexInsert = {
	"#version 150\n"
	"#define PC\n"
	"\n"
	"float saturate( float v ) { return clamp( v, 0.0, 1.0 ); }\n"
	"vec2 saturate( vec2 v ) { return clamp( v, 0.0, 1.0 ); }\n"
	"vec3 saturate( vec3 v ) { return clamp( v, 0.0, 1.0 ); }\n"
	"vec4 saturate( vec4 v ) { return clamp( v, 0.0, 1.0 ); }\n"
	"vec4 tex2Dlod( sampler2D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xy, texcoord.w ); }\n"
	"\n"
};

const char * fragmentInsert = {
	"#version 150\n"
	"#define PC\n"
	"\n"
	"void clip( float v ) { if ( v < 0.0 ) { discard; } }\n"
	"void clip( vec2 v ) { if ( any( lessThan( v, vec2( 0.0 ) ) ) ) { discard; } }\n"
	"void clip( vec3 v ) { if ( any( lessThan( v, vec3( 0.0 ) ) ) ) { discard; } }\n"
	"void clip( vec4 v ) { if ( any( lessThan( v, vec4( 0.0 ) ) ) ) { discard; } }\n"
	"\n"
	"float saturate( float v ) { return clamp( v, 0.0, 1.0 ); }\n"
	"vec2 saturate( vec2 v ) { return clamp( v, 0.0, 1.0 ); }\n"
	"vec3 saturate( vec3 v ) { return clamp( v, 0.0, 1.0 ); }\n"
	"vec4 saturate( vec4 v ) { return clamp( v, 0.0, 1.0 ); }\n"
	"\n"
	"vec4 tex2D( sampler2D sampler, vec2 texcoord ) { return texture( sampler, texcoord.xy ); }\n"
	"vec4 tex2D( sampler2DShadow sampler, vec3 texcoord ) { return vec4( texture( sampler, texcoord.xyz ) ); }\n"
	"\n"
	"vec4 tex2D( sampler2D sampler, vec2 texcoord, vec2 dx, vec2 dy ) { return textureGrad( sampler, texcoord.xy, dx, dy ); }\n"
	"vec4 tex2D( sampler2DShadow sampler, vec3 texcoord, vec2 dx, vec2 dy ) { return vec4( textureGrad( sampler, texcoord.xyz, dx, dy ) ); }\n"
	"\n"
	"vec4 texCUBE( samplerCube sampler, vec3 texcoord ) { return texture( sampler, texcoord.xyz ); }\n"
	"vec4 texCUBE( samplerCubeShadow sampler, vec4 texcoord ) { return vec4( texture( sampler, texcoord.xyzw ) ); }\n"
	"\n"
	"vec4 tex1Dproj( sampler1D sampler, vec2 texcoord ) { return textureProj( sampler, texcoord ); }\n"
	"vec4 tex2Dproj( sampler2D sampler, vec3 texcoord ) { return textureProj( sampler, texcoord ); }\n"
	"vec4 tex3Dproj( sampler3D sampler, vec4 texcoord ) { return textureProj( sampler, texcoord ); }\n"
	"\n"
	"vec4 tex1Dbias( sampler1D sampler, vec4 texcoord ) { return texture( sampler, texcoord.x, texcoord.w ); }\n"
	"vec4 tex2Dbias( sampler2D sampler, vec4 texcoord ) { return texture( sampler, texcoord.xy, texcoord.w ); }\n"
	"vec4 tex3Dbias( sampler3D sampler, vec4 texcoord ) { return texture( sampler, texcoord.xyz, texcoord.w ); }\n"
	"vec4 texCUBEbias( samplerCube sampler, vec4 texcoord ) { return texture( sampler, texcoord.xyz, texcoord.w ); }\n"
	"\n"
	"vec4 tex1Dlod( sampler1D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.x, texcoord.w ); }\n"
	"vec4 tex2Dlod( sampler2D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xy, texcoord.w ); }\n"
	"vec4 tex3Dlod( sampler3D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xyz, texcoord.w ); }\n"
	"vec4 texCUBElod( samplerCube sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xyz, texcoord.w ); }\n"
	"\n"
};

struct builtinConversion_t {
	const char * nameCG;
	const char * nameGLSL;
} builtinConversion[] = {
	{ "frac",		"fract" },
	{ "lerp",		"mix" },
	{ "rsqrt",		"inversesqrt" },
	{ "ddx",		"dFdx" },
	{ "ddy",		"dFdy" },

	{ NULL, NULL }
};

struct inOutVariable_t {
	idStr	type;
	idStr	nameCg;
	idStr	nameGLSL;
	bool	declareInOut;
};

/*
========================
ParseInOutStruct
========================
*/
void ParseInOutStruct( idLexer & src, int attribType, idList< inOutVariable_t > & inOutVars ) {
	src.ExpectTokenString( "{" );

	while( !src.CheckTokenString( "}" ) ) {
		inOutVariable_t var;

		idToken token;
		src.ReadToken( &token );
		var.type = token;
		src.ReadToken( &token );
		var.nameCg = token;

		if ( !src.CheckTokenString( ":" ) ) {
			src.SkipUntilString( ";" );
			continue;
		}

		src.ReadToken( &token );
		var.nameGLSL = token;
		src.ExpectTokenString( ";" );

		// convert the type
		for ( int i = 0; typeConversion[i].typeCG != NULL; i++ ) {
			if ( var.type.Cmp( typeConversion[i].typeCG ) == 0 ) {
				var.type = typeConversion[i].typeGLSL;
				break;
			}
		}

		// convert the semantic to a GLSL name
		for ( int i = 0; attribsPC[i].semantic != NULL; i++ ) {
			if ( ( attribsPC[i].flags & attribType ) != 0 ) {
				if ( var.nameGLSL.Cmp( attribsPC[i].semantic ) == 0 ) {
					var.nameGLSL = attribsPC[i].glsl;
					break;
				}
			}
		}

		// check if it was defined previously
		var.declareInOut = true;
		for ( int i = 0; i < inOutVars.Num(); i++ ) {
			if ( var.nameGLSL == inOutVars[i].nameGLSL ) {
				var.declareInOut = false;
				break;
			}
		}

		inOutVars.Append( var );
	}

	src.ExpectTokenString( ";" );
}

/*
========================
ConvertCG2GLSL
========================
*/
idStr ConvertCG2GLSL( const idStr & in, const char * name, bool isVertexProgram, idStr & uniforms ) {
	idStr program;
	program.ReAllocate( in.Length() * 2, false );

	idList< inOutVariable_t, TAG_RENDERPROG > varsIn;
	idList< inOutVariable_t, TAG_RENDERPROG > varsOut;
	idList< idStr > uniformList;

	idLexer src( LEXFL_NOFATALERRORS );
	src.LoadMemory( in.c_str(), in.Length(), name );

	bool inMain = false;
	const char * uniformArrayName = isVertexProgram ? VERTEX_UNIFORM_ARRAY_NAME : FRAGMENT_UNIFORM_ARRAY_NAME;
	char newline[128] = { "\n" };

	idToken token;
	while ( src.ReadToken( &token ) ) {

		// check for uniforms
		while ( token == "uniform" && src.CheckTokenString( "float4" ) ) {
			src.ReadToken( &token );
			uniformList.Append( token );

			// strip ': register()' from uniforms
			if ( src.CheckTokenString( ":" ) ) {
				if ( src.CheckTokenString( "register" ) ) {
					src.SkipUntilString( ";" );
				}
			}

			src.ReadToken( & token );
		}

		// convert the in/out structs
		if ( token == "struct" ) {
			if ( src.CheckTokenString( "VS_IN" ) ) {
				ParseInOutStruct( src, AT_VS_IN, varsIn );
				program += "\n\n";
				for ( int i = 0; i < varsIn.Num(); i++ ) {
					if ( varsIn[i].declareInOut ) {
						program += "in " + varsIn[i].type + " " + varsIn[i].nameGLSL + ";\n";
					}
				}
				continue;
			} else if ( src.CheckTokenString( "VS_OUT" ) ) {
				ParseInOutStruct( src, AT_VS_OUT, varsOut );
				program += "\n";
				for ( int i = 0; i < varsOut.Num(); i++ ) {
					if ( varsOut[i].declareInOut ) {
						program += "out " + varsOut[i].type + " " + varsOut[i].nameGLSL + ";\n";
					}
				}
				continue;
			} else if ( src.CheckTokenString( "PS_IN" ) ) {
				ParseInOutStruct( src, AT_PS_IN, varsIn );
				program += "\n\n";
				for ( int i = 0; i < varsIn.Num(); i++ ) {
					if ( varsIn[i].declareInOut ) {
						program += "in " + varsIn[i].type + " " + varsIn[i].nameGLSL + ";\n";
					}
				}
				inOutVariable_t var;
				var.type = "vec4";
				var.nameCg = "position";
				var.nameGLSL = "gl_FragCoord";
				varsIn.Append( var );
				continue;
			} else if ( src.CheckTokenString( "PS_OUT" ) ) {
				ParseInOutStruct( src, AT_PS_OUT, varsOut );
				program += "\n";
				for ( int i = 0; i < varsOut.Num(); i++ ) {
					if ( varsOut[i].declareInOut ) {
						program += "out " + varsOut[i].type + " " + varsOut[i].nameGLSL + ";\n";
					}
				}
				continue;
			}
		}

		// strip 'static'
		if ( token == "static" ) {
			program += ( token.linesCrossed > 0 ) ? newline : ( token.WhiteSpaceBeforeToken() > 0 ? " " : "" );
			src.SkipWhiteSpace( true ); // remove white space between 'static' and the next token
			continue;
		}

		// strip ': register()' from uniforms
		if ( token == ":" ) {
			if ( src.CheckTokenString( "register" ) ) {
				src.SkipUntilString( ";" );
				program += ";";
				continue;
			}
		}

		// strip in/program parameters from main
		if ( token == "void" && src.CheckTokenString( "main" ) ) {
			src.ExpectTokenString( "(" );
			while( src.ReadToken( &token ) ) {
				if ( token == ")" ) {
					break;
				}
			}

			program += "\nvoid main()";
			inMain = true;
			continue;
		}

		// strip 'const' from local variables in main()
		if ( token == "const" && inMain ) {
			program += ( token.linesCrossed > 0 ) ? newline : ( token.WhiteSpaceBeforeToken() > 0 ? " " : "" );
			src.SkipWhiteSpace( true ); // remove white space between 'const' and the next token
			continue;
		}

		// maintain indentation
		if ( token == "{" ) {
			program += ( token.linesCrossed > 0 ) ? newline : ( token.WhiteSpaceBeforeToken() > 0 ? " " : "" );
			program += "{";

			int len = Min( idStr::Length( newline ) + 1, (int)sizeof( newline ) - 1 );
			newline[len - 1] = '\t';
			newline[len - 0] = '\0';
			continue;
		}
		if ( token == "}" ) {
			int len = Max( idStr::Length( newline ) - 1, 0 );
			newline[len] = '\0';

			program += ( token.linesCrossed > 0 ) ? newline : ( token.WhiteSpaceBeforeToken() > 0 ? " " : "" );
			program += "}";
			continue;
		}

		// check for a type conversion
		bool foundType = false;
		for ( int i = 0; typeConversion[i].typeCG != NULL; i++ ) {
			if ( token.Cmp( typeConversion[i].typeCG ) == 0 ) {
				program += ( token.linesCrossed > 0 ) ? newline : ( token.WhiteSpaceBeforeToken() > 0 ? " " : "" );
				program += typeConversion[i].typeGLSL;
				foundType = true;
				break;
			}
		}
		if ( foundType ) {
			continue;
		}

		// check for uniforms that need to be converted to the array
		bool isUniform = false;
		for ( int i = 0; i < uniformList.Num(); i++ ) {
			if ( token == uniformList[i] ) {
				program += ( token.linesCrossed > 0 ) ? newline : ( token.WhiteSpaceBeforeToken() > 0 ? " " : "" );
				program += va( "%s[%d /* %s */]", uniformArrayName, i, uniformList[i].c_str() );
				isUniform = true;
				break;
			}
		}
		if ( isUniform ) {
			continue;
		}

		// check for input/output parameters
		if ( src.CheckTokenString( "." ) ) {

			if ( token == "vertex" || token == "fragment" ) {
				idToken member;
				src.ReadToken( &member );

				bool foundInOut = false;
				for ( int i = 0; i < varsIn.Num(); i++ ) {
					if ( member.Cmp( varsIn[i].nameCg ) == 0 ) {
						program += ( token.linesCrossed > 0 ) ? newline : ( token.WhiteSpaceBeforeToken() > 0 ? " " : "" );
						program += varsIn[i].nameGLSL;
						foundInOut = true;
						break;
					}
				}
				if ( !foundInOut ) {
					src.Error( "invalid input parameter %s.%s", token.c_str(), member.c_str() );
					program += ( token.linesCrossed > 0 ) ? newline : ( token.WhiteSpaceBeforeToken() > 0 ? " " : "" );
					program += token;
					program += ".";
					program += member;
				}
				continue;
			}

			if ( token == "result" ) {
				idToken member;
				src.ReadToken( &member );

				bool foundInOut = false;
				for ( int i = 0; i < varsOut.Num(); i++ ) {
					if ( member.Cmp( varsOut[i].nameCg ) == 0 ) {
						program += ( token.linesCrossed > 0 ) ? newline : ( token.WhiteSpaceBeforeToken() > 0 ? " " : "" );
						program += varsOut[i].nameGLSL;
						foundInOut = true;
						break;
					}
				}
				if ( !foundInOut ) {
					src.Error( "invalid output parameter %s.%s", token.c_str(), member.c_str() );
					program += ( token.linesCrossed > 0 ) ? newline : ( token.WhiteSpaceBeforeToken() > 0 ? " " : "" );
					program += token;
					program += ".";
					program += member;
				}
				continue;
			}

			program += ( token.linesCrossed > 0 ) ? newline : ( token.WhiteSpaceBeforeToken() > 0 ? " " : "" );
			program += token;
			program += ".";
			continue;
		}

		// check for a function conversion
		bool foundFunction = false;
		for ( int i = 0; builtinConversion[i].nameCG != NULL; i++ ) {
			if ( token.Cmp( builtinConversion[i].nameCG ) == 0 ) {
				program += ( token.linesCrossed > 0 ) ? newline : ( token.WhiteSpaceBeforeToken() > 0 ? " " : "" );
				program += builtinConversion[i].nameGLSL;
				foundFunction = true;
				break;
			}
		}
		if ( foundFunction ) {
			continue;
		}

		program += ( token.linesCrossed > 0 ) ? newline : ( token.WhiteSpaceBeforeToken() > 0 ? " " : "" );
		program += token;
	}

	idStr out;

	if ( isVertexProgram ) {
		out.ReAllocate( idStr::Length( vertexInsert ) + in.Length() * 2, false );
		out += vertexInsert;
	} else {
		out.ReAllocate( idStr::Length( fragmentInsert ) + in.Length() * 2, false );
		out += fragmentInsert;
	}

	if ( uniformList.Num() > 0 ) {
		out += va( "\nuniform vec4 %s[%d];\n", uniformArrayName, uniformList.Num() );
	}

	out += program;

	for ( int i = 0; i < uniformList.Num(); i++ ) {
		uniforms += uniformList[i];
		uniforms += "\n";
	}
	uniforms += "\n";

	return out;
}

/*
========================
idRenderProgManager::idRenderProgManager
========================
*/
idRenderProgManager::idRenderProgManager() : 
	m_current( 0 ) {
	
}

/*
========================
idRenderProgManager::Init
========================
*/
void idRenderProgManager::Init() {
	idLib::Printf( "----- Initializing Render Shaders -----\n" );

	struct builtinShaders_t {
		int index;
		const char * name;
		rpStage_t stages;
		vertexLayoutType_t layout;
	} builtins[ MAX_BUILTINS ] = {
		{ BUILTIN_GUI, "gui", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_COLOR, "color", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_SIMPLESHADE, "simpleshade", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_TEXTURED, "texture", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_TEXTURE_VERTEXCOLOR, "texture_color", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_TEXTURE_VERTEXCOLOR_SKINNED, "texture_color_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_TEXTURE_TEXGEN_VERTEXCOLOR, "texture_color_texgen", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_INTERACTION, "interaction", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_INTERACTION_SKINNED, "interaction_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_INTERACTION_AMBIENT, "interactionAmbient", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_INTERACTION_AMBIENT_SKINNED, "interactionAmbient_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_ENVIRONMENT, "environment", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_ENVIRONMENT_SKINNED, "environment_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_BUMPY_ENVIRONMENT, "bumpyEnvironment", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_BUMPY_ENVIRONMENT_SKINNED, "bumpyEnvironment_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },

		{ BUILTIN_DEPTH, "depth", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_DEPTH_SKINNED, "depth_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_SHADOW, "shadow", SHADER_STAGE_VERTEX, LAYOUT_DRAW_SHADOW_VERT },
		{ BUILTIN_SHADOW_SKINNED, "shadow_skinned", SHADER_STAGE_VERTEX, LAYOUT_DRAW_SHADOW_VERT_SKINNED },
		{ BUILTIN_SHADOW_DEBUG, "shadowDebug", SHADER_STAGE_ALL, LAYOUT_DRAW_SHADOW_VERT },
		{ BUILTIN_SHADOW_DEBUG_SKINNED, "shadowDebug_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_SHADOW_VERT_SKINNED },

		{ BUILTIN_BLENDLIGHT, "blendlight", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_FOG, "fog", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_FOG_SKINNED, "fog_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_SKYBOX, "skybox", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_WOBBLESKY, "wobblesky", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_BINK, "bink", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_BINK_GUI, "bink_gui", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
	};
	m_renderProgs.SetNum( MAX_BUILTINS );

	for ( int i = 0; i < MAX_BUILTINS; i++ ) {
		int vIndex = -1;
		if ( builtins[ i ].stages & SHADER_STAGE_VERTEX ) {
			vIndex = FindShader( builtins[ i ].name, SHADER_STAGE_VERTEX );
		}

		int fIndex = -1;
		if ( builtins[ i ].stages & SHADER_STAGE_FRAGMENT ) {
			fIndex = FindShader( builtins[ i ].name, SHADER_STAGE_FRAGMENT );
		}
		
		LoadGLSLProgram( i, vIndex, fIndex );

		m_renderProgs[ i ].vertexLayoutType = builtins[ i ].layout;
	}

	m_uniforms.SetNum( RENDERPARM_TOTAL, vec4_zero );

	m_renderProgs[ BUILTIN_TEXTURE_VERTEXCOLOR_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_INTERACTION_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_INTERACTION_AMBIENT_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_ENVIRONMENT_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_BUMPY_ENVIRONMENT_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_DEPTH_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_SHADOW_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_SHADOW_DEBUG_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_FOG_SKINNED ].usesJoints = true;
}

/*
========================
idRenderProgManager::Shutdown
========================
*/
void idRenderProgManager::Shutdown() {
	Unbind();
	for ( int i = 0; i < m_shaders.Num(); i++ ) {
		shader_t & shader = m_shaders[ i ];
		if ( shader.progId != INVALID_PROGID ) {
			qglDeleteShader( shader.progId );
			shader.progId = INVALID_PROGID;
		}
	}
	for ( int i = 0; i < m_renderProgs.Num(); ++i ) {
		renderProg_t & prog = m_renderProgs[ i ];
		if ( prog.progId != INVALID_PROGID ) {
			qglDeleteProgram( prog.progId );
			prog.progId = INVALID_PROGID;
		}
	}
}

/*
========================
idRenderProgManager::StartFrame
========================
*/
void idRenderProgManager::StartFrame() {
	
}

/*
========================
idRenderProgManager::BindProgram
========================
*/
void idRenderProgManager::BindProgram( int index ) {
	if ( m_current == index ) {
		return;
	}

	m_current = index;
	RENDERLOG_PRINTF( "Binding GLSL Program %s\n", m_renderProgs[ index ].name.c_str() );
	qglUseProgram( m_renderProgs[ index ].progId );
}

/*
========================
idRenderProgManager::Unbind
========================
*/
void idRenderProgManager::Unbind() {
	m_current = -1;
	qglUseProgram( 0 );
}

/*
========================
idRenderProgManager::CommitCurrent
========================
*/
void idRenderProgManager::CommitCurrent( uint64 stateBits ) {
	const int progID = m_current;
	renderProg_t & prog = m_renderProgs[ progID ];

	prog.states.AddUnique( stateBits );

	ALIGNTYPE16 idVec4 localVectors[ RENDERPARM_TOTAL ];

	auto commitarray = [&] ( idVec4 (&vectors)[ RENDERPARM_TOTAL ] , shader_t & shader ) {
		const int numUniforms = shader.uniforms.Num();
		if ( shader.uniformArray != -1 && numUniforms > 0 ) {
			for ( int i = 0; i < numUniforms; ++i ) {
				vectors[ i ] = m_uniforms[ shader.uniforms[ i ] ];
			}
			qglUniform4fv( shader.uniformArray, numUniforms, localVectors->ToFloatPtr() );
		}
	};

	if ( prog.vertexShaderIndex >= 0 ) {
		commitarray( localVectors, m_shaders[ prog.vertexShaderIndex ] );
	}

	if ( prog.fragmentShaderIndex >= 0 ) {
		commitarray( localVectors, m_shaders[ prog.fragmentShaderIndex ] );
	}

	switch ( prog.vertexLayoutType ) {
		case LAYOUT_DRAW_VERT: {
			qglEnableVertexAttribArrayARB( PC_ATTRIB_INDEX_VERTEX );
			qglEnableVertexAttribArrayARB( PC_ATTRIB_INDEX_NORMAL );
			qglEnableVertexAttribArrayARB( PC_ATTRIB_INDEX_COLOR );
			qglEnableVertexAttribArrayARB( PC_ATTRIB_INDEX_COLOR2 );
			qglEnableVertexAttribArrayARB( PC_ATTRIB_INDEX_ST );
			qglEnableVertexAttribArrayARB( PC_ATTRIB_INDEX_TANGENT );

			qglVertexAttribPointerARB( PC_ATTRIB_INDEX_VERTEX, 3, GL_FLOAT, GL_FALSE, sizeof( idDrawVert ), (void *)( DRAWVERT_XYZ_OFFSET ) );
			qglVertexAttribPointerARB( PC_ATTRIB_INDEX_NORMAL, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof( idDrawVert ), (void *)( DRAWVERT_NORMAL_OFFSET ) );
			qglVertexAttribPointerARB( PC_ATTRIB_INDEX_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof( idDrawVert ), (void *)( DRAWVERT_COLOR_OFFSET ) );
			qglVertexAttribPointerARB( PC_ATTRIB_INDEX_COLOR2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof( idDrawVert ), (void *)( DRAWVERT_COLOR2_OFFSET ) );
			qglVertexAttribPointerARB( PC_ATTRIB_INDEX_ST, 2, GL_HALF_FLOAT, GL_TRUE, sizeof( idDrawVert ), (void *)( DRAWVERT_ST_OFFSET ) );
			qglVertexAttribPointerARB( PC_ATTRIB_INDEX_TANGENT, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof( idDrawVert ), (void *)( DRAWVERT_TANGENT_OFFSET ) );
			break;
		}
		case LAYOUT_DRAW_SHADOW_VERT: {
			qglEnableVertexAttribArrayARB( PC_ATTRIB_INDEX_VERTEX );
			qglDisableVertexAttribArrayARB( PC_ATTRIB_INDEX_NORMAL );
			qglDisableVertexAttribArrayARB( PC_ATTRIB_INDEX_COLOR );
			qglDisableVertexAttribArrayARB( PC_ATTRIB_INDEX_COLOR2 );
			qglDisableVertexAttribArrayARB( PC_ATTRIB_INDEX_ST );
			qglDisableVertexAttribArrayARB( PC_ATTRIB_INDEX_TANGENT );

			qglVertexAttribPointerARB( PC_ATTRIB_INDEX_VERTEX, 4, GL_FLOAT, GL_FALSE, sizeof( idShadowVert ), (void *)( SHADOWVERT_XYZW_OFFSET ) );
			break;
		}
		case LAYOUT_DRAW_SHADOW_VERT_SKINNED: {
			qglEnableVertexAttribArrayARB( PC_ATTRIB_INDEX_VERTEX );
			qglDisableVertexAttribArrayARB( PC_ATTRIB_INDEX_NORMAL );
			qglEnableVertexAttribArrayARB( PC_ATTRIB_INDEX_COLOR );
			qglEnableVertexAttribArrayARB( PC_ATTRIB_INDEX_COLOR2 );
			qglDisableVertexAttribArrayARB( PC_ATTRIB_INDEX_ST );
			qglDisableVertexAttribArrayARB( PC_ATTRIB_INDEX_TANGENT );

			qglVertexAttribPointerARB( PC_ATTRIB_INDEX_VERTEX, 4, GL_FLOAT, GL_FALSE, sizeof( idShadowVertSkinned ), (void *)( SHADOWVERTSKINNED_XYZW_OFFSET ) );
			qglVertexAttribPointerARB( PC_ATTRIB_INDEX_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof( idShadowVertSkinned ), (void *)( SHADOWVERTSKINNED_COLOR_OFFSET ) );
			qglVertexAttribPointerARB( PC_ATTRIB_INDEX_COLOR2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof( idShadowVertSkinned ), (void *)( SHADOWVERTSKINNED_COLOR2_OFFSET ) );
			break;
		}
		default:
			idLib::FatalError( "idRenderProgManager::CommitCurrent: %s has an invalid vertex layout.", prog.name.c_str() );
			break;
	}
}

/*
========================
idRenderProgManager::FindProgram
========================
*/
int idRenderProgManager::FindProgram( const char * name, int vIndex, int fIndex ) {
	for ( int i = 0; i < m_renderProgs.Num(); ++i ) {
		renderProg_t & prog = m_renderProgs[ i ];
		if ( ( prog.vertexShaderIndex == vIndex ) && ( prog.fragmentShaderIndex == fIndex ) ) {
			LoadGLSLProgram( i, vIndex, fIndex );
			return i;
		}
	}

	renderProg_t program;
	program.name = name;
	int index = m_renderProgs.Append( program );
	LoadGLSLProgram( index, vIndex, fIndex );
	return index;
}

/*
========================
idRenderProgManager::LoadShader
========================
*/
void idRenderProgManager::LoadShader( int index ) {
	if ( m_shaders[ index ].progId != INVALID_PROGID ) {
		return; // Already loaded
	}
	
	LoadShader( m_shaders[ index ] );
}

/*
========================
idRenderProgManager::LoadShader
========================
*/
void idRenderProgManager::LoadShader( shader_t & shader ) {
	idStr inFile;
	idStr outFileHLSL;
	idStr outFileGLSL;
	idStr outFileUniforms;
	inFile.Format( "renderprogs\\%s", shader.name.c_str() );
	inFile.StripFileExtension();
	outFileHLSL.Format( "renderprogs\\glsl\\%s", shader.name.c_str() );
	outFileHLSL.StripFileExtension();
	outFileGLSL.Format( "renderprogs\\glsl\\%s", shader.name.c_str() );
	outFileGLSL.StripFileExtension();
	outFileUniforms.Format( "renderprogs\\glsl\\%s", shader.name.c_str() );
	outFileUniforms.StripFileExtension();

	GLenum glTarget;
	if ( shader.stage == SHADER_STAGE_FRAGMENT ) {
		glTarget = GL_FRAGMENT_SHADER;
		inFile += ".pixel";
		outFileHLSL += "_fragment.hlsl";
		outFileGLSL += "_fragment.glsl";
		outFileUniforms += "_fragment.uniforms";
	} else {
		glTarget = GL_VERTEX_SHADER;
		inFile += ".vertex";
		outFileHLSL += "_vertex.hlsl";
		outFileGLSL += "_vertex.glsl";
		outFileUniforms += "_vertex.uniforms";
	}

	// first check whether we already have a valid GLSL file and compare it to the hlsl timestamp;
	ID_TIME_T hlslTimeStamp;
	int hlslFileLength = fileSystem->ReadFile( inFile.c_str(), NULL, &hlslTimeStamp );

	ID_TIME_T glslTimeStamp;
	int glslFileLength = fileSystem->ReadFile( outFileGLSL.c_str(), NULL, &glslTimeStamp );

	// if the glsl file doesn't exist or we have a newer HLSL file we need to recreate the glsl file.
	idStr programGLSL;
	idStr programUniforms;
	if ( ( glslFileLength <= 0 ) || ( hlslTimeStamp > glslTimeStamp ) ) {
		if ( hlslFileLength <= 0 ) {
			// hlsl file doesn't even exist bail out
			return;
		}

		void * hlslFileBuffer = NULL;
		int len = fileSystem->ReadFile( inFile.c_str(), &hlslFileBuffer );
		if ( len <= 0 ) {
			return;
		}
		idStr programHLSL( ( const char* ) hlslFileBuffer );
		programGLSL = ConvertCG2GLSL( programHLSL, inFile, shader.stage == SHADER_STAGE_VERTEX, programUniforms );

		fileSystem->WriteFile( outFileHLSL, programHLSL.c_str(), programHLSL.Length(), "fs_basepath" );
		fileSystem->WriteFile( outFileGLSL, programGLSL.c_str(), programGLSL.Length(), "fs_basepath" );
		fileSystem->WriteFile( outFileUniforms, programUniforms.c_str(), programUniforms.Length(), "fs_basepath" );
	} else {
		// read in the glsl file
		void * fileBufferGLSL = NULL;
		int lengthGLSL = fileSystem->ReadFile( outFileGLSL.c_str(), &fileBufferGLSL );
		if ( lengthGLSL <= 0 ) {
			idLib::Error( "GLSL file %s could not be loaded and may be corrupt", outFileGLSL.c_str() );
		}
		programGLSL = ( const char * ) fileBufferGLSL;
		Mem_Free( fileBufferGLSL );

		//fileSystem->WriteFile( outFileGLSL, programGLSL.c_str(), programGLSL.Length() );

		// read in the uniform file
		void * fileBufferUniforms = NULL;
		int lengthUniforms = fileSystem->ReadFile( outFileUniforms.c_str(), &fileBufferUniforms );
		if ( lengthUniforms <= 0 ) {
			idLib::Error( "uniform file %s could not be loaded and may be corrupt", outFileUniforms.c_str() );
		}
		programUniforms = ( const char* ) fileBufferUniforms;

		//fileSystem->WriteFile( outFileUniforms.c_str(), fileBufferUniforms, lengthUniforms );

		Mem_Free( fileBufferUniforms );
	}

	// find the uniforms locations in either the vertex or fragment uniform array
	shader.uniforms.Clear();

	idLexer src( programUniforms, programUniforms.Length(), "uniforms" );
	idToken token;
	while ( src.ReadToken( &token ) ) {
		int index = -1;
		for ( int i = 0; i < RENDERPARM_TOTAL && index == -1; i++ ) {
			if ( token == GLSLParmNames[ i ] ) {
				index = i;
			}
		}

		if ( index == -1 ) {
			idLib::Error( "Invalid uniform %s", token.c_str() );
		}

		shader.uniforms.Append( static_cast< renderParm_t >( index ) );
	}

	// create and compile the shader
	shader.progId = qglCreateShader( glTarget );
	if ( shader.progId ) {
		const char * glslSource[1] = { programGLSL.c_str() };

		qglShaderSource( shader.progId, 1, glslSource, NULL );
		qglCompileShader( shader.progId );

		int infologLength = 0;
		qglGetShaderiv( shader.progId, GL_INFO_LOG_LENGTH, &infologLength );
		if ( infologLength > 1 ) {
			idTempArray<char> infoLog( infologLength );
			int charsWritten = 0;
			qglGetShaderInfoLog( shader.progId, infologLength, &charsWritten, infoLog.Ptr() );

			// catch the strings the ATI and Intel drivers output on success
			if ( strstr( infoLog.Ptr(), "successfully compiled to run on hardware" ) != NULL || 
					strstr( infoLog.Ptr(), "No errors." ) != NULL ) {
				//idLib::Printf( "%s program %s from %s compiled to run on hardware\n", typeName, GetName(), GetFileName() );
			} else {
				idLib::Printf( "While compiling %s program %s\n", ( shader.stage == SHADER_STAGE_FRAGMENT ) ? "fragment" : "vertex" , inFile.c_str() );

				const char separator = '\n';
				idList<idStr> lines;
				lines.Clear();
				idStr source( programGLSL );
				lines.Append( source );
				for ( int index = 0, ofs = lines[index].Find( separator ); ofs != -1; index++, ofs = lines[index].Find( separator ) ) {
					lines.Append( lines[index].c_str() + ofs + 1 );
					lines[index].CapLength( ofs );
				}

				idLib::Printf( "-----------------\n" );
				for ( int i = 0; i < lines.Num(); i++ ) {
					idLib::Printf( "%3d: %s\n", i+1, lines[i].c_str() );
				}
				idLib::Printf( "-----------------\n" );

				idLib::Printf( "%s\n", infoLog.Ptr() );
			}
		}

		GLint compiled = GL_FALSE;
		qglGetShaderiv( shader.progId, GL_COMPILE_STATUS, &compiled );
		if ( compiled == GL_FALSE ) {
			qglDeleteShader( shader.progId );
			return;
		}
	}
}

/*
========================
idRenderProgManager::LoadGLSLProgram
========================
*/
void idRenderProgManager::LoadGLSLProgram( const int programIndex, const int vertexShaderIndex, const int fragmentShaderIndex ) {
	renderProg_t & prog = m_renderProgs[ programIndex ];

	if ( prog.progId != INVALID_PROGID ) {
		return; // Already loaded
	}

	GLuint vertexProgID = ( vertexShaderIndex != -1 ) ? m_shaders[ vertexShaderIndex ].progId : INVALID_PROGID;
	GLuint fragmentProgID = ( fragmentShaderIndex != -1 ) ? m_shaders[ fragmentShaderIndex ].progId : INVALID_PROGID;

	const GLuint program = qglCreateProgram();
	if ( program ) {

		if ( vertexProgID != INVALID_PROGID ) {
			qglAttachShader( program, vertexProgID );
		}

		if ( fragmentProgID != INVALID_PROGID ) {
			qglAttachShader( program, fragmentProgID );
		}

		// bind vertex attribute locations
		for ( int i = 0; attribsPC[i].glsl != NULL; i++ ) {
			if ( ( attribsPC[i].flags & AT_VS_IN ) != 0 ) {
				qglBindAttribLocation( program, attribsPC[i].bind, attribsPC[i].glsl );
			}
		}

		qglLinkProgram( program );

		int infologLength = 0;
		qglGetProgramiv( program, GL_INFO_LOG_LENGTH, &infologLength );
		if ( infologLength > 1 ) {
			char * infoLog = (char *)malloc( infologLength );
			int charsWritten = 0;
			qglGetProgramInfoLog( program, infologLength, &charsWritten, infoLog );

			// catch the strings the ATI and Intel drivers output on success
			if ( strstr( infoLog, "Vertex shader(s) linked, fragment shader(s) linked." ) != NULL || strstr( infoLog, "No errors." ) != NULL ) {
				//idLib::Printf( "render prog %s from %s linked\n", GetName(), GetFileName() );
			} else {
				idLib::Printf( "While linking GLSL program %d with vertexShader %s and fragmentShader %s\n", 
					programIndex, 
					( vertexShaderIndex >= 0 ) ? m_shaders[ vertexShaderIndex ].name.c_str() : "<Invalid>", 
					( fragmentShaderIndex >= 0 ) ? m_shaders[ fragmentShaderIndex ].name.c_str() : "<Invalid>" );
				idLib::Printf( "%s\n", infoLog );
			}

			free( infoLog );
		}
	}

	int linked = GL_FALSE;
	qglGetProgramiv( program, GL_LINK_STATUS, &linked );
	if ( linked == GL_FALSE ) {
		qglDeleteProgram( program );
		idLib::Error( "While linking GLSL program %d with vertexShader %s and fragmentShader %s\n", 
			programIndex, 
			( vertexShaderIndex >= 0 ) ? m_shaders[ vertexShaderIndex ].name.c_str() : "<Invalid>", 
			( fragmentShaderIndex >= 0 ) ? m_shaders[ fragmentShaderIndex ].name.c_str() : "<Invalid>" );
		return;
	}

	if ( vertexShaderIndex > -1 && m_shaders[ vertexShaderIndex ].uniforms.Num() > 0 ) {
		m_shaders[ vertexShaderIndex ].uniformArray = qglGetUniformLocation( program, VERTEX_UNIFORM_ARRAY_NAME );
	}
	if ( fragmentShaderIndex > -1 && m_shaders[ fragmentShaderIndex ].uniforms.Num() > 0 ) {
		m_shaders[ fragmentShaderIndex ].uniformArray = qglGetUniformLocation( program, FRAGMENT_UNIFORM_ARRAY_NAME );
	}

	// get the uniform buffer binding for skinning joint matrices
	GLint blockIndex = qglGetUniformBlockIndex( program, "matrices_ubo" );
	if ( blockIndex != -1 ) {
		qglUniformBlockBinding( program, blockIndex, 0 );
	}

	// set the texture unit locations once for the render program. We only need to do this once since we only link the program once
	qglUseProgram( program );
	for ( int i = 0; i < MAX_PROG_TEXTURE_PARMS; ++i ) {
		GLint loc = qglGetUniformLocation( program, va( "samp%d", i ) );
		if ( loc != -1 ) {
			qglUniform1i( loc, i );
		}
	}

	idStr programName = m_shaders[ vertexShaderIndex ].name;
	programName.StripFileExtension();
	prog.name = programName;
	prog.progId = program;
	prog.fragmentShaderIndex = fragmentShaderIndex;
	prog.vertexShaderIndex = vertexShaderIndex;

	// FIXME: we should really scan the program source code for using rpEnableSkinning but at this
	// point we directly load a binary and the program source code is not available on the consoles
	if (	idStr::Icmp( prog.name.c_str(), "heatHaze.vfp" ) == 0 ||
			idStr::Icmp( prog.name.c_str(), "heatHazeWithMask.vfp" ) == 0 ||
			idStr::Icmp( prog.name.c_str(), "heatHazeWithMaskAndVertex.vfp" ) == 0 ) {
		prog.usesJoints = true;
		prog.optionalSkinning = true;
	}
}

CONSOLE_COMMAND( OpenGL_PrintRenderProgStates, "Print the GLState bits associated with each renderprog.", 0 ) {
	for ( int i = 0; i < renderProgManager.m_renderProgs.Num(); ++i ) {
		renderProg_t & prog = renderProgManager.m_renderProgs[ i ];
		for ( int j = 0; j < prog.states.Num(); ++j ) {
			idLib::Printf( "%s: %llu\n", prog.name.c_str(), prog.states[ j ] );
			idLib::Printf( "------------------------------------------\n" );
			RpPrintState( prog.states[ j ], glcontext.stencilOperations );
			idLib::Printf( "\n" );
		}
	}
}