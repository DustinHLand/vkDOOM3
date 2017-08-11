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
#include "../idlib/precompiled.h"
#include "RenderSystem_local.h"
#include "GLMatrix.h"
#include "GuiModel.h"

/*
================
idGuiModel::idGuiModel
================
*/
idGuiModel::idGuiModel() : 
	m_surf( NULL ), 
	m_vertexBlock( 0 ),
	m_indexBlock( 0 ),
	m_vertexPointer( NULL ),
	m_indexPointer( NULL ), 
	m_numVerts( 0 ),
	m_numIndexes( 0 ) {

	// identity color for drawsurf register evaluation
	for ( int i = 0; i < MAX_ENTITY_SHADER_PARMS; i++ ) {
		m_shaderParms[i] = 1.0f;
	}
}

/*
================
idGuiModel::Clear

Begins collecting draw commands into surfaces
================
*/
void idGuiModel::Clear() {
	m_surfaces.SetNum( 0 );
	AdvanceSurf();
}

/*
================
idGuiModel::BeginFrame
================
*/
void idGuiModel::BeginFrame() {
	m_vertexBlock = vertexCache.AllocVertex( NULL, MAX_VERTS );
	m_indexBlock = vertexCache.AllocIndex( NULL, MAX_INDEXES );
	m_vertexPointer = (idDrawVert *)vertexCache.MappedVertexBuffer( m_vertexBlock );
	m_indexPointer = (triIndex_t *)vertexCache.MappedIndexBuffer( m_indexBlock );
	m_numVerts = 0;
	m_numIndexes = 0;
	Clear();
}

/*
================
EmitSurfaces

For full screen GUIs, we can add in per-surface stereoscopic depth effects
================
*/
void R_LinkDrawSurfToView( drawSurf_t * drawSurf, viewDef_t * viewDef );
void idGuiModel::EmitSurfaces( float modelMatrix[16], float modelViewMatrix[16], bool depthHack, bool linkAsEntity ) {

	viewEntity_t * guiSpace = (viewEntity_t *)renderSystem->ClearedFrameAlloc( sizeof( *guiSpace ), FRAME_ALLOC_VIEW_ENTITY );
	memcpy( guiSpace->modelMatrix, modelMatrix, sizeof( guiSpace->modelMatrix ) );
	memcpy( guiSpace->modelViewMatrix, modelViewMatrix, sizeof( guiSpace->modelViewMatrix ) );
	guiSpace->weaponDepthHack = depthHack;
	guiSpace->isGuiSurface = true;

	// If this is an in-game gui, we need to be able to find the matrix again for head mounted
	// display bypass matrix fixup.
	if ( linkAsEntity ) {
		guiSpace->next = tr.m_viewDef->viewEntitys;
		tr.m_viewDef->viewEntitys = guiSpace;
	}

	//---------------------------
	// make a tech5 renderMatrix
	//---------------------------
	idRenderMatrix viewMat;
	idRenderMatrix::Transpose( *(idRenderMatrix *)modelViewMatrix, viewMat );
	idRenderMatrix::Multiply( tr.m_viewDef->projectionRenderMatrix, viewMat, guiSpace->mvp );
	if ( depthHack ) {
		idRenderMatrix::ApplyDepthHack( guiSpace->mvp );
	}

	// add the surfaces to this view
	for ( int i = 0; i < m_surfaces.Num(); i++ ) {
		const guiModelSurface_t & guiSurf = m_surfaces[i];
		if ( guiSurf.numIndexes == 0 ) {
			continue;
		}

		const idMaterial * shader = guiSurf.material;
		drawSurf_t * drawSurf = (drawSurf_t *)renderSystem->FrameAlloc( sizeof( *drawSurf ), FRAME_ALLOC_DRAW_SURFACE );

		drawSurf->numIndexes = guiSurf.numIndexes;
		drawSurf->ambientCache = m_vertexBlock;
		// build a vertCacheHandle_t that points inside the allocated block
		drawSurf->indexCache = m_indexBlock + ( (int64)(guiSurf.firstIndex*sizeof(triIndex_t)) << VERTCACHE_OFFSET_SHIFT );
 		drawSurf->shadowCache = 0;
		drawSurf->jointCache = 0;
		drawSurf->frontEndGeo = NULL;
		drawSurf->space = guiSpace;
		drawSurf->material = shader;
		drawSurf->extraGLState = guiSurf.glState;
		drawSurf->scissorRect = tr.m_viewDef->scissor;
		drawSurf->sort = shader->GetSort();
		drawSurf->renderZFail = 0;
		// process the shader expressions for conditionals / color / texcoords
		const float	*constRegs = shader->ConstantRegisters();
		if ( constRegs ) {
			// shader only uses constant values
			drawSurf->shaderRegisters = constRegs;
		} else {
			float *regs = (float *)renderSystem->FrameAlloc( shader->GetNumRegisters() * sizeof( float ), FRAME_ALLOC_SHADER_REGISTER );
			drawSurf->shaderRegisters = regs;
			shader->EvaluateRegisters( regs, m_shaderParms, tr.m_viewDef->renderView.shaderParms, tr.m_viewDef->renderView.time[1] * 0.001f, NULL );
		}
		R_LinkDrawSurfToView( drawSurf, tr.m_viewDef );
	}
}

