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
#include "../RenderSystem_local.h"
#include "../RenderBackend.h"
#include "../RenderLog.h"
#include "../RenderProgs.h"
#include "../GLState.h"
#include "../GLMatrix.h"
#include "../Interaction.h"
#include "../Image.h"
#include "../simplex.h"	// line font definition
#include "../../sys/win32/win_local.h"

idCVar r_showCenterOfProjection( "r_showCenterOfProjection", "0", CVAR_RENDERER | CVAR_BOOL, "Draw a cross to show the center of projection" );
idCVar r_showLines( "r_showLines", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = draw alternate horizontal lines, 2 = draw alternate vertical lines" );

extern idCVar r_vidMode;
extern idCVar r_fullscreen;
extern idCVar r_showTrace;
extern idCVar r_useScissor;
extern idCVar r_showTris;
extern idCVar r_showNormals;
extern idCVar r_showSurfaceInfo;
extern idCVar r_showViewEntitys;
extern idCVar r_showUnsmoothedTangents;
extern idCVar r_showTexturePolarity;
extern idCVar r_showTangentSpace;
extern idCVar r_showVertexColor;
extern idCVar r_showOverDraw;
extern idCVar r_showIntensity;
extern idCVar r_showEdges;
extern idCVar r_showDepth;
extern idCVar r_showLightCount;
extern idCVar r_showLights;
extern idCVar r_showSilhouette;
extern idCVar r_showDominantTri;
extern idCVar r_showPortals;
extern idCVar r_showTextureVectors;
extern idCVar r_debugLineDepthTest;
extern idCVar r_debugPolygonFilled;
extern idCVar r_debugLineWidth;
extern idCVar r_jitter;
extern idCVar r_swapInterval;
extern idCVar r_screenFraction;
extern idCVar r_testGamma;
extern idCVar r_testGammaBias;

static void RB_DrawText( const char *text, const idVec3 &origin, float scale, const idVec4 &color, const idMat3 &viewAxis, const int align );

void RB_SetMVP( const idRenderMatrix & mvp );

extern debugLine_t	rb_debugLines[ MAX_DEBUG_LINES ];
extern int			rb_numDebugLines;
extern int			rb_debugLineTime;

extern debugText_t	rb_debugText[ MAX_DEBUG_TEXT ];
extern int			rb_numDebugText;
extern int			rb_debugTextTime;

extern debugPolygon_t rb_debugPolygons[ MAX_DEBUG_POLYGONS ];
extern int			rb_numDebugPolygons;
extern int			rb_debugPolygonTime;

extern idImage *	testImage;
extern idCinematic*	testVideo;
extern int			testVideoStartTime;


/*
================
RB_DrawBounds
================
*/
void RB_DrawBounds( const idBounds &bounds ) {
	if ( bounds.IsCleared() ) {
		return;
	}
	qglBegin( GL_LINE_LOOP );
	qglVertex3f( bounds[0][0], bounds[0][1], bounds[0][2] );
	qglVertex3f( bounds[0][0], bounds[1][1], bounds[0][2] );
	qglVertex3f( bounds[1][0], bounds[1][1], bounds[0][2] );
	qglVertex3f( bounds[1][0], bounds[0][1], bounds[0][2] );
	qglEnd();
	qglBegin( GL_LINE_LOOP );
	qglVertex3f( bounds[0][0], bounds[0][1], bounds[1][2] );
	qglVertex3f( bounds[0][0], bounds[1][1], bounds[1][2] );
	qglVertex3f( bounds[1][0], bounds[1][1], bounds[1][2] );
	qglVertex3f( bounds[1][0], bounds[0][1], bounds[1][2] );
	qglEnd();

	qglBegin( GL_LINES );
	qglVertex3f( bounds[0][0], bounds[0][1], bounds[0][2] );
	qglVertex3f( bounds[0][0], bounds[0][1], bounds[1][2] );

	qglVertex3f( bounds[0][0], bounds[1][1], bounds[0][2] );
	qglVertex3f( bounds[0][0], bounds[1][1], bounds[1][2] );

	qglVertex3f( bounds[1][0], bounds[0][1], bounds[0][2] );
	qglVertex3f( bounds[1][0], bounds[0][1], bounds[1][2] );

	qglVertex3f( bounds[1][0], bounds[1][1], bounds[0][2] );
	qglVertex3f( bounds[1][0], bounds[1][1], bounds[1][2] );
	qglEnd();
}


/*
================
idRenderBackend::DBG_SimpleSurfaceSetup
================
*/
void idRenderBackend::DBG_SimpleSurfaceSetup( const drawSurf_t *drawSurf ) {
	// change the matrix if needed
	if ( drawSurf->space != m_currentSpace ) {
		qglLoadMatrixf( drawSurf->space->modelViewMatrix );
		m_currentSpace = drawSurf->space;
	}

	// change the scissor if needed
	if ( !m_currentScissor.Equals( drawSurf->scissorRect ) && r_useScissor.GetBool() ) {
		GL_Scissor( m_viewDef->viewport.x1 + drawSurf->scissorRect.x1, 
					m_viewDef->viewport.y1 + drawSurf->scissorRect.y1,
					drawSurf->scissorRect.x2 + 1 - drawSurf->scissorRect.x1,
					drawSurf->scissorRect.y2 + 1 - drawSurf->scissorRect.y1 );
		m_currentScissor = drawSurf->scissorRect;
	}
}

/*
================
idRenderBackend::DBG_SimpleWorldSetup
================
*/
void idRenderBackend::DBG_SimpleWorldSetup() {
	m_currentSpace = &m_viewDef->worldSpace;

	qglLoadMatrixf( m_viewDef->worldSpace.modelViewMatrix );

	GL_Scissor( m_viewDef->viewport.x1 + m_viewDef->scissor.x1,
				m_viewDef->viewport.y1 + m_viewDef->scissor.y1,
				m_viewDef->scissor.x2 + 1 - m_viewDef->scissor.x1,
				m_viewDef->scissor.y2 + 1 - m_viewDef->scissor.y1 );
	m_currentScissor = m_viewDef->scissor;
}

/*
=================
RB_PolygonClear

This will cover the entire screen with normal rasterization.
Texturing is disabled, but the existing glColor, glDepthMask,
glColorMask, and the enabled state of depth buffering and
stenciling will matter.
=================
*/
void RB_PolygonClear() {
	qglPushMatrix();
	qglPushAttrib( GL_ALL_ATTRIB_BITS  );
	qglLoadIdentity();
	qglDisable( GL_TEXTURE_2D );
	qglDisable( GL_DEPTH_TEST );
	qglDisable( GL_CULL_FACE );
	qglDisable( GL_SCISSOR_TEST );
	qglBegin( GL_POLYGON );
	qglVertex3f( -20, -20, -10 );
	qglVertex3f( 20, -20, -10 );
	qglVertex3f( 20, 20, -10 );
	qglVertex3f( -20, 20, -10 );
	qglEnd();
	qglPopAttrib();
	qglPopMatrix();
}

/*
====================
idRenderBackend::DBG_ShowDestinationAlpha
====================
*/
void idRenderBackend::DBG_ShowDestinationAlpha() {
	GL_State( GLS_SRCBLEND_DST_ALPHA | GLS_DSTBLEND_ZERO | GLS_DEPTHMASK | GLS_DEPTHFUNC_ALWAYS );
	GL_Color( 1, 1, 1 );
	RB_PolygonClear();
}

/*
===================
RB_CountStencilBuffer

Print an overdraw count based on stencil index values
===================
*/
static void RB_CountStencilBuffer() {
	int		count;
	int		i;
	byte	*stencilReadback;


	stencilReadback = (byte *)R_StaticAlloc( renderSystem->GetWidth() * renderSystem->GetHeight(), TAG_RENDER_TOOLS );
	qglReadPixels( 0, 0, renderSystem->GetWidth(), renderSystem->GetHeight(), GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback );

	count = 0;
	for ( i = 0; i < renderSystem->GetWidth() * renderSystem->GetHeight(); i++ ) {
		count += stencilReadback[i];
	}

	R_StaticFree( stencilReadback );

	// print some stats (not supposed to do from back end in SMP...)
	idLib::Printf( "overdraw: %5.1f\n", (float)count/(renderSystem->GetWidth() * renderSystem->GetHeight())  );
}

/*
===================
idRenderBackend::DBG_ColorByStencilBuffer

Sets the screen colors based on the contents of the
stencil buffer.  Stencil of 0 = black, 1 = red, 2 = green,
3 = blue, ..., 7+ = white
===================
*/
void idRenderBackend::DBG_ColorByStencilBuffer() {
	int		i;
	static float	colors[8][3] = {
		{0,0,0},
		{1,0,0},
		{0,1,0},
		{0,0,1},
		{0,1,1},
		{1,0,1},
		{1,1,0},
		{1,1,1},
	};

	// clear color buffer to white (>6 passes)
	GL_Clear( true, false, false, 0, 1.0f, 1.0f, 1.0f, 1.0f );

	// now draw color for each stencil value
	qglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
	for ( i = 0; i < 6; i++ ) {
		GL_Color( colors[i] );
		renderProgManager.BindProgram( BUILTIN_COLOR );
		qglStencilFunc( GL_EQUAL, i, 255 );
		RB_PolygonClear();
	}

	qglStencilFunc( GL_ALWAYS, 0, 255 );
}

/*
==================
idRenderBackend::DBG_ShowOverdraw
==================
*/
void idRenderBackend::DBG_ShowOverdraw() {
	const idMaterial *	material;
	int					i;
	drawSurf_t * *		drawSurfs;
	const drawSurf_t *	surf;
	int					numDrawSurfs;
	viewLight_t *		vLight;

	if ( r_showOverDraw.GetInteger() == 0 ) {
		return;
	}

	material = declManager->FindMaterial( "textures/common/overdrawtest", false );
	if ( material == NULL ) {
		return;
	}

	drawSurfs = m_viewDef->drawSurfs;
	numDrawSurfs = m_viewDef->numDrawSurfs;

	int interactions = 0;
	for ( vLight = m_viewDef->viewLights; vLight; vLight = vLight->next ) {
		for ( surf = vLight->localInteractions; surf; surf = surf->nextOnLight ) {
			interactions++;
		}
		for ( surf = vLight->globalInteractions; surf; surf = surf->nextOnLight ) {
			interactions++;
		}
	}

	// FIXME: can't frame alloc from the renderer back-end
	drawSurf_t **newDrawSurfs = (drawSurf_t **)renderSystem->FrameAlloc( numDrawSurfs + interactions * sizeof( newDrawSurfs[0] ), FRAME_ALLOC_DRAW_SURFACE_POINTER );

	for ( i = 0; i < numDrawSurfs; i++ ) {
		surf = drawSurfs[i];
		if ( surf->material ) {
			const_cast<drawSurf_t *>(surf)->material = material;
		}
		newDrawSurfs[i] = const_cast<drawSurf_t *>(surf);
	}

	for ( vLight = m_viewDef->viewLights; vLight; vLight = vLight->next ) {
		for ( surf = vLight->localInteractions; surf; surf = surf->nextOnLight ) {
			const_cast<drawSurf_t *>(surf)->material = material;
			newDrawSurfs[i++] = const_cast<drawSurf_t *>(surf);
		}
		for ( surf = vLight->globalInteractions; surf; surf = surf->nextOnLight ) {
			const_cast<drawSurf_t *>(surf)->material = material;
			newDrawSurfs[i++] = const_cast<drawSurf_t *>(surf);
		}
		vLight->localInteractions = NULL;
		vLight->globalInteractions = NULL;
	}

	switch( r_showOverDraw.GetInteger() ) {
		case 1: // geometry overdraw
			const_cast<viewDef_t *>(m_viewDef)->drawSurfs = newDrawSurfs;
			const_cast<viewDef_t *>(m_viewDef)->numDrawSurfs = numDrawSurfs;
			break;
		case 2: // light interaction overdraw
			const_cast<viewDef_t *>(m_viewDef)->drawSurfs = &newDrawSurfs[numDrawSurfs];
			const_cast<viewDef_t *>(m_viewDef)->numDrawSurfs = interactions;
			break;
		case 3: // geometry + light interaction overdraw
			const_cast<viewDef_t *>(m_viewDef)->drawSurfs = newDrawSurfs;
			const_cast<viewDef_t *>(m_viewDef)->numDrawSurfs += interactions;
			break;
	}
}

/*
===================
idRenderBackend::DBG_ShowIntensity

Debugging tool to see how much dynamic range a scene is using.
The greatest of the rgb values at each pixel will be used, with
the resulting color shading from red at 0 to green at 128 to blue at 255
===================
*/
void idRenderBackend::DBG_ShowIntensity() {
	byte	*colorReadback;
	int		i, j, c;

	if ( !r_showIntensity.GetBool() ) {
		return;
	}

	colorReadback = (byte *)R_StaticAlloc( renderSystem->GetWidth() * renderSystem->GetHeight() * 4, TAG_RENDER_TOOLS );
	qglReadPixels( 0, 0, renderSystem->GetWidth(), renderSystem->GetHeight(), GL_RGBA, GL_UNSIGNED_BYTE, colorReadback );

	c = renderSystem->GetWidth() * renderSystem->GetHeight() * 4;
	for ( i = 0; i < c; i+=4 ) {
		j = colorReadback[i];
		if ( colorReadback[i+1] > j ) {
			j = colorReadback[i+1];
		}
		if ( colorReadback[i+2] > j ) {
			j = colorReadback[i+2];
		}
		if ( j < 128 ) {
			colorReadback[i+0] = 2*(128-j);
			colorReadback[i+1] = 2*j;
			colorReadback[i+2] = 0;
		} else {
			colorReadback[i+0] = 0;
			colorReadback[i+1] = 2*(255-j);
			colorReadback[i+2] = 2*(j-128);
		}
	}

	// draw it back to the screen
	qglLoadIdentity();
	qglMatrixMode( GL_PROJECTION );
	GL_State( GLS_DEPTHFUNC_ALWAYS );
	qglPushMatrix();
	qglLoadIdentity(); 
    qglOrtho( 0, 1, 0, 1, -1, 1 );
	qglRasterPos2f( 0, 0 );
	qglPopMatrix();
	GL_Color( 1, 1, 1 );
	qglMatrixMode( GL_MODELVIEW );

	qglDrawPixels( renderSystem->GetWidth(), renderSystem->GetHeight(), GL_RGBA , GL_UNSIGNED_BYTE, colorReadback );

	R_StaticFree( colorReadback );
}


/*
===================
idRenderBackend::DBG_ShowDepthBuffer

Draw the depth buffer as colors
===================
*/
void idRenderBackend::DBG_ShowDepthBuffer() {
	void	*depthReadback;

	if ( !r_showDepth.GetBool() ) {
		return;
	}

	qglPushMatrix();
	qglLoadIdentity();
	qglMatrixMode( GL_PROJECTION );
	qglPushMatrix();
	qglLoadIdentity(); 
    qglOrtho( 0, 1, 0, 1, -1, 1 );
	qglRasterPos2f( 0, 0 );
	qglPopMatrix();
	qglMatrixMode( GL_MODELVIEW );
	qglPopMatrix();

	GL_State( GLS_DEPTHFUNC_ALWAYS );
	GL_Color( 1, 1, 1 );

	depthReadback = R_StaticAlloc( renderSystem->GetWidth() * renderSystem->GetHeight()*4, TAG_RENDER_TOOLS );
	memset( depthReadback, 0, renderSystem->GetWidth() * renderSystem->GetHeight()*4 );

	qglReadPixels( 0, 0, renderSystem->GetWidth(), renderSystem->GetHeight(), GL_DEPTH_COMPONENT , GL_FLOAT, depthReadback );

#if 0
	for ( i = 0; i < renderSystem->GetWidth() * renderSystem->GetHeight(); i++ ) {
		((byte *)depthReadback)[i*4] = 
		((byte *)depthReadback)[i*4+1] = 
		((byte *)depthReadback)[i*4+2] = 255 * ((float *)depthReadback)[i];
		((byte *)depthReadback)[i*4+3] = 1;
	}
#endif

	qglDrawPixels( renderSystem->GetWidth(), renderSystem->GetHeight(), GL_RGBA , GL_UNSIGNED_BYTE, depthReadback );
	R_StaticFree( depthReadback );
}

/*
=================
idRenderBackend::DBG_ShowLightCount

This is a debugging tool that will draw each surface with a color
based on how many lights are effecting it
=================
*/
void idRenderBackend::DBG_ShowLightCount() {
	int		i;
	const drawSurf_t	*surf;
	const viewLight_t	*vLight;

	if ( !r_showLightCount.GetBool() ) {
		return;
	}

	DBG_SimpleWorldSetup();

	GL_Clear( false, false, true, 0, 0.0f, 0.0f, 0.0f, 0.0f );

	// optionally count everything through walls
	if ( r_showLightCount.GetInteger() >= 2 ) {
		GL_State( GLS_DEPTHFUNC_EQUAL | GLS_STENCIL_OP_FAIL_KEEP | GLS_STENCIL_OP_ZFAIL_INCR | GLS_STENCIL_OP_PASS_INCR );
	} else {
		GL_State( GLS_DEPTHFUNC_EQUAL | GLS_STENCIL_OP_FAIL_KEEP | GLS_STENCIL_OP_ZFAIL_KEEP | GLS_STENCIL_OP_PASS_INCR );
	}

	GL_BindTexture( globalImages->m_defaultImage );

	for ( vLight = m_viewDef->viewLights; vLight; vLight = vLight->next ) {
		for ( i = 0; i < 2; i++ ) {
			for ( surf = i ? vLight->localInteractions: vLight->globalInteractions; surf; surf = (drawSurf_t *)surf->nextOnLight ) {
				DBG_SimpleSurfaceSetup( surf );
				DrawElementsWithCounters( surf );
			}
		}
	}

	// display the results
	DBG_ColorByStencilBuffer();

	if ( r_showLightCount.GetInteger() > 2 ) {
		RB_CountStencilBuffer();
	}
}

/*
===============
idRenderBackend::RB_EnterWeaponDepthHack
===============
*/
void idRenderBackend::DBG_EnterWeaponDepthHack() {
	float	matrix[16];

	memcpy( matrix, m_viewDef->projectionMatrix, sizeof( matrix ) );

	const float modelDepthHack = 0.25f;
	matrix[2] *= modelDepthHack;
	matrix[6] *= modelDepthHack;
	matrix[10] *= modelDepthHack;
	matrix[14] *= modelDepthHack;

	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( matrix );
	qglMatrixMode( GL_MODELVIEW );
}

/*
===============
idRenderBackend::DBG_EnterModelDepthHacks
===============
*/
void idRenderBackend::DBG_EnterModelDepthHack( float depth ) {
	float matrix[16];

	memcpy( matrix, m_viewDef->projectionMatrix, sizeof( matrix ) );

	matrix[14] -= depth;

	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( matrix );
	qglMatrixMode( GL_MODELVIEW );
}

/*
===============
idRenderBackend::DBG_LeaveDepthHack
===============
*/
void idRenderBackend::DBG_LeaveDepthHack() {
	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( m_viewDef->projectionMatrix );
	qglMatrixMode( GL_MODELVIEW );
}

/*
====================
idRenderBackend::RB_RenderDrawSurfListWithFunction

The triangle functions can check backEnd.currentSpace != surf->space
to see if they need to perform any new matrix setup.  The modelview
matrix will already have been loaded, and backEnd.currentSpace will
be updated after the triangle function completes.
====================
*/
void idRenderBackend::DBG_RenderDrawSurfListWithFunction( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	m_currentSpace = NULL;

	for ( int i = 0 ; i < numDrawSurfs ; i++ ) {
		const drawSurf_t * drawSurf = drawSurfs[i];
		if ( drawSurf == NULL ) {
			continue;
		}
		assert( drawSurf->space != NULL );
		if ( drawSurf->space != NULL ) {	// is it ever NULL?  Do we need to check?
			// Set these values ahead of time so we don't have to reconstruct the matrices on the consoles

			// change the matrix if needed
			if ( drawSurf->space != m_currentSpace ) {
				glLoadMatrixf( drawSurf->space->modelViewMatrix );
			}

			if ( drawSurf->space->weaponDepthHack ) {
				DBG_EnterWeaponDepthHack();
			}

			if ( drawSurf->space->modelDepthHack != 0.0f ) {
				DBG_EnterModelDepthHack( drawSurf->space->modelDepthHack );
			}
		}

		// change the scissor if needed
		if ( r_useScissor.GetBool() && !m_currentScissor.Equals( drawSurf->scissorRect ) ) {
			m_currentScissor = drawSurf->scissorRect;
			GL_Scissor( m_viewDef->viewport.x1 + m_currentScissor.x1, 
				m_viewDef->viewport.y1 + m_currentScissor.y1,
				m_currentScissor.x2 + 1 - m_currentScissor.x1,
				m_currentScissor.y2 + 1 - m_currentScissor.y1 );
		}

		// render it
		DrawElementsWithCounters( drawSurf );

		if ( drawSurf->space != NULL && ( drawSurf->space->weaponDepthHack || drawSurf->space->modelDepthHack != 0.0f ) ) {
			DBG_LeaveDepthHack();
		}

		m_currentSpace = drawSurf->space;
	}
}

/*
=================
idRenderBackend::DBG_ShowSilhouette

Blacks out all edges, then adds color for each edge that a shadow
plane extends from, allowing you to see doubled edges

FIXME: not thread safe!
=================
*/
void idRenderBackend::DBG_ShowSilhouette() {
	int		i;
	const drawSurf_t	*surf;
	const viewLight_t	*vLight;

	if ( !r_showSilhouette.GetBool() ) {
		return;
	}

	//
	// clear all triangle edges to black
	//
	qglDisable( GL_TEXTURE_2D );

	GL_Color( 0, 0, 0 );

	GL_State( GLS_DEPTHFUNC_ALWAYS | GLS_POLYMODE_LINE | GLS_CULL_TWOSIDED );

	DBG_RenderDrawSurfListWithFunction( m_viewDef->drawSurfs, m_viewDef->numDrawSurfs );

	//
	// now blend in edges that cast silhouettes
	//
	DBG_SimpleWorldSetup();
	GL_Color( 0.5, 0, 0 );
	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );

	for ( vLight = m_viewDef->viewLights; vLight; vLight = vLight->next ) {
		for ( i = 0; i < 2; i++ ) {
			for ( surf = i ? vLight->localShadows : vLight->globalShadows
				; surf; surf = (drawSurf_t *)surf->nextOnLight ) {
				DBG_SimpleSurfaceSetup( surf );

				const srfTriangles_t * tri = surf->frontEndGeo;

				idVertexBuffer vertexBuffer;
				if ( !vertexCache.GetVertexBuffer( tri->shadowCache, &vertexBuffer ) ) {
					continue;
				}

				qglBindBufferARB( GL_ARRAY_BUFFER_ARB, (GLuint)vertexBuffer.GetAPIObject() );
				int vertOffset = vertexBuffer.GetOffset();

				qglVertexPointer( 3, GL_FLOAT, sizeof( idShadowVert ), (void *)vertOffset );
				qglBegin( GL_LINES );

				for ( int j = 0; j < tri->numIndexes; j+=3 ) {
					int		i1 = tri->indexes[j+0];
					int		i2 = tri->indexes[j+1];
					int		i3 = tri->indexes[j+2];

					if ( (i1 & 1) + (i2 & 1) + (i3 & 1) == 1 ) {
						if ( (i1 & 1) + (i2 & 1) == 0 ) {
							qglArrayElement( i1 );
							qglArrayElement( i2 );
						} else if ( (i1 & 1 ) + (i3 & 1) == 0 ) {
							qglArrayElement( i1 );
							qglArrayElement( i3 );
						}
					}
				}
				qglEnd();

			}
		}
	}

	GL_State( GLS_DEFAULT | GLS_CULL_TWOSIDED );
	GL_Color( 1,1,1 );
}

