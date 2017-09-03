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
#include "RenderSystem_local.h"
#include "GLMatrix.h"
#include "GuiModel.h"

extern idCVar r_skipFrontEnd;
extern idCVar r_screenFraction;
extern idCVar r_znear;
extern idCVar r_subviewOnly;
extern idCVar r_useShadowSurfaceScissor;

/*
==========================================================================================

FONT-END STATIC MEMORY ALLOCATION

==========================================================================================
*/

/*
=================
R_StaticAlloc
=================
*/
void *R_StaticAlloc( int bytes, const memTag_t tag ) {
	tr.pc.c_alloc++;

    void * buf = Mem_Alloc( bytes, tag );

	// don't exit on failure on zero length allocations since the old code didn't
	if ( buf == NULL && bytes != 0 ) {
		common->FatalError( "R_StaticAlloc failed on %i bytes", bytes );
	}
	return buf;
}

/*
=================
R_ClearedStaticAlloc
=================
*/
void *R_ClearedStaticAlloc( int bytes ) {
	void * buf = R_StaticAlloc( bytes );
	memset( buf, 0, bytes );
	return buf;
}

/*
=================
R_StaticFree
=================
*/
void R_StaticFree( void *data ) {
	tr.pc.c_free++;
    Mem_Free( data );
}

/*
==========================================================================================

FONT-END RENDERING

==========================================================================================
*/

/*
=================
R_SortDrawSurfs
=================
*/
static void R_SortDrawSurfs( drawSurf_t ** drawSurfs, const int numDrawSurfs ) {
	uint64 * indices = (uint64 *) _alloca16( numDrawSurfs * sizeof( indices[0] ) );

	// sort the draw surfs based on:
	// 1. sort value (largest first)
	// 2. depth (smallest first)
	// 3. index (largest first)
	assert( numDrawSurfs <= 0xFFFF );
	for ( int i = 0; i < numDrawSurfs; i++ ) {
		float sort = SS_POST_PROCESS - drawSurfs[i]->sort;
		assert( sort >= 0.0f );

		uint64 dist = 0;
		if ( drawSurfs[i]->frontEndGeo != NULL ) {
			float min = 0.0f;
			float max = 1.0f;
			idRenderMatrix::DepthBoundsForBounds( min, max, drawSurfs[i]->space->mvp, drawSurfs[i]->frontEndGeo->bounds );
			dist = idMath::Ftoui16( min * 0xFFFF );
		}
		
		indices[i] = ( ( numDrawSurfs - i ) & 0xFFFF ) | ( dist << 16 ) | ( (uint64) ( *(uint32 *)&sort ) << 32 );
	}

	const int64 MAX_LEVELS = 128;
	int64 lo[MAX_LEVELS];
	int64 hi[MAX_LEVELS];

	// Keep the top of the stack in registers to avoid load-hit-stores.
	register int64 st_lo = 0;
	register int64 st_hi = numDrawSurfs - 1;
	register int64 level = 0;

	for ( ; ; ) {
		register int64 i = st_lo;
		register int64 j = st_hi;
		if ( j - i >= 4 && level < MAX_LEVELS - 1 ) {
			register uint64 pivot = indices[( i + j ) / 2];
			do {
				while ( indices[i] > pivot ) i++;
				while ( indices[j] < pivot ) j--;
				if ( i > j ) break;
				uint64 h = indices[i]; indices[i] = indices[j]; indices[j] = h;
			} while ( ++i <= --j );

			// No need for these iterations because we are always sorting unique values.
			//while ( indices[j] == pivot && st_lo < j ) j--;
			//while ( indices[i] == pivot && i < st_hi ) i++;

			assert( level < MAX_LEVELS - 1 );
			lo[level] = i;
			hi[level] = st_hi;
			st_hi = j;
			level++;
		} else {
			for( ; i < j; j-- ) {
				register int64 m = i;
				for ( int64 k = i + 1; k <= j; k++ ) {
					if ( indices[k] < indices[m] ) {
						m = k;
					}
				}
				uint64 h = indices[m]; indices[m] = indices[j]; indices[j] = h;
			}
			if ( --level < 0 ) {
				break;
			}
			st_lo = lo[level];
			st_hi = hi[level];
		}
	}

	drawSurf_t ** newDrawSurfs = (drawSurf_t **) indices;
	for ( int i = 0; i < numDrawSurfs; i++ ) {
		newDrawSurfs[i] = drawSurfs[numDrawSurfs - ( indices[i] & 0xFFFF )];
	}
	memcpy( drawSurfs, newDrawSurfs, numDrawSurfs * sizeof( drawSurfs[0] ) );
}