/*
====================
EmitToCurrentView
====================
*/
void idGuiModel::EmitToCurrentView( float modelMatrix[16], bool depthHack ) {
	float	modelViewMatrix[16];

	R_MatrixMultiply( modelMatrix, tr.m_viewDef->worldSpace.modelViewMatrix, modelViewMatrix );

	EmitSurfaces( modelMatrix, modelViewMatrix, depthHack, true /* link as entity */ );
}


/*
================
idGuiModel::EmitFullScreen

Creates a view that covers the screen and emit the surfaces
================
*/
viewDef_t * idGuiModel::EmitFullScreen() {

	if ( m_surfaces[0].numIndexes == 0 ) {
		return NULL;
	}

	SCOPED_PROFILE_EVENT( "Gui::EmitFullScreen" );

	viewDef_t * viewDef = (viewDef_t *)renderSystem->ClearedFrameAlloc( sizeof( *viewDef ), FRAME_ALLOC_VIEW_DEF );
	viewDef->is2Dgui = true;
	tr.GetCroppedViewport( &viewDef->viewport );

	viewDef->scissor.x1 = 0;
	viewDef->scissor.y1 = 0;
	viewDef->scissor.x2 = viewDef->viewport.x2 - viewDef->viewport.x1;
	viewDef->scissor.y2 = viewDef->viewport.y2 - viewDef->viewport.y1;

	viewDef->projectionMatrix[0*4+0] = 2.0f / SCREEN_WIDTH;
	viewDef->projectionMatrix[0*4+1] = 0.0f;
	viewDef->projectionMatrix[0*4+2] = 0.0f;
	viewDef->projectionMatrix[0*4+3] = 0.0f;

	viewDef->projectionMatrix[1*4+0] = 0.0f;
#if defined( ID_VULKAN )
	viewDef->projectionMatrix[1*4+1] = 2.0f / SCREEN_HEIGHT;
#else
	viewDef->projectionMatrix[1*4+1] = -2.0f / SCREEN_HEIGHT;
#endif
	viewDef->projectionMatrix[1*4+2] = 0.0f;
	viewDef->projectionMatrix[1*4+3] = 0.0f;

	viewDef->projectionMatrix[2*4+0] = 0.0f;
	viewDef->projectionMatrix[2*4+1] = 0.0f;
	viewDef->projectionMatrix[2*4+2] = -1.0f;
	viewDef->projectionMatrix[2*4+3] = 0.0f;

	viewDef->projectionMatrix[3*4+0] = -1.0f;
#if defined( ID_VULKAN)
	viewDef->projectionMatrix[3*4+1] = -1.0f;
#else
	viewDef->projectionMatrix[3*4+1] = 1.0f;
#endif
	viewDef->projectionMatrix[3*4+2] = 0.0f;
	viewDef->projectionMatrix[3*4+3] = 1.0f;

	// make a tech5 renderMatrix for faster culling
	idRenderMatrix::Transpose( *(idRenderMatrix *)viewDef->projectionMatrix, viewDef->projectionRenderMatrix );

	viewDef->worldSpace.modelMatrix[0*4+0] = 1.0f;
	viewDef->worldSpace.modelMatrix[1*4+1] = 1.0f;
	viewDef->worldSpace.modelMatrix[2*4+2] = 1.0f;
	viewDef->worldSpace.modelMatrix[3*4+3] = 1.0f;

	viewDef->worldSpace.modelViewMatrix[0*4+0] = 1.0f;
	viewDef->worldSpace.modelViewMatrix[1*4+1] = 1.0f;
	viewDef->worldSpace.modelViewMatrix[2*4+2] = 1.0f;
	viewDef->worldSpace.modelViewMatrix[3*4+3] = 1.0f;

	viewDef->maxDrawSurfs = m_surfaces.Num();
	viewDef->drawSurfs = (drawSurf_t **)renderSystem->FrameAlloc( viewDef->maxDrawSurfs * sizeof( viewDef->drawSurfs[0] ), FRAME_ALLOC_DRAW_SURFACE_POINTER );
	viewDef->numDrawSurfs = 0;

	viewDef_t * oldViewDef = tr.m_viewDef;
	tr.m_viewDef = viewDef;

	EmitSurfaces( viewDef->worldSpace.modelMatrix, viewDef->worldSpace.modelViewMatrix, 
		false /* depthHack */ , false /* link as entity */ );

	tr.m_viewDef = oldViewDef;

	return viewDef;
}

