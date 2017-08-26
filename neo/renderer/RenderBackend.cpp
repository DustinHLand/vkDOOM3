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
#include "../sys/win32/win_local.h"
#include "../framework/Common_local.h"
#include "RenderSystem_local.h"
#include "RenderBackend.h"
#include "RenderLog.h"
#include "RenderProgs.h"
#include "ResolutionScale.h"
#include "GLState.h"
#include "GLMatrix.h"
#include "Image.h"

idCVar r_forceZPassStencilShadows( "r_forceZPassStencilShadows", "0", CVAR_RENDERER | CVAR_BOOL, "force Z-pass rendering for performance testing" );
idCVar r_useStencilShadowPreload( "r_useStencilShadowPreload", "1", CVAR_RENDERER | CVAR_BOOL, "use stencil shadow preload algorithm instead of Z-fail" );
idCVar r_skipShaderPasses( "r_skipShaderPasses", "0", CVAR_RENDERER | CVAR_BOOL, "" );
idCVar r_useLightStencilSelect( "r_useLightStencilSelect", "0", CVAR_RENDERER | CVAR_BOOL, "use stencil select pass" );
idCVar r_drawFlickerBox( "r_drawFlickerBox", "0", CVAR_RENDERER | CVAR_BOOL, "visual test for dropping frames" );
idCVar r_showSwapBuffers( "r_showSwapBuffers", "0", CVAR_BOOL, "Show timings from GL_BlockingSwapBuffers" ); 
idCVar r_syncEveryFrame( "r_syncEveryFrame", "1", CVAR_BOOL, "Don't let the GPU buffer execution past swapbuffers" ); 

extern idCVar r_gamma;
extern idCVar r_brightness;
extern idCVar r_fullscreen;
extern idCVar r_windowX;
extern idCVar r_windowY;
extern idCVar r_windowWidth;
extern idCVar r_windowHeight;
extern idCVar r_vidMode;
extern idCVar r_customWidth;
extern idCVar r_customHeight;
extern idCVar r_displayRefresh;
extern idCVar r_multiSamples;
extern idCVar r_lightScale;
extern idCVar rs_enable;
extern idCVar r_offsetFactor;
extern idCVar r_offsetUnits;
extern idCVar r_useScissor;
extern idCVar r_useLightDepthBounds;
extern idCVar r_showOverDraw;

extern idCVar r_skipRender;
extern idCVar r_skipDiffuse;
extern idCVar r_skipSpecular;
extern idCVar r_skipBump;
extern idCVar r_skipNewAmbient;
extern idCVar r_skipAmbient;
extern idCVar r_skipTranslucent;
extern idCVar r_skipPostProcess;
extern idCVar r_skipCopyTexture;
extern idCVar r_skipDynamicTextures;
extern idCVar r_skipInteractions;
extern idCVar r_skipBlendLights;
extern idCVar r_skipFogLights;
extern idCVar r_skipShadows;
extern idCVar r_showShadows;
extern idCVar r_shadowPolygonFactor;
extern idCVar r_shadowPolygonOffset;
extern idCVar r_useShadowDepthBounds;
extern idCVar r_singleTriangle;

/*
====================
PrintState
====================
*/
void PrintState( uint64 stateBits ) {
	if ( renderLog.Active() == 0 ) {
		return;
	}

	renderLog.OpenBlock( "GL_State" );

	// culling
	renderLog.Printf( "Culling: " );
	switch ( stateBits & GLS_CULL_BITS ) {
		case GLS_CULL_FRONTSIDED:	renderLog.Printf_NoIndent( "FRONTSIDED -> BACK" ); break;
		case GLS_CULL_BACKSIDED:	renderLog.Printf_NoIndent( "BACKSIDED -> FRONT" ); break;
		case GLS_CULL_TWOSIDED:		renderLog.Printf_NoIndent( "TWOSIDED" ); break;
		default:					renderLog.Printf_NoIndent( "NA" ); break;
	}
	renderLog.Printf_NoIndent( "\n" );

	// polygon mode
	renderLog.Printf( "PolygonMode: %s\n", ( stateBits & GLS_POLYMODE_LINE ) ? "LINE" : "FILL" );

	// color mask
	renderLog.Printf( "ColorMask: " );
	renderLog.Printf_NoIndent( ( stateBits & GLS_REDMASK ) ? "_" : "R" );
	renderLog.Printf_NoIndent( ( stateBits & GLS_GREENMASK ) ? "_" : "G" );
	renderLog.Printf_NoIndent( ( stateBits & GLS_BLUEMASK ) ? "_" : "B" );
	renderLog.Printf_NoIndent( ( stateBits & GLS_ALPHAMASK ) ? "_" : "A" );
	renderLog.Printf_NoIndent( "\n" );
	
	// blend
	renderLog.Printf( "Blend: src=" );
	switch ( stateBits & GLS_SRCBLEND_BITS ) {
		case GLS_SRCBLEND_ZERO:					renderLog.Printf_NoIndent( "ZERO" ); break;
		case GLS_SRCBLEND_ONE:					renderLog.Printf_NoIndent( "ONE" ); break;
		case GLS_SRCBLEND_DST_COLOR:			renderLog.Printf_NoIndent( "DST_COLOR" ); break;
		case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:	renderLog.Printf_NoIndent( "ONE_MINUS_DST_COLOR" ); break;
		case GLS_SRCBLEND_SRC_ALPHA:			renderLog.Printf_NoIndent( "SRC_ALPHA" ); break;
		case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:	renderLog.Printf_NoIndent( "ONE_MINUS_SRC_ALPHA" ); break;
		case GLS_SRCBLEND_DST_ALPHA:			renderLog.Printf_NoIndent( "DST_ALPHA" ); break;
		case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:	renderLog.Printf_NoIndent( "ONE_MINUS_DST_ALPHA" ); break;
		default:								renderLog.Printf_NoIndent( "NA" ); break;
	}
	renderLog.Printf_NoIndent( ", dst=" );
	switch ( stateBits & GLS_DSTBLEND_BITS ) {
		case GLS_DSTBLEND_ZERO:					renderLog.Printf_NoIndent( "ZERO" ); break;
		case GLS_DSTBLEND_ONE:					renderLog.Printf_NoIndent( "ONE" ); break;
		case GLS_DSTBLEND_SRC_COLOR:			renderLog.Printf_NoIndent( "SRC_COLOR" ); break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:	renderLog.Printf_NoIndent( "ONE_MINUS_SRC_COLOR" ); break;
		case GLS_DSTBLEND_SRC_ALPHA:			renderLog.Printf_NoIndent( "SRC_ALPHA" ); break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:	renderLog.Printf_NoIndent( "ONE_MINUS_SRC_ALPHA" ); break;
		case GLS_DSTBLEND_DST_ALPHA:			renderLog.Printf_NoIndent( "DST_ALPHA" ); break;
		case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:	renderLog.Printf_NoIndent( "ONE_MINUS_DST_ALPHA" ); break;
		default:								renderLog.Printf_NoIndent( "NA" );
	}
	renderLog.Printf_NoIndent( "\n" );
	
	// depth func
	renderLog.Printf( "DepthFunc: " );
	switch ( stateBits & GLS_DEPTHFUNC_BITS ) {
		case GLS_DEPTHFUNC_EQUAL:	renderLog.Printf_NoIndent( "EQUAL" ); break;
		case GLS_DEPTHFUNC_ALWAYS:	renderLog.Printf_NoIndent( "ALWAYS" ); break;
		case GLS_DEPTHFUNC_LESS:	renderLog.Printf_NoIndent( "LEQUAL" ); break;
		case GLS_DEPTHFUNC_GREATER: renderLog.Printf_NoIndent( "GEQUAL" ); break;
		default:					renderLog.Printf_NoIndent( "NA" ); break;
	}
	renderLog.Printf_NoIndent( "\n" );
	
	// depth mask
	renderLog.Printf( "DepthWrite: %s\n", ( stateBits & GLS_DEPTHMASK ) ? "FALSE" : "TRUE" );

	renderLog.Printf( "DepthBounds: %s\n", ( stateBits & GLS_DEPTH_TEST_MASK ) ? "TRUE" : "FALSE" );

	// depth bias
	renderLog.Printf( "DepthBias: %s\n", ( stateBits & GLS_POLYGON_OFFSET ) ? "TRUE" : "FALSE" );

	// stencil
	auto printStencil = [&] ( stencilFace_t face, uint64 bits, uint64 mask, uint64 ref ) {
		renderLog.Printf( "Stencil: %s, ", ( bits & ( GLS_STENCIL_FUNC_BITS | GLS_STENCIL_OP_BITS ) ) ? "ON" : "OFF" );
		renderLog.Printf_NoIndent( "Face=" );
		switch ( face ) {
			case STENCIL_FACE_FRONT: renderLog.Printf_NoIndent( "FRONT" ); break;
			case STENCIL_FACE_BACK: renderLog.Printf_NoIndent( "BACK" ); break;
			default: renderLog.Printf_NoIndent( "BOTH" ); break;
		}
		renderLog.Printf_NoIndent( ", Func=" );
		switch ( bits & GLS_STENCIL_FUNC_BITS ) {
			case GLS_STENCIL_FUNC_NEVER:	renderLog.Printf_NoIndent( "NEVER" ); break;
			case GLS_STENCIL_FUNC_LESS:		renderLog.Printf_NoIndent( "LESS" ); break;
			case GLS_STENCIL_FUNC_EQUAL:	renderLog.Printf_NoIndent( "EQUAL" ); break;
			case GLS_STENCIL_FUNC_LEQUAL:	renderLog.Printf_NoIndent( "LEQUAL" ); break;
			case GLS_STENCIL_FUNC_GREATER:	renderLog.Printf_NoIndent( "GREATER" ); break;
			case GLS_STENCIL_FUNC_NOTEQUAL: renderLog.Printf_NoIndent( "NOTEQUAL" ); break;
			case GLS_STENCIL_FUNC_GEQUAL:	renderLog.Printf_NoIndent( "GEQUAL" ); break;
			case GLS_STENCIL_FUNC_ALWAYS:	renderLog.Printf_NoIndent( "ALWAYS" ); break;
			default:						renderLog.Printf_NoIndent( "NA" ); break;
		}
		renderLog.Printf_NoIndent( ", OpFail=" );
		switch( bits & GLS_STENCIL_OP_FAIL_BITS ) {
			case GLS_STENCIL_OP_FAIL_KEEP:		renderLog.Printf_NoIndent( "KEEP" ); break;
			case GLS_STENCIL_OP_FAIL_ZERO:		renderLog.Printf_NoIndent( "ZERO" ); break;
			case GLS_STENCIL_OP_FAIL_REPLACE:	renderLog.Printf_NoIndent( "REPLACE" ); break;
			case GLS_STENCIL_OP_FAIL_INCR:		renderLog.Printf_NoIndent( "INCR" ); break;
			case GLS_STENCIL_OP_FAIL_DECR:		renderLog.Printf_NoIndent( "DECR" ); break;
			case GLS_STENCIL_OP_FAIL_INVERT:	renderLog.Printf_NoIndent( "INVERT" ); break;
			case GLS_STENCIL_OP_FAIL_INCR_WRAP: renderLog.Printf_NoIndent( "INCR_WRAP" ); break;
			case GLS_STENCIL_OP_FAIL_DECR_WRAP: renderLog.Printf_NoIndent( "DECR_WRAP" ); break;
			default:							renderLog.Printf_NoIndent( "NA" ); break;
		}
		renderLog.Printf_NoIndent( ", ZFail=" );
		switch( bits & GLS_STENCIL_OP_ZFAIL_BITS ) {
			case GLS_STENCIL_OP_ZFAIL_KEEP:			renderLog.Printf_NoIndent( "KEEP" ); break;
			case GLS_STENCIL_OP_ZFAIL_ZERO:			renderLog.Printf_NoIndent( "ZERO" ); break;
			case GLS_STENCIL_OP_ZFAIL_REPLACE:		renderLog.Printf_NoIndent( "REPLACE" ); break;
			case GLS_STENCIL_OP_ZFAIL_INCR:			renderLog.Printf_NoIndent( "INCR" ); break;
			case GLS_STENCIL_OP_ZFAIL_DECR:			renderLog.Printf_NoIndent( "DECR" ); break;
			case GLS_STENCIL_OP_ZFAIL_INVERT:		renderLog.Printf_NoIndent( "INVERT" ); break;
			case GLS_STENCIL_OP_ZFAIL_INCR_WRAP:	renderLog.Printf_NoIndent( "INCR_WRAP" ); break;
			case GLS_STENCIL_OP_ZFAIL_DECR_WRAP:	renderLog.Printf_NoIndent( "DECR_WRAP" ); break;
			default:								renderLog.Printf_NoIndent( "NA" ); break;
		}
		renderLog.Printf_NoIndent( ", OpPass=" );
		switch( bits & GLS_STENCIL_OP_PASS_BITS ) {
			case GLS_STENCIL_OP_PASS_KEEP:			renderLog.Printf_NoIndent( "KEEP" ); break;
			case GLS_STENCIL_OP_PASS_ZERO:			renderLog.Printf_NoIndent( "ZERO" ); break;
			case GLS_STENCIL_OP_PASS_REPLACE:		renderLog.Printf_NoIndent( "REPLACE" ); break;
			case GLS_STENCIL_OP_PASS_INCR:			renderLog.Printf_NoIndent( "INCR" ); break;
			case GLS_STENCIL_OP_PASS_DECR:			renderLog.Printf_NoIndent( "DECR" ); break;
			case GLS_STENCIL_OP_PASS_INVERT:		renderLog.Printf_NoIndent( "INVERT" ); break;
			case GLS_STENCIL_OP_PASS_INCR_WRAP:		renderLog.Printf_NoIndent( "INCR_WRAP" ); break;
			case GLS_STENCIL_OP_PASS_DECR_WRAP:		renderLog.Printf_NoIndent( "DECR_WRAP" ); break;
			default:								renderLog.Printf_NoIndent( "NA" ); break;
		}
		renderLog.Printf_NoIndent( ", mask=%llu, ref=%llu\n", mask, ref );
	};

	uint32 mask = uint32( ( stateBits & GLS_STENCIL_FUNC_MASK_BITS ) >> GLS_STENCIL_FUNC_MASK_SHIFT );
	uint32 ref = uint32( ( stateBits & GLS_STENCIL_FUNC_REF_BITS ) >> GLS_STENCIL_FUNC_REF_SHIFT );
	if ( stateBits & GLS_SEPARATE_STENCIL ) {
		printStencil( STENCIL_FACE_FRONT, ( stateBits & GLS_STENCIL_FRONT_OPS ), mask, ref );
		printStencil( STENCIL_FACE_BACK, ( ( stateBits & GLS_STENCIL_BACK_OPS ) >> 12 ), mask, ref );
	} else {
		printStencil( STENCIL_FACE_NUM, stateBits, mask, ref );
	}

	renderLog.CloseBlock();
}

/*
================
RB_SetMVP
================
*/
void RB_SetMVP( const idRenderMatrix & mvp ) { 
	renderProgManager.SetRenderParms( RENDERPARM_MVPMATRIX_X, mvp[0], 4 );
}

static const float zero[4] = { 0, 0, 0, 0 };
static const float one[4] = { 1, 1, 1, 1 };
static const float negOne[4] = { -1, -1, -1, -1 };

/*
================
RB_SetVertexColorParms
================
*/
void RB_SetVertexColorParms( stageVertexColor_t svc ) {
	switch ( svc ) {
		case SVC_IGNORE:
			renderProgManager.SetRenderParm( RENDERPARM_VERTEXCOLOR_MODULATE, zero );
			renderProgManager.SetRenderParm( RENDERPARM_VERTEXCOLOR_ADD, one );
			break;
		case SVC_MODULATE:
			renderProgManager.SetRenderParm( RENDERPARM_VERTEXCOLOR_MODULATE, one );
			renderProgManager.SetRenderParm( RENDERPARM_VERTEXCOLOR_ADD, zero );
			break;
		case SVC_INVERSE_MODULATE:
			renderProgManager.SetRenderParm( RENDERPARM_VERTEXCOLOR_MODULATE, negOne );
			renderProgManager.SetRenderParm( RENDERPARM_VERTEXCOLOR_ADD, one );
			break;
	}
}