/*
=====================
R_OptimizeViewLightsList
=====================
*/
void R_OptimizeViewLightsList( viewLight_t ** viewLights ) {
	// go through each visible light
	int numViewLights = 0;
	for ( viewLight_t * vLight = *viewLights; vLight != NULL; vLight = vLight->next ) {
		numViewLights++;
		// If the light didn't have any lit surfaces visible, there is no need to
		// draw any of the shadows.  We still keep the vLight for debugging draws.
		if ( !vLight->localInteractions && !vLight->globalInteractions && !vLight->translucentInteractions ) {
			vLight->localShadows = NULL;
			vLight->globalShadows = NULL;
		}
	}

	if ( r_useShadowSurfaceScissor.GetBool() ) {
		// shrink the light scissor rect to only intersect the surfaces that will actually be drawn.
		// This doesn't seem to actually help, perhaps because the surface scissor
		// rects aren't actually the surface, but only the portal clippings.
		for ( viewLight_t * vLight = *viewLights; vLight; vLight = vLight->next ) {
			drawSurf_t * surf;
			idScreenRect surfRect;

			if ( !vLight->lightShader->LightCastsShadows() ) {
				continue;
			}

			surfRect.Clear();

			for ( surf = vLight->globalInteractions; surf != NULL; surf = surf->nextOnLight ) {
				surfRect.Union( surf->scissorRect );
			}
			for ( surf = vLight->localShadows; surf != NULL; surf = surf->nextOnLight ) {
				surf->scissorRect.Intersect( surfRect );
			}

			for ( surf = vLight->localInteractions; surf != NULL; surf = surf->nextOnLight ) {
				surfRect.Union( surf->scissorRect );
			}
			for ( surf = vLight->globalShadows; surf != NULL; surf = surf->nextOnLight ) {
				surf->scissorRect.Intersect( surfRect );
			}

			for ( surf = vLight->translucentInteractions; surf != NULL; surf = surf->nextOnLight ) {
				surfRect.Union( surf->scissorRect );
			}

			vLight->scissorRect.Intersect( surfRect );
		}
	}

	// sort the viewLights list so the largest lights come first, which will reduce
	// the chance of GPU pipeline bubbles
	struct sortLight_t {
		viewLight_t *	vLight;
		int				screenArea;
		static int sort( const void * a, const void * b ) {
			return ((sortLight_t *)a)->screenArea - ((sortLight_t *)b)->screenArea;
		}
	};
	sortLight_t * sortLights = (sortLight_t *)_alloca( sizeof( sortLight_t ) * numViewLights );
	int	numSortLightsFilled = 0;
	for ( viewLight_t * vLight = *viewLights; vLight != NULL; vLight = vLight->next ) {
		sortLights[ numSortLightsFilled ].vLight = vLight;
		sortLights[ numSortLightsFilled ].screenArea = vLight->scissorRect.GetArea();
		numSortLightsFilled++;
	}

	qsort( sortLights, numSortLightsFilled, sizeof( sortLights[0] ), sortLight_t::sort );

	// rebuild the linked list in order
	*viewLights = NULL;
	for ( int i = 0; i < numSortLightsFilled; i++ ) {
		sortLights[i].vLight->next = *viewLights;
		*viewLights = sortLights[i].vLight;
	}
}