/*
=============
AdvanceSurf
=============
*/
void idGuiModel::AdvanceSurf() {
	guiModelSurface_t	s;

	if ( m_surfaces.Num() ) {
		s.material = m_surf->material;
		s.glState = m_surf->glState;
	} else {
		s.material = tr.defaultMaterial;
		s.glState = 0;
	}

	// advance indexes so the pointer to each surface will be 16 byte aligned
	m_numIndexes = ALIGN( m_numIndexes, 8 );

	s.numIndexes = 0;
	s.firstIndex = m_numIndexes;

	m_surfaces.Append( s );
	m_surf = &m_surfaces[ m_surfaces.Num() - 1 ];
}

/*
=============
AllocTris
=============
*/
idDrawVert * idGuiModel::AllocTris( int vertCount, const triIndex_t * tempIndexes, int indexCount, const idMaterial * material, const uint64 glState ) {
	if ( material == NULL ) {
		return NULL;
	}
	if ( m_numIndexes + indexCount > MAX_INDEXES ) {
		static int warningFrame = 0;
		if ( warningFrame != tr.frameCount ) {
			warningFrame = tr.frameCount;
			idLib::Warning( "idGuiModel::AllocTris: MAX_INDEXES exceeded" );
		}
		return NULL;
	}
	if ( m_numVerts + vertCount > MAX_VERTS ) {
		static int warningFrame = 0;
		if ( warningFrame != tr.frameCount ) {
			warningFrame = tr.frameCount;
			idLib::Warning( "idGuiModel::AllocTris: MAX_VERTS exceeded" );
		}
		return NULL;
	}

	// break the current surface if we are changing to a new material or we can't
	// fit the data into our allocated block
	if ( material != m_surf->material || glState != m_surf->glState ) {
		if ( m_surf->numIndexes ) {
			AdvanceSurf();
		}
		m_surf->material = material;
		m_surf->glState = glState;
	}

	int startVert = m_numVerts;
	int startIndex = m_numIndexes;

	m_numVerts += vertCount;
	m_numIndexes += indexCount;

	m_surf->numIndexes += indexCount;

	if ( ( startIndex & 1 ) || ( indexCount & 1 ) ) {
		// slow for write combined memory!
		// this should be very rare, since quads are always an even index count
		for ( int i = 0; i < indexCount; i++ ) {
			m_indexPointer[ startIndex + i ] = startVert + tempIndexes[i];
		}
	} else {
		for ( int i = 0; i < indexCount; i += 2 ) {
			WriteIndexPair( m_indexPointer + startIndex + i, startVert + tempIndexes[i], startVert + tempIndexes[i+1] );
		}
	}

	return m_vertexPointer + startVert;
}