/*
======================
RB_GetShaderTextureMatrix
======================
*/
void RB_GetShaderTextureMatrix( const float *shaderRegisters, const textureStage_t *texture, float matrix[16] ) {
	matrix[0*4+0] = shaderRegisters[ texture->matrix[0][0] ];
	matrix[1*4+0] = shaderRegisters[ texture->matrix[0][1] ];
	matrix[2*4+0] = 0.0f;
	matrix[3*4+0] = shaderRegisters[ texture->matrix[0][2] ];

	matrix[0*4+1] = shaderRegisters[ texture->matrix[1][0] ];
	matrix[1*4+1] = shaderRegisters[ texture->matrix[1][1] ];
	matrix[2*4+1] = 0.0f;
	matrix[3*4+1] = shaderRegisters[ texture->matrix[1][2] ];

	// we attempt to keep scrolls from generating incredibly large texture values, but
	// center rotations and center scales can still generate offsets that need to be > 1
	if ( matrix[3*4+0] < -40.0f || matrix[12] > 40.0f ) {
		matrix[3*4+0] -= (int)matrix[3*4+0];
	}
	if ( matrix[13] < -40.0f || matrix[13] > 40.0f ) {
		matrix[13] -= (int)matrix[13];
	}

	matrix[0*4+2] = 0.0f;
	matrix[1*4+2] = 0.0f;
	matrix[2*4+2] = 1.0f;
	matrix[3*4+2] = 0.0f;

	matrix[0*4+3] = 0.0f;
	matrix[1*4+3] = 0.0f;
	matrix[2*4+3] = 0.0f;
	matrix[3*4+3] = 1.0f;
}

