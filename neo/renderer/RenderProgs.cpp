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
#include "../framework/precompiled.h"
#include "GLState.h"
#include "RenderProgs.h"
#include "RenderBackend.h"

idRenderProgManager renderProgManager;

// For GLSL we need to have the names for the renderparms so we can look up 
// their run time indices within the renderprograms
const char * GLSLParmNames[ RENDERPARM_TOTAL ] = {
	"rpScreenCorrectionFactor",
	"rpWindowCoord",
	"rpDiffuseModifier",
	"rpSpecularModifier",

	"rpLocalLightOrigin",
	"rpLocalViewOrigin",

	"rpLightProjectionS",
	"rpLightProjectionT",
	"rpLightProjectionQ",
	"rpLightFalloffS",

	"rpBumpMatrixS",
	"rpBumpMatrixT",

	"rpDiffuseMatrixS",
	"rpDiffuseMatrixT",

	"rpSpecularMatrixS",
	"rpSpecularMatrixT",

	"rpVertexColorModulate",
	"rpVertexColorAdd",

	"rpColor",
	"rpViewOrigin",
	"rpGlobalEyePos",

	"rpMVPmatrixX",
	"rpMVPmatrixY",
	"rpMVPmatrixZ",
	"rpMVPmatrixW",

	"rpModelMatrixX",
	"rpModelMatrixY",
	"rpModelMatrixZ",
	"rpModelMatrixW",

	"rpProjectionMatrixX",
	"rpProjectionMatrixY",
	"rpProjectionMatrixZ",
	"rpProjectionMatrixW",

	"rpModelViewMatrixX",
	"rpModelViewMatrixY",
	"rpModelViewMatrixZ",
	"rpModelViewMatrixW",

	"rpTextureMatrixS",
	"rpTextureMatrixT",

	"rpTexGen0S",
	"rpTexGen0T",
	"rpTexGen0Q",
	"rpTexGen0Enabled",

	"rpTexGen1S",
	"rpTexGen1T",
	"rpTexGen1Q",
	"rpTexGen1Enabled",

	"rpWobbleSkyX",
	"rpWobbleSkyY",
	"rpWobbleSkyZ",

	"rpOverbright",
	"rpEnableSkinning",
	"rpAlphaTest",

	"rpUser0",
	"rpUser1",
	"rpUser2",
	"rpUser3",
	"rpUser4",
	"rpUser5",
	"rpUser6",
	"rpUser7"
};

/*
========================
idRenderProgManager::GetRenderParm
========================
*/
const idVec4 & idRenderProgManager::GetRenderParm( renderParm_t rp ) {
	return m_uniforms[ rp ];
}

/*
========================
idRenderProgManager::SetRenderParm
========================
*/
void idRenderProgManager::SetRenderParm( renderParm_t rp, const float * value ) {
	for ( int i = 0; i < 4; ++i ) {
		m_uniforms[rp][i] = value[i];
	}
}

/*
========================
idRenderProgManager::SetRenderParms
========================
*/
void idRenderProgManager::SetRenderParms( renderParm_t rp, const float * value, int num ) {
	for ( int i = 0; i < num; ++i ) {
		SetRenderParm( (renderParm_t)(rp + i), value + ( i * 4 ) );
	}
}

/*
========================
idRenderProgManager::FindShader
========================
*/
int idRenderProgManager::FindShader( const char * name, rpStage_t stage ) {
	idStr shaderName( name );
	shaderName.StripFileExtension();

	for ( int i = 0; i < m_shaders.Num(); i++ ) {
		shader_t & shader = m_shaders[ i ];
		if ( shader.name.Icmp( shaderName.c_str() ) == 0 && shader.stage == stage ) {
			LoadShader( i );
			return i;
		}
	}
	shader_t shader;
	shader.name = shaderName;
	shader.stage = stage;
	int index = m_shaders.Append( shader );
	LoadShader( index );
	return index;
}