/*
================
idRenderSystemLocal::RenderScene

Draw a 3D view into a part of the window, then return
to 2D drawing.

Rendering a scene may require multiple views to be rendered
to handle mirrors.
================
*/
void idRenderSystemLocal::RenderScene( idRenderWorld * world, const renderView_t * renderView ) {
	if ( !m_bInitialized ) {
		return;
	}

	// skip front end rendering work, which will result
	// in only gui drawing
	if ( r_skipFrontEnd.GetBool() ) {
		return;
	}

	SCOPED_PROFILE_EVENT( "idRenderSystemLocal::RenderScene" );

	if ( renderView->fov_x <= 0 || renderView->fov_y <= 0 ) {
		idLib::Error( "idRenderSystemLocal::RenderScene: bad FOVs: %f, %f", renderView->fov_x, renderView->fov_y );
	}

	EmitFullscreenGui();

	int startTime = Sys_Microseconds();

	// setup viewDef for the intial view
	viewDef_t * parms = (viewDef_t *)ClearedFrameAlloc( sizeof( *parms ), FRAME_ALLOC_VIEW_DEF );
	parms->renderView = *renderView;

	if ( m_takingScreenshot ) {
		parms->renderView.forceUpdate = true;
	}

	int windowWidth = GetWidth();
	int windowHeight = GetHeight();

	PerformResolutionScaling( windowWidth, windowHeight );

	// screenFraction is just for quickly testing fill rate limitations
	if ( r_screenFraction.GetInteger() != 100 ) {
		windowWidth = ( windowWidth * r_screenFraction.GetInteger() ) / 100;
		windowHeight = ( windowHeight * r_screenFraction.GetInteger() ) / 100;
	}

	GetDefaultViewport( parms->viewport );

	// the scissor bounds may be shrunk in subviews even if
	// the viewport stays the same
	// this scissor range is local inside the viewport
	parms->scissor.x1 = 0;
	parms->scissor.y1 = 0;
	parms->scissor.x2 = parms->viewport.x2 - parms->viewport.x1;
	parms->scissor.y2 = parms->viewport.y2 - parms->viewport.y1;

	parms->isSubview = false;
	parms->initialViewAreaOrigin = renderView->vieworg;
	parms->renderWorld = world;

	// see if the view needs to reverse the culling sense in mirrors
	// or environment cube sides
	idVec3 cross;
	cross = parms->renderView.viewaxis[ 1 ].Cross( parms->renderView.viewaxis[ 2 ] );
	if ( cross * parms->renderView.viewaxis[ 0 ] > 0 ) {
		parms->isMirror = false;
	} else {
		parms->isMirror = true;
	}

	// save this world for use by some console commands
	primaryWorld = world;
	m_primaryRenderView = *renderView;
	primaryView = parms;

	// rendering this view may cause other views to be rendered
	// for mirrors / portals / shadows / environment maps
	// this will also cause any necessary entities and lights to be
	// updated to the demo file
	RenderView( parms );

	int endTime = Sys_Microseconds();

	pc.frontEndMicroSec += endTime - startTime;

	// prepare for any 2D drawing after this
	m_guiModel->Clear();
}

/*
================
idRenderSystemLocal::RenderView

A view may be either the actual camera view,
a mirror / remote location, or a 3D view on a gui surface.

Parms will typically be allocated with R_FrameAlloc
================
*/
void idRenderSystemLocal::RenderView( viewDef_t * parms ) {
	// save view in case we are a subview
	viewDef_t * oldView = m_viewDef;

	m_viewDef = parms;

	// setup the matrix for world space to eye space
	R_SetupViewMatrix( m_viewDef );

	// we need to set the projection matrix before doing
	// portal-to-screen scissor calculations
	R_SetupProjectionMatrix( m_viewDef );

	// setup render matrices for faster culling
	idRenderMatrix::Transpose( *(idRenderMatrix *)m_viewDef->projectionMatrix, m_viewDef->projectionRenderMatrix );
	idRenderMatrix viewRenderMatrix;
	idRenderMatrix::Transpose( *(idRenderMatrix *)m_viewDef->worldSpace.modelViewMatrix, viewRenderMatrix );
	idRenderMatrix::Multiply( m_viewDef->projectionRenderMatrix, viewRenderMatrix, m_viewDef->worldSpace.mvp );

	// the planes of the view frustum are needed for portal visibility culling
	idRenderMatrix::GetFrustumPlanes( m_viewDef->frustum, m_viewDef->worldSpace.mvp, false, true );

	// the DOOM 3 frustum planes point outside the frustum
	for ( int i = 0; i < 6; i++ ) {
		m_viewDef->frustum[i] = - m_viewDef->frustum[i];
	}
	// remove the Z-near to avoid portals from being near clipped
	m_viewDef->frustum[4][3] -= r_znear.GetFloat();

	// identify all the visible portal areas, and create view lights and view entities
	// for all the the entityDefs and lightDefs that are in the visible portal areas
	parms->renderWorld->FindViewLightsAndEntities();

	// wait for any shadow volume jobs from the previous frame to finish
	m_frontEndJobList->Wait();

	// make sure that interactions exist for all light / entity combinations that are visible
	// add any pre-generated light shadows, and calculate the light shader values
	AddLights();

	// adds ambient surfaces and create any necessary interaction surfaces to add to the light lists
	AddModels();

	// build up the GUIs on world surfaces
	AddInGameGuis( m_viewDef->drawSurfs, m_viewDef->numDrawSurfs );

	// any viewLight that didn't have visible surfaces can have it's shadows removed
	R_OptimizeViewLightsList( &m_viewDef->viewLights );

	// sort all the ambient surfaces for translucency ordering
	R_SortDrawSurfs( m_viewDef->drawSurfs, m_viewDef->numDrawSurfs );

	// generate any subviews (mirrors, cameras, etc) before adding this view
	if ( GenerateSubViews( m_viewDef->drawSurfs, m_viewDef->numDrawSurfs ) ) {
		// if we are debugging subviews, allow the skipping of the main view draw
		if ( r_subviewOnly.GetBool() ) {
			return;
		}
	}

	// add the rendering commands for this viewDef
	AddDrawViewCmd( parms, false );

	// restore view in case we are a subview
	m_viewDef = oldView;
}
