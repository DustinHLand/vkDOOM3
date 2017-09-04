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
#include "Model_local.h"
#include "GLMatrix.h"
#include "Image.h"

extern idCVar r_skipSubviews;
extern idCVar r_useGPUSkinning;

/*
==========================================================================================

SUBVIEW SURFACES

==========================================================================================
*/

struct orientation_t {
	idVec3		origin;
	idMat3		axis;
};

/*
=================
R_MirrorPoint
=================
*/
static void R_MirrorPoint( const idVec3 in, orientation_t *surface, orientation_t *camera, idVec3 &out ) {
	const idVec3 local = in - surface->origin;

	idVec3 transformed( 0.0f );
	for ( int i = 0; i < 3; i++ ) {
		const float d = local * surface->axis[i];
		transformed += d * camera->axis[i];
	}

	out = transformed + camera->origin;
}

/*
=================
R_MirrorVector
=================
*/
static void R_MirrorVector( const idVec3 in, orientation_t *surface, orientation_t *camera, idVec3 &out ) {
	out.Zero();
	for ( int i = 0; i < 3; i++ ) {
		const float d = in * surface->axis[i];
		out += d * camera->axis[i];
	}
}

/*
=============
R_PlaneForSurface

Returns the plane for the first triangle in the surface
FIXME: check for degenerate triangle?
=============
*/
static void R_PlaneForSurface( const srfTriangles_t *tri, idPlane &plane ) {
	idDrawVert * v1 = tri->verts + tri->indexes[0];
	idDrawVert * v2 = tri->verts + tri->indexes[1];
	idDrawVert * v3 = tri->verts + tri->indexes[2];
	plane.FromPoints( v1->xyz, v2->xyz, v3->xyz );
}

/*
=========================
idRenderSystemLocal::PreciseCullSurface

Check the surface for visibility on a per-triangle basis
for cases when it is going to be VERY expensive to draw (subviews)

If not culled, also returns the bounding box of the surface in 
Normalized Device Coordinates, so it can be used to crop the scissor rect.

OPTIMIZE: we could also take exact portal passing into consideration
=========================
*/
bool idRenderSystemLocal::PreciseCullSurface( const drawSurf_t *drawSurf, idBounds &ndcBounds ) {
	const srfTriangles_t * tri = drawSurf->frontEndGeo;

	unsigned int pointOr = 0;
	unsigned int pointAnd = (unsigned int)~0;

	// get an exact bounds of the triangles for scissor cropping
	ndcBounds.Clear();

	const idJointMat * joints = ( tri->staticModelWithJoints != NULL && r_useGPUSkinning.GetBool() ) ? tri->staticModelWithJoints->jointsInverted : NULL;

	for ( int i = 0; i < tri->numVerts; i++ ) {
		const idVec3 vXYZ = idDrawVert::GetSkinnedDrawVertPosition( tri->verts[i], joints );

		idPlane eye, clip;
		R_TransformModelToClip( vXYZ, drawSurf->space->modelViewMatrix, m_viewDef->projectionMatrix, eye, clip );

		unsigned int pointFlags = 0;
		for ( int j = 0; j < 3; j++ ) {
			if ( clip[j] >= clip[3] ) {
				pointFlags |= ( 1 << (j*2+0) );
			} else if ( clip[j] <= -clip[3] ) {	// FIXME: the D3D near clip plane is at zero instead of -1
				pointFlags |= ( 1 << (j*2+1) );
			}
		}

		pointAnd &= pointFlags;
		pointOr |= pointFlags;
	}

	// trivially reject
	if ( pointAnd != 0 ) {
		return true;
	}

	// backface and frustum cull
	idVec3 localViewOrigin;
	R_GlobalPointToLocal( drawSurf->space->modelMatrix, m_viewDef->renderView.vieworg, localViewOrigin );

	for ( int i = 0; i < tri->numIndexes; i += 3 ) {
		const idVec3 v1 = idDrawVert::GetSkinnedDrawVertPosition( tri->verts[ tri->indexes[ i+0 ] ], joints );
		const idVec3 v2 = idDrawVert::GetSkinnedDrawVertPosition( tri->verts[ tri->indexes[ i+1 ] ], joints );
		const idVec3 v3 = idDrawVert::GetSkinnedDrawVertPosition( tri->verts[ tri->indexes[ i+2 ] ], joints );

		// this is a hack, because R_GlobalPointToLocal doesn't work with the non-normalized
		// axis that we get from the gui view transform.  It doesn't hurt anything, because
		// we know that all gui generated surfaces are front facing
		if ( m_guiRecursionLevel == 0 ) {
			// we don't care that it isn't normalized,
			// all we want is the sign
			const idVec3 d1 = v2 - v1;
			const idVec3 d2 = v3 - v1;
			const idVec3 normal = d2.Cross( d1 );

			const idVec3 dir = v1 - localViewOrigin;

			const float dot = normal * dir;
			if ( dot >= 0.0f ) {
				return true;
			}
		}

		// now find the exact screen bounds of the clipped triangle
		idFixedWinding w;
		w.SetNumPoints( 3 );
		R_LocalPointToGlobal( drawSurf->space->modelMatrix, v1, w[0].ToVec3() );
		R_LocalPointToGlobal( drawSurf->space->modelMatrix, v2, w[1].ToVec3() );
		R_LocalPointToGlobal( drawSurf->space->modelMatrix, v3, w[2].ToVec3() );
		w[0].s = w[0].t = w[1].s = w[1].t = w[2].s = w[2].t = 0.0f;

		for ( int j = 0; j < 4; j++ ) {
			if ( !w.ClipInPlace( -m_viewDef->frustum[j], 0.1f ) ) {
				break;
			}
		}
		for ( int j = 0; j < w.GetNumPoints(); j++ ) {
			idVec3 screen;

			R_GlobalToNormalizedDeviceCoordinates( w[j].ToVec3(), screen );
			ndcBounds.AddPoint( screen );
		}
	}

	// if we don't enclose any area, return
	if ( ndcBounds.IsCleared() ) {
		return true;
	}

	return false;
}