/*
======================
RB_LoadShaderTextureMatrix
======================
*/
void RB_LoadShaderTextureMatrix( const float *shaderRegisters, const textureStage_t *texture ) {	
	float texS[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
	float texT[4] = { 0.0f, 1.0f, 0.0f, 0.0f };

	if ( texture->hasMatrix ) {
		float matrix[16];
		RB_GetShaderTextureMatrix( shaderRegisters, texture, matrix );
		texS[0] = matrix[0*4+0];
		texS[1] = matrix[1*4+0];
		texS[2] = matrix[2*4+0];
		texS[3] = matrix[3*4+0];
	
		texT[0] = matrix[0*4+1];
		texT[1] = matrix[1*4+1];
		texT[2] = matrix[2*4+1];
		texT[3] = matrix[3*4+1];

		RENDERLOG_PRINTF( "Setting Texture Matrix\n");
		renderLog.Indent();
		RENDERLOG_PRINTF( "Texture Matrix S : %4.3f, %4.3f, %4.3f, %4.3f\n", texS[0], texS[1], texS[2], texS[3] );
		RENDERLOG_PRINTF( "Texture Matrix T : %4.3f, %4.3f, %4.3f, %4.3f\n", texT[0], texT[1], texT[2], texT[3] );
		renderLog.Outdent();
	} 

	renderProgManager.SetRenderParm( RENDERPARM_TEXTUREMATRIX_S, texS );
	renderProgManager.SetRenderParm( RENDERPARM_TEXTUREMATRIX_T, texT );
}

/*
=====================
RB_BakeTextureMatrixIntoTexgen
=====================
*/
void RB_BakeTextureMatrixIntoTexgen( idPlane lightProject[3], const float *textureMatrix ) {
	float genMatrix[16];
	float final[16];

	genMatrix[0*4+0] = lightProject[0][0];
	genMatrix[1*4+0] = lightProject[0][1];
	genMatrix[2*4+0] = lightProject[0][2];
	genMatrix[3*4+0] = lightProject[0][3];

	genMatrix[0*4+1] = lightProject[1][0];
	genMatrix[1*4+1] = lightProject[1][1];
	genMatrix[2*4+1] = lightProject[1][2];
	genMatrix[3*4+1] = lightProject[1][3];

	genMatrix[0*4+2] = 0.0f;
	genMatrix[1*4+2] = 0.0f;
	genMatrix[2*4+2] = 0.0f;
	genMatrix[3*4+2] = 0.0f;

	genMatrix[0*4+3] = lightProject[2][0];
	genMatrix[1*4+3] = lightProject[2][1];
	genMatrix[2*4+3] = lightProject[2][2];
	genMatrix[3*4+3] = lightProject[2][3];

	R_MatrixMultiply( genMatrix, textureMatrix, final );

	lightProject[0][0] = final[0*4+0];
	lightProject[0][1] = final[1*4+0];
	lightProject[0][2] = final[2*4+0];
	lightProject[0][3] = final[3*4+0];

	lightProject[1][0] = final[0*4+1];
	lightProject[1][1] = final[1*4+1];
	lightProject[1][2] = final[2*4+1];
	lightProject[1][3] = final[3*4+1];
}

/*
==================
RB_SetupInteractionStage
==================
*/
void RB_SetupInteractionStage( const shaderStage_t *surfaceStage, const float *surfaceRegs, const float lightColor[4],
									idVec4 matrix[2], float color[4] ) {

	if ( surfaceStage->texture.hasMatrix ) {
		matrix[0][0] = surfaceRegs[surfaceStage->texture.matrix[0][0]];
		matrix[0][1] = surfaceRegs[surfaceStage->texture.matrix[0][1]];
		matrix[0][2] = 0.0f;
		matrix[0][3] = surfaceRegs[surfaceStage->texture.matrix[0][2]];

		matrix[1][0] = surfaceRegs[surfaceStage->texture.matrix[1][0]];
		matrix[1][1] = surfaceRegs[surfaceStage->texture.matrix[1][1]];
		matrix[1][2] = 0.0f;
		matrix[1][3] = surfaceRegs[surfaceStage->texture.matrix[1][2]];

		// we attempt to keep scrolls from generating incredibly large texture values, but
		// center rotations and center scales can still generate offsets that need to be > 1
		if ( matrix[0][3] < -40.0f || matrix[0][3] > 40.0f ) {
			matrix[0][3] -= idMath::Ftoi( matrix[0][3] );
		}
		if ( matrix[1][3] < -40.0f || matrix[1][3] > 40.0f ) {
			matrix[1][3] -= idMath::Ftoi( matrix[1][3] );
		}
	} else {
		matrix[0][0] = 1.0f;
		matrix[0][1] = 0.0f;
		matrix[0][2] = 0.0f;
		matrix[0][3] = 0.0f;

		matrix[1][0] = 0.0f;
		matrix[1][1] = 1.0f;
		matrix[1][2] = 0.0f;
		matrix[1][3] = 0.0f;
	}

	if ( color != NULL ) {
		for ( int i = 0; i < 4; i++ ) {
			// clamp here, so cards with a greater range don't look different.
			// we could perform overbrighting like we do for lights, but
			// it doesn't currently look worth it.
			color[i] = idMath::ClampFloat( 0.0f, 1.0f, surfaceRegs[surfaceStage->color.registers[i]] ) * lightColor[i];
		}
	}
}

/*
========================
GetDisplayName
========================
*/
const char * GetDisplayName( const int deviceNum ) {
	static DISPLAY_DEVICE	device;
	device.cb = sizeof( device );
	if ( !EnumDisplayDevices(
			0,			// lpDevice
			deviceNum,
			&device,
			0 /* dwFlags */ ) ) {
		return NULL;
	}
	return device.DeviceName;
}

/*
========================
GetDeviceName
========================
*/
idStr GetDeviceName( const int deviceNum ) {
	DISPLAY_DEVICE	device = {};
	device.cb = sizeof( device );
	if ( !EnumDisplayDevices(
			0,			// lpDevice
			deviceNum,
			&device,
			0 /* dwFlags */ ) ) {
		return false;
	}

	// get the monitor for this display
	if ( ! (device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP ) ) {
		return false;
	}

	return idStr( device.DeviceName );
}

/*
========================
SetGamma

The renderer calls this when the user adjusts r_gamma or r_brightness

Sets the hardware gamma ramps for gamma and brightness adjustment.
These are now taken as 16 bit values, so we can take full advantage
of dacs with >8 bits of precision
========================
*/
void SetGamma( unsigned short red[256], unsigned short green[256], unsigned short blue[256] ) {
	unsigned short table[3][256];
	int i;

	if ( !win32.hDC ) {
		return;
	}

	for ( i = 0; i < 256; i++ ) {
		table[0][i] = red[i];
		table[1][i] = green[i];
		table[2][i] = blue[i];
	}

	if ( !SetDeviceGammaRamp( win32.hDC, table ) ) {
		idLib::Printf( "WARNING: SetDeviceGammaRamp failed.\n" );
	}
}

/*
========================
GetDisplayCoordinates
========================
*/
bool GetDisplayCoordinates( const int deviceNum, int & x, int & y, int & width, int & height, int & displayHz ) {
	idStr deviceName = GetDeviceName( deviceNum );
	if ( deviceName.Length() == 0 ) {
		return false;
	}

	DISPLAY_DEVICE	device = {};
	device.cb = sizeof( device );
	if ( !EnumDisplayDevices(
			0,			// lpDevice
			deviceNum,
			&device,
			0 /* dwFlags */ ) ) {
		return false;
	}

	DISPLAY_DEVICE	monitor;
	monitor.cb = sizeof( monitor );
	if ( !EnumDisplayDevices(
			deviceName.c_str(),
			0,
			&monitor,
			0 /* dwFlags */ ) ) {
		return false;
	}

	DEVMODE	devmode;
	devmode.dmSize = sizeof( devmode );
	if ( !EnumDisplaySettings( deviceName.c_str(),ENUM_CURRENT_SETTINGS, &devmode ) ) {
		return false;
	}

	idLib::Printf( "display device: %i\n", deviceNum );
	idLib::Printf( "  DeviceName  : %s\n", device.DeviceName );
	idLib::Printf( "  DeviceString: %s\n", device.DeviceString );
	idLib::Printf( "  StateFlags  : 0x%x\n", device.StateFlags );
	idLib::Printf( "  DeviceID    : %s\n", device.DeviceID );
	idLib::Printf( "  DeviceKey   : %s\n", device.DeviceKey );
	idLib::Printf( "      DeviceName  : %s\n", monitor.DeviceName );
	idLib::Printf( "      DeviceString: %s\n", monitor.DeviceString );
	idLib::Printf( "      StateFlags  : 0x%x\n", monitor.StateFlags );
	idLib::Printf( "      DeviceID    : %s\n", monitor.DeviceID );
	idLib::Printf( "      DeviceKey   : %s\n", monitor.DeviceKey );
	idLib::Printf( "          dmPosition.x      : %i\n", devmode.dmPosition.x );
	idLib::Printf( "          dmPosition.y      : %i\n", devmode.dmPosition.y );
	idLib::Printf( "          dmBitsPerPel      : %i\n", devmode.dmBitsPerPel );
	idLib::Printf( "          dmPelsWidth       : %i\n", devmode.dmPelsWidth );
	idLib::Printf( "          dmPelsHeight      : %i\n", devmode.dmPelsHeight );
	idLib::Printf( "          dmDisplayFlags    : 0x%x\n", devmode.dmDisplayFlags );
	idLib::Printf( "          dmDisplayFrequency: %i\n", devmode.dmDisplayFrequency );

	x = devmode.dmPosition.x;
	y = devmode.dmPosition.y;
	width = devmode.dmPelsWidth;
	height = devmode.dmPelsHeight;
	displayHz = devmode.dmDisplayFrequency;

	return true;
}

/*
===================
ChangeDisplaySettingsIfNeeded

Optionally ChangeDisplaySettings to get a different fullscreen resolution.
Default uses the full desktop resolution.
===================
*/
bool ChangeDisplaySettingsIfNeeded( gfxImpParms_t parms ) {
	// If we had previously changed the display settings on a different monitor,
	// go back to standard.
	if ( win32.cdsFullscreen != 0 && win32.cdsFullscreen != parms.fullScreen ) {
		win32.cdsFullscreen = 0;
		ChangeDisplaySettings( 0, 0 );
		Sys_Sleep( 1000 ); // Give the driver some time to think about this change
	}

	// 0 is dragable mode on desktop, -1 is borderless window on desktop
	if ( parms.fullScreen <= 0 ) {
		return true;
	}

	// if we are already in the right resolution, don't do a ChangeDisplaySettings
	int x, y, width, height, displayHz;

	if ( !GetDisplayCoordinates( parms.fullScreen - 1, x, y, width, height, displayHz ) ) {
		return false;
	}
	if ( width == parms.width && height == parms.height && ( displayHz == parms.displayHz || parms.displayHz == 0 ) ) {
		return true;
	}

	DEVMODE dm = {};

	dm.dmSize = sizeof( dm );

	dm.dmPelsWidth  = parms.width;
	dm.dmPelsHeight = parms.height;
	dm.dmBitsPerPel = 32;
	dm.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
	if ( parms.displayHz != 0 ) {
		dm.dmDisplayFrequency = parms.displayHz;
		dm.dmFields |= DM_DISPLAYFREQUENCY;
	}
	
	idLib::Printf( "...calling CDS: " );
	
	const char * const deviceName = GetDisplayName( parms.fullScreen - 1 );

	int		cdsRet;
	if ( ( cdsRet = ChangeDisplaySettingsEx(
		deviceName,
		&dm, 
		NULL,
		CDS_FULLSCREEN,
		NULL) ) == DISP_CHANGE_SUCCESSFUL ) {
		idLib::Printf( "ok\n" );
		win32.cdsFullscreen = parms.fullScreen;
		return true;
	}

	idLib::Printf( "^3failed^0, " );
	
	switch ( cdsRet ) {
	case DISP_CHANGE_RESTART:
		idLib::Printf( "restart required\n" );
		break;
	case DISP_CHANGE_BADPARAM:
		idLib::Printf( "bad param\n" );
		break;
	case DISP_CHANGE_BADFLAGS:
		idLib::Printf( "bad flags\n" );
		break;
	case DISP_CHANGE_FAILED:
		idLib::Printf( "DISP_CHANGE_FAILED\n" );
		break;
	case DISP_CHANGE_BADMODE:
		idLib::Printf( "bad mode\n" );
		break;
	case DISP_CHANGE_NOTUPDATED:
		idLib::Printf( "not updated\n" );
		break;
	default:
		idLib::Printf( "unknown error %d\n", cdsRet );
		break;
	}

	return false;
}

/*
====================
GetWindowDimensions
====================
*/
bool GetWindowDimensions( const gfxImpParms_t parms, int &x, int &y, int &w, int &h ) {
	//
	// compute width and height
	//
	if ( parms.fullScreen != 0 ) {
		if ( parms.fullScreen == -1 ) {
			// borderless window at specific location, as for spanning
			// multiple monitor outputs
			x = parms.x;
			y = parms.y;
			w = parms.width;
			h = parms.height;
		} else {
			// get the current monitor position and size on the desktop, assuming
			// any required ChangeDisplaySettings has already been done
			int displayHz = 0;
			if ( !GetDisplayCoordinates( parms.fullScreen - 1, x, y, w, h, displayHz ) ) {
				return false;
			}
		}
	} else {
		RECT	r;

		// adjust width and height for window border
		r.bottom = parms.height;
		r.left = 0;
		r.top = 0;
		r.right = parms.width;

		AdjustWindowRect (&r, WINDOW_STYLE|WS_SYSMENU, FALSE);

		w = r.right - r.left;
		h = r.bottom - r.top;

		x = parms.x;
		y = parms.y;
	}

	return true;
}

/*
====================
DMDFO
====================
*/
const char * DMDFO( int dmDisplayFixedOutput ) {
	switch( dmDisplayFixedOutput ) {
	case DMDFO_DEFAULT: return "DMDFO_DEFAULT";
	case DMDFO_CENTER: return "DMDFO_CENTER";
	case DMDFO_STRETCH: return "DMDFO_STRETCH";
	}
	return "UNKNOWN";
}

/*
====================
PrintDevMode
====================
*/
void PrintDevMode( DEVMODE & devmode ) {
	idLib::Printf( "          dmPosition.x        : %i\n", devmode.dmPosition.x );
	idLib::Printf( "          dmPosition.y        : %i\n", devmode.dmPosition.y );
	idLib::Printf( "          dmBitsPerPel        : %i\n", devmode.dmBitsPerPel );
	idLib::Printf( "          dmPelsWidth         : %i\n", devmode.dmPelsWidth );
	idLib::Printf( "          dmPelsHeight        : %i\n", devmode.dmPelsHeight );
	idLib::Printf( "          dmDisplayFixedOutput: %s\n", DMDFO( devmode.dmDisplayFixedOutput ) );
	idLib::Printf( "          dmDisplayFlags      : 0x%x\n", devmode.dmDisplayFlags );
	idLib::Printf( "          dmDisplayFrequency  : %i\n", devmode.dmDisplayFrequency );
}

/*
====================
DumpAllDisplayDevices
====================
*/
void DumpAllDisplayDevices() {
	idLib::Printf( "\n" );
	for ( int deviceNum = 0 ; ; deviceNum++ ) {
		DISPLAY_DEVICE	device = {};
		device.cb = sizeof( device );
		if ( !EnumDisplayDevices(
				0,			// lpDevice
				deviceNum,
				&device,
				0 /* dwFlags */ ) ) {
			break;
		}

		idLib::Printf( "display device: %i\n", deviceNum );
		idLib::Printf( "  DeviceName  : %s\n", device.DeviceName );
		idLib::Printf( "  DeviceString: %s\n", device.DeviceString );
		idLib::Printf( "  StateFlags  : 0x%x\n", device.StateFlags );
		idLib::Printf( "  DeviceID    : %s\n", device.DeviceID );
		idLib::Printf( "  DeviceKey   : %s\n", device.DeviceKey );

		for ( int monitorNum = 0 ; ; monitorNum++ ) {
			DISPLAY_DEVICE	monitor = {};
			monitor.cb = sizeof( monitor );
			if ( !EnumDisplayDevices(
					device.DeviceName,
					monitorNum,
					&monitor,
					0 /* dwFlags */ ) ) {
				break;
			}

			idLib::Printf( "      DeviceName  : %s\n", monitor.DeviceName );
			idLib::Printf( "      DeviceString: %s\n", monitor.DeviceString );
			idLib::Printf( "      StateFlags  : 0x%x\n", monitor.StateFlags );
			idLib::Printf( "      DeviceID    : %s\n", monitor.DeviceID );
			idLib::Printf( "      DeviceKey   : %s\n", monitor.DeviceKey );

			DEVMODE	currentDevmode = {};
			if ( !EnumDisplaySettings( device.DeviceName,ENUM_CURRENT_SETTINGS, &currentDevmode ) ) {
				idLib::Printf( "ERROR:  EnumDisplaySettings(ENUM_CURRENT_SETTINGS) failed!\n" );
			}
			idLib::Printf( "          -------------------\n" );
			idLib::Printf( "          ENUM_CURRENT_SETTINGS\n" );
			PrintDevMode( currentDevmode );

			DEVMODE	registryDevmode = {};
			if ( !EnumDisplaySettings( device.DeviceName,ENUM_REGISTRY_SETTINGS, &registryDevmode ) ) {
				idLib::Printf( "ERROR:  EnumDisplaySettings(ENUM_CURRENT_SETTINGS) failed!\n" );
			}
			idLib::Printf( "          -------------------\n" );
			idLib::Printf( "          ENUM_CURRENT_SETTINGS\n" );
			PrintDevMode( registryDevmode );

			for ( int modeNum = 0 ; ; modeNum++ ) {
				DEVMODE	devmode = {};

				if ( !EnumDisplaySettings( device.DeviceName,modeNum, &devmode ) ) {
					break;
				}

				if ( devmode.dmBitsPerPel != 32 ) {
					continue;
				}
				if ( devmode.dmDisplayFrequency < 60 ) {
					continue;
				}
				if ( devmode.dmPelsHeight < 720 ) {
					continue;
				}
				idLib::Printf( "          -------------------\n" );
				idLib::Printf( "          modeNum             : %i\n", modeNum );
				PrintDevMode( devmode );
			}
		}
	}
	idLib::Printf( "\n" );
}

/*
====================
R_GetModeListForDisplay

the number of displays can be found by itterating this until it returns false
displayNum is the 0 based value passed to EnumDisplayDevices(), you must add
1 to this to get an r_fullScreen value.
====================
*/
bool R_GetModeListForDisplay( const int requestedDisplayNum, idList<vidMode_t> & modeList ) {
	modeList.Clear();

	bool	verbose = false;

	for ( int displayNum = requestedDisplayNum; ; displayNum++ ) {
		DISPLAY_DEVICE	device;
		device.cb = sizeof( device );
		if ( !EnumDisplayDevices(
				0,			// lpDevice
				displayNum,
				&device,
				0 /* dwFlags */ ) ) {
			return false;
		}

		// get the monitor for this display
		if ( ! (device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP ) ) {
			continue;
		}

		DISPLAY_DEVICE	monitor;
		monitor.cb = sizeof( monitor );
		if ( !EnumDisplayDevices(
				device.DeviceName,
				0,
				&monitor,
				0 /* dwFlags */ ) ) {
			continue;
		}

		DEVMODE	devmode;
		devmode.dmSize = sizeof( devmode );

		if ( verbose ) {
			idLib::Printf( "display device: %i\n", displayNum );
			idLib::Printf( "  DeviceName  : %s\n", device.DeviceName );
			idLib::Printf( "  DeviceString: %s\n", device.DeviceString );
			idLib::Printf( "  StateFlags  : 0x%x\n", device.StateFlags );
			idLib::Printf( "  DeviceID    : %s\n", device.DeviceID );
			idLib::Printf( "  DeviceKey   : %s\n", device.DeviceKey );
			idLib::Printf( "      DeviceName  : %s\n", monitor.DeviceName );
			idLib::Printf( "      DeviceString: %s\n", monitor.DeviceString );
			idLib::Printf( "      StateFlags  : 0x%x\n", monitor.StateFlags );
			idLib::Printf( "      DeviceID    : %s\n", monitor.DeviceID );
			idLib::Printf( "      DeviceKey   : %s\n", monitor.DeviceKey );
		}

		for ( int modeNum = 0 ; ; modeNum++ ) {
			if ( !EnumDisplaySettings( device.DeviceName,modeNum, &devmode ) ) {
				break;
			}

			if ( devmode.dmBitsPerPel != 32 ) {
				continue;
			}
			if ( ( devmode.dmDisplayFrequency != 60 ) && ( devmode.dmDisplayFrequency != 120 ) ) {
				continue;
			}
			if ( devmode.dmPelsHeight < 720 ) {
				continue;
			}
			if ( verbose ) {
				idLib::Printf( "          -------------------\n" );
				idLib::Printf( "          modeNum             : %i\n", modeNum );
				idLib::Printf( "          dmPosition.x        : %i\n", devmode.dmPosition.x );
				idLib::Printf( "          dmPosition.y        : %i\n", devmode.dmPosition.y );
				idLib::Printf( "          dmBitsPerPel        : %i\n", devmode.dmBitsPerPel );
				idLib::Printf( "          dmPelsWidth         : %i\n", devmode.dmPelsWidth );
				idLib::Printf( "          dmPelsHeight        : %i\n", devmode.dmPelsHeight );
				idLib::Printf( "          dmDisplayFixedOutput: %s\n", DMDFO( devmode.dmDisplayFixedOutput ) );
				idLib::Printf( "          dmDisplayFlags      : 0x%x\n", devmode.dmDisplayFlags );
				idLib::Printf( "          dmDisplayFrequency  : %i\n", devmode.dmDisplayFrequency );
			}
			vidMode_t mode;
			mode.width = devmode.dmPelsWidth;
			mode.height = devmode.dmPelsHeight;
			mode.displayHz = devmode.dmDisplayFrequency;
			modeList.AddUnique( mode );
		}
		if ( modeList.Num() > 0 ) {

			class idSort_VidMode : public idSort_Quick< vidMode_t, idSort_VidMode > {
			public:
				int Compare( const vidMode_t & a, const vidMode_t & b ) const {
					int wd = a.width - b.width;
					int hd = a.height - b.height;
					int fd = a.displayHz - b.displayHz;
					return ( hd != 0 ) ? hd : ( wd != 0 ) ? wd : fd;
				}
			};

			// sort with lowest resolution first
			modeList.SortWithTemplate( idSort_VidMode() );

			return true;
		}
	}
	// Never gets here
}

/*
===================
SetScreenParms

Sets up the screen based on passed parms.. 
===================
*/
bool SetScreenParms( gfxImpParms_t parms ) {
	// Optionally ChangeDisplaySettings to get a different fullscreen resolution.
	if ( !ChangeDisplaySettingsIfNeeded( parms ) ) {
		return false;
	}

	int x, y, w, h;
	if ( !GetWindowDimensions( parms, x, y, w, h ) ) {
		return false;
	}

	int exstyle;
	int stylebits;

	if ( parms.fullScreen ) {
		exstyle = WS_EX_TOPMOST;
		stylebits = WS_POPUP|WS_VISIBLE|WS_SYSMENU;
	} else {
		exstyle = 0;
		stylebits = WINDOW_STYLE|WS_SYSMENU;
	}

	SetWindowLong( win32.hWnd, GWL_STYLE, stylebits );
	SetWindowLong( win32.hWnd, GWL_EXSTYLE, exstyle );
	SetWindowPos( win32.hWnd, parms.fullScreen ? HWND_TOPMOST : HWND_NOTOPMOST, x, y, w, h, SWP_SHOWWINDOW );

	win32.isFullscreen = parms.fullScreen;
	win32.nativeScreenWidth = parms.width;
	win32.nativeScreenHeight = parms.height;
	win32.pixelAspect = 1.0f;	// FIXME: some monitor modes may be distorted

	return true;
}

/*
=============================
R_GetModeParms

r_fullScreen -1		borderless window at exact desktop coordinates
r_fullScreen 0		bordered window at exact desktop coordinates
r_fullScreen 1		fullscreen on monitor 1 at r_vidMode
r_fullScreen 2		fullscreen on monitor 2 at r_vidMode
...

r_vidMode -1		use r_customWidth / r_customHeight, even if they don't appear on the mode list
r_vidMode 0			use first mode returned by EnumDisplaySettings()
r_vidMode 1			use second mode returned by EnumDisplaySettings()
...

r_displayRefresh 0	don't specify refresh
r_displayRefresh 70	specify 70 hz, etc
=============================
*/
gfxImpParms_t R_GetModeParms() {
	// try up to three different configurations

	gfxImpParms_t parms;

	if ( r_fullscreen.GetInteger() <= 0 ) {
		// use explicit position / size for window
		parms.x = r_windowX.GetInteger();
		parms.y = r_windowY.GetInteger();
		parms.width = r_windowWidth.GetInteger();
		parms.height = r_windowHeight.GetInteger();
		// may still be -1 to force a borderless window
		parms.fullScreen = r_fullscreen.GetInteger();
		parms.displayHz = 0;		// ignored
	} else {
		// get the mode list for this monitor
		idList<vidMode_t> modeList;
		if ( !R_GetModeListForDisplay( r_fullscreen.GetInteger()-1, modeList ) ) {
			idLib::Printf( "r_fullscreen reset from %i to 1 because mode list failed.", r_fullscreen.GetInteger() );
			r_fullscreen.SetInteger( 1 );
			R_GetModeListForDisplay( r_fullscreen.GetInteger()-1, modeList );
		}
		if ( modeList.Num() < 1 ) {
			idLib::FatalError( "No modes available." );
		}

		parms.x = 0;		// ignored
		parms.y = 0;		// ignored
		parms.fullScreen = r_fullscreen.GetInteger();

		// set the parameters we are trying
		if ( r_vidMode.GetInteger() < 0 ) {
			// try forcing a specific mode, even if it isn't on the list
			parms.width = r_customWidth.GetInteger();
			parms.height = r_customHeight.GetInteger();
			parms.displayHz = r_displayRefresh.GetInteger();
		} else {
			if ( r_vidMode.GetInteger() > modeList.Num() ) {
				idLib::Printf( "r_vidMode reset from %i to 0.\n", r_vidMode.GetInteger() );
				r_vidMode.SetInteger( 0 );
			}

			parms.width = modeList[ r_vidMode.GetInteger() ].width;
			parms.height = modeList[ r_vidMode.GetInteger() ].height;
			parms.displayHz = modeList[ r_vidMode.GetInteger() ].displayHz;
		}
	}

	parms.multiSamples = r_multiSamples.GetInteger();

	return parms;
}

/*
=======================
CreateGameWindow

Responsible for creating the Win32 window.
If fullscreen, it won't have a border
=======================
*/
bool CreateGameWindow( gfxImpParms_t parms ) {
	int				x, y, w, h;
	if ( !GetWindowDimensions( parms, x, y, w, h ) ) {
		return false;
	}

	int				stylebits;
	int				exstyle;
	if ( parms.fullScreen != 0 ) {
		exstyle = WS_EX_TOPMOST;
		stylebits = WS_POPUP|WS_VISIBLE|WS_SYSMENU;
	} else {
		exstyle = 0;
		stylebits = WINDOW_STYLE|WS_SYSMENU;
	}

	win32.hWnd = CreateWindowEx (
		 exstyle, 
		 WIN32_WINDOW_CLASS_NAME,
		 GAME_NAME,
		 stylebits,
		 x, y, w, h,
		 NULL,
		 NULL,
		 win32.hInstance,
		 NULL);

	if ( !win32.hWnd ) {
		idLib::Printf( "^3GLW_CreateWindow() - Couldn't create window^0\n" );
		return false;
	}

	::SetTimer( win32.hWnd, 0, 100, NULL );

	ShowWindow( win32.hWnd, SW_SHOW );
	UpdateWindow( win32.hWnd );
	idLib::Printf( "...created window @ %d,%d (%dx%d)\n", x, y, w, h );

	// makeCurrent NULL frees the DC, so get another
	win32.hDC = GetDC( win32.hWnd );
	if ( !win32.hDC ) {
		idLib::Printf( "^3GLW_CreateWindow() - GetDC()failed^0\n" );
		return false;
	}

	SetForegroundWindow( win32.hWnd );
	SetFocus( win32.hWnd );

	win32.isFullscreen = parms.fullScreen;

	return true;
}

/*
=========================================================================================================

GL COMMANDS

=========================================================================================================
*/

/*
======================
idRenderBackend::GL_GetCurrentStateMinusStencil

Handles generating a cinematic frame if needed
======================
*/
uint64 idRenderBackend::GL_GetCurrentStateMinusStencil() const {
	return m_glStateBits & ~(GLS_STENCIL_OP_BITS|GLS_STENCIL_FUNC_BITS|GLS_STENCIL_FUNC_REF_BITS|GLS_STENCIL_FUNC_MASK_BITS);
}

/*
====================
idRenderBackend::GL_Color
====================
*/
void idRenderBackend::GL_Color( float * color ) {
	if ( color == NULL ) {
		return;
	}
	GL_Color( color[0], color[1], color[2], color[3] );
}

/*
====================
idRenderBackend::GL_Color
====================
*/
void idRenderBackend::GL_Color( float r, float g, float b, float a ) {
	float parm[4];
	parm[0] = idMath::ClampFloat( 0.0f, 1.0f, r );
	parm[1] = idMath::ClampFloat( 0.0f, 1.0f, g );
	parm[2] = idMath::ClampFloat( 0.0f, 1.0f, b );
	parm[3] = idMath::ClampFloat( 0.0f, 1.0f, a );
	renderProgManager.SetRenderParm( RENDERPARM_COLOR, parm );
}

/*
===============
idRenderBackend::SetColorMappings
===============
*/
void idRenderBackend::SetColorMappings() {
	float b = r_brightness.GetFloat();
	float invg = 1.0f / r_gamma.GetFloat();

	float j = 0.0f;
	for ( int i = 0; i < 256; i++, j += b ) {
		int inf = idMath::Ftoi( 0xffff * pow( j / 255.0f, invg ) + 0.5f );
		m_gammaTable[i] = idMath::ClampInt( 0, 0xFFFF, inf );
	}

	SetGamma( m_gammaTable, m_gammaTable, m_gammaTable );
}

/*
=========================================================================================================

BACKEND COMMANDS

=========================================================================================================
*/

/*
=============
idRenderBackend::ExecuteBackEndCommands

This function will be called syncronously if running without
smp extensions, or asyncronously by another thread.
=============
*/
void idRenderBackend::ExecuteBackEndCommands( const renderCommand_t *cmds ) {
	CheckCVars();

	resolutionScale.SetCurrentGPUFrameTime( commonLocal.m_mainFrameTiming.gpuTime );

	if ( cmds->commandId == RC_NOP && !cmds->next ) {
		return;
	}

	ResizeImages();

	renderLog.StartFrame();
	GL_StartFrame();

	uint64 backEndStartTime = Sys_Microseconds();

	// needed for editor rendering
	GL_SetDefaultState();

	for ( ; cmds != NULL; cmds = (const renderCommand_t *)cmds->next ) {
		switch ( cmds->commandId ) {
			case RC_NOP:
				break;
			case RC_DRAW_VIEW:
				DrawView( cmds );
				break;
			case RC_COPY_RENDER:
				CopyRender( cmds );
				break;
			default:
				idLib::Error( "ExecuteBackEndCommands: bad commandId" );
				break;
		}
	}

	GL_EndFrame();

	// stop rendering on this thread
	m_pc.totalMicroSec = Sys_Microseconds() - backEndStartTime;

	renderLog.EndFrame();
}

/*
==================
idRenderBackend::DrawView
==================
*/
void idRenderBackend::DrawView( const void * data ) {
	const drawSurfsCommand_t * cmd = (const drawSurfsCommand_t *)data;

	m_viewDef = cmd->viewDef;

	// we will need to do a new copyTexSubImage of the screen
	// when a SS_POST_PROCESS material is used
	m_currentRenderCopied = false;

	// if there aren't any drawsurfs, do nothing
	if ( !m_viewDef->numDrawSurfs ) {
		return;
	}

	// skip render bypasses everything that has models, assuming
	// them to be 3D views, but leaves 2D rendering visible
	if ( r_skipRender.GetBool() && m_viewDef->viewEntitys ) {
		return;
	}

	m_pc.c_surfaces += m_viewDef->numDrawSurfs;

	DBG_ShowOverdraw();

	// render the scene
	{
		renderLog.OpenBlock( "DrawViewInternal" );

		//-------------------------------------------------
		// guis can wind up referencing purged images that need to be loaded.
		// this used to be in the gui emit code, but now that it can be running
		// in a separate thread, it must not try to load images, so do it here.
		//-------------------------------------------------
		drawSurf_t **drawSurfs = (drawSurf_t **)&m_viewDef->drawSurfs[0];
		const int numDrawSurfs = m_viewDef->numDrawSurfs;

		for ( int i = 0; i < numDrawSurfs; i++ ) {
			const drawSurf_t * ds = m_viewDef->drawSurfs[ i ];
			if ( ds->material != NULL ) {
				const_cast<idMaterial *>( ds->material )->EnsureNotPurged();
			}
		}

		//-------------------------------------------------
		// RB_BeginDrawingView
		//
		// Any mirrored or portaled views have already been drawn, so prepare
		// to actually render the visible surfaces for this view
		//
		// clear the z buffer, set the projection matrix, etc
		//-------------------------------------------------

		// set the window clipping
		GL_Viewport( m_viewDef->viewport.x1,
			m_viewDef->viewport.y1,
			m_viewDef->viewport.x2 + 1 - m_viewDef->viewport.x1,
			m_viewDef->viewport.y2 + 1 - m_viewDef->viewport.y1 );

		// the scissor may be smaller than the viewport for subviews
		GL_Scissor( m_viewDef->viewport.x1 + m_viewDef->scissor.x1,
					m_viewDef->viewport.y1 + m_viewDef->scissor.y1,
					m_viewDef->scissor.x2 + 1 - m_viewDef->scissor.x1,
					m_viewDef->scissor.y2 + 1 - m_viewDef->scissor.y1 );
		m_currentScissor = m_viewDef->scissor;

		// ensures that depth writes are enabled for the depth clear
		GL_State( GLS_DEFAULT | GLS_CULL_FRONTSIDED, true );

		// Clear the depth buffer and clear the stencil to 128 for stencil shadows as well as gui masking
		GL_Clear( false, true, true, STENCIL_SHADOW_TEST_VALUE, 0.0f, 0.0f, 0.0f, 0.0f );

		//------------------------------------
		// sets variables that can be used by all programs
		//------------------------------------
		{
			//
			// set eye position in global space
			//
			float parm[4];
			parm[0] = m_viewDef->renderView.vieworg[0];
			parm[1] = m_viewDef->renderView.vieworg[1];
			parm[2] = m_viewDef->renderView.vieworg[2];
			parm[3] = 1.0f;

			renderProgManager.SetRenderParm( RENDERPARM_GLOBALEYEPOS, parm ); // rpGlobalEyePos

			// sets overbright to make world brighter
			// This value is baked into the specularScale and diffuseScale values so
			// the interaction programs don't need to perform the extra multiply,
			// but any other renderprogs that want to obey the brightness value
			// can reference this.
			float overbright = r_lightScale.GetFloat() * 0.5f;
			parm[0] = overbright;
			parm[1] = overbright;
			parm[2] = overbright;
			parm[3] = overbright;
			renderProgManager.SetRenderParm( RENDERPARM_OVERBRIGHT, parm );

			// Set Projection Matrix
			float projMatrixTranspose[16];
			R_MatrixTranspose( m_viewDef->projectionMatrix, projMatrixTranspose );
			renderProgManager.SetRenderParms( RENDERPARM_PROJMATRIX_X, projMatrixTranspose, 4 );
		}

		//-------------------------------------------------
		// fill the depth buffer and clear color buffer to black except on subviews
		//-------------------------------------------------
		{
			uint64 start = Sys_Microseconds();
			FillDepthBufferFast( drawSurfs, numDrawSurfs );
			m_pc.depthMicroSec += Sys_Microseconds() - start;
		}

		//-------------------------------------------------
		// main light renderer
		//-------------------------------------------------
		{
			uint64 start = Sys_Microseconds();
			DrawInteractions();
			m_pc.interactionMicroSec += Sys_Microseconds() - start;
		}

		//-------------------------------------------------
		// now draw any non-light dependent shading passes
		//-------------------------------------------------
		int processed = 0;
		if ( !r_skipShaderPasses.GetBool() ) {
			uint64 start = Sys_Microseconds();
			renderLog.OpenMainBlock( MRB_DRAW_SHADER_PASSES );
			processed = DrawShaderPasses( drawSurfs, numDrawSurfs );
			renderLog.CloseMainBlock();
			m_pc.shaderPassMicroSec += Sys_Microseconds() - start;
		}

		//-------------------------------------------------
		// fog and blend lights, drawn after emissive surfaces
		// so they are properly dimmed down
		//-------------------------------------------------
		FogAllLights();

		//-------------------------------------------------
		// now draw any screen warping post-process effects using _currentRender
		//-------------------------------------------------
		if ( processed < numDrawSurfs && !r_skipPostProcess.GetBool() ) {
			int x = m_viewDef->viewport.x1;
			int y = m_viewDef->viewport.y1;
			int	w = m_viewDef->viewport.x2 - m_viewDef->viewport.x1 + 1;
			int	h = m_viewDef->viewport.y2 - m_viewDef->viewport.y1 + 1;

			RENDERLOG_PRINTF( "Resolve to %i x %i buffer\n", w, h );

			// resolve the screen
			GL_CopyFrameBuffer( globalImages->m_currentRenderImage, x, y, w, h );
			m_currentRenderCopied = true;

			// RENDERPARM_SCREENCORRECTIONFACTOR amd RENDERPARM_WINDOWCOORD overlap
			// diffuseScale and specularScale

			// screen power of two correction factor (no longer relevant now)
			float screenCorrectionParm[4];
			screenCorrectionParm[0] = 1.0f;
			screenCorrectionParm[1] = 1.0f;
			screenCorrectionParm[2] = 0.0f;
			screenCorrectionParm[3] = 1.0f;
			renderProgManager.SetRenderParm( RENDERPARM_SCREENCORRECTIONFACTOR, screenCorrectionParm ); // rpScreenCorrectionFactor

			// window coord to 0.0 to 1.0 conversion
			float windowCoordParm[4];
			windowCoordParm[0] = 1.0f / w;
			windowCoordParm[1] = 1.0f / h;
			windowCoordParm[2] = 0.0f;
			windowCoordParm[3] = 1.0f;
			renderProgManager.SetRenderParm( RENDERPARM_WINDOWCOORD, windowCoordParm ); // rpWindowCoord

			// render the remaining surfaces
			renderLog.OpenMainBlock( MRB_DRAW_SHADER_PASSES_POST );
			DrawShaderPasses( drawSurfs + processed, numDrawSurfs - processed );
			renderLog.CloseMainBlock();
		}

		//-------------------------------------------------
		// render debug tools
		//-------------------------------------------------
		DBG_RenderDebugTools( drawSurfs, numDrawSurfs );

		renderLog.CloseBlock();
	}
}

/*
==================
idRenderBackend::CopyRender

Copy part of the current framebuffer to an image
==================
*/
void idRenderBackend::CopyRender( const void *data ) {
	const copyRenderCommand_t * cmd = (const copyRenderCommand_t *)data;

	if ( r_skipCopyTexture.GetBool() ) {
		return;
	}

	RENDERLOG_PRINTF( "***************** CopyRender *****************\n" );

	if ( cmd->image ) {
		GL_CopyFrameBuffer( cmd->image, cmd->x, cmd->y, cmd->imageWidth, cmd->imageHeight );
	}

	if ( cmd->clearColorAfterCopy ) {
		GL_Clear( true, false, false, STENCIL_SHADOW_TEST_VALUE, 0, 0, 0, 0 );
	}
}

/*
======================
idRenderBackend::BindVariableStageImage

Handles generating a cinematic frame if needed
======================
*/
void idRenderBackend::BindVariableStageImage( const textureStage_t *texture, const float *shaderRegisters ) {
	if ( texture->cinematic ) {
		cinData_t cin;

		if ( r_skipDynamicTextures.GetBool() ) {
			GL_BindTexture( 0, globalImages->m_defaultImage );
			return;
		}

		// offset time by shaderParm[7] (FIXME: make the time offset a parameter of the shader?)
		// We make no attempt to optimize for multiple identical cinematics being in view, or
		// for cinematics going at a lower framerate than the renderer.
		cin = texture->cinematic->ImageForTime( 
			m_viewDef->renderView.time[0] + idMath::Ftoi( 1000.0f * m_viewDef->renderView.shaderParms[11] ) );

		if ( cin.imageY != NULL ) {
			GL_BindTexture( 0, cin.imageY );
			GL_BindTexture( 1, cin.imageCr );
			GL_BindTexture( 2, cin.imageCb );
		} else {
			GL_BindTexture( 0, globalImages->m_blackImage );
			// because the shaders may have already been set - we need to make sure we are not using a bink shader which would 
			// display incorrectly.  We may want to get rid of RB_BindVariableStageImage and inline the code so that the
			// SWF GUI case is handled better, too
			renderProgManager.BindProgram( BUILTIN_TEXTURE_VERTEXCOLOR );
		}
	} else {
		// FIXME: see why image is invalid
		if ( texture->image != NULL ) {
			GL_BindTexture( 0, texture->image );
		}
	}
}

/*
================
idRenderBackend::PrepareStageTexturing
================
*/
void idRenderBackend::PrepareStageTexturing( const shaderStage_t * pStage,  const drawSurf_t * surf ) {
	float useTexGenParm[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// set the texture matrix if needed
	RB_LoadShaderTextureMatrix( surf->shaderRegisters, &pStage->texture );

	// texgens
	if ( pStage->texture.texgen == TG_REFLECT_CUBE ) {

		// see if there is also a bump map specified
		const shaderStage_t *bumpStage = surf->material->GetBumpStage();
		if ( bumpStage != NULL ) {
			// per-pixel reflection mapping with bump mapping
			GL_BindTexture( 1, bumpStage->texture.image );

			RENDERLOG_PRINTF( "TexGen: TG_REFLECT_CUBE: Bumpy Environment\n" );
			if ( surf->jointCache ) {
				renderProgManager.BindProgram( BUILTIN_BUMPY_ENVIRONMENT_SKINNED );
			} else {
				renderProgManager.BindProgram( BUILTIN_BUMPY_ENVIRONMENT );
			}
		} else {
			RENDERLOG_PRINTF( "TexGen: TG_REFLECT_CUBE: Environment\n" );
			if ( surf->jointCache ) {
				renderProgManager.BindProgram( BUILTIN_ENVIRONMENT_SKINNED );
			} else {
				renderProgManager.BindProgram( BUILTIN_ENVIRONMENT );
			}
		}

	} else if ( pStage->texture.texgen == TG_SKYBOX_CUBE ) {

		renderProgManager.BindProgram( BUILTIN_SKYBOX );

	} else if ( pStage->texture.texgen == TG_WOBBLESKY_CUBE ) {

		const int * parms = surf->material->GetTexGenRegisters();

		float wobbleDegrees = surf->shaderRegisters[ parms[0] ] * ( idMath::PI / 180.0f );
		float wobbleSpeed = surf->shaderRegisters[ parms[1] ] * ( 2.0f * idMath::PI / 60.0f );
		float rotateSpeed = surf->shaderRegisters[ parms[2] ] * ( 2.0f * idMath::PI / 60.0f );

		idVec3 axis[3];
		{
			// very ad-hoc "wobble" transform
			float s, c;
			idMath::SinCos( wobbleSpeed * m_viewDef->renderView.time[0] * 0.001f, s, c );

			float ws, wc;
			idMath::SinCos( wobbleDegrees, ws, wc );

			axis[2][0] = ws * c;
			axis[2][1] = ws * s;
			axis[2][2] = wc;

			axis[1][0] = -s * s * ws;
			axis[1][2] = -s * ws * ws;
			axis[1][1] = idMath::Sqrt( idMath::Fabs( 1.0f - ( axis[1][0] * axis[1][0] + axis[1][2] * axis[1][2] ) ) );

			// make the second vector exactly perpendicular to the first
			axis[1] -= ( axis[2] * axis[1] ) * axis[2];
			axis[1].Normalize();

			// construct the third with a cross
			axis[0].Cross( axis[1], axis[2] );
		}

		// add the rotate
		float rs, rc;
		idMath::SinCos( rotateSpeed * m_viewDef->renderView.time[0] * 0.001f, rs, rc );

		float transform[12];
		transform[0*4+0] = axis[0][0] * rc + axis[1][0] * rs;
		transform[0*4+1] = axis[0][1] * rc + axis[1][1] * rs;
		transform[0*4+2] = axis[0][2] * rc + axis[1][2] * rs;
		transform[0*4+3] = 0.0f;

		transform[1*4+0] = axis[1][0] * rc - axis[0][0] * rs;
		transform[1*4+1] = axis[1][1] * rc - axis[0][1] * rs;
		transform[1*4+2] = axis[1][2] * rc - axis[0][2] * rs;
		transform[1*4+3] = 0.0f;

		transform[2*4+0] = axis[2][0];
		transform[2*4+1] = axis[2][1];
		transform[2*4+2] = axis[2][2];
		transform[2*4+3] = 0.0f;

		renderProgManager.SetRenderParms( RENDERPARM_WOBBLESKY_X, transform, 3 );
		renderProgManager.BindProgram( BUILTIN_WOBBLESKY );

	} else if ( ( pStage->texture.texgen == TG_SCREEN ) || ( pStage->texture.texgen == TG_SCREEN2 ) ) {

		useTexGenParm[0] = 1.0f;
		useTexGenParm[1] = 1.0f;
		useTexGenParm[2] = 1.0f;
		useTexGenParm[3] = 1.0f;

		float mat[16];
		R_MatrixMultiply( surf->space->modelViewMatrix, m_viewDef->projectionMatrix, mat );

		RENDERLOG_PRINTF( "TexGen : %s\n", ( pStage->texture.texgen == TG_SCREEN ) ? "TG_SCREEN" : "TG_SCREEN2" );
		renderLog.Indent();

		float plane[4];
		plane[0] = mat[0*4+0];
		plane[1] = mat[1*4+0];
		plane[2] = mat[2*4+0];
		plane[3] = mat[3*4+0];
		renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_0_S, plane );
		RENDERLOG_PRINTF( "TEXGEN_S = %4.3f, %4.3f, %4.3f, %4.3f\n",  plane[0], plane[1], plane[2], plane[3] );

		plane[0] = mat[0*4+1];
		plane[1] = mat[1*4+1];
		plane[2] = mat[2*4+1];
		plane[3] = mat[3*4+1];
		renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_0_T, plane );
		RENDERLOG_PRINTF( "TEXGEN_T = %4.3f, %4.3f, %4.3f, %4.3f\n",  plane[0], plane[1], plane[2], plane[3] );

		plane[0] = mat[0*4+3];
		plane[1] = mat[1*4+3];
		plane[2] = mat[2*4+3];
		plane[3] = mat[3*4+3];
		renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_0_Q, plane );	
		RENDERLOG_PRINTF( "TEXGEN_Q = %4.3f, %4.3f, %4.3f, %4.3f\n",  plane[0], plane[1], plane[2], plane[3] );

		renderLog.Outdent();

	} else if ( pStage->texture.texgen == TG_DIFFUSE_CUBE ) {

		// As far as I can tell, this is never used
		idLib::Warning( "Using Diffuse Cube! Please contact Brian!" );

	} else if ( pStage->texture.texgen == TG_GLASSWARP ) {

		// As far as I can tell, this is never used
		idLib::Warning( "Using GlassWarp! Please contact Brian!" );
	}

	renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_0_ENABLED, useTexGenParm );
}