/*
========================
RpPrintState
========================
*/
void RpPrintState( uint64 stateBits ) {

	// culling
	idLib::Printf( "Culling: " );
	switch ( stateBits & GLS_CULL_BITS ) {
		case GLS_CULL_FRONTSIDED:	idLib::Printf( "FRONTSIDED -> BACK" ); break;
		case GLS_CULL_BACKSIDED:	idLib::Printf( "BACKSIDED -> FRONT" ); break;
		case GLS_CULL_TWOSIDED:		idLib::Printf( "TWOSIDED" ); break;
		default:					idLib::Printf( "NA" ); break;
	}
	idLib::Printf( "\n" );

	// polygon mode
	idLib::Printf( "PolygonMode: %s\n", ( stateBits & GLS_POLYMODE_LINE ) ? "LINE" : "FILL" );

	// color mask
	idLib::Printf( "ColorMask: " );
	idLib::Printf( ( stateBits & GLS_REDMASK ) ? "_" : "R" );
	idLib::Printf( ( stateBits & GLS_GREENMASK ) ? "_" : "G" );
	idLib::Printf( ( stateBits & GLS_BLUEMASK ) ? "_" : "B" );
	idLib::Printf( ( stateBits & GLS_ALPHAMASK ) ? "_" : "A" );
	idLib::Printf( "\n" );
	
	// blend
	idLib::Printf( "Blend: src=" );
	switch ( stateBits & GLS_SRCBLEND_BITS ) {
		case GLS_SRCBLEND_ZERO:					idLib::Printf( "ZERO" ); break;
		case GLS_SRCBLEND_ONE:					idLib::Printf( "ONE" ); break;
		case GLS_SRCBLEND_DST_COLOR:			idLib::Printf( "DST_COLOR" ); break;
		case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:	idLib::Printf( "ONE_MINUS_DST_COLOR" ); break;
		case GLS_SRCBLEND_SRC_ALPHA:			idLib::Printf( "SRC_ALPHA" ); break;
		case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:	idLib::Printf( "ONE_MINUS_SRC_ALPHA" ); break;
		case GLS_SRCBLEND_DST_ALPHA:			idLib::Printf( "DST_ALPHA" ); break;
		case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:	idLib::Printf( "ONE_MINUS_DST_ALPHA" ); break;
		default:								idLib::Printf( "NA" ); break;
	}
	idLib::Printf( ", dst=" );
	switch ( stateBits & GLS_DSTBLEND_BITS ) {
		case GLS_DSTBLEND_ZERO:					idLib::Printf( "ZERO" ); break;
		case GLS_DSTBLEND_ONE:					idLib::Printf( "ONE" ); break;
		case GLS_DSTBLEND_SRC_COLOR:			idLib::Printf( "SRC_COLOR" ); break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:	idLib::Printf( "ONE_MINUS_SRC_COLOR" ); break;
		case GLS_DSTBLEND_SRC_ALPHA:			idLib::Printf( "SRC_ALPHA" ); break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:	idLib::Printf( "ONE_MINUS_SRC_ALPHA" ); break;
		case GLS_DSTBLEND_DST_ALPHA:			idLib::Printf( "DST_ALPHA" ); break;
		case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:	idLib::Printf( "ONE_MINUS_DST_ALPHA" ); break;
		default:								idLib::Printf( "NA" );
	}
	idLib::Printf( "\n" );
	
	// depth func
	idLib::Printf( "DepthFunc: " );
	switch ( stateBits & GLS_DEPTHFUNC_BITS ) {
		case GLS_DEPTHFUNC_EQUAL:	idLib::Printf( "EQUAL" ); break;
		case GLS_DEPTHFUNC_ALWAYS:	idLib::Printf( "ALWAYS" ); break;
		case GLS_DEPTHFUNC_LESS:	idLib::Printf( "LEQUAL" ); break;
		case GLS_DEPTHFUNC_GREATER: idLib::Printf( "GEQUAL" ); break;
		default:					idLib::Printf( "NA" ); break;
	}
	idLib::Printf( "\n" );
	
	// depth mask
	idLib::Printf( "DepthWrite: %s\n", ( stateBits & GLS_DEPTHMASK ) ? "FALSE" : "TRUE" );

	// depth bounds
	idLib::Printf( "DepthBounds: %s\n", ( stateBits & GLS_DEPTH_TEST_MASK ) ? "TRUE" : "FALSE" );

	// depth bias
	idLib::Printf( "DepthBias: %s\n", ( stateBits & GLS_POLYGON_OFFSET ) ? "TRUE" : "FALSE" );

	// stencil
	auto printStencil = [&] ( stencilFace_t face, uint64 bits, uint64 mask, uint64 ref ) {
		idLib::Printf( "Stencil: %s, ", ( bits & ( GLS_STENCIL_FUNC_BITS | GLS_STENCIL_OP_BITS ) ) ? "ON" : "OFF" );
		idLib::Printf( "Face=" );
		switch ( face ) {
			case STENCIL_FACE_FRONT: idLib::Printf( "FRONT" ); break;
			case STENCIL_FACE_BACK: idLib::Printf( "BACK" ); break;
		default: idLib::Printf( "BOTH" ); break;
		}
		idLib::Printf( ", Func=" );
		switch ( bits & GLS_STENCIL_FUNC_BITS ) {
			case GLS_STENCIL_FUNC_NEVER:	idLib::Printf( "NEVER" ); break;
			case GLS_STENCIL_FUNC_LESS:		idLib::Printf( "LESS" ); break;
			case GLS_STENCIL_FUNC_EQUAL:	idLib::Printf( "EQUAL" ); break;
			case GLS_STENCIL_FUNC_LEQUAL:	idLib::Printf( "LEQUAL" ); break;
			case GLS_STENCIL_FUNC_GREATER:	idLib::Printf( "GREATER" ); break;
			case GLS_STENCIL_FUNC_NOTEQUAL: idLib::Printf( "NOTEQUAL" ); break;
			case GLS_STENCIL_FUNC_GEQUAL:	idLib::Printf( "GEQUAL" ); break;
			case GLS_STENCIL_FUNC_ALWAYS:	idLib::Printf( "ALWAYS" ); break;
			default:						idLib::Printf( "NA" ); break;
		}
		idLib::Printf( ", OpFail=" );
		switch( bits & GLS_STENCIL_OP_FAIL_BITS ) {
			case GLS_STENCIL_OP_FAIL_KEEP:		idLib::Printf( "KEEP" ); break;
			case GLS_STENCIL_OP_FAIL_ZERO:		idLib::Printf( "ZERO" ); break;
			case GLS_STENCIL_OP_FAIL_REPLACE:	idLib::Printf( "REPLACE" ); break;
			case GLS_STENCIL_OP_FAIL_INCR:		idLib::Printf( "INCR" ); break;
			case GLS_STENCIL_OP_FAIL_DECR:		idLib::Printf( "DECR" ); break;
			case GLS_STENCIL_OP_FAIL_INVERT:	idLib::Printf( "INVERT" ); break;
			case GLS_STENCIL_OP_FAIL_INCR_WRAP: idLib::Printf( "INCR_WRAP" ); break;
			case GLS_STENCIL_OP_FAIL_DECR_WRAP: idLib::Printf( "DECR_WRAP" ); break;
			default:							idLib::Printf( "NA" ); break;
		}
		idLib::Printf( ", ZFail=" );
		switch( bits & GLS_STENCIL_OP_ZFAIL_BITS ) {
			case GLS_STENCIL_OP_ZFAIL_KEEP:			idLib::Printf( "KEEP" ); break;
			case GLS_STENCIL_OP_ZFAIL_ZERO:			idLib::Printf( "ZERO" ); break;
			case GLS_STENCIL_OP_ZFAIL_REPLACE:		idLib::Printf( "REPLACE" ); break;
			case GLS_STENCIL_OP_ZFAIL_INCR:			idLib::Printf( "INCR" ); break;
			case GLS_STENCIL_OP_ZFAIL_DECR:			idLib::Printf( "DECR" ); break;
			case GLS_STENCIL_OP_ZFAIL_INVERT:		idLib::Printf( "INVERT" ); break;
			case GLS_STENCIL_OP_ZFAIL_INCR_WRAP:	idLib::Printf( "INCR_WRAP" ); break;
			case GLS_STENCIL_OP_ZFAIL_DECR_WRAP:	idLib::Printf( "DECR_WRAP" ); break;
			default:								idLib::Printf( "NA" ); break;
		}
		idLib::Printf( ", OpPass=" );
		switch( bits & GLS_STENCIL_OP_PASS_BITS ) {
			case GLS_STENCIL_OP_PASS_KEEP:			idLib::Printf( "KEEP" ); break;
			case GLS_STENCIL_OP_PASS_ZERO:			idLib::Printf( "ZERO" ); break;
			case GLS_STENCIL_OP_PASS_REPLACE:		idLib::Printf( "REPLACE" ); break;
			case GLS_STENCIL_OP_PASS_INCR:			idLib::Printf( "INCR" ); break;
			case GLS_STENCIL_OP_PASS_DECR:			idLib::Printf( "DECR" ); break;
			case GLS_STENCIL_OP_PASS_INVERT:		idLib::Printf( "INVERT" ); break;
			case GLS_STENCIL_OP_PASS_INCR_WRAP:		idLib::Printf( "INCR_WRAP" ); break;
			case GLS_STENCIL_OP_PASS_DECR_WRAP:		idLib::Printf( "DECR_WRAP" ); break;
			default:								idLib::Printf( "NA" ); break;
		}
		idLib::Printf( ", mask=%llu, ref=%llu\n", mask, ref );
	};

	uint32 mask = uint32( ( stateBits & GLS_STENCIL_FUNC_MASK_BITS ) >> GLS_STENCIL_FUNC_MASK_SHIFT );
	uint32 ref = uint32( ( stateBits & GLS_STENCIL_FUNC_REF_BITS ) >> GLS_STENCIL_FUNC_REF_SHIFT );
	if ( stateBits & GLS_SEPARATE_STENCIL ) {
		printStencil( STENCIL_FACE_FRONT, ( stateBits & GLS_STENCIL_FRONT_OPS ), mask, ref );
		printStencil( STENCIL_FACE_BACK, ( stateBits & GLS_STENCIL_BACK_OPS ), mask, ref );
	} else {
		printStencil( STENCIL_FACE_NUM, stateBits, mask, ref );
	}
}