/*
========================
idRenderSystemLocal::MirrorViewBySurface
========================
*/
viewDef_t * idRenderSystemLocal::MirrorViewBySurface( const drawSurf_t * drawSurf ) {
	// copy the viewport size from the original
	viewDef_t * parms = (viewDef_t *)FrameAlloc( sizeof( *parms ) );
	*parms = *m_viewDef;
	parms->renderView.viewID = 0;	// clear to allow player bodies to show up, and suppress view weapons

	parms->isSubview = true;
	parms->isMirror = true;

	// create plane axis for the portal we are seeing
	idPlane originalPlane, plane;
	R_PlaneForSurface( drawSurf->frontEndGeo, originalPlane );
	R_LocalPlaneToGlobal( drawSurf->space->modelMatrix, originalPlane, plane );

	orientation_t surface;
	surface.origin = plane.Normal() * -plane[3];
	surface.axis[0] = plane.Normal();
	surface.axis[0].NormalVectors( surface.axis[1], surface.axis[2] );
	surface.axis[2] = -surface.axis[2];

	orientation_t camera;
	camera.origin = surface.origin;
	camera.axis[0] = -surface.axis[0];
	camera.axis[1] = surface.axis[1];
	camera.axis[2] = surface.axis[2];

	// set the mirrored origin and axis
	R_MirrorPoint( m_viewDef->renderView.vieworg, &surface, &camera, parms->renderView.vieworg );

	R_MirrorVector( m_viewDef->renderView.viewaxis[0], &surface, &camera, parms->renderView.viewaxis[0] );
	R_MirrorVector( m_viewDef->renderView.viewaxis[1], &surface, &camera, parms->renderView.viewaxis[1] );
	R_MirrorVector( m_viewDef->renderView.viewaxis[2], &surface, &camera, parms->renderView.viewaxis[2] );

	// make the view origin 16 units away from the center of the surface
	const idVec3 center = ( drawSurf->frontEndGeo->bounds[0] + drawSurf->frontEndGeo->bounds[1] ) * 0.5f;
	const idVec3 viewOrigin = center + ( originalPlane.Normal() * 16.0f );

	R_LocalPointToGlobal( drawSurf->space->modelMatrix, viewOrigin, parms->initialViewAreaOrigin );

	// set the mirror clip plane
	parms->numClipPlanes = 1;
	parms->clipPlanes[0] = -camera.axis[0];

	parms->clipPlanes[0][3] = -( camera.origin * parms->clipPlanes[0].Normal() );
	
	return parms;
}

/*
========================
idRenderSystemLocal::XrayViewBySurface
========================
*/
viewDef_t * idRenderSystemLocal::XrayViewBySurface( const drawSurf_t *drawSurf ) {
	// copy the viewport size from the original
	viewDef_t * parms = (viewDef_t *)FrameAlloc( sizeof( *parms ) );
	*parms = *m_viewDef;
	parms->renderView.viewID = 0;	// clear to allow player bodies to show up, and suppress view weapons

	parms->isSubview = true;
	parms->isXraySubview = true;

	return parms;
}