/*
=========================================================================================

DEPTH BUFFER RENDERING

=========================================================================================
*/

/*
==================
idRenderBackend::FillDepthBufferGeneric
==================
*/
void idRenderBackend::FillDepthBufferGeneric( const drawSurf_t * const * drawSurfs, int numDrawSurfs ) {
	for ( int i = 0; i < numDrawSurfs; i++ ) {
		const drawSurf_t * drawSurf = drawSurfs[i];
		const idMaterial * shader = drawSurf->material;

		// translucent surfaces don't put anything in the depth buffer and don't
		// test against it, which makes them fail the mirror clip plane operation
		if ( shader->Coverage() == MC_TRANSLUCENT ) {
			continue;
		}

		// get the expressions for conditionals / color / texcoords
		const float * regs = drawSurf->shaderRegisters;

		// if all stages of a material have been conditioned off, don't do anything
		int stage = 0;
		for ( ; stage < shader->GetNumStages(); stage++ ) {		
			const shaderStage_t * pStage = shader->GetStage( stage );
			// check the stage enable condition
			if ( regs[ pStage->conditionRegister ] != 0 ) {
				break;
			}
		}
		if ( stage == shader->GetNumStages() ) {
			continue;
		}

		// change the matrix if needed
		if ( drawSurf->space != m_currentSpace ) {
			RB_SetMVP( drawSurf->space->mvp );

			m_currentSpace = drawSurf->space;
		}

		uint64 surfGLState = 0;

		// set polygon offset if necessary
		if ( shader->TestMaterialFlag( MF_POLYGONOFFSET ) ) {
			surfGLState |= GLS_POLYGON_OFFSET;
			GL_PolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset() );
		}

		// subviews will just down-modulate the color buffer
		float color[4];
		if ( shader->GetSort() == SS_SUBVIEW ) {
			surfGLState |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO | GLS_DEPTHFUNC_LESS;
			color[0] = 1.0f;
			color[1] = 1.0f;
			color[2] = 1.0f;
			color[3] = 1.0f;
		} else {
			// others just draw black
			color[0] = 0.0f;
			color[1] = 0.0f;
			color[2] = 0.0f;
			color[3] = 1.0f;
		}

		renderLog.OpenBlock( shader->GetName() );

		bool drawSolid = false;
		if ( shader->Coverage() == MC_OPAQUE ) {
			drawSolid = true;
		} else if ( shader->Coverage() == MC_PERFORATED ) {
			// we may have multiple alpha tested stages
			// if the only alpha tested stages are condition register omitted,
			// draw a normal opaque surface
			bool didDraw = false;

			// perforated surfaces may have multiple alpha tested stages
			for ( stage = 0; stage < shader->GetNumStages(); stage++ ) {		
				const shaderStage_t *pStage = shader->GetStage(stage);

				if ( !pStage->hasAlphaTest ) {
					continue;
				}

				// check the stage enable condition
				if ( regs[ pStage->conditionRegister ] == 0 ) {
					continue;
				}

				// if we at least tried to draw an alpha tested stage,
				// we won't draw the opaque surface
				didDraw = true;

				// set the alpha modulate
				color[3] = regs[ pStage->color.registers[3] ];

				// skip the entire stage if alpha would be black
				if ( color[3] <= 0.0f ) {
					continue;
				}

				uint64 stageGLState = surfGLState;

				// set privatePolygonOffset if necessary
				if ( pStage->privatePolygonOffset ) {
					GL_PolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * pStage->privatePolygonOffset );
					stageGLState |= GLS_POLYGON_OFFSET;
				}

				GL_Color( color );

				GL_State( stageGLState );
				idVec4 alphaTestValue( regs[ pStage->alphaTestRegister ] );
				renderProgManager.SetRenderParm( RENDERPARM_ALPHA_TEST, alphaTestValue.ToFloatPtr() );

				if ( drawSurf->jointCache ) {
					renderProgManager.BindProgram( BUILTIN_TEXTURE_VERTEXCOLOR_SKINNED );
				} else {
					renderProgManager.BindProgram( BUILTIN_TEXTURE_VERTEXCOLOR );
				}

				RB_SetVertexColorParms( SVC_IGNORE );

				// bind the texture
				GL_BindTexture( 0, pStage->texture.image );

				// set texture matrix and texGens
				PrepareStageTexturing( pStage, drawSurf );

				// must render with less-equal for Z-Cull to work properly
				assert( ( m_glStateBits & GLS_DEPTHFUNC_BITS ) == GLS_DEPTHFUNC_LESS );

				// draw it
				DrawElementsWithCounters( drawSurf );

				// unset privatePolygonOffset if necessary
				if ( pStage->privatePolygonOffset ) {
					GL_PolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset() );
				}
			}

			if ( !didDraw ) {
				drawSolid = true;
			}
		}

		// draw the entire surface solid
		if ( drawSolid ) {
			if ( shader->GetSort() == SS_SUBVIEW ) {
				renderProgManager.BindProgram( BUILTIN_COLOR );
				GL_Color( color );
				GL_State( surfGLState );
			} else {
				if ( drawSurf->jointCache ) {
					renderProgManager.BindProgram( BUILTIN_DEPTH_SKINNED );
				} else {
					renderProgManager.BindProgram( BUILTIN_DEPTH );
				}
				GL_State( surfGLState | GLS_ALPHAMASK );
			}

			// must render with less-equal for Z-Cull to work properly
			assert( ( m_glStateBits & GLS_DEPTHFUNC_BITS ) == GLS_DEPTHFUNC_LESS );

			// draw it
			DrawElementsWithCounters( drawSurf );
		}

		renderLog.CloseBlock();
	}

	renderProgManager.SetRenderParm( RENDERPARM_ALPHA_TEST, vec4_zero.ToFloatPtr() );
}

