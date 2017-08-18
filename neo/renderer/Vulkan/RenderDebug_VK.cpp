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
#include "../../framework/precompiled.h"
#include "../RenderSystem_local.h"
#include "../RenderBackend.h"

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
=================
idRenderBackend::DBG_SimpleSurfaceSetup
=================
*/
void idRenderBackend::DBG_SimpleSurfaceSetup( const drawSurf_t * drawSurf ) {

}

/*
=================
idRenderBackend::DBG_SimpleWorldSetup
=================
*/
void idRenderBackend::DBG_SimpleWorldSetup() {

}

/*
=================
idRenderBackend::DBG_ShowDestinationAlpha
=================
*/
void idRenderBackend::DBG_ShowDestinationAlpha() {

}

/*
=================
idRenderBackend::DBG_ColorByStencilBuffer
=================
*/
void idRenderBackend::DBG_ColorByStencilBuffer() {

}

/*
=================
idRenderBackend::DBG_ShowOverdraw
=================
*/
void idRenderBackend::DBG_ShowOverdraw() {

}

/*
=================
idRenderBackend::DBG_ShowIntensity
=================
*/
void idRenderBackend::DBG_ShowIntensity() {

}

/*
=================
idRenderBackend::DBG_ShowDepthBuffer
=================
*/
void idRenderBackend::DBG_ShowDepthBuffer() {

}

/*
=================
idRenderBackend::DBG_ShowLightCount
=================
*/
void idRenderBackend::DBG_ShowLightCount() {

}

/*
=================
idRenderBackend::DBG_EnterWeaponDepthHack
=================
*/
void idRenderBackend::DBG_EnterWeaponDepthHack() {

}

/*
=================
idRenderBackend::DBG_EnterModelDepthHack
=================
*/
void idRenderBackend::DBG_EnterModelDepthHack( float depth ) {

}

/*
=================
idRenderBackend::DBG_LeaveDepthHack
=================
*/
void idRenderBackend::DBG_LeaveDepthHack() {

}

/*
=================
idRenderBackend::DBG_RenderDrawSurfListWithFunction
=================
*/
void idRenderBackend::DBG_RenderDrawSurfListWithFunction( drawSurf_t **drawSurfs, int numDrawSurfs ) {

}

/*
=================
idRenderBackend::DBG_ShowSilhouette
=================
*/
void idRenderBackend::DBG_ShowSilhouette() {

}

/*
=================
idRenderBackend::DBG_ShowTris
=================
*/
void idRenderBackend::DBG_ShowTris( drawSurf_t **drawSurfs, int numDrawSurfs ) {

}

/*
=================
idRenderBackend::DBG_ShowSurfaceInfo
=================
*/
void idRenderBackend::DBG_ShowSurfaceInfo( drawSurf_t **drawSurfs, int numDrawSurfs ) {

}

/*
=================
idRenderBackend::DBG_ShowViewEntitys
=================
*/
void idRenderBackend::DBG_ShowViewEntitys( viewEntity_t *vModels ) {

}

/*
=================
idRenderBackend::DBG_ShowTexturePolarity
=================
*/
void idRenderBackend::DBG_ShowTexturePolarity( drawSurf_t **drawSurfs, int numDrawSurfs ) {

}

/*
=================
idRenderBackend::DBG_ShowUnsmoothedTangents
=================
*/
void idRenderBackend::DBG_ShowUnsmoothedTangents( drawSurf_t **drawSurfs, int numDrawSurfs ) {

}

/*
=================
idRenderBackend::DBG_ShowTangentSpace
=================
*/
void idRenderBackend::DBG_ShowTangentSpace( drawSurf_t **drawSurfs, int numDrawSurfs ) {

}

/*
=================
idRenderBackend::DBG_ShowVertexColor
=================
*/
void idRenderBackend::DBG_ShowVertexColor( drawSurf_t **drawSurfs, int numDrawSurfs ) {

}

/*
=================
idRenderBackend::DBG_ShowNormals
=================
*/
void idRenderBackend::DBG_ShowNormals( drawSurf_t **drawSurfs, int numDrawSurfs ) {

}

/*
=================
idRenderBackend::DBG_ShowTextureVectors
=================
*/
void idRenderBackend::DBG_ShowTextureVectors( drawSurf_t **drawSurfs, int numDrawSurfs ) {

}

/*
=================
idRenderBackend::DBG_ShowDominantTris
=================
*/
void idRenderBackend::DBG_ShowDominantTris( drawSurf_t **drawSurfs, int numDrawSurfs ) {

}

/*
=================
idRenderBackend::DBG_ShowEdges
=================
*/
void idRenderBackend::DBG_ShowEdges( drawSurf_t **drawSurfs, int numDrawSurfs ) {

}

/*
=================
idRenderBackend::DBG_ShowLights
=================
*/
void idRenderBackend::DBG_ShowLights() {

}

/*
=================
idRenderBackend::DBG_ShowPortals
=================
*/
void idRenderBackend::DBG_ShowPortals() {

}

/*
=================
idRenderBackend::DBG_ShowDebugText
=================
*/
void idRenderBackend::DBG_ShowDebugText() {

}

/*
=================
idRenderBackend::DBG_ShowDebugLines
=================
*/
void idRenderBackend::DBG_ShowDebugLines() {

}

/*
=================
idRenderBackend::DBG_ShowDebugPolygons
=================
*/
void idRenderBackend::DBG_ShowDebugPolygons() {

}

/*
=================
idRenderBackend::DBG_ShowCenterOfProjection
=================
*/
void idRenderBackend::DBG_ShowCenterOfProjection() {

}

/*
=================
idRenderBackend::DBG_TestGamma
=================
*/
void idRenderBackend::DBG_TestGamma() {

}

/*
=================
idRenderBackend::DBG_TestGammaBias
=================
*/
void idRenderBackend::DBG_TestGammaBias() {

}

/*
=================
idRenderBackend::DBG_TestImage
=================
*/
void idRenderBackend::DBG_TestImage() {

}

/*
=================
idRenderBackend::DBG_ShowTrace
=================
*/
void idRenderBackend::DBG_ShowTrace( drawSurf_t **drawSurfs, int numDrawSurfs ) {

}

/*
=================
idRenderBackend::DBG_RenderDebugTools
=================
*/
void idRenderBackend::DBG_RenderDebugTools( drawSurf_t **drawSurfs, int numDrawSurfs ) {

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
	// TODO_VK
}