/*
===============
idRenderSystemLocal::RemoteRender
===============
*/
void idRenderSystemLocal::RemoteRender( const drawSurf_t *surf, textureStage_t *stage ) {
	// remote views can be reused in a single frame
	if ( stage->dynamicFrameCount == frameCount ) {
		return;
	}

	// if the entity doesn't have a remoteRenderView, do nothing
	if ( !surf->space->entityDef->parms.remoteRenderView ) {
		return;
	}

	int stageWidth = stage->width;
	int stageHeight = stage->height;

	// copy the viewport size from the original
	viewDef_t * parms = (viewDef_t *)FrameAlloc( sizeof( *parms ) );
	*parms = *m_viewDef;

	parms->renderView = *surf->space->entityDef->parms.remoteRenderView;
	parms->renderView.viewID = 0;	// clear to allow player bodies to show up, and suppress view weapons
	parms->initialViewAreaOrigin = parms->renderView.vieworg;
	parms->isSubview = true;
	parms->isMirror = false;

	idScreenRect & viewport = parms->viewport;
	viewport.x1 = 0;
	viewport.y1 = 0;
	viewport.x2 = stageWidth - 1;
	viewport.y2 = stageHeight - 1;

	parms->scissor.x1 = 0;
	parms->scissor.y1 = 0;
	parms->scissor.x2 = parms->viewport.x2 - parms->viewport.x1;
	parms->scissor.y2 = parms->viewport.y2 - parms->viewport.y1;

	parms->superView = m_viewDef;
	parms->subviewSurface = surf;

	if ( stage->image == NULL ) {
		stage->image = globalImages->m_scratchImage;
	}

	// generate render commands for it
	RenderView( parms, stage->image );

	stage->dynamicFrameCount = frameCount;
}

/*
=================
idRenderSystemLocal::MirrorRender
=================
*/
void idRenderSystemLocal::MirrorRender( const drawSurf_t *surf, textureStage_t *stage, idScreenRect scissor ) {
	// remote views can be reused in a single frame
	if ( stage->dynamicFrameCount == frameCount ) {
		return;
	}

	// issue a new view command
	viewDef_t * parms = MirrorViewBySurface( surf );
	if ( parms == NULL ) {
		return;
	}

	idScreenRect & viewport = parms->viewport;
	viewport.x1 = 0;
	viewport.y1 = 0;
	viewport.x2 = stage->width - 1;
	viewport.y2 = stage->height - 1;

	parms->scissor.x1 = 0;
	parms->scissor.y1 = 0;
	parms->scissor.x2 = parms->viewport.x2 - parms->viewport.x1;
	parms->scissor.y2 = parms->viewport.y2 - parms->viewport.y1;

	parms->superView = m_viewDef;
	parms->subviewSurface = surf;

	// triangle culling order changes with mirroring
	parms->isMirror = ( ( (int)parms->isMirror ^ (int)m_viewDef->isMirror ) != 0 );

	stage->image = globalImages->m_scratchImage;

	// generate render commands for it
	RenderView( parms, stage->image );

	stage->dynamicFrameCount = frameCount;
}

/*
=================
idRenderSystemLocal::XrayRender
=================
*/
void idRenderSystemLocal::XrayRender( const drawSurf_t *surf, textureStage_t *stage, idScreenRect scissor ) {
	// remote views can be reused in a single frame
	if ( stage->dynamicFrameCount == frameCount ) {
		return;
	}

	// issue a new view command
	viewDef_t * parms = XrayViewBySurface( surf );
	if ( parms == NULL ) {
		return;
	}

	int stageWidth = stage->width;
	int stageHeight = stage->height;

	idScreenRect & viewport = parms->viewport;
	viewport.x1 = 0;
	viewport.y1 = 0;
	viewport.x2 = stageWidth - 1;
	viewport.y2 = stageHeight - 1;

	parms->scissor.x1 = 0;
	parms->scissor.y1 = 0;
	parms->scissor.x2 = parms->viewport.x2 - parms->viewport.x1;
	parms->scissor.y2 = parms->viewport.y2 - parms->viewport.y1;

	parms->superView = m_viewDef;
	parms->subviewSurface = surf;

	// triangle culling order changes with mirroring
	parms->isMirror = ( ( (int)parms->isMirror ^ (int)m_viewDef->isMirror ) != 0 );

	stage->image = globalImages->m_scratchImage2;

	// generate render commands for it
	RenderView( parms, stage->image );

	stage->dynamicFrameCount = frameCount;
}