/*
=====================
idRenderBackend::DBG_ShowTris

Debugging tool
=====================
*/
void idRenderBackend::DBG_ShowTris( drawSurf_t **drawSurfs, int numDrawSurfs ) {

	modelTrace_t mt;
	idVec3 end;

	if ( r_showTris.GetInteger() == 0 ) {
		return;
	}

	float color[4] = { 1, 1, 1, 1 };

	GL_PolygonOffset( -1.0f, -2.0f );
	
	switch ( r_showTris.GetInteger() ) {			
		case 1: // only draw visible ones
			GL_State( GLS_DEPTHMASK | GLS_ALPHAMASK | GLS_POLYMODE_LINE | GLS_POLYGON_OFFSET );
			break;
		case 2:	// draw all front facing
		case 3: // draw all
			GL_State( GLS_DEPTHMASK | GLS_ALPHAMASK | GLS_POLYMODE_LINE | GLS_POLYGON_OFFSET | GLS_DEPTHFUNC_ALWAYS );
			break;
		case 4: // only draw visible ones with blended lines
			GL_State( GLS_DEPTHMASK | GLS_ALPHAMASK | GLS_POLYMODE_LINE | GLS_POLYGON_OFFSET | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
			color[3] = 0.4f;
			break;
	}

	if ( r_showTris.GetInteger() == 3 ) {
		GL_State( m_glStateBits & ~( GLS_CULL_MASK ) | GLS_CULL_TWOSIDED );
	}

	GL_Color( color );
	renderProgManager.BindProgram( BUILTIN_COLOR );

	DBG_RenderDrawSurfListWithFunction( drawSurfs, numDrawSurfs );

	if ( r_showTris.GetInteger() == 3 ) {
		GL_State( m_glStateBits & ~( GLS_CULL_MASK ) | GLS_CULL_FRONTSIDED );
	}
}

/*
=====================
idRenderBackend::DBG_ShowSurfaceInfo

Debugging tool
=====================
*/
void idRenderBackend::DBG_ShowSurfaceInfo( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	modelTrace_t mt;
	idVec3 start, end;
	
	if ( !r_showSurfaceInfo.GetBool() ) {
		return;
	}

	// start far enough away that we don't hit the player model
	start = tr.primaryView->renderView.vieworg + tr.primaryView->renderView.viewaxis[0] * 16;
	end = start + tr.primaryView->renderView.viewaxis[0] * 1000.0f;
	if ( !tr.primaryWorld->Trace( mt, start, end, 0.0f, false ) ) {
		return;
	}

	qglDisable( GL_TEXTURE_2D );

	GL_Color( 1, 1, 1 );

	static float scale = -1;
	static float bias = -2;

	GL_PolygonOffset( scale, bias );
	GL_State( GLS_DEPTHFUNC_ALWAYS | GLS_POLYMODE_LINE | GLS_POLYGON_OFFSET );

	idVec3	trans[3];
	float	matrix[16];

	// transform the object verts into global space
	R_AxisToModelMatrix( mt.entity->axis, mt.entity->origin, matrix );

	tr.primaryWorld->DrawText( mt.entity->hModel->Name(), mt.point + tr.primaryView->renderView.viewaxis[2] * 12,
		0.35f, colorRed, tr.primaryView->renderView.viewaxis );
	tr.primaryWorld->DrawText( mt.material->GetName(), mt.point, 
		0.35f, colorBlue, tr.primaryView->renderView.viewaxis );
}

/*
=====================
idRenderBackend::DBG_ShowViewEntitys

Debugging tool
=====================
*/
idRenderModel *R_EntityDefDynamicModel( idRenderEntity *def );
void idRenderBackend::DBG_ShowViewEntitys( viewEntity_t *vModels ) {
	if ( !r_showViewEntitys.GetBool() ) {
		return;
	}
	if ( r_showViewEntitys.GetInteger() >= 2 ) {
		idLib::Printf( "view entities: " );
		for ( const viewEntity_t * vModel = vModels; vModel; vModel = vModel->next ) {
			if ( vModel->entityDef->IsDirectlyVisible() ) {
				idLib::Printf( "<%i> ", vModel->entityDef->index );
			} else {
				idLib::Printf( "%i ", vModel->entityDef->index );
			}
		}
		idLib::Printf( "\n" );
	}

	renderProgManager.BindProgram( BUILTIN_COLOR );

	GL_Color( 1, 1, 1 );
	GL_State( GLS_DEPTHFUNC_ALWAYS | GLS_POLYMODE_LINE | GLS_CULL_TWOSIDED );

	for ( const viewEntity_t * vModel = vModels; vModel; vModel = vModel->next ) {
		idBounds	b;

		qglLoadMatrixf( vModel->modelViewMatrix );

		const idRenderEntity * edef = vModel->entityDef;
		if ( !edef ) {
			continue;
		}



		// draw the model bounds in white if directly visible,
		// or, blue if it is only-for-sahdow
		idVec4	color;
		if ( edef->IsDirectlyVisible() ) {
			color.Set( 1, 1, 1, 1 );
		} else {
			color.Set( 0, 0, 1, 1 );
		}
		GL_Color( color[0], color[1], color[2] );
		RB_DrawBounds( edef->localReferenceBounds );

		// transform the upper bounds corner into global space
		if ( r_showViewEntitys.GetInteger() >= 2 ) {
			idVec3 corner;
			R_LocalPointToGlobal( vModel->modelMatrix, edef->localReferenceBounds[1], corner );

			tr.primaryWorld->DrawText( 
				va( "%i:%s", edef->index, edef->parms.hModel->Name() ), 
				corner,
				0.25f, color, 
				tr.primaryView->renderView.viewaxis );
		}

		// draw the actual bounds in yellow if different
		if ( r_showViewEntitys.GetInteger() >= 3 ) {
			GL_Color( 1, 1, 0 );
			// FIXME: cannot instantiate a dynamic model from the renderer back-end
			idRenderModel *model = R_EntityDefDynamicModel( vModel->entityDef );
			if ( !model ) {
				continue;	// particles won't instantiate without a current view
			}
			b = model->Bounds( &vModel->entityDef->parms );
			if ( b != vModel->entityDef->localReferenceBounds ) {
				RB_DrawBounds( b );
			}
		}
	}
}

/*
=====================
idRenderBackend::DBG_ShowTexturePolarity

Shade triangle red if they have a positive texture area
green if they have a negative texture area, or blue if degenerate area
=====================
*/
void idRenderBackend::DBG_ShowTexturePolarity( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	int		i, j;
	drawSurf_t	*drawSurf;
	const srfTriangles_t	*tri;

	if ( !r_showTexturePolarity.GetBool() ) {
		return;
	}

	GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	GL_Color( 1, 1, 1 );

	for ( i = 0; i < numDrawSurfs; i++ ) {
		drawSurf = drawSurfs[i];
		tri = drawSurf->frontEndGeo;
		if ( !tri->verts ) {
			continue;
		}

		DBG_SimpleSurfaceSetup( drawSurf );

		qglBegin( GL_TRIANGLES );
		for ( j = 0; j < tri->numIndexes; j+=3 ) {
			idDrawVert	*a, *b, *c;
			float		d0[5], d1[5];
			float		area;

			a = tri->verts + tri->indexes[j];
			b = tri->verts + tri->indexes[j+1];
			c = tri->verts + tri->indexes[j+2];

			const idVec2 aST = a->GetTexCoord();
			const idVec2 bST = b->GetTexCoord();
			const idVec2 cST = c->GetTexCoord();

			d0[3] = bST[0] - aST[0];
			d0[4] = bST[1] - aST[1];

			d1[3] = cST[0] - aST[0];
			d1[4] = cST[1] - aST[1];

			area = d0[3] * d1[4] - d0[4] * d1[3];

			if ( idMath::Fabs( area ) < 0.0001 ) {
				GL_Color( 0, 0, 1, 0.5 );
			} else  if ( area < 0 ) {
				GL_Color( 1, 0, 0, 0.5 );
			} else {
				GL_Color( 0, 1, 0, 0.5 );
			}
			qglVertex3fv( a->xyz.ToFloatPtr() );
			qglVertex3fv( b->xyz.ToFloatPtr() );
			qglVertex3fv( c->xyz.ToFloatPtr() );
		}
		qglEnd();
	}

	GL_State( GLS_DEFAULT );
}

/*
=====================
idRenderBackend::DBG_ShowUnsmoothedTangents

Shade materials that are using unsmoothed tangents
=====================
*/
void idRenderBackend::DBG_ShowUnsmoothedTangents( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	int		i, j;
	drawSurf_t	*drawSurf;
	const srfTriangles_t	*tri;

	if ( !r_showUnsmoothedTangents.GetBool() ) {
		return;
	}

	GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	GL_Color( 0, 1, 0, 0.5 );

	for ( i = 0; i < numDrawSurfs; i++ ) {
		drawSurf = drawSurfs[i];

		if ( !drawSurf->material->UseUnsmoothedTangents() ) {
			continue;
		}

		DBG_SimpleSurfaceSetup( drawSurf );

		tri = drawSurf->frontEndGeo;
		qglBegin( GL_TRIANGLES );
		for ( j = 0; j < tri->numIndexes; j+=3 ) {
			idDrawVert	*a, *b, *c;

			a = tri->verts + tri->indexes[j];
			b = tri->verts + tri->indexes[j+1];
			c = tri->verts + tri->indexes[j+2];

			qglVertex3fv( a->xyz.ToFloatPtr() );
			qglVertex3fv( b->xyz.ToFloatPtr() );
			qglVertex3fv( c->xyz.ToFloatPtr() );
		}
		qglEnd();
	}

	GL_State( GLS_DEFAULT );
}

/*
=====================
idRenderBackend::DBG_ShowTangentSpace

Shade a triangle by the RGB colors of its tangent space
1 = tangents[0]
2 = tangents[1]
3 = normal
=====================
*/
void idRenderBackend::DBG_ShowTangentSpace( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	int		i, j;
	drawSurf_t	*drawSurf;
	const srfTriangles_t	*tri;

	if ( !r_showTangentSpace.GetInteger() ) {
		return;
	}

	GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	for ( i = 0; i < numDrawSurfs; i++ ) {
		drawSurf = drawSurfs[i];

		DBG_SimpleSurfaceSetup( drawSurf );

		tri = drawSurf->frontEndGeo;
		if ( !tri->verts ) {
			continue;
		}
		qglBegin( GL_TRIANGLES );
		for ( j = 0; j < tri->numIndexes; j++ ) {
			const idDrawVert *v;

			v = &tri->verts[tri->indexes[j]];

			if ( r_showTangentSpace.GetInteger() == 1 ) {
				const idVec3 vertexTangent = v->GetTangent();
				GL_Color( 0.5 + 0.5 * vertexTangent[0],  0.5 + 0.5 * vertexTangent[1],  
					0.5 + 0.5 * vertexTangent[2], 0.5 );
			} else if ( r_showTangentSpace.GetInteger() == 2 ) {
				const idVec3 vertexBiTangent = v->GetBiTangent();
				GL_Color( 0.5 + 0.5 *vertexBiTangent[0],  0.5 + 0.5 * vertexBiTangent[1],  
					0.5 + 0.5 * vertexBiTangent[2], 0.5 );
			} else {
				const idVec3 vertexNormal = v->GetNormal();
				GL_Color( 0.5 + 0.5 * vertexNormal[0],  0.5 + 0.5 * vertexNormal[1],  
					0.5 + 0.5 * vertexNormal[2], 0.5 );
			}
			qglVertex3fv( v->xyz.ToFloatPtr() );
		}
		qglEnd();
	}

	GL_State( GLS_DEFAULT );
}

/*
=====================
idRenderBackend::DBG_ShowVertexColor

Draw each triangle with the solid vertex colors
=====================
*/
void idRenderBackend::DBG_ShowVertexColor( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	int		i, j;
	drawSurf_t	*drawSurf;
	const srfTriangles_t	*tri;

	if ( !r_showVertexColor.GetBool() ) {
		return;
	}

	GL_State( GLS_DEPTHFUNC_LESS );

	for ( i = 0; i < numDrawSurfs; i++ ) {
		drawSurf = drawSurfs[i];

		DBG_SimpleSurfaceSetup( drawSurf );

		tri = drawSurf->frontEndGeo;
		if ( !tri->verts ) {
			continue;
		}
		qglBegin( GL_TRIANGLES );
		for ( j = 0; j < tri->numIndexes; j++ ) {
			const idDrawVert *v;

			v = &tri->verts[tri->indexes[j]];
			qglColor4ubv( v->color );
			qglVertex3fv( v->xyz.ToFloatPtr() );
		}
		qglEnd();
	}

	GL_State( GLS_DEFAULT );
}

/*
=====================
idRenderBackend::DBG_ShowNormals

Debugging tool
=====================
*/
void idRenderBackend::DBG_ShowNormals( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	int			i, j;
	drawSurf_t	*drawSurf;
	idVec3		end;
	const srfTriangles_t	*tri;
	float		size;
	bool		showNumbers;
	idVec3		pos;

	if ( r_showNormals.GetFloat() == 0.0f ) {
		return;
	}

	if ( !r_debugLineDepthTest.GetBool() ) {
		GL_State( GLS_POLYMODE_LINE | GLS_DEPTHFUNC_ALWAYS );
	} else {
		GL_State( GLS_POLYMODE_LINE );
	}

	size = r_showNormals.GetFloat();
	if ( size < 0.0f ) {
		size = -size;
		showNumbers = true;
	} else {
		showNumbers = false;
	}

	for ( i = 0; i < numDrawSurfs; i++ ) {
		drawSurf = drawSurfs[i];

		DBG_SimpleSurfaceSetup( drawSurf );

		tri = drawSurf->frontEndGeo;
		if ( !tri->verts ) {
			continue;
		}

		qglBegin( GL_LINES );
		for ( j = 0; j < tri->numVerts; j++ ) {
			const idVec3 normal = tri->verts[j].GetNormal();
			const idVec3 tangent = tri->verts[j].GetTangent();
			const idVec3 bitangent = tri->verts[j].GetBiTangent();
			GL_Color( 0, 0, 1 );
			qglVertex3fv( tri->verts[j].xyz.ToFloatPtr() );
			VectorMA( tri->verts[j].xyz, size, normal, end );
			qglVertex3fv( end.ToFloatPtr() );

			GL_Color( 1, 0, 0 );
			qglVertex3fv( tri->verts[j].xyz.ToFloatPtr() );
			VectorMA( tri->verts[j].xyz, size, tangent, end );
			qglVertex3fv( end.ToFloatPtr() );

			GL_Color( 0, 1, 0 );
			qglVertex3fv( tri->verts[j].xyz.ToFloatPtr() );
			VectorMA( tri->verts[j].xyz, size, bitangent, end );
			qglVertex3fv( end.ToFloatPtr() );
		}
		qglEnd();
	}

	if ( showNumbers ) {
		DBG_SimpleWorldSetup();
		for ( i = 0; i < numDrawSurfs; i++ ) {
			drawSurf = drawSurfs[i];
			tri = drawSurf->frontEndGeo;
			if ( !tri->verts ) {
				continue;
			}
			
			for ( j = 0; j < tri->numVerts; j++ ) {
				const idVec3 normal = tri->verts[j].GetNormal();
				const idVec3 tangent = tri->verts[j].GetTangent();
				R_LocalPointToGlobal( drawSurf->space->modelMatrix, tri->verts[j].xyz + tangent + normal * 0.2f, pos );
				RB_DrawText( va( "%d", j ), pos, 0.01f, colorWhite, m_viewDef->renderView.viewaxis, 1 );
			}

			for ( j = 0; j < tri->numIndexes; j += 3 ) {
				const idVec3 normal = tri->verts[ tri->indexes[ j + 0 ] ].GetNormal();
				R_LocalPointToGlobal( 
					drawSurf->space->modelMatrix, 
					( tri->verts[ tri->indexes[ j + 0 ] ].xyz 
					+ tri->verts[ tri->indexes[ j + 1 ] ].xyz 
					+ tri->verts[ tri->indexes[ j + 2 ] ].xyz ) * ( 1.0f / 3.0f ) + normal * 0.2f, pos );
				RB_DrawText( va( "%d", j / 3 ), pos, 0.01f, colorCyan, m_viewDef->renderView.viewaxis, 1 );
			}
		}
	}
}

/*
=====================
idRenderBackend::DBG_ShowTextureVectors

Draw texture vectors in the center of each triangle
=====================
*/
void idRenderBackend::DBG_ShowTextureVectors( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	if ( r_showTextureVectors.GetFloat() == 0.0f ) {
		return;
	}

	GL_State( GLS_DEPTHFUNC_LESS );

	for ( int i = 0; i < numDrawSurfs; i++ ) {
		drawSurf_t * drawSurf = drawSurfs[i];

		const srfTriangles_t * tri = drawSurf->frontEndGeo;

		if ( tri->verts == NULL ) {
			continue;
		}

		DBG_SimpleSurfaceSetup( drawSurf );

		// draw non-shared edges in yellow
		qglBegin( GL_LINES );

		for ( int j = 0; j < tri->numIndexes; j+= 3 ) {
			float d0[5], d1[5];
			idVec3 temp;
			idVec3 tangents[2];

			const idDrawVert *a = &tri->verts[tri->indexes[j+0]];
			const idDrawVert *b = &tri->verts[tri->indexes[j+1]];
			const idDrawVert *c = &tri->verts[tri->indexes[j+2]];

			const idPlane plane( a->xyz, b->xyz, c->xyz );

			// make the midpoint slightly above the triangle
			const idVec3 mid = ( a->xyz + b->xyz + c->xyz ) * ( 1.0f / 3.0f ) + 0.1f * plane.Normal();

			// calculate the texture vectors
			const idVec2 aST = a->GetTexCoord();
			const idVec2 bST = b->GetTexCoord();
			const idVec2 cST = c->GetTexCoord();

			d0[0] = b->xyz[0] - a->xyz[0];
			d0[1] = b->xyz[1] - a->xyz[1];
			d0[2] = b->xyz[2] - a->xyz[2];
			d0[3] = bST[0] - aST[0];
			d0[4] = bST[1] - aST[1];

			d1[0] = c->xyz[0] - a->xyz[0];
			d1[1] = c->xyz[1] - a->xyz[1];
			d1[2] = c->xyz[2] - a->xyz[2];
			d1[3] = cST[0] - aST[0];
			d1[4] = cST[1] - aST[1];

			const float area = d0[3] * d1[4] - d0[4] * d1[3];
			if ( area == 0 ) {
				continue;
			}
			const float inva = 1.0f / area;

			temp[0] = (d0[0] * d1[4] - d0[4] * d1[0]) * inva;
			temp[1] = (d0[1] * d1[4] - d0[4] * d1[1]) * inva;
			temp[2] = (d0[2] * d1[4] - d0[4] * d1[2]) * inva;
			temp.Normalize();
			tangents[0] = temp;
        
			temp[0] = (d0[3] * d1[0] - d0[0] * d1[3]) * inva;
			temp[1] = (d0[3] * d1[1] - d0[1] * d1[3]) * inva;
			temp[2] = (d0[3] * d1[2] - d0[2] * d1[3]) * inva;
			temp.Normalize();
			tangents[1] = temp;

			// draw the tangents
			tangents[0] = mid + tangents[0] * r_showTextureVectors.GetFloat();
			tangents[1] = mid + tangents[1] * r_showTextureVectors.GetFloat();

			GL_Color( 1, 0, 0 );
			qglVertex3fv( mid.ToFloatPtr() );
			qglVertex3fv( tangents[0].ToFloatPtr() );

			GL_Color( 0, 1, 0 );
			qglVertex3fv( mid.ToFloatPtr() );
			qglVertex3fv( tangents[1].ToFloatPtr() );
		}

		qglEnd();
	}
}

/*
=====================
idRenderBackend::DBG_ShowDominantTris

Draw lines from each vertex to the dominant triangle center
=====================
*/
void idRenderBackend::DBG_ShowDominantTris( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	int			i, j;
	drawSurf_t	*drawSurf;
	const srfTriangles_t	*tri;

	if ( !r_showDominantTri.GetBool() ) {
		return;
	}

	GL_State( GLS_DEPTHFUNC_LESS );

	GL_PolygonOffset( -1, -2 );
	qglEnable( GL_POLYGON_OFFSET_LINE );

	for ( i = 0; i < numDrawSurfs; i++ ) {
		drawSurf = drawSurfs[i];

		tri = drawSurf->frontEndGeo;

		if ( !tri->verts ) {
			continue;
		}
		if ( !tri->dominantTris ) {
			continue;
		}
		DBG_SimpleSurfaceSetup( drawSurf );

		GL_Color( 1, 1, 0 );
		qglBegin( GL_LINES );

		for ( j = 0; j < tri->numVerts; j++ ) {
			const idDrawVert *a, *b, *c;
			idVec3		mid;

			// find the midpoint of the dominant tri

			a = &tri->verts[j];
			b = &tri->verts[tri->dominantTris[j].v2];
			c = &tri->verts[tri->dominantTris[j].v3];

			mid = ( a->xyz + b->xyz + c->xyz ) * ( 1.0f / 3.0f );

			qglVertex3fv( mid.ToFloatPtr() );
			qglVertex3fv( a->xyz.ToFloatPtr() );
		}

		qglEnd();
	}
	qglDisable( GL_POLYGON_OFFSET_LINE );
}

/*
=====================
idRenderBackend::DBG_ShowEdges

Debugging tool
=====================
*/
void idRenderBackend::DBG_ShowEdges( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	int			i, j, k, m, n, o;
	drawSurf_t	*drawSurf;
	const srfTriangles_t	*tri;
	const silEdge_t			*edge;
	int			danglePlane;

	if ( !r_showEdges.GetBool() ) {
		return;
	}

	GL_State( GLS_DEPTHFUNC_ALWAYS );

	for ( i = 0; i < numDrawSurfs; i++ ) {
		drawSurf = drawSurfs[i];

		tri = drawSurf->frontEndGeo;

		idDrawVert *ac = (idDrawVert *)tri->verts;
		if ( !ac ) {
			continue;
		}

		DBG_SimpleSurfaceSetup( drawSurf );

		// draw non-shared edges in yellow
		GL_Color( 1, 1, 0 );
		qglBegin( GL_LINES );

		for ( j = 0; j < tri->numIndexes; j+= 3 ) {
			for ( k = 0; k < 3; k++ ) {
				int		l, i1, i2;
				l = ( k == 2 ) ? 0 : k + 1;
				i1 = tri->indexes[j+k];
				i2 = tri->indexes[j+l];

				// if these are used backwards, the edge is shared
				for ( m = 0; m < tri->numIndexes; m += 3 ) {
					for ( n = 0; n < 3; n++ ) {
						o = ( n == 2 ) ? 0 : n + 1;
						if ( tri->indexes[m+n] == i2 && tri->indexes[m+o] == i1 ) {
							break;
						}
					}
					if ( n != 3 ) {
						break;
					}
				}

				// if we didn't find a backwards listing, draw it in yellow
				if ( m == tri->numIndexes ) {
					qglVertex3fv( ac[ i1 ].xyz.ToFloatPtr() );
					qglVertex3fv( ac[ i2 ].xyz.ToFloatPtr() );
				}

			}
		}

		qglEnd();

		// draw dangling sil edges in red
		if ( !tri->silEdges ) {
			continue;
		}

		// the plane number after all real planes
		// is the dangling edge
		danglePlane = tri->numIndexes / 3;

		GL_Color( 1, 0, 0 );

		qglBegin( GL_LINES );
		for ( j = 0; j < tri->numSilEdges; j++ ) {
			edge = tri->silEdges + j;

			if ( edge->p1 != danglePlane && edge->p2 != danglePlane ) {
				continue;
			}

			qglVertex3fv( ac[ edge->v1 ].xyz.ToFloatPtr() );
			qglVertex3fv( ac[ edge->v2 ].xyz.ToFloatPtr() );
		}
		qglEnd();
	}
}

/*
==============
idRenderBackend::DBG_ShowLights

Visualize all light volumes used in the current scene
r_showLights 1	: just print volumes numbers, highlighting ones covering the view
r_showLights 2	: also draw planes of each volume
r_showLights 3	: also draw edges of each volume
==============
*/
void idRenderBackend::DBG_ShowLights() {
	if ( !r_showLights.GetInteger() ) {
		return;
	}

	GL_State( GLS_DEFAULT );

	// we use the 'vLight->invProjectMVPMatrix'
	qglMatrixMode( GL_PROJECTION );
	qglLoadIdentity(); 

	renderProgManager.BindProgram( BUILTIN_COLOR );

	idLib::Printf( "volumes: " );	// FIXME: not in back end!

	int count = 0;
	for ( viewLight_t * vLight = m_viewDef->viewLights; vLight != NULL; vLight = vLight->next ) {
		count++;

		// depth buffered planes
		if ( r_showLights.GetInteger() >= 2 ) {
			GL_State( GLS_DEPTHFUNC_ALWAYS | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHMASK | GLS_CULL_TWOSIDED );
			GL_Color( 0.0f, 0.0f, 1.0f, 0.25f );
			idRenderMatrix invProjectMVPMatrix;
			idRenderMatrix::Multiply( m_viewDef->worldSpace.mvp, vLight->inverseBaseLightProject, invProjectMVPMatrix );
			RB_SetMVP( invProjectMVPMatrix );
			DrawElementsWithCounters( &m_zeroOneCubeSurface );
		}

		// non-hidden lines
		if ( r_showLights.GetInteger() >= 3 ) {
			GL_State( GLS_DEPTHFUNC_ALWAYS | GLS_POLYMODE_LINE | GLS_DEPTHMASK  );
			GL_Color( 1.0f, 1.0f, 1.0f );
			idRenderMatrix invProjectMVPMatrix;
			idRenderMatrix::Multiply( m_viewDef->worldSpace.mvp, vLight->inverseBaseLightProject, invProjectMVPMatrix );
			RB_SetMVP( invProjectMVPMatrix );
			DrawElementsWithCounters( &m_zeroOneCubeSurface );
		}

		idLib::Printf( "%i ", vLight->lightDef->index );
	}

	idLib::Printf( " = %i total\n", count );

	// set back the default projection matrix
	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( m_viewDef->projectionMatrix );
	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();
}

/*
=====================
idRenderBackend::DBG_ShowPortals

Debugging tool, won't work correctly with SMP or when mirrors are present
=====================
*/
void idRenderBackend::DBG_ShowPortals() {
	if ( !r_showPortals.GetBool() ) {
		return;
	}

	// all portals are expressed in world coordinates
	DBG_SimpleWorldSetup();

	renderProgManager.BindProgram( BUILTIN_COLOR );
	GL_State( GLS_DEPTHFUNC_ALWAYS );

	idRenderWorld & world = *m_viewDef->renderWorld;

	// flood out through portals, setting area viewCount
	for ( int i = 0; i < world.m_numPortalAreas; i++ ) {
		portalArea_t * area = &world.m_portalAreas[ i ];
		if ( area->viewCount != tr.viewCount ) {
			continue;
		}
		for ( portal_t * p = area->portals; p; p = p->next ) {
			idWinding * w = p->w;
			if ( !w ) {
				continue;
			}

			if ( world.m_portalAreas[ p->intoArea ].viewCount != tr.viewCount ) {
				// red = can't see
				GL_Color( 1, 0, 0 );
			} else {
				// green = see through
				GL_Color( 0, 1, 0 );
			}

			qglBegin( GL_LINE_LOOP );
			for ( int j = 0; j < w->GetNumPoints(); j++ ) {
				qglVertex3fv( (*w)[j].ToFloatPtr() );
			}
			qglEnd();
		}
	}
}

/*
================
RB_DrawTextLength

  returns the length of the given text
================
*/
float RB_DrawTextLength( const char *text, float scale, int len ) {
	int i, num, index, charIndex;
	float spacing, textLen = 0.0f;

	if ( text && *text ) {
		if ( !len ) {
			len = strlen(text);
		}
		for ( i = 0; i < len; i++ ) {
			charIndex = text[i] - 32;
			if ( charIndex < 0 || charIndex > NUM_SIMPLEX_CHARS ) {
				continue;
			}
			num = simplex[charIndex][0] * 2;
			spacing = simplex[charIndex][1];
			index = 2;

			while( index - 2 < num ) {   
				if ( simplex[charIndex][index] < 0) {  
					index++;
					continue; 
				} 
				index += 2;
				if ( simplex[charIndex][index] < 0) {  
					index++;
					continue; 
				} 
			}   
			textLen += spacing * scale;  
		}
	}
	return textLen;
}

/*
================
RB_DrawText

  oriented on the viewaxis
  align can be 0-left, 1-center (default), 2-right
================
*/
static void RB_DrawText( const char *text, const idVec3 &origin, float scale, const idVec4 &color, const idMat3 &viewAxis, const int align ) {
	renderProgManager.BindProgram( BUILTIN_COLOR );

	int i, j, len, num, index, charIndex, line;
	float textLen = 1.0f, spacing = 1.0f;
	idVec3 org, p1, p2;

	if ( text && *text ) {
		qglBegin( GL_LINES );
		qglColor3fv( color.ToFloatPtr() );

		if ( text[0] == '\n' ) {
			line = 1;
		} else {
			line = 0;
		}

		len = strlen( text );
		for ( i = 0; i < len; i++ ) {

			if ( i == 0 || text[i] == '\n' ) {
				org = origin - viewAxis[2] * ( line * 36.0f * scale );
				if ( align != 0 ) {
					for ( j = 1; i+j <= len; j++ ) {
						if ( i+j == len || text[i+j] == '\n' ) {
							textLen = RB_DrawTextLength( text+i, scale, j );
							break;
						}
					}
					if ( align == 2 ) {
						// right
						org += viewAxis[1] * textLen;
					} else {
						// center
						org += viewAxis[1] * ( textLen * 0.5f );
					}
				}
				line++;
			}

			charIndex = text[i] - 32;
			if ( charIndex < 0 || charIndex > NUM_SIMPLEX_CHARS ) {
				continue;
			}
			num = simplex[charIndex][0] * 2;
			spacing = simplex[charIndex][1];
			index = 2;

			while( index - 2 < num ) {
				if ( simplex[charIndex][index] < 0) {  
					index++;
					continue; 
				}
				p1 = org + scale * simplex[charIndex][index] * -viewAxis[1] + scale * simplex[charIndex][index+1] * viewAxis[2];
				index += 2;
				if ( simplex[charIndex][index] < 0) {
					index++;
					continue;
				}
				p2 = org + scale * simplex[charIndex][index] * -viewAxis[1] + scale * simplex[charIndex][index+1] * viewAxis[2];

				qglVertex3fv( p1.ToFloatPtr() );
				qglVertex3fv( p2.ToFloatPtr() );
			}
			org -= viewAxis[1] * ( spacing * scale );
		}

		qglEnd();
	}
}

/*
================
idRenderBackend::DBG_ShowDebugText
================
*/
void idRenderBackend::DBG_ShowDebugText() {
	int			i;
	int			width;
	debugText_t	*text;

	if ( !rb_numDebugText ) {
		return;
	}

	// all lines are expressed in world coordinates
	DBG_SimpleWorldSetup();

	width = r_debugLineWidth.GetInteger();
	if ( width < 1 ) {
		width = 1;
	} else if ( width > 10 ) {
		width = 10;
	}

	// draw lines
	qglLineWidth( width );


	if ( !r_debugLineDepthTest.GetBool() ) {
		GL_State( GLS_POLYMODE_LINE | GLS_DEPTHFUNC_ALWAYS );
	} else {
		GL_State( GLS_POLYMODE_LINE );
	}

	text = rb_debugText;
	for ( i = 0; i < rb_numDebugText; i++, text++ ) {
		if ( !text->depthTest ) {
			RB_DrawText( text->text, text->origin, text->scale, text->color, text->viewAxis, text->align );
		}
	}

	if ( !r_debugLineDepthTest.GetBool() ) {
		GL_State( GLS_POLYMODE_LINE );
	}

	text = rb_debugText;
	for ( i = 0; i < rb_numDebugText; i++, text++ ) {
		if ( text->depthTest ) {
			RB_DrawText( text->text, text->origin, text->scale, text->color, text->viewAxis, text->align );
		}
	}

	qglLineWidth( 1 );
}

/*
================
idRenderBackend::DBG_ShowDebugLines
================
*/
void idRenderBackend::DBG_ShowDebugLines() {
	int			i;
	int			width;
	debugLine_t	*line;

	if ( !rb_numDebugLines ) {
		return;
	}

	// all lines are expressed in world coordinates
	DBG_SimpleWorldSetup();

	width = r_debugLineWidth.GetInteger();
	if ( width < 1 ) {
		width = 1;
	} else if ( width > 10 ) {
		width = 10;
	}

	// draw lines
	qglLineWidth( width );

	if ( !r_debugLineDepthTest.GetBool() ) {
		GL_State( GLS_POLYMODE_LINE | GLS_DEPTHFUNC_ALWAYS );
	} else {
		GL_State( GLS_POLYMODE_LINE );
	}

	qglBegin( GL_LINES );

	line = rb_debugLines;
	for ( i = 0; i < rb_numDebugLines; i++, line++ ) {
		if ( !line->depthTest ) {
			qglColor3fv( line->rgb.ToFloatPtr() );
			qglVertex3fv( line->start.ToFloatPtr() );
			qglVertex3fv( line->end.ToFloatPtr() );
		}
	}
	qglEnd();

	if ( !r_debugLineDepthTest.GetBool() ) {
		GL_State( GLS_POLYMODE_LINE );
	}

	qglBegin( GL_LINES );

	line = rb_debugLines;
	for ( i = 0; i < rb_numDebugLines; i++, line++ ) {
		if ( line->depthTest ) {
			qglColor4fv( line->rgb.ToFloatPtr() );
			qglVertex3fv( line->start.ToFloatPtr() );
			qglVertex3fv( line->end.ToFloatPtr() );
		}
	}

	qglEnd();

	qglLineWidth( 1 );
	GL_State( GLS_DEFAULT );
}

/*
================
idRenderBackend::DBG_ShowDebugPolygons
================
*/
void idRenderBackend::DBG_ShowDebugPolygons() {
	int				i, j;
	debugPolygon_t	*poly;

	if ( !rb_numDebugPolygons ) {
		return;
	}

	// all lines are expressed in world coordinates
	DBG_SimpleWorldSetup();

	qglDisable( GL_TEXTURE_2D );

	if ( r_debugPolygonFilled.GetBool() ) {
		GL_State( GLS_POLYGON_OFFSET | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHMASK );
		GL_PolygonOffset( -1, -2 );
	} else {
		GL_State( GLS_POLYGON_OFFSET | GLS_POLYMODE_LINE );
		GL_PolygonOffset( -1, -2 );
	}

	poly = rb_debugPolygons;
	for ( i = 0; i < rb_numDebugPolygons; i++, poly++ ) {
//		if ( !poly->depthTest ) {

			qglColor4fv( poly->rgb.ToFloatPtr() );

			qglBegin( GL_POLYGON );

			for ( j = 0; j < poly->winding.GetNumPoints(); j++) {
				qglVertex3fv( poly->winding[j].ToFloatPtr() );
			}

			qglEnd();
//		}
	}

	GL_State( GLS_DEFAULT );

	if ( r_debugPolygonFilled.GetBool() ) {
		qglDisable( GL_POLYGON_OFFSET_FILL );
	} else {
		qglDisable( GL_POLYGON_OFFSET_LINE );
	}

	GL_State( GLS_DEFAULT );
}

/*
================
idRenderBackend::DBG_ShowCenterOfProjection
================
*/
void idRenderBackend::DBG_ShowCenterOfProjection() {
	if ( !r_showCenterOfProjection.GetBool() ) {
		return;
	}

	const int w = m_viewDef->scissor.GetWidth();
	const int h = m_viewDef->scissor.GetHeight();
	qglClearColor( 1, 0, 0, 1 );
	for ( float f = 0.0f ; f <= 1.0f ; f += 0.125f ) {
		qglScissor( w * f - 1 , 0, 3, h );
		qglClear( GL_COLOR_BUFFER_BIT );
		qglScissor( 0, h * f - 1 , w, 3 );
		qglClear( GL_COLOR_BUFFER_BIT );
	}
	qglClearColor( 0, 1, 0, 1 );
	float f = 0.5f;
	qglScissor( w * f - 1 , 0, 3, h );
	qglClear( GL_COLOR_BUFFER_BIT );
	qglScissor( 0, h * f - 1 , w, 3 );
	qglClear( GL_COLOR_BUFFER_BIT );
	qglScissor( 0, 0, w, h );
}

/*
================
DBG_ShowLines

Draw exact pixel lines to check pixel center sampling
================
*/
void DBG_ShowLines() {
	if ( !r_showLines.GetBool() ) {
		return;
	}

	glEnable( GL_SCISSOR_TEST );
	glClearColor( 0, 0, 1, 1 );

	const int start = ( r_showLines.GetInteger() > 2 );	// 1,3 = horizontal, 2,4 = vertical
	if ( r_showLines.GetInteger() == 1 || r_showLines.GetInteger() == 3 ) {
		for ( int i = start ; i < tr.GetHeight() ; i+=2 ) {
			glScissor( 0, i, tr.GetWidth(), 1 );
			glClear( GL_COLOR_BUFFER_BIT );
		}
	} else {
		for ( int i = start ; i < tr.GetWidth() ; i+=2 ) {
			glScissor( i, 0, 1, tr.GetHeight() );
			glClear( GL_COLOR_BUFFER_BIT );
		}
	}
}


/*
================
idRenderBackend::DBG_TestGamma
================
*/
#define	G_WIDTH		512
#define	G_HEIGHT	512
#define	BAR_HEIGHT	64

void idRenderBackend::DBG_TestGamma() {
	byte	image[G_HEIGHT][G_WIDTH][4];
	int		i, j;
	int		c, comp;
	int		v, dither;
	int		mask, y;

	if ( r_testGamma.GetInteger() <= 0 ) {
		return;
	}

	v = r_testGamma.GetInteger();
	if ( v <= 1 || v >= 196 ) {
		v = 128;
	}

	memset( image, 0, sizeof( image ) );

	for ( mask = 0; mask < 8; mask++ ) {
		y = mask * BAR_HEIGHT;
		for ( c = 0; c < 4; c++ ) {
			v = c * 64 + 32;
			// solid color
			for ( i = 0; i < BAR_HEIGHT/2; i++ ) {
				for ( j = 0; j < G_WIDTH/4; j++ ) {
					for ( comp = 0; comp < 3; comp++ ) {
						if ( mask & ( 1 << comp ) ) {
							image[y+i][c*G_WIDTH/4+j][comp] = v;
						}
					}
				}
				// dithered color
				for ( j = 0; j < G_WIDTH/4; j++ ) {
					if ( ( i ^ j ) & 1 ) {
						dither = c * 64;
					} else {
						dither = c * 64 + 63;
					}
					for ( comp = 0; comp < 3; comp++ ) {
						if ( mask & ( 1 << comp ) ) {
							image[y+BAR_HEIGHT/2+i][c*G_WIDTH/4+j][comp] = dither;
						}
					}
				}
			}
		}
	}

	// draw geometrically increasing steps in the bottom row
	y = 0 * BAR_HEIGHT;
	float	scale = 1;
	for ( c = 0; c < 4; c++ ) {
		v = (int)(64 * scale);
		if ( v < 0 ) {
			v = 0;
		} else if ( v > 255 ) {
			v = 255;
		}
		scale = scale * 1.5;
		for ( i = 0; i < BAR_HEIGHT; i++ ) {
			for ( j = 0; j < G_WIDTH/4; j++ ) {
				image[y+i][c*G_WIDTH/4+j][0] = v;
				image[y+i][c*G_WIDTH/4+j][1] = v;
				image[y+i][c*G_WIDTH/4+j][2] = v;
			}
		}
	}

	qglLoadIdentity();

	qglMatrixMode( GL_PROJECTION );
	GL_State( GLS_DEPTHFUNC_ALWAYS );
	GL_Color( 1, 1, 1 );
	qglPushMatrix();
	qglLoadIdentity(); 
	qglDisable( GL_TEXTURE_2D );
    qglOrtho( 0, 1, 0, 1, -1, 1 );
	qglRasterPos2f( 0.01f, 0.01f );
	qglDrawPixels( G_WIDTH, G_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, image );
	qglPopMatrix();
	qglEnable( GL_TEXTURE_2D );
	qglMatrixMode( GL_MODELVIEW );
}


/*
==================
idRenderBackend::DBG_TestGammaBias
==================
*/
void idRenderBackend::DBG_TestGammaBias() {
	byte	image[G_HEIGHT][G_WIDTH][4];

	if ( r_testGammaBias.GetInteger() <= 0 ) {
		return;
	}

	int y = 0;
	for ( int bias = -40; bias < 40; bias+=10, y += BAR_HEIGHT ) {
		float	scale = 1;
		for ( int c = 0; c < 4; c++ ) {
			int v = (int)(64 * scale + bias);
			scale = scale * 1.5;
			if ( v < 0 ) {
				v = 0;
			} else if ( v > 255 ) {
				v = 255;
			}
			for ( int i = 0; i < BAR_HEIGHT; i++ ) {
				for ( int j = 0; j < G_WIDTH/4; j++ ) {
					image[y+i][c*G_WIDTH/4+j][0] = v;
					image[y+i][c*G_WIDTH/4+j][1] = v;
					image[y+i][c*G_WIDTH/4+j][2] = v;
				}
			}
		}
	}

	qglLoadIdentity();
	qglMatrixMode( GL_PROJECTION );
	GL_State( GLS_DEPTHFUNC_ALWAYS );
	GL_Color( 1, 1, 1 );
	qglPushMatrix();
	qglLoadIdentity(); 
	qglDisable( GL_TEXTURE_2D );
    qglOrtho( 0, 1, 0, 1, -1, 1 );
	qglRasterPos2f( 0.01f, 0.01f );
	qglDrawPixels( G_WIDTH, G_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, image );
	qglPopMatrix();
	qglEnable( GL_TEXTURE_2D );
	qglMatrixMode( GL_MODELVIEW );
}

/*
================
idRenderBackend::DBG_TestImage

Display a single image over most of the screen
================
*/
void idRenderBackend::DBG_TestImage() {
	idImage	*image = NULL;
	idImage *imageCr = NULL;
	idImage *imageCb = NULL;
	int		max;
	float	w, h;

	image = testImage;
	if ( !image ) {
		return;
	}

	if ( testVideo ) {
		cinData_t	cin;

		cin = testVideo->ImageForTime( m_viewDef->renderView.time[1] - testVideoStartTime );
		if ( cin.imageY != NULL ) {
			image = cin.imageY;
			imageCr = cin.imageCr;
			imageCb = cin.imageCb;
		} else {
			testImage = NULL;
			return;
		}
		w = 0.25;
		h = 0.25;
	} else {
		max = image->GetUploadWidth() > image->GetUploadHeight() ? image->GetUploadWidth() : image->GetUploadHeight();

		w = 0.25 * image->GetUploadWidth() / max;
		h = 0.25 * image->GetUploadHeight() / max;

		w *= (float)renderSystem->GetHeight() / renderSystem->GetWidth();
	}

	// Set State
	GL_State( GLS_DEPTHFUNC_ALWAYS | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );

	// Set Parms
	float texS[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
	float texT[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
	renderProgManager.SetRenderParm( RENDERPARM_TEXTUREMATRIX_S, texS );
	renderProgManager.SetRenderParm( RENDERPARM_TEXTUREMATRIX_T, texT );

	float texGenEnabled[4] = { 0, 0, 0, 0 };
	renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_0_ENABLED, texGenEnabled );

	// not really necessary but just for clarity
	const float screenWidth = 1.0f;
	const float screenHeight = 1.0f;
	const float halfScreenWidth = screenWidth * 0.5f;
	const float halfScreenHeight = screenHeight * 0.5f;

	float scale[16] = { 0 };
	scale[0] = w; // scale
	scale[5] = h; // scale
	scale[12] = halfScreenWidth - ( halfScreenWidth * w ); // translate
	scale[13] = halfScreenHeight - ( halfScreenHeight * h ); // translate
	scale[10] = 1.0f;
	scale[15] = 1.0f;

	float ortho[16] = { 0 };
	ortho[0] = 2.0f / screenWidth;
	ortho[5] = -2.0f / screenHeight;
	ortho[10] = -2.0f;
	ortho[12] = -1.0f;
	ortho[13] = 1.0f;
	ortho[14] = -1.0f;
	ortho[15] = 1.0f;

	float finalOrtho[16];
	R_MatrixMultiply( scale, ortho, finalOrtho );

	float projMatrixTranspose[16];
	R_MatrixTranspose( finalOrtho, projMatrixTranspose );
	renderProgManager.SetRenderParms( RENDERPARM_MVPMATRIX_X, projMatrixTranspose, 4 );
	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( finalOrtho );
	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();

	// Set Color
	GL_Color( 1, 1, 1, 1 );

	// Bind the Texture
	if ( ( imageCr != NULL ) && ( imageCb != NULL ) ) {
		GL_SelectTexture( 0 );
		GL_BindTexture( image );
		GL_SelectTexture( 1 );
		GL_BindTexture( imageCr );
		GL_SelectTexture( 2 );
		GL_BindTexture( imageCb );
		renderProgManager.BindProgram( BUILTIN_BINK );
	} else {
		GL_SelectTexture( 0 );
		GL_BindTexture( image );
		// Set Shader
		renderProgManager.BindProgram( BUILTIN_TEXTURED );
	}
	
	// Draw!
	DrawElementsWithCounters( &m_testImageSurface );
}

/*
=================
RB_DrawExpandedTriangles
=================
*/
void RB_DrawExpandedTriangles( const srfTriangles_t *tri, const float radius, const idVec3 &vieworg ) {
	int i, j, k;
	idVec3 dir[6], normal, point;

	for ( i = 0; i < tri->numIndexes; i += 3 ) {

		idVec3 p[3] = { tri->verts[ tri->indexes[ i + 0 ] ].xyz, tri->verts[ tri->indexes[ i + 1 ] ].xyz, tri->verts[ tri->indexes[ i + 2 ] ].xyz };

		dir[0] = p[0] - p[1];
		dir[1] = p[1] - p[2];
		dir[2] = p[2] - p[0];

		normal = dir[0].Cross( dir[1] );

		if ( normal * p[0] < normal * vieworg ) {
			continue;
		}

		dir[0] = normal.Cross( dir[0] );
		dir[1] = normal.Cross( dir[1] );
		dir[2] = normal.Cross( dir[2] );

		dir[0].Normalize();
		dir[1].Normalize();
		dir[2].Normalize();

		qglBegin( GL_LINE_LOOP );

		for ( j = 0; j < 3; j++ ) {
			k = ( j + 1 ) % 3;

			dir[4] = ( dir[j] + dir[k] ) * 0.5f;
			dir[4].Normalize();

			dir[3] = ( dir[j] + dir[4] ) * 0.5f;
			dir[3].Normalize();

			dir[5] = ( dir[4] + dir[k] ) * 0.5f;
			dir[5].Normalize();

			point = p[k] + dir[j] * radius;
			qglVertex3f( point[0], point[1], point[2] );

			point = p[k] + dir[3] * radius;
			qglVertex3f( point[0], point[1], point[2] );

			point = p[k] + dir[4] * radius;
			qglVertex3f( point[0], point[1], point[2] );

			point = p[k] + dir[5] * radius;
			qglVertex3f( point[0], point[1], point[2] );

			point = p[k] + dir[k] * radius;
			qglVertex3f( point[0], point[1], point[2] );
		}

		qglEnd();
	}
}

/*
================
idRenderBackend::DBG_ShowTrace

Debug visualization

FIXME: not thread safe!
================
*/
localTrace_t R_LocalTrace( const idVec3 &start, const idVec3 &end, const float radius, const srfTriangles_t *tri );
void idRenderBackend::DBG_ShowTrace( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	int						i;
	const srfTriangles_t	*tri;
	const drawSurf_t		*surf;
	idVec3					start, end;
	idVec3					localStart, localEnd;
	localTrace_t			hit;
	float					radius;

	if ( r_showTrace.GetInteger() == 0 ) {
		return;
	}

	if ( r_showTrace.GetInteger() == 2 ) {
		radius = 5.0f;
	} else {
		radius = 0.0f;
	}

	// determine the points of the trace
	start = m_viewDef->renderView.vieworg;
	end = start + 4000 * m_viewDef->renderView.viewaxis[0];

	// check and draw the surfaces
	GL_BindTexture( globalImages->m_whiteImage );

	// find how many are ambient
	for ( i = 0; i < numDrawSurfs; i++ ) {
		surf = drawSurfs[i];
		tri = surf->frontEndGeo;

		if ( tri == NULL || tri->verts == NULL ) {
			continue;
		}

		// transform the points into local space
		R_GlobalPointToLocal( surf->space->modelMatrix, start, localStart );
		R_GlobalPointToLocal( surf->space->modelMatrix, end, localEnd );

		// check the bounding box
		if ( !tri->bounds.Expand( radius ).LineIntersection( localStart, localEnd ) ) {
			continue;
		}

		qglLoadMatrixf( surf->space->modelViewMatrix );

		// highlight the surface
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

		GL_Color( 1, 0, 0, 0.25 );
		DrawElementsWithCounters( surf );

		// draw the bounding box
		GL_State( GLS_DEPTHFUNC_ALWAYS );

		GL_Color( 1, 1, 1, 1 );
		RB_DrawBounds( tri->bounds );

		if ( radius != 0.0f ) {
			// draw the expanded triangles
			GL_Color( 0.5f, 0.5f, 1.0f, 1.0f );
			RB_DrawExpandedTriangles( tri, radius, localStart );
		}

		// check the exact surfaces
		hit = R_LocalTrace( localStart, localEnd, radius, tri );
		if ( hit.fraction < 1.0 ) {
			GL_Color( 1, 1, 1, 1 );
			RB_DrawBounds( idBounds( hit.point ).Expand( 1 ) );
		}
	}
}

/*
=================
idRenderBackend::DBG_RenderDebugTools
=================
*/
void idRenderBackend::DBG_RenderDebugTools( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	// don't do much if this was a 2D rendering
	if ( !m_viewDef->viewEntitys ) {
		DBG_TestImage();
		DBG_ShowLines();
		return;
	}

	renderLog.OpenMainBlock( MRB_DRAW_DEBUG_TOOLS );
	renderLog.OpenBlock( "RB_RenderDebugTools" );

	GL_State( GLS_DEFAULT );

	GL_Scissor( m_viewDef->viewport.x1 + m_viewDef->scissor.x1,
				m_viewDef->viewport.y1 + m_viewDef->scissor.y1,
				m_viewDef->scissor.x2 + 1 - m_viewDef->scissor.x1,
				m_viewDef->scissor.y2 + 1 - m_viewDef->scissor.y1 );
	m_currentScissor = m_viewDef->scissor;

	DBG_ShowLightCount();
	DBG_ShowTexturePolarity( drawSurfs, numDrawSurfs );
	DBG_ShowTangentSpace( drawSurfs, numDrawSurfs );
	DBG_ShowVertexColor( drawSurfs, numDrawSurfs );
	DBG_ShowTris( drawSurfs, numDrawSurfs );
	DBG_ShowUnsmoothedTangents( drawSurfs, numDrawSurfs );
	DBG_ShowSurfaceInfo( drawSurfs, numDrawSurfs );
	DBG_ShowEdges( drawSurfs, numDrawSurfs );
	DBG_ShowNormals( drawSurfs, numDrawSurfs );
	DBG_ShowViewEntitys( m_viewDef->viewEntitys );
	DBG_ShowLights();
	DBG_ShowTextureVectors( drawSurfs, numDrawSurfs );
	DBG_ShowDominantTris( drawSurfs, numDrawSurfs );
	if ( r_testGamma.GetInteger() > 0 ) {	// test here so stack check isn't so damn slow on debug builds
		DBG_TestGamma();
	}
	if ( r_testGammaBias.GetInteger() > 0 ) {
		DBG_TestGammaBias();
	}
	DBG_TestImage();
	DBG_ShowPortals();
	DBG_ShowSilhouette();
	DBG_ShowDepthBuffer();
	DBG_ShowIntensity();
	DBG_ShowCenterOfProjection();
	DBG_ShowLines();
	DBG_ShowDebugLines();
	DBG_ShowDebugText();
	DBG_ShowDebugPolygons();
	DBG_ShowTrace( drawSurfs, numDrawSurfs );

	renderLog.CloseBlock();
	renderLog.CloseMainBlock();
}

/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 

/*
====================
idRenderSystemLocal::ReadTiledPixels

NO LONGER SUPPORTED (FIXME: make standard case work)

Used to allow the rendering of an image larger than the actual window by
tiling it into window-sized chunks and rendering each chunk separately

If ref isn't specified, the full session UpdateScreen will be done.
====================
*/
void idRenderSystemLocal::ReadTiledPixels( int width, int height, byte *buffer, renderView_t *ref ) {
	// include extra space for OpenGL padding to word boundaries
	int sysWidth = GetWidth();
	int sysHeight = GetHeight();
	byte * temp = (byte *)R_StaticAlloc( (sysWidth+3) * sysHeight * 3 );

	// disable scissor, so we don't need to adjust all those rects
	r_useScissor.SetBool( false );

	for ( int xo = 0 ; xo < width ; xo += sysWidth ) {
		for ( int yo = 0 ; yo < height ; yo += sysHeight ) {
			if ( ref ) {
				// discard anything currently on the list
				SwapCommandBuffers( NULL, NULL, NULL, NULL );

				// build commands to render the scene
				RenderScene( primaryWorld, ref );

				// finish off these commands
				const renderCommand_t * cmd = tr.SwapCommandBuffers( NULL, NULL, NULL, NULL );

				// issue the commands to the GPU
				tr.RenderCommandBuffers( cmd );
			} else {
				common->UpdateScreen();
			}

			int w = sysWidth;
			if ( xo + w > width ) {
				w = width - xo;
			}
			int h = sysHeight;
			if ( yo + h > height ) {
				h = height - yo;
			}

			qglReadBuffer( GL_FRONT );
			qglReadPixels( 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, temp ); 

			int	row = ( w * 3 + 3 ) & ~3;		// OpenGL pads to dword boundaries

			for ( int y = 0 ; y < h ; y++ ) {
				memcpy( buffer + ( ( yo + y )* width + xo ) * 3,
					temp + y * row, w * 3 );
			}
		}
	}

	r_useScissor.SetBool( true );

	R_StaticFree( temp );
}

/*
===============
R_StencilShot
Save out a screenshot showing the stencil buffer expanded by 16x range
===============
*/
void R_StencilShot() {
	int			i, c;

	int	width = tr.GetWidth();
	int	height = tr.GetHeight();

	int	pix = width * height;

	c = pix * 3 + 18;
	idTempArray< byte > buffer( c );
	memset( buffer.Ptr(), 0, 18 );

	idTempArray< byte > byteBuffer( pix );

	qglReadPixels( 0, 0, width, height, GL_STENCIL_INDEX , GL_UNSIGNED_BYTE, byteBuffer.Ptr() ); 

	for ( i = 0 ; i < pix ; i++ ) {
		buffer[18+i*3] =
		buffer[18+i*3+1] =
			//		buffer[18+i*3+2] = ( byteBuffer[i] & 15 ) * 16;
		buffer[18+i*3+2] = byteBuffer[i];
	}

	// fill in the header (this is vertically flipped, which qglReadPixels emits)
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;	// pixel size

	fileSystem->WriteFile( "screenshots/stencilShot.tga", buffer.Ptr(), c, "fs_savepath" );
}