/*
=====================
idRenderBackend::FillDepthBufferFast

Optimized fast path code.

If there are subview surfaces, they must be guarded in the depth buffer to allow
the mirror / subview to show through underneath the current view rendering.

Surfaces with perforated shaders need the full shader setup done, but should be
drawn after the opaque surfaces.

The bulk of the surfaces should be simple opaque geometry that can be drawn very rapidly.

If there are no subview surfaces, we could clear to black and use fast-Z rendering
on the 360.
=====================
*/
void idRenderBackend::FillDepthBufferFast( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	if ( numDrawSurfs == 0 ) {
		return;
	}

	// if we are just doing 2D rendering, no need to fill the depth buffer
	if ( m_viewDef->viewEntitys == NULL ) {
		return;
	}

	renderLog.OpenMainBlock( MRB_FILL_DEPTH_BUFFER );
	renderLog.OpenBlock( "RB_FillDepthBufferFast" );

	// force MVP change on first surface
	m_currentSpace = NULL;

	// draw all the subview surfaces, which will already be at the start of the sorted list,
	// with the general purpose path
	GL_State( GLS_DEFAULT );

	int	surfNum;
	for ( surfNum = 0; surfNum < numDrawSurfs; surfNum++ ) {
		if ( drawSurfs[surfNum]->material->GetSort() != SS_SUBVIEW ) {
			break;
		}
		FillDepthBufferGeneric( &drawSurfs[surfNum], 1 );
	}

	const drawSurf_t ** perforatedSurfaces = (const drawSurf_t ** )_alloca( numDrawSurfs * sizeof( drawSurf_t * ) );
	int numPerforatedSurfaces = 0;

	// draw all the opaque surfaces and build up a list of perforated surfaces that
	// we will defer drawing until all opaque surfaces are done
	GL_State( GLS_DEFAULT );

	// continue checking past the subview surfaces
	for ( ; surfNum < numDrawSurfs; surfNum++ ) {
		const drawSurf_t * surf = drawSurfs[ surfNum ];
		const idMaterial * shader = surf->material;

		// translucent surfaces don't put anything in the depth buffer
		if ( shader->Coverage() == MC_TRANSLUCENT ) {
			continue;
		}
		if ( shader->Coverage() == MC_PERFORATED ) {
			// save for later drawing
			perforatedSurfaces[ numPerforatedSurfaces ] = surf;
			numPerforatedSurfaces++;
			continue;
		}

		// set polygon offset?

		// set mvp matrix
		if ( surf->space != m_currentSpace ) {
			RB_SetMVP( surf->space->mvp );
			m_currentSpace = surf->space;
		}

		renderLog.OpenBlock( shader->GetName() );

		if ( surf->jointCache ) {
			renderProgManager.BindProgram( BUILTIN_DEPTH_SKINNED );
		} else {
			renderProgManager.BindProgram( BUILTIN_DEPTH );
		}

		// must render with less-equal for Z-Cull to work properly
		assert( ( m_glStateBits & GLS_DEPTHFUNC_BITS ) == GLS_DEPTHFUNC_LESS );

		// draw it solid
		DrawElementsWithCounters( surf );

		renderLog.CloseBlock();
	}

	// draw all perforated surfaces with the general code path
	if ( numPerforatedSurfaces > 0 ) {
		FillDepthBufferGeneric( perforatedSurfaces, numPerforatedSurfaces );
	}

	renderLog.CloseBlock();
	renderLog.CloseMainBlock();
}

/*
=========================================================================================

GENERAL INTERACTION RENDERING

=========================================================================================
*/

const int INTERACTION_TEXUNIT_BUMP			= 0;
const int INTERACTION_TEXUNIT_FALLOFF		= 1;
const int INTERACTION_TEXUNIT_PROJECTION	= 2;
const int INTERACTION_TEXUNIT_DIFFUSE		= 3;
const int INTERACTION_TEXUNIT_SPECULAR		= 4;

/*
=================
idRenderBackend::DrawSingleInteraction
=================
*/
void idRenderBackend::DrawSingleInteraction( drawInteraction_t * din ) {
	if ( din->bumpImage == NULL ) {
		// stage wasn't actually an interaction
		return;
	}

	if ( din->diffuseImage == NULL || r_skipDiffuse.GetBool() ) {
		// this isn't a YCoCg black, but it doesn't matter, because
		// the diffuseColor will also be 0
		din->diffuseImage = globalImages->m_blackImage;
	}
	if ( din->specularImage == NULL || r_skipSpecular.GetBool() || din->ambientLight ) {
		din->specularImage = globalImages->m_blackImage;
	}
	if ( r_skipBump.GetBool() ) {
		din->bumpImage = globalImages->m_flatNormalMap;
	}

	// if we wouldn't draw anything, don't call the Draw function
	const bool diffuseIsBlack = ( din->diffuseImage == globalImages->m_blackImage )
									|| ( ( din->diffuseColor[0] <= 0 ) && ( din->diffuseColor[1] <= 0 ) && ( din->diffuseColor[2] <= 0 ) );
	const bool specularIsBlack = ( din->specularImage == globalImages->m_blackImage )
									|| ( ( din->specularColor[0] <= 0 ) && ( din->specularColor[1] <= 0 ) && ( din->specularColor[2] <= 0 ) );
	if ( diffuseIsBlack && specularIsBlack ) {
		return;
	}

	// bump matrix
	renderProgManager.SetRenderParm( RENDERPARM_BUMPMATRIX_S, din->bumpMatrix[0].ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_BUMPMATRIX_T, din->bumpMatrix[1].ToFloatPtr() );

	// diffuse matrix
	renderProgManager.SetRenderParm( RENDERPARM_DIFFUSEMATRIX_S, din->diffuseMatrix[0].ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_DIFFUSEMATRIX_T, din->diffuseMatrix[1].ToFloatPtr() );

	// specular matrix
	renderProgManager.SetRenderParm( RENDERPARM_SPECULARMATRIX_S, din->specularMatrix[0].ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_SPECULARMATRIX_T, din->specularMatrix[1].ToFloatPtr() );

	RB_SetVertexColorParms( din->vertexColor );

	renderProgManager.SetRenderParm( RENDERPARM_DIFFUSEMODIFIER, din->diffuseColor.ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_SPECULARMODIFIER, din->specularColor.ToFloatPtr() );

	// texture 0 will be the per-surface bump map
	GL_BindTexture( INTERACTION_TEXUNIT_BUMP, din->bumpImage );

	// texture 3 is the per-surface diffuse map
	GL_BindTexture( INTERACTION_TEXUNIT_DIFFUSE, din->diffuseImage );

	// texture 4 is the per-surface specular map
	GL_BindTexture( INTERACTION_TEXUNIT_SPECULAR, din->specularImage );

	DrawElementsWithCounters( din->surf );
}

/*
=================
RB_SetupForFastPathInteractions

These are common for all fast path surfaces
=================
*/
static void RB_SetupForFastPathInteractions( const idVec4 & diffuseColor, const idVec4 & specularColor ) {
	const idVec4 sMatrix( 1, 0, 0, 0 );
	const idVec4 tMatrix( 0, 1, 0, 0 );

	// bump matrix
	renderProgManager.SetRenderParm( RENDERPARM_BUMPMATRIX_S, sMatrix.ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_BUMPMATRIX_T, tMatrix.ToFloatPtr() );

	// diffuse matrix
	renderProgManager.SetRenderParm( RENDERPARM_DIFFUSEMATRIX_S, sMatrix.ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_DIFFUSEMATRIX_T, tMatrix.ToFloatPtr() );

	// specular matrix
	renderProgManager.SetRenderParm( RENDERPARM_SPECULARMATRIX_S, sMatrix.ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_SPECULARMATRIX_T, tMatrix.ToFloatPtr() );

	RB_SetVertexColorParms( SVC_IGNORE );

	renderProgManager.SetRenderParm( RENDERPARM_DIFFUSEMODIFIER, diffuseColor.ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_SPECULARMODIFIER, specularColor.ToFloatPtr() );
}

/*
=============
idRenderBackend::RenderInteractions

With added sorting and trivial path work.
=============
*/
void idRenderBackend::RenderInteractions( const drawSurf_t *surfList, const viewLight_t * vLight, int depthFunc, bool performStencilTest, bool useLightDepthBounds ) {
	if ( surfList == NULL ) {
		return;
	}

	// change the scissor if needed, it will be constant across all the surfaces lit by the light
	if ( !m_currentScissor.Equals( vLight->scissorRect ) && r_useScissor.GetBool() ) {
		GL_Scissor( m_viewDef->viewport.x1 + vLight->scissorRect.x1, 
					m_viewDef->viewport.y1 + vLight->scissorRect.y1,
					vLight->scissorRect.x2 + 1 - vLight->scissorRect.x1,
					vLight->scissorRect.y2 + 1 - vLight->scissorRect.y1 );
		m_currentScissor = vLight->scissorRect;
	}

	// perform setup here that will be constant for all interactions
	if ( performStencilTest ) {
		GL_State( 
			GLS_SRCBLEND_ONE | 
			GLS_DSTBLEND_ONE | 
			GLS_DEPTHMASK | 
			depthFunc | 
			GLS_STENCIL_FUNC_EQUAL | 
			GLS_STENCIL_MAKE_REF( STENCIL_SHADOW_TEST_VALUE ) | 
			GLS_STENCIL_MAKE_MASK( STENCIL_SHADOW_MASK_VALUE ) );
	} else {
		GL_State( 
			GLS_SRCBLEND_ONE | 
			GLS_DSTBLEND_ONE | 
			GLS_DEPTHMASK | 
			depthFunc | 
			GLS_STENCIL_FUNC_ALWAYS );
	}

	// some rare lights have multiple animating stages, loop over them outside the surface list
	const idMaterial * lightShader = vLight->lightShader;
	const float * lightRegs = vLight->shaderRegisters;

	drawInteraction_t inter = {};
	inter.ambientLight = lightShader->IsAmbientLight();

	//---------------------------------
	// Split out the complex surfaces from the fast-path surfaces
	// so we can do the fast path ones all in a row.
	// The surfaces should already be sorted by space because they
	// are added single-threaded, and there is only a negligable amount
	// of benefit to trying to sort by materials.
	//---------------------------------
	static const int MAX_INTERACTIONS_PER_LIGHT = 1024;
	static const int MAX_COMPLEX_INTERACTIONS_PER_LIGHT = 128;
	idStaticList< const drawSurf_t *, MAX_INTERACTIONS_PER_LIGHT > allSurfaces;
	for ( const drawSurf_t * walk = surfList; walk != NULL; walk = walk->nextOnLight ) {

		// make sure the triangle culling is done
		if ( walk->shadowVolumeState != SHADOWVOLUME_DONE ) {
			assert( walk->shadowVolumeState == SHADOWVOLUME_UNFINISHED || walk->shadowVolumeState == SHADOWVOLUME_DONE );
			
			while ( walk->shadowVolumeState == SHADOWVOLUME_UNFINISHED ) {
				Sys_Yield();
			}
		}

		allSurfaces.Append( walk );
	}

	bool lightDepthBoundsDisabled = false;

	for ( int lightStageNum = 0; lightStageNum < lightShader->GetNumStages(); lightStageNum++ ) {
		const shaderStage_t	*lightStage = lightShader->GetStage( lightStageNum );

		// ignore stages that fail the condition
		if ( !lightRegs[ lightStage->conditionRegister ] ) {
			continue;
		}

		const float lightScale = r_lightScale.GetFloat();
		const idVec4 lightColor(
			lightScale * lightRegs[ lightStage->color.registers[0] ],
			lightScale * lightRegs[ lightStage->color.registers[1] ],
			lightScale * lightRegs[ lightStage->color.registers[2] ],
			lightRegs[ lightStage->color.registers[3] ] );
		// apply the world-global overbright and the 2x factor for specular
		const idVec4 diffuseColor = lightColor;
		const idVec4 specularColor = lightColor * 2.0f;

		float lightTextureMatrix[16];
		if ( lightStage->texture.hasMatrix ) {
			RB_GetShaderTextureMatrix( lightRegs, &lightStage->texture, lightTextureMatrix );
		}

		// texture 1 will be the light falloff texture
		GL_BindTexture( INTERACTION_TEXUNIT_FALLOFF, vLight->falloffImage );

		// texture 2 will be the light projection texture
		GL_BindTexture( INTERACTION_TEXUNIT_PROJECTION, lightStage->texture.image );

		// force the light textures to not use anisotropic filtering, which is wasted on them
		// all of the texture sampler parms should be constant for all interactions, only
		// the actual texture image bindings will change

		//----------------------------------
		// For all surfaces on this light list, generate an interaction for this light stage
		//----------------------------------

		// setup renderparms assuming we will be drawing trivial surfaces first
		RB_SetupForFastPathInteractions( diffuseColor, specularColor );

		// even if the space does not change between light stages, each light stage may need a different lightTextureMatrix baked in
		m_currentSpace = NULL;

		for ( int sortedSurfNum = 0; sortedSurfNum < allSurfaces.Num(); sortedSurfNum++ ) {
			const drawSurf_t * const surf = allSurfaces[ sortedSurfNum ];

			// select the render prog
			if ( lightShader->IsAmbientLight() ) {
				if ( surf->jointCache ) {
					renderProgManager.BindProgram( BUILTIN_INTERACTION_AMBIENT_SKINNED );
				} else {
					renderProgManager.BindProgram( BUILTIN_INTERACTION_AMBIENT );
				}
			} else {
				if ( surf->jointCache ) {
					renderProgManager.BindProgram( BUILTIN_INTERACTION_SKINNED );
				} else {
					renderProgManager.BindProgram( BUILTIN_INTERACTION );
				}
			}

			const idMaterial * surfaceShader = surf->material;
			const float * surfaceRegs = surf->shaderRegisters;

			inter.surf = surf;

			// change the MVP matrix, view/light origin and light projection vectors if needed
			if ( surf->space != m_currentSpace ) {
				m_currentSpace = surf->space;

				// turn off the light depth bounds test if this model is rendered with a depth hack
				if ( useLightDepthBounds ) {
					if ( !surf->space->weaponDepthHack && surf->space->modelDepthHack == 0.0f ) {
						if ( lightDepthBoundsDisabled ) {
							GL_DepthBoundsTest( vLight->scissorRect.zmin, vLight->scissorRect.zmax );
							lightDepthBoundsDisabled = false;
						}
					} else {
						if ( !lightDepthBoundsDisabled ) {
							GL_DepthBoundsTest( 0.0f, 0.0f );
							lightDepthBoundsDisabled = true;
						}
					}
				}

				// model-view-projection
				RB_SetMVP( surf->space->mvp );

				// tranform the light/view origin into model local space
				idVec4 localLightOrigin( 0.0f );
				idVec4 localViewOrigin( 1.0f );
				R_GlobalPointToLocal( surf->space->modelMatrix, vLight->globalLightOrigin, localLightOrigin.ToVec3() );
				R_GlobalPointToLocal( surf->space->modelMatrix, m_viewDef->renderView.vieworg, localViewOrigin.ToVec3() );

				// set the local light/view origin
				renderProgManager.SetRenderParm( RENDERPARM_LOCALLIGHTORIGIN, localLightOrigin.ToFloatPtr() );
				renderProgManager.SetRenderParm( RENDERPARM_LOCALVIEWORIGIN, localViewOrigin.ToFloatPtr() );

				// transform the light project into model local space
				idPlane lightProjection[4];
				for ( int i = 0; i < 4; i++ ) {
					R_GlobalPlaneToLocal( surf->space->modelMatrix, vLight->lightProject[i], lightProjection[i] );
				}

				// optionally multiply the local light projection by the light texture matrix
				if ( lightStage->texture.hasMatrix ) {
					RB_BakeTextureMatrixIntoTexgen( lightProjection, lightTextureMatrix );
				}

				// set the light projection
				renderProgManager.SetRenderParm( RENDERPARM_LIGHTPROJECTION_S, lightProjection[0].ToFloatPtr() );
				renderProgManager.SetRenderParm( RENDERPARM_LIGHTPROJECTION_T, lightProjection[1].ToFloatPtr() );
				renderProgManager.SetRenderParm( RENDERPARM_LIGHTPROJECTION_Q, lightProjection[2].ToFloatPtr() );
				renderProgManager.SetRenderParm( RENDERPARM_LIGHTFALLOFF_S, lightProjection[3].ToFloatPtr() );
			}
			
			renderLog.OpenBlock( surf->material->GetName() );

			inter.bumpImage = NULL;
			inter.specularImage = NULL;
			inter.diffuseImage = NULL;
			inter.diffuseColor[0] = inter.diffuseColor[1] = inter.diffuseColor[2] = inter.diffuseColor[3] = 0;
			inter.specularColor[0] = inter.specularColor[1] = inter.specularColor[2] = inter.specularColor[3] = 0;

			// go through the individual surface stages
			//
			// This is somewhat arcane because of the old support for video cards that had to render
			// interactions in multiple passes.
			//
			// We also have the very rare case of some materials that have conditional interactions
			// for the "hell writing" that can be shined on them.
			for ( int surfaceStageNum = 0; surfaceStageNum < surfaceShader->GetNumStages(); surfaceStageNum++ ) {
				const shaderStage_t	*surfaceStage = surfaceShader->GetStage( surfaceStageNum );

				switch( surfaceStage->lighting ) {
					case SL_COVERAGE: {
						// ignore any coverage stages since they should only be used for the depth fill pass
						// for diffuse stages that use alpha test.
						break;
					}
					case SL_AMBIENT: {
						// ignore ambient stages while drawing interactions
						break;
					}
					case SL_BUMP: {
						// ignore stage that fails the condition
						if ( !surfaceRegs[ surfaceStage->conditionRegister ] ) {
							break;
						}
						// draw any previous interaction
						if ( inter.bumpImage != NULL ) {
							DrawSingleInteraction( &inter );
						}
						inter.bumpImage = surfaceStage->texture.image;
						inter.diffuseImage = NULL;
						inter.specularImage = NULL;
						RB_SetupInteractionStage( surfaceStage, surfaceRegs, NULL,
												inter.bumpMatrix, NULL );
						break;
					}
					case SL_DIFFUSE: {
						// ignore stage that fails the condition
						if ( !surfaceRegs[ surfaceStage->conditionRegister ] ) {
							break;
						}
						// draw any previous interaction
						if ( inter.diffuseImage != NULL ) {
							DrawSingleInteraction( &inter );
						}
						inter.diffuseImage = surfaceStage->texture.image;
						inter.vertexColor = surfaceStage->vertexColor;
						RB_SetupInteractionStage( surfaceStage, surfaceRegs, diffuseColor.ToFloatPtr(),
												inter.diffuseMatrix, inter.diffuseColor.ToFloatPtr() );
						break;
					}
					case SL_SPECULAR: {
						// ignore stage that fails the condition
						if ( !surfaceRegs[ surfaceStage->conditionRegister ] ) {
							break;
						}
						// draw any previous interaction
						if ( inter.specularImage != NULL ) {
							DrawSingleInteraction( &inter );
						}
						inter.specularImage = surfaceStage->texture.image;
						inter.vertexColor = surfaceStage->vertexColor;
						RB_SetupInteractionStage( surfaceStage, surfaceRegs, specularColor.ToFloatPtr(),
												inter.specularMatrix, inter.specularColor.ToFloatPtr() );
						break;
					}
				}
			}

			// draw the final interaction
			DrawSingleInteraction( &inter );

			renderLog.CloseBlock();
		}
	}

	if ( useLightDepthBounds && lightDepthBoundsDisabled ) {
		GL_DepthBoundsTest( vLight->scissorRect.zmin, vLight->scissorRect.zmax );
	}
}

/*
==============================================================================================

DRAW INTERACTIONS

==============================================================================================
*/
/*
==================
idRenderBackend::DrawInteractions
==================
*/
void idRenderBackend::DrawInteractions() {
	if ( r_skipInteractions.GetBool() ) {
		return;
	}

	renderLog.OpenMainBlock( MRB_DRAW_INTERACTIONS );
	renderLog.OpenBlock( "RB_DrawInteractions" );

	const bool useLightDepthBounds = r_useLightDepthBounds.GetBool();

	//
	// for each light, perform shadowing and adding
	//
	for ( const viewLight_t * vLight = m_viewDef->viewLights; vLight != NULL; vLight = vLight->next ) {
		// do fogging later
		if ( vLight->lightShader->IsFogLight() ) {
			continue;
		}
		if ( vLight->lightShader->IsBlendLight() ) {
			continue;
		}

		if ( vLight->localInteractions == NULL && vLight->globalInteractions == NULL && vLight->translucentInteractions == NULL ) {
			continue;
		}

		const idMaterial * lightShader = vLight->lightShader;
		renderLog.OpenBlock( lightShader->GetName() );

		// set the depth bounds for the whole light
		if ( useLightDepthBounds ) {
			GL_DepthBoundsTest( vLight->scissorRect.zmin, vLight->scissorRect.zmax );
		}

		// only need to clear the stencil buffer and perform stencil testing if there are shadows
		const bool performStencilTest = ( vLight->globalShadows != NULL || vLight->localShadows != NULL );

		// mirror flips the sense of the stencil select, and I don't want to risk accidentally breaking it
		// in the normal case, so simply disable the stencil select in the mirror case
		const bool useLightStencilSelect = ( r_useLightStencilSelect.GetBool() && m_viewDef->isMirror == false );

		if ( performStencilTest ) {
			if ( useLightStencilSelect ) {
				// write a stencil mask for the visible light bounds to hi-stencil
				StencilSelectLight( vLight );
			} else {
				// always clear whole S-Cull tiles
				idScreenRect rect;
				rect.x1 = ( vLight->scissorRect.x1 +  0 ) & ~15;
				rect.y1 = ( vLight->scissorRect.y1 +  0 ) & ~15;
				rect.x2 = ( vLight->scissorRect.x2 + 15 ) & ~15;
				rect.y2 = ( vLight->scissorRect.y2 + 15 ) & ~15;

				if ( !m_currentScissor.Equals( rect ) && r_useScissor.GetBool() ) {
					GL_Scissor( m_viewDef->viewport.x1 + rect.x1,
								m_viewDef->viewport.y1 + rect.y1,
								rect.x2 + 1 - rect.x1,
								rect.y2 + 1 - rect.y1 );
					m_currentScissor = rect;
				}
				GL_State( GLS_DEFAULT );	// make sure stencil mask passes for the clear
				GL_Clear( false, false, true, STENCIL_SHADOW_TEST_VALUE, 0.0f, 0.0f, 0.0f, 0.0f );
			}
		}

		if ( vLight->globalShadows != NULL ) {
			renderLog.OpenBlock( "Global Light Shadows" );
			StencilShadowPass( vLight->globalShadows, vLight );
			renderLog.CloseBlock();
		}

		if ( vLight->localInteractions != NULL ) {
			renderLog.OpenBlock( "Local Light Interactions" );
			RenderInteractions( vLight->localInteractions, vLight, GLS_DEPTHFUNC_EQUAL, performStencilTest, useLightDepthBounds );
			renderLog.CloseBlock();
		}

		if ( vLight->localShadows != NULL ) {
			renderLog.OpenBlock( "Local Light Shadows" );
			StencilShadowPass( vLight->localShadows, vLight );
			renderLog.CloseBlock();
		}

		if ( vLight->globalInteractions != NULL ) {
			renderLog.OpenBlock( "Global Light Interactions" );
			RenderInteractions( vLight->globalInteractions, vLight, GLS_DEPTHFUNC_EQUAL, performStencilTest, useLightDepthBounds );
			renderLog.CloseBlock();
		}

		if ( vLight->translucentInteractions != NULL && !r_skipTranslucent.GetBool() ) {
			renderLog.OpenBlock( "Translucent Interactions" );

			// Disable the depth bounds test because translucent surfaces don't work with
			// the depth bounds tests since they did not write depth during the depth pass.
			if ( useLightDepthBounds ) {
				GL_DepthBoundsTest( 0.0f, 0.0f );
			}

			// The depth buffer wasn't filled in for translucent surfaces, so they
			// can never be constrained to perforated surfaces with the depthfunc equal.

			// Translucent surfaces do not receive shadows. This is a case where a
			// shadow buffer solution would work but stencil shadows do not because
			// stencil shadows only affect surfaces that contribute to the view depth
			// buffer and translucent surfaces do not contribute to the view depth buffer.

			RenderInteractions( vLight->translucentInteractions, vLight, GLS_DEPTHFUNC_LESS, false, false );

			renderLog.CloseBlock();
		}

		renderLog.CloseBlock();
	}

	// disable stencil shadow test
	GL_State( GLS_DEFAULT );

	// reset depth bounds
	if ( useLightDepthBounds ) {
		GL_DepthBoundsTest( 0.0f, 0.0f );
	}

	renderLog.CloseBlock();
	renderLog.CloseMainBlock();
}

/*
=============================================================================================

NON-INTERACTION SHADER PASSES

=============================================================================================
*/

/*
=====================
idRenderBackend::DrawShaderPasses

Draw non-light dependent passes

If we are rendering Guis, the drawSurf_t::sort value is a depth offset that can
be multiplied by guiEye for polarity and screenSeparation for scale.
=====================
*/
int idRenderBackend::DrawShaderPasses( const drawSurf_t * const * const drawSurfs, const int numDrawSurfs ) {
	// only obey skipAmbient if we are rendering a view
	if ( m_viewDef->viewEntitys && r_skipAmbient.GetBool() ) {
		return numDrawSurfs;
	}

	renderLog.OpenBlock( "RB_DrawShaderPasses" );

	m_currentSpace = (const viewEntity_t *)1;	// using NULL makes /analyze think surf->space needs to be checked...

	int i = 0;
	for ( ; i < numDrawSurfs; i++ ) {
		const drawSurf_t * surf = drawSurfs[i];
		const idMaterial * shader = surf->material;

		if ( !shader->HasAmbient() ) {
			continue;
		}

		if ( shader->IsPortalSky() ) {
			continue;
		}

		// some deforms may disable themselves by setting numIndexes = 0
		if ( surf->numIndexes == 0 ) {
			continue;
		}

		if ( shader->SuppressInSubview() ) {
			continue;
		}

		if ( m_viewDef->isXraySubview && surf->space->entityDef ) {
			if ( surf->space->entityDef->parms.xrayIndex != 2 ) {
				continue;
			}
		}

		// we need to draw the post process shaders after we have drawn the fog lights
		if ( shader->GetSort() >= SS_POST_PROCESS && !m_currentRenderCopied ) {
			break;
		}

		renderLog.OpenBlock( shader->GetName() );

		// change the matrix and other space related vars if needed
		if ( surf->space != m_currentSpace ) {
			m_currentSpace = surf->space;

			const viewEntity_t *space = m_currentSpace;

			RB_SetMVP( space->mvp );

			// set eye position in local space
			idVec4 localViewOrigin( 1.0f );
			R_GlobalPointToLocal( space->modelMatrix, m_viewDef->renderView.vieworg, localViewOrigin.ToVec3() );
			renderProgManager.SetRenderParm( RENDERPARM_LOCALVIEWORIGIN, localViewOrigin.ToFloatPtr() );

			// set model Matrix
			float modelMatrixTranspose[16];
			R_MatrixTranspose( space->modelMatrix, modelMatrixTranspose );
			renderProgManager.SetRenderParms( RENDERPARM_MODELMATRIX_X, modelMatrixTranspose, 4 );

			// Set ModelView Matrix
			float modelViewMatrixTranspose[16];
			R_MatrixTranspose( space->modelViewMatrix, modelViewMatrixTranspose );
			renderProgManager.SetRenderParms( RENDERPARM_MODELVIEWMATRIX_X, modelViewMatrixTranspose, 4 );
		}

		// change the scissor if needed
		if ( !m_currentScissor.Equals( surf->scissorRect ) && r_useScissor.GetBool() ) {
			GL_Scissor( m_viewDef->viewport.x1 + surf->scissorRect.x1, 
						m_viewDef->viewport.y1 + surf->scissorRect.y1,
						surf->scissorRect.x2 + 1 - surf->scissorRect.x1,
						surf->scissorRect.y2 + 1 - surf->scissorRect.y1 );
			m_currentScissor = surf->scissorRect;
		}

		// get the expressions for conditionals / color / texcoords
		const float	*regs = surf->shaderRegisters;

		// set face culling appropriately
		uint64 cullMode;
		if ( surf->space->isGuiSurface ) {
			cullMode = GLS_CULL_TWOSIDED;
		} else {
			switch ( shader->GetCullType() ) {
				case CT_TWO_SIDED:
					cullMode = GLS_CULL_TWOSIDED;
					break;
				case CT_BACK_SIDED:
					cullMode = GLS_CULL_BACKSIDED;
					break;
				case CT_FRONT_SIDED:
				default:
					cullMode = GLS_CULL_FRONTSIDED;
					break;
			}
		}

		uint64 surfGLState = surf->extraGLState | cullMode;

		// set polygon offset if necessary
		if ( shader->TestMaterialFlag( MF_POLYGONOFFSET ) ) {
			GL_PolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset() );
			surfGLState = GLS_POLYGON_OFFSET;
		}

		for ( int stage = 0; stage < shader->GetNumStages(); stage++ ) {		
			const shaderStage_t *pStage = shader->GetStage( stage );

			// check the enable condition
			if ( regs[ pStage->conditionRegister ] == 0 ) {
				continue;
			}

			// skip the stages involved in lighting
			if ( pStage->lighting != SL_AMBIENT ) {
				continue;
			}

			uint64 stageGLState = surfGLState;
			if ( ( surfGLState & GLS_OVERRIDE ) == 0 ) {
				stageGLState |= pStage->drawStateBits;
			}

			// skip if the stage is ( GL_ZERO, GL_ONE ), which is used for some alpha masks
			if ( ( stageGLState & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) == ( GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE ) ) {
				continue;
			}

			// see if we are a new-style stage
			newShaderStage_t *newStage = pStage->newStage;
			if ( newStage != NULL ) {
				//--------------------------
				//
				// new style stages
				//
				//--------------------------
				if ( r_skipNewAmbient.GetBool() ) {
					continue;
				}
				renderLog.OpenBlock( "New Shader Stage" );

				GL_State( stageGLState );
			
				renderProgManager.BindProgram( newStage->glslProgram );

				for ( int j = 0; j < newStage->numVertexParms; j++ ) {
					float parm[4];
					parm[0] = regs[ newStage->vertexParms[j][0] ];
					parm[1] = regs[ newStage->vertexParms[j][1] ];
					parm[2] = regs[ newStage->vertexParms[j][2] ];
					parm[3] = regs[ newStage->vertexParms[j][3] ];
					renderProgManager.SetRenderParm( (renderParm_t)( RENDERPARM_USER0 + j ), parm );
				}

				const renderProg_t & prog = renderProgManager.GetCurrentRenderProg();

				// set rpEnableSkinning if the shader has optional support for skinning
				if ( surf->jointCache && prog.optionalSkinning ) {
					const idVec4 skinningParm( 1.0f );
					renderProgManager.SetRenderParm( RENDERPARM_ENABLE_SKINNING, skinningParm.ToFloatPtr() );
				}

				// bind texture units
				for ( int j = 0; j < newStage->numFragmentProgramImages; j++ ) {
					idImage * image = newStage->fragmentProgramImages[ j ];
					if ( image != NULL ) {
						GL_BindTexture( j, image );
					}
				}

				// draw it
				DrawElementsWithCounters( surf );

				// clear rpEnableSkinning if it was set
				if ( surf->jointCache && prog.optionalSkinning ) {
					const idVec4 skinningParm( 0.0f );
					renderProgManager.SetRenderParm( RENDERPARM_ENABLE_SKINNING, skinningParm.ToFloatPtr() );
				}

				renderLog.CloseBlock();
				continue;
			}

			//--------------------------
			//
			// old style stages
			//
			//--------------------------

			// set the color
			float color[4];
			color[0] = regs[ pStage->color.registers[0] ];
			color[1] = regs[ pStage->color.registers[1] ];
			color[2] = regs[ pStage->color.registers[2] ];
			color[3] = regs[ pStage->color.registers[3] ];

			// skip the entire stage if an add would be black
			if ( ( stageGLState & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) == ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE ) 
				&& color[0] <= 0 && color[1] <= 0 && color[2] <= 0 ) {
				continue;
			}

			// skip the entire stage if a blend would be completely transparent
			if ( ( stageGLState & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) == ( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA )
				&& color[3] <= 0 ) {
				continue;
			}

			stageVertexColor_t svc = pStage->vertexColor;

			renderLog.OpenBlock( "Old Shader Stage" );
			GL_Color( color );

			if ( surf->space->isGuiSurface ) {
				// Force gui surfaces to always be SVC_MODULATE
				svc = SVC_MODULATE;

				// use special shaders for bink cinematics
				if ( pStage->texture.cinematic ) {
					if ( ( stageGLState & GLS_OVERRIDE ) != 0 ) {
						// This is a hack... Only SWF Guis set GLS_OVERRIDE
						// Old style guis do not, and we don't want them to use the new GUI renederProg
						renderProgManager.BindProgram( BUILTIN_BINK_GUI );
					} else {
						renderProgManager.BindProgram( BUILTIN_BINK );
					}
				} else {
					if ( ( stageGLState & GLS_OVERRIDE ) != 0 ) {
						// This is a hack... Only SWF Guis set GLS_OVERRIDE
						// Old style guis do not, and we don't want them to use the new GUI renderProg
						renderProgManager.BindProgram( BUILTIN_GUI );
					} else {
						if ( surf->jointCache ) {
							renderProgManager.BindProgram( BUILTIN_TEXTURE_VERTEXCOLOR_SKINNED );
						} else {
							renderProgManager.BindProgram( BUILTIN_TEXTURE_VERTEXCOLOR );
						}
					}
				}
			} else if ( ( pStage->texture.texgen == TG_SCREEN ) || ( pStage->texture.texgen == TG_SCREEN2 ) ) {
				renderProgManager.BindProgram( BUILTIN_TEXTURE_TEXGEN_VERTEXCOLOR );
			} else if ( pStage->texture.cinematic ) {
				renderProgManager.BindProgram( BUILTIN_BINK );
			} else {
				if ( surf->jointCache ) {
					renderProgManager.BindProgram( BUILTIN_TEXTURE_VERTEXCOLOR_SKINNED );
				} else {
					renderProgManager.BindProgram( BUILTIN_TEXTURE_VERTEXCOLOR );
				}
			}
		
			RB_SetVertexColorParms( svc );

			// bind the texture
			BindVariableStageImage( &pStage->texture, regs );

			// set privatePolygonOffset if necessary
			if ( pStage->privatePolygonOffset ) {
				GL_PolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * pStage->privatePolygonOffset );
				stageGLState |= GLS_POLYGON_OFFSET;
			}

			// set the state
			GL_State( stageGLState );
		
			PrepareStageTexturing( pStage, surf );

			// draw it
			DrawElementsWithCounters( surf );

			// unset privatePolygonOffset if necessary
			if ( pStage->privatePolygonOffset ) {
				GL_PolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset() );
			}
			renderLog.CloseBlock();
		}

		renderLog.CloseBlock();
	}

	GL_State( m_glStateBits & ~( GLS_CULL_MASK ) | GLS_CULL_FRONTSIDED );
	GL_Color( 1.0f, 1.0f, 1.0f );

	renderLog.CloseBlock();
	return i;
}

/*
=============================================================================================

BLEND LIGHT PROJECTION

=============================================================================================
*/

/*
=====================
idRenderBackend::T_BlendLight
=====================
*/
void idRenderBackend::T_BlendLight( const drawSurf_t *drawSurfs, const viewLight_t * vLight ) {
	m_currentSpace = NULL;

	for ( const drawSurf_t * drawSurf = drawSurfs; drawSurf != NULL; drawSurf = drawSurf->nextOnLight ) {
		if ( drawSurf->scissorRect.IsEmpty() ) {
			continue;	// !@# FIXME: find out why this is sometimes being hit!
						// temporarily jump over the scissor and draw so the gl error callback doesn't get hit
		}

		if ( !m_currentScissor.Equals( drawSurf->scissorRect ) && r_useScissor.GetBool() ) {
			// change the scissor
			GL_Scissor( m_viewDef->viewport.x1 + drawSurf->scissorRect.x1,
						m_viewDef->viewport.y1 + drawSurf->scissorRect.y1,
						drawSurf->scissorRect.x2 + 1 - drawSurf->scissorRect.x1,
						drawSurf->scissorRect.y2 + 1 - drawSurf->scissorRect.y1 );
			m_currentScissor = drawSurf->scissorRect;
		}

		if ( drawSurf->space != m_currentSpace ) {
			// change the matrix
			RB_SetMVP( drawSurf->space->mvp );

			// change the light projection matrix
			idPlane	lightProjectInCurrentSpace[4];
			for ( int i = 0; i < 4; i++ ) {
				R_GlobalPlaneToLocal( drawSurf->space->modelMatrix, vLight->lightProject[i], lightProjectInCurrentSpace[i] );
			}

			renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_0_S, lightProjectInCurrentSpace[0].ToFloatPtr() );
			renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_0_T, lightProjectInCurrentSpace[1].ToFloatPtr() );
			renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_0_Q, lightProjectInCurrentSpace[2].ToFloatPtr() );
			renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_1_S, lightProjectInCurrentSpace[3].ToFloatPtr() );	// falloff

			m_currentSpace = drawSurf->space;
		}

		DrawElementsWithCounters( drawSurf );
	}
}

/*
=====================
idRenderBackend::BlendLight

Dual texture together the falloff and projection texture with a blend
mode to the framebuffer, instead of interacting with the surface texture
=====================
*/
void idRenderBackend::BlendLight( const drawSurf_t *drawSurfs, const drawSurf_t *drawSurfs2, const viewLight_t * vLight ) {
	if ( drawSurfs == NULL ) {
		return;
	}
	if ( r_skipBlendLights.GetBool() ) {
		return;
	}
	renderLog.OpenBlock( vLight->lightShader->GetName() );

	const idMaterial * lightShader = vLight->lightShader;
	const float	* regs = vLight->shaderRegisters;

	// texture 1 will get the falloff texture
	GL_BindTexture( 1, vLight->falloffImage );

	// texture 0 will get the projected texture

	renderProgManager.BindProgram( BUILTIN_BLENDLIGHT );

	for ( int i = 0; i < lightShader->GetNumStages(); i++ ) {
		const shaderStage_t	*stage = lightShader->GetStage(i);

		if ( !regs[ stage->conditionRegister ] ) {
			continue;
		}

		GL_State( GLS_DEPTHMASK | stage->drawStateBits | GLS_DEPTHFUNC_EQUAL );

		GL_BindTexture( 0, stage->texture.image );

		if ( stage->texture.hasMatrix ) {
			RB_LoadShaderTextureMatrix( regs, &stage->texture );
		}

		// get the modulate values from the light, including alpha, unlike normal lights
		float lightColor[4];
		lightColor[0] = regs[ stage->color.registers[0] ];
		lightColor[1] = regs[ stage->color.registers[1] ];
		lightColor[2] = regs[ stage->color.registers[2] ];
		lightColor[3] = regs[ stage->color.registers[3] ];
		GL_Color( lightColor );

		T_BlendLight( drawSurfs, vLight );
		T_BlendLight( drawSurfs2, vLight );
	}

	renderLog.CloseBlock();
}

/*
=========================================================================================================

FOG LIGHTS

=========================================================================================================
*/

/*
=====================
idRenderBackend::T_BasicFog
=====================
*/
void idRenderBackend::T_BasicFog( const drawSurf_t * drawSurfs, const idPlane fogPlanes[ 4 ], const idRenderMatrix * inverseBaseLightProject ) {
	m_currentSpace = NULL;

	for ( const drawSurf_t * drawSurf = drawSurfs; drawSurf != NULL; drawSurf = drawSurf->nextOnLight ) {
		if ( drawSurf->scissorRect.IsEmpty() ) {
			continue;	// !@# FIXME: find out why this is sometimes being hit!
						// temporarily jump over the scissor and draw so the gl error callback doesn't get hit
		}

		if ( !m_currentScissor.Equals( drawSurf->scissorRect ) && r_useScissor.GetBool() ) {
			// change the scissor
			GL_Scissor( m_viewDef->viewport.x1 + drawSurf->scissorRect.x1,
						m_viewDef->viewport.y1 + drawSurf->scissorRect.y1,
						drawSurf->scissorRect.x2 + 1 - drawSurf->scissorRect.x1,
						drawSurf->scissorRect.y2 + 1 - drawSurf->scissorRect.y1 );
			m_currentScissor = drawSurf->scissorRect;
		}

		if ( drawSurf->space != m_currentSpace ) {
			idPlane localFogPlanes[4];
			if ( inverseBaseLightProject == NULL ) {
				RB_SetMVP( drawSurf->space->mvp );
				for ( int i = 0; i < 4; i++ ) {
					R_GlobalPlaneToLocal( drawSurf->space->modelMatrix, fogPlanes[i], localFogPlanes[i] );
				}
			} else {
				idRenderMatrix invProjectMVPMatrix;
				idRenderMatrix::Multiply( m_viewDef->worldSpace.mvp, *inverseBaseLightProject, invProjectMVPMatrix );
				RB_SetMVP( invProjectMVPMatrix );
				for ( int i = 0; i < 4; i++ ) {
					inverseBaseLightProject->InverseTransformPlane( fogPlanes[i], localFogPlanes[i], false );
				}
			}

			renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_0_S, localFogPlanes[0].ToFloatPtr() );
			renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_0_T, localFogPlanes[1].ToFloatPtr() );
			renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_1_T, localFogPlanes[2].ToFloatPtr() );
			renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_1_S, localFogPlanes[3].ToFloatPtr() );

			m_currentSpace = ( inverseBaseLightProject == NULL ) ? drawSurf->space : NULL;
		}

		if ( drawSurf->jointCache ) {
			renderProgManager.BindProgram( BUILTIN_FOG_SKINNED );
		} else {
			renderProgManager.BindProgram( BUILTIN_FOG );
		}

		DrawElementsWithCounters( drawSurf );
	}
}

/*
==================
idRenderBackend::FogPass
==================
*/
void idRenderBackend::FogPass( const drawSurf_t * drawSurfs,  const drawSurf_t * drawSurfs2, const viewLight_t * vLight ) {
	renderLog.OpenBlock( vLight->lightShader->GetName() );

	// find the current color and density of the fog
	const idMaterial * lightShader = vLight->lightShader;
	const float * regs = vLight->shaderRegisters;
	// assume fog shaders have only a single stage
	const shaderStage_t * stage = lightShader->GetStage( 0 );

	float lightColor[4];
	lightColor[0] = regs[ stage->color.registers[0] ];
	lightColor[1] = regs[ stage->color.registers[1] ];
	lightColor[2] = regs[ stage->color.registers[2] ];
	lightColor[3] = regs[ stage->color.registers[3] ];

	GL_Color( lightColor );

	// calculate the falloff planes
	float a;

	// if they left the default value on, set a fog distance of 500
	if ( lightColor[3] <= 1.0f ) {
		a = -0.5f / DEFAULT_FOG_DISTANCE;
	} else {
		// otherwise, distance = alpha color
		a = -0.5f / lightColor[3];
	}

	// texture 0 is the falloff image
	GL_BindTexture( 0, globalImages->m_fogImage );

	// texture 1 is the entering plane fade correction
	GL_BindTexture( 1, globalImages->m_fogEnterImage );

	// S is based on the view origin
	const float s = vLight->fogPlane.Distance( m_viewDef->renderView.vieworg );

	const float FOG_SCALE = 0.001f;

	idPlane fogPlanes[4];

	// S-0
	fogPlanes[0][0] = a * m_viewDef->worldSpace.modelViewMatrix[0*4+2];
	fogPlanes[0][1] = a * m_viewDef->worldSpace.modelViewMatrix[1*4+2];
	fogPlanes[0][2] = a * m_viewDef->worldSpace.modelViewMatrix[2*4+2];
	fogPlanes[0][3] = a * m_viewDef->worldSpace.modelViewMatrix[3*4+2] + 0.5f;

	// T-0
	fogPlanes[1][0] = 0.0f;//a * m_viewDef->worldSpace.modelViewMatrix[0*4+0];
	fogPlanes[1][1] = 0.0f;//a * m_viewDef->worldSpace.modelViewMatrix[1*4+0];
	fogPlanes[1][2] = 0.0f;//a * m_viewDef->worldSpace.modelViewMatrix[2*4+0];
	fogPlanes[1][3] = 0.5f;//a * m_viewDef->worldSpace.modelViewMatrix[3*4+0] + 0.5f;

	// T-1 will get a texgen for the fade plane, which is always the "top" plane on unrotated lights
	fogPlanes[2][0] = FOG_SCALE * vLight->fogPlane[0];
	fogPlanes[2][1] = FOG_SCALE * vLight->fogPlane[1];
	fogPlanes[2][2] = FOG_SCALE * vLight->fogPlane[2];
	fogPlanes[2][3] = FOG_SCALE * vLight->fogPlane[3] + FOG_ENTER;

	// S-1
	fogPlanes[3][0] = 0.0f;
	fogPlanes[3][1] = 0.0f;
	fogPlanes[3][2] = 0.0f;
	fogPlanes[3][3] = FOG_SCALE * s + FOG_ENTER;

	// draw it
	GL_State( GLS_DEPTHMASK | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL );
	T_BasicFog( drawSurfs, fogPlanes, NULL );
	T_BasicFog( drawSurfs2, fogPlanes, NULL );

	// the light frustum bounding planes aren't in the depth buffer, so use depthfunc_less instead
	// of depthfunc_equal
	GL_State( GLS_DEPTHMASK | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_LESS | GLS_CULL_BACKSIDED );

	m_zeroOneCubeSurface.space = &m_viewDef->worldSpace;
	m_zeroOneCubeSurface.scissorRect = m_viewDef->scissor;
	T_BasicFog( &m_zeroOneCubeSurface, fogPlanes, &vLight->inverseBaseLightProject );

	GL_State( m_glStateBits & ~( GLS_CULL_MASK ) | GLS_CULL_FRONTSIDED );

	renderLog.CloseBlock();
}

/*
==================
idRenderBackend::FogAllLights
==================
*/
void idRenderBackend::FogAllLights() {
	if ( r_skipFogLights.GetBool() || r_showOverDraw.GetInteger() != 0 
		 || m_viewDef->isXraySubview /* don't fog in xray mode*/ ) {
		return;
	}
	renderLog.OpenMainBlock( MRB_FOG_ALL_LIGHTS );
	renderLog.OpenBlock( "RB_FogAllLights" );

	// force fog plane to recalculate
	m_currentSpace = NULL;

	for ( viewLight_t * vLight = m_viewDef->viewLights; vLight != NULL; vLight = vLight->next ) {
		if ( vLight->lightShader->IsFogLight() ) {
			FogPass( vLight->globalInteractions, vLight->localInteractions, vLight );
		} else if ( vLight->lightShader->IsBlendLight() ) {
			BlendLight( vLight->globalInteractions, vLight->localInteractions, vLight );
		}
	}

	renderLog.CloseBlock();
	renderLog.CloseMainBlock();
}

/*
==============================================================================================

STENCIL SHADOW RENDERING

==============================================================================================
*/

/*
=====================
idRenderBackend::StencilShadowPass

The stencil buffer should have been set to 128 on any surfaces that might receive shadows.
=====================
*/
void idRenderBackend::StencilShadowPass( const drawSurf_t *drawSurfs, const viewLight_t * vLight ) {
	if ( r_skipShadows.GetBool() ) {
		return;
	}

	if ( drawSurfs == NULL ) {
		return;
	}

	RENDERLOG_PRINTF( "---------- RB_StencilShadowPass ----------\n" );

	renderProgManager.BindProgram( BUILTIN_SHADOW );

	uint64 glState = 0;

	// for visualizing the shadows
	if ( r_showShadows.GetInteger() ) {
		// set the debug shadow color
		renderProgManager.SetRenderParm( RENDERPARM_COLOR, colorMagenta.ToFloatPtr() );
		if ( r_showShadows.GetInteger() == 2 ) {
			// draw filled in
			glState = GLS_DEPTHMASK | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_LESS;
		} else {
			// draw as lines, filling the depth buffer
			glState = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO | GLS_POLYMODE_LINE | GLS_DEPTHFUNC_ALWAYS;
		}
	} else {
		// don't write to the color or depth buffer, just the stencil buffer
		glState = GLS_DEPTHMASK | GLS_COLORMASK | GLS_ALPHAMASK | GLS_DEPTHFUNC_LESS;
	}

	GL_PolygonOffset( r_shadowPolygonFactor.GetFloat(), -r_shadowPolygonOffset.GetFloat() );

	// the actual stencil func will be set in the draw code, but we need to make sure it isn't
	// disabled here, and that the value will get reset for the interactions without looking
	// like a no-change-required
	// Two Sided Stencil reduces two draw calls to one for slightly faster shadows
	GL_State( 
		glState | 
		GLS_STENCIL_OP_FAIL_KEEP | 
		GLS_STENCIL_OP_ZFAIL_KEEP | 
		GLS_STENCIL_OP_PASS_INCR | 
		GLS_STENCIL_MAKE_REF( STENCIL_SHADOW_TEST_VALUE ) | 
		GLS_STENCIL_MAKE_MASK( STENCIL_SHADOW_MASK_VALUE ) | 
		GLS_POLYGON_OFFSET | 
		GLS_CULL_TWOSIDED );

	// process the chain of shadows with the current rendering state
	m_currentSpace = NULL;

	for ( const drawSurf_t * drawSurf = drawSurfs; drawSurf != NULL; drawSurf = drawSurf->nextOnLight ) {
		if ( drawSurf->scissorRect.IsEmpty() ) {
			continue;	// !@# FIXME: find out why this is sometimes being hit!
						// temporarily jump over the scissor and draw so the gl error callback doesn't get hit
		}

		// make sure the shadow volume is done
		if ( drawSurf->shadowVolumeState != SHADOWVOLUME_DONE ) {
			assert( drawSurf->shadowVolumeState == SHADOWVOLUME_UNFINISHED || drawSurf->shadowVolumeState == SHADOWVOLUME_DONE );

			while ( drawSurf->shadowVolumeState == SHADOWVOLUME_UNFINISHED ) {
				Sys_Yield();
			}
		}

		if ( drawSurf->numIndexes == 0 ) {
			continue;	// a job may have created an empty shadow volume
		}

		if ( !m_currentScissor.Equals( drawSurf->scissorRect ) && r_useScissor.GetBool() ) {
			// change the scissor
			GL_Scissor( m_viewDef->viewport.x1 + drawSurf->scissorRect.x1,
						m_viewDef->viewport.y1 + drawSurf->scissorRect.y1,
						drawSurf->scissorRect.x2 + 1 - drawSurf->scissorRect.x1,
						drawSurf->scissorRect.y2 + 1 - drawSurf->scissorRect.y1 );
			m_currentScissor = drawSurf->scissorRect;
		}

		if ( drawSurf->space != m_currentSpace ) {
			// change the matrix
			RB_SetMVP( drawSurf->space->mvp );

			// set the local light position to allow the vertex program to project the shadow volume end cap to infinity
			idVec4 localLight( 0.0f );
			R_GlobalPointToLocal( drawSurf->space->modelMatrix, vLight->globalLightOrigin, localLight.ToVec3() );
			renderProgManager.SetRenderParm( RENDERPARM_LOCALLIGHTORIGIN, localLight.ToFloatPtr() );

			m_currentSpace = drawSurf->space;
		}

		if ( r_showShadows.GetInteger() == 0 ) {
			if ( drawSurf->jointCache ) {
				renderProgManager.BindProgram( BUILTIN_SHADOW_SKINNED );
			} else {
				renderProgManager.BindProgram( BUILTIN_SHADOW );
			}
		} else {
			if ( drawSurf->jointCache ) {
				renderProgManager.BindProgram( BUILTIN_SHADOW_DEBUG_SKINNED );
			} else {
				renderProgManager.BindProgram( BUILTIN_SHADOW_DEBUG );
			}
		}

		// set depth bounds per shadow
		if ( r_useShadowDepthBounds.GetBool() ) {
			GL_DepthBoundsTest( drawSurf->scissorRect.zmin, drawSurf->scissorRect.zmax );
		}

		// Determine whether or not the shadow volume needs to be rendered with Z-pass or
		// Z-fail. It is worthwhile to spend significant resources to reduce the number of
		// cases where shadow volumes need to be rendered with Z-fail because Z-fail
		// rendering can be significantly slower even on today's hardware. For instance,
		// on NVIDIA hardware Z-fail rendering causes the Z-Cull to be used in reverse:
		// Z-near becomes Z-far (trivial accept becomes trivial reject). Using the Z-Cull
		// in reverse is far less efficient because the Z-Cull only stores Z-near per 16x16
		// pixels while the Z-far is stored per 4x2 pixels. (The Z-near coallesce buffer
		// which has 4x4 granularity is only used when updating the depth which is not the
		// case for shadow volumes.) Note that it is also important to NOT use a Z-Cull
		// reconstruct because that would clear the Z-near of the Z-Cull which results in
		// no trivial rejection for Z-fail stencil shadow rendering.

		const bool renderZPass = ( drawSurf->renderZFail == 0 ) || r_forceZPassStencilShadows.GetBool();

		DrawStencilShadowPass( drawSurf, renderZPass );
	}

	// cleanup the shadow specific rendering state

	GL_State( m_glStateBits & ~( GLS_CULL_MASK ) | GLS_CULL_FRONTSIDED );

	// reset depth bounds
	if ( r_useShadowDepthBounds.GetBool() ) {
		if ( r_useLightDepthBounds.GetBool() ) {
			GL_DepthBoundsTest( vLight->scissorRect.zmin, vLight->scissorRect.zmax );
		} else {
			GL_DepthBoundsTest( 0.0f, 0.0f );
		}
	}
}

/*
==================
idRenderBackend::StencilSelectLight

Deform the zeroOneCubeModel to exactly cover the light volume. Render the deformed cube model to the stencil buffer in
such a way that only fragments that are directly visible and contained within the volume will be written creating a 
mask to be used by the following stencil shadow and draw interaction passes.
==================
*/
void idRenderBackend::StencilSelectLight( const viewLight_t * vLight ) {
	renderLog.OpenBlock( "Stencil Select" );

	// enable the light scissor
	if ( !m_currentScissor.Equals( vLight->scissorRect ) && r_useScissor.GetBool() ) {
		GL_Scissor( m_viewDef->viewport.x1 + vLight->scissorRect.x1, 
					m_viewDef->viewport.y1 + vLight->scissorRect.y1,
					vLight->scissorRect.x2 + 1 - vLight->scissorRect.x1,
					vLight->scissorRect.y2 + 1 - vLight->scissorRect.y1 );
		m_currentScissor = vLight->scissorRect;
	}

	// clear stencil buffer to 0 (not drawable)
	uint64 glStateMinusStencil = GL_GetCurrentStateMinusStencil();
	GL_State( 
		glStateMinusStencil | 
		GLS_STENCIL_FUNC_ALWAYS | 
		GLS_STENCIL_MAKE_REF( STENCIL_SHADOW_TEST_VALUE ) | 
		GLS_STENCIL_MAKE_MASK( STENCIL_SHADOW_MASK_VALUE ) );	// make sure stencil mask passes for the clear
	GL_Clear( false, false, true, 0, 0.0f, 0.0f, 0.0f, 0.0f );	// clear to 0 for stencil select

	// set the depthbounds
	GL_DepthBoundsTest( vLight->scissorRect.zmin, vLight->scissorRect.zmax );

	// two-sided stencil test
	GL_State( 
		GLS_COLORMASK | 
		GLS_ALPHAMASK | 
		GLS_CULL_TWOSIDED | 
		GLS_DEPTHMASK | 
		GLS_DEPTHFUNC_LESS | 
		GLS_STENCIL_FUNC_ALWAYS | 
		GLS_STENCIL_OP_FAIL_KEEP | GLS_STENCIL_OP_ZFAIL_REPLACE | GLS_STENCIL_OP_PASS_ZERO |
		GLS_BACK_STENCIL_OP_FAIL_KEEP | GLS_BACK_STENCIL_OP_ZFAIL_ZERO | GLS_BACK_STENCIL_OP_PASS_REPLACE |
		GLS_STENCIL_MAKE_REF( STENCIL_SHADOW_TEST_VALUE ) | 
		GLS_STENCIL_MAKE_MASK( STENCIL_SHADOW_MASK_VALUE ) );

	renderProgManager.BindProgram( BUILTIN_DEPTH );

	// set the matrix for deforming the 'zeroOneCubeModel' into the frustum to exactly cover the light volume
	idRenderMatrix invProjectMVPMatrix;
	idRenderMatrix::Multiply( m_viewDef->worldSpace.mvp, vLight->inverseBaseLightProject, invProjectMVPMatrix );
	RB_SetMVP( invProjectMVPMatrix );

	DrawElementsWithCounters( &m_zeroOneCubeSurface );

	// reset stencil state
	GL_State( m_glStateBits & ~( GLS_CULL_MASK ) | GLS_CULL_FRONTSIDED );

	// unset the depthbounds
	GL_DepthBoundsTest( 0.0f, 0.0f );

	renderLog.CloseBlock();
}