/*
==================
idRenderSystemLocal::GenerateSurfaceSubview
==================
*/
bool idRenderSystemLocal::GenerateSurfaceSubview( const drawSurf_t * drawSurf ) {
	// for testing the performance hit
	if ( r_skipSubviews.GetBool() ) {
		return false;
	}

	idBounds ndcBounds;
	if ( PreciseCullSurface( drawSurf, ndcBounds ) ) {
		return false;
	}

	const idMaterial * shader = drawSurf->material;

	// never recurse through a subview surface that we are
	// already seeing through
	viewDef_t * parms = NULL;
	for ( parms = m_viewDef; parms != NULL; parms = parms->superView ) {
		if ( parms->subviewSurface != NULL
			&& parms->subviewSurface->frontEndGeo == drawSurf->frontEndGeo
			&& parms->subviewSurface->space->entityDef == drawSurf->space->entityDef ) {
			break;
		}
	}
	if ( parms ) {
		return false;
	}

	// crop the scissor bounds based on the precise cull
	assert( m_viewDef != NULL );
	idScreenRect * v = &m_viewDef->viewport;
	idScreenRect scissor;
	scissor.x1 = v->x1 + idMath::Ftoi( ( v->x2 - v->x1 + 1 ) * 0.5f * ( ndcBounds[0][0] + 1.0f ) );
	scissor.y1 = v->y1 + idMath::Ftoi( ( v->y2 - v->y1 + 1 ) * 0.5f * ( ndcBounds[0][1] + 1.0f ) );
	scissor.x2 = v->x1 + idMath::Ftoi( ( v->x2 - v->x1 + 1 ) * 0.5f * ( ndcBounds[1][0] + 1.0f ) );
	scissor.y2 = v->y1 + idMath::Ftoi( ( v->y2 - v->y1 + 1 ) * 0.5f * ( ndcBounds[1][1] + 1.0f ) );

	// nudge a bit for safety
	scissor.Expand();

	scissor.Intersect( m_viewDef->scissor );

	if ( scissor.IsEmpty() ) {
		// cropped out
		return false;
	}

	// see what kind of subview we are making
	if ( shader->GetSort() != SS_SUBVIEW ) {
		for ( int i = 0; i < shader->GetNumStages(); i++ ) {
			const shaderStage_t	*stage = shader->GetStage( i );
			switch ( stage->texture.dynamic ) {
			case DI_REMOTE_RENDER:
				RemoteRender( drawSurf, const_cast<textureStage_t *>(&stage->texture) );
				break;
			case DI_MIRROR_RENDER:
				MirrorRender( drawSurf, const_cast<textureStage_t *>(&stage->texture), scissor );
				break;
			case DI_XRAY_RENDER:
				XrayRender( drawSurf, const_cast<textureStage_t *>(&stage->texture), scissor );
				break;
			}
		}
		return true;
	}

	// issue a new view command
	parms = MirrorViewBySurface( drawSurf );
	if ( parms == NULL ) {
		return false;
	}

	parms->scissor = scissor;
	parms->superView = m_viewDef;
	parms->subviewSurface = drawSurf;

	// triangle culling order changes with mirroring
	parms->isMirror = ( ( (int)parms->isMirror ^ (int)m_viewDef->isMirror ) != 0 );

	// generate render commands for it
	RenderView( parms );

	return true;
}

/*
================
idRenderSystemLocal::GenerateSubViews

If we need to render another view to complete the current view,
generate it first.

It is important to do this after all drawSurfs for the current
view have been generated, because it may create a subview which
would change viewCount.
================
*/
bool idRenderSystemLocal::GenerateSubViews( const drawSurf_t * const drawSurfs[], const int numDrawSurfs ) {
	SCOPED_PROFILE_EVENT( "GenerateSubViews" );

	// for testing the performance hit
	if ( r_skipSubviews.GetBool() ) {
		return false;
	}

	// scan the surfaces until we either find a subview, or determine
	// there are no more subview surfaces.
	bool subviews = false;
	for ( int i = 0; i < numDrawSurfs; i++ ) {
		const drawSurf_t * drawSurf = drawSurfs[i];

		if ( !drawSurf->material->HasSubview() ) {
			continue;
		}

		if ( GenerateSurfaceSubview( drawSurf ) ) {
			subviews = true;
		}
	}

	return subviews;
}
