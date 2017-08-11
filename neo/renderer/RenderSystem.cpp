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
#include "RenderBackend.h"
#include "ResolutionScale.h"
#include "Font.h"
#include "GuiModel.h"
#include "Image.h"

#include "../sys/win32/win_local.h"

idRenderSystemLocal	tr;
idRenderSystem * renderSystem = &tr;

// DeviceContext bypasses RenderSystem to work directly with this
idGuiModel * tr_guiModel;

idCVar r_debugContext( "r_debugContext", "0", CVAR_RENDERER, "Enable various levels of context debug." );
idCVar r_multiSamples( "r_multiSamples", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "number of antialiasing samples" );
idCVar r_vidMode( "r_vidMode", "0", CVAR_ARCHIVE | CVAR_RENDERER | CVAR_INTEGER, "fullscreen video mode number" );
idCVar r_displayRefresh( "r_displayRefresh", "0", CVAR_RENDERER | CVAR_INTEGER | CVAR_NOCHEAT, "optional display refresh rate option for vid mode", 0.0f, 240.0f );
idCVar r_fullscreen( "r_fullscreen", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "0 = windowed, 1 = full screen on monitor 1, 2 = full screen on monitor 2, etc" );
idCVar r_customWidth( "r_customWidth", "1280", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "custom screen width. set r_vidMode to -1 to activate" );
idCVar r_customHeight( "r_customHeight", "720", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "custom screen height. set r_vidMode to -1 to activate" );
idCVar r_windowX( "r_windowX", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "Non-fullscreen parameter" );
idCVar r_windowY( "r_windowY", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "Non-fullscreen parameter" );
idCVar r_windowWidth( "r_windowWidth", "1280", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "Non-fullscreen parameter" );
idCVar r_windowHeight( "r_windowHeight", "720", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "Non-fullscreen parameter" );

idCVar r_useViewBypass( "r_useViewBypass", "1", CVAR_RENDERER | CVAR_INTEGER, "bypass a frame of latency to the view" );
idCVar r_useLightPortalFlow( "r_useLightPortalFlow", "1", CVAR_RENDERER | CVAR_BOOL, "use a more precise area reference determination" );
idCVar r_singleTriangle( "r_singleTriangle", "0", CVAR_RENDERER | CVAR_BOOL, "only draw a single triangle per primitive" );
idCVar r_checkBounds( "r_checkBounds", "0", CVAR_RENDERER | CVAR_BOOL, "compare all surface bounds with precalculated ones" );
idCVar r_useConstantMaterials( "r_useConstantMaterials", "1", CVAR_RENDERER | CVAR_BOOL, "use pre-calculated material registers if possible" );
idCVar r_useSilRemap( "r_useSilRemap", "1", CVAR_RENDERER | CVAR_BOOL, "consider verts with the same XYZ, but different ST the same for shadows" );
idCVar r_useNodeCommonChildren( "r_useNodeCommonChildren", "1", CVAR_RENDERER | CVAR_BOOL, "stop pushing reference bounds early when possible" );
idCVar r_useShadowSurfaceScissor( "r_useShadowSurfaceScissor", "1", CVAR_RENDERER | CVAR_BOOL, "scissor shadows by the scissor rect of the interaction surfaces" );
idCVar r_useCachedDynamicModels( "r_useCachedDynamicModels", "1", CVAR_RENDERER | CVAR_BOOL, "cache snapshots of dynamic models" );
idCVar r_maxAnisotropicFiltering( "r_maxAnisotropicFiltering", "8", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "limit aniso filtering" );
idCVar r_useTrilinearFiltering( "r_useTrilinearFiltering", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "Extra quality filtering" );
idCVar r_lodBias( "r_lodBias", "0.5", CVAR_RENDERER | CVAR_ARCHIVE, "image lod bias" );

idCVar r_znear( "r_znear", "3", CVAR_RENDERER | CVAR_FLOAT, "near Z clip plane distance", 0.001f, 200.0f );

idCVar r_ignoreGLErrors( "r_ignoreGLErrors", "1", CVAR_RENDERER | CVAR_BOOL, "ignore GL errors" );
idCVar r_swapInterval( "r_swapInterval", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "0 = tear, 1 = swap-tear where available, 2 = always v-sync" );

idCVar r_gamma( "r_gamma", "1.0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_FLOAT, "changes gamma tables", 0.5f, 3.0f );
idCVar r_brightness( "r_brightness", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_FLOAT, "changes gamma tables", 0.5f, 2.0f );

idCVar r_skipStaticInteractions( "r_skipStaticInteractions", "0", CVAR_RENDERER | CVAR_BOOL, "skip interactions created at level load" );
idCVar r_skipDynamicInteractions( "r_skipDynamicInteractions", "0", CVAR_RENDERER | CVAR_BOOL, "skip interactions created after level load" );
idCVar r_skipSuppress( "r_skipSuppress", "0", CVAR_RENDERER | CVAR_BOOL, "ignore the per-view suppressions" );
idCVar r_skipPostProcess( "r_skipPostProcess", "0", CVAR_RENDERER | CVAR_BOOL, "skip all post-process renderings" );
idCVar r_skipInteractions( "r_skipInteractions", "0", CVAR_RENDERER | CVAR_BOOL, "skip all light/surface interaction drawing" );
idCVar r_skipDynamicTextures( "r_skipDynamicTextures", "0", CVAR_RENDERER | CVAR_BOOL, "don't dynamically create textures" );
idCVar r_skipCopyTexture( "r_skipCopyTexture", "0", CVAR_RENDERER | CVAR_BOOL, "do all rendering, but don't actually copyTexSubImage2D" );
idCVar r_skipBackEnd( "r_skipBackEnd", "0", CVAR_RENDERER | CVAR_BOOL, "don't draw anything" );
idCVar r_skipRender( "r_skipRender", "0", CVAR_RENDERER | CVAR_BOOL, "skip 3D rendering, but pass 2D" );
idCVar r_skipTranslucent( "r_skipTranslucent", "0", CVAR_RENDERER | CVAR_BOOL, "skip the translucent interaction rendering" );
idCVar r_skipAmbient( "r_skipAmbient", "0", CVAR_RENDERER | CVAR_BOOL, "bypasses all non-interaction drawing" );
idCVar r_skipNewAmbient( "r_skipNewAmbient", "0", CVAR_RENDERER | CVAR_BOOL | CVAR_ARCHIVE, "bypasses all vertex/fragment program ambient drawing" );
idCVar r_skipBlendLights( "r_skipBlendLights", "0", CVAR_RENDERER | CVAR_BOOL, "skip all blend lights" );
idCVar r_skipFogLights( "r_skipFogLights", "0", CVAR_RENDERER | CVAR_BOOL, "skip all fog lights" );
idCVar r_skipDeforms( "r_skipDeforms", "0", CVAR_RENDERER | CVAR_BOOL, "leave all deform materials in their original state" );
idCVar r_skipFrontEnd( "r_skipFrontEnd", "0", CVAR_RENDERER | CVAR_BOOL, "bypasses all front end work, but 2D gui rendering still draws" );
idCVar r_skipUpdates( "r_skipUpdates", "0", CVAR_RENDERER | CVAR_BOOL, "1 = don't accept any entity or light updates, making everything static" );
idCVar r_skipDecals( "r_skipDecals", "0", CVAR_RENDERER | CVAR_BOOL, "skip decal surfaces" );
idCVar r_skipOverlays( "r_skipOverlays", "0", CVAR_RENDERER | CVAR_BOOL, "skip overlay surfaces" );
idCVar r_skipSpecular( "r_skipSpecular", "0", CVAR_RENDERER | CVAR_BOOL | CVAR_CHEAT | CVAR_ARCHIVE, "use black for specular1" );
idCVar r_skipBump( "r_skipBump", "0", CVAR_RENDERER | CVAR_BOOL | CVAR_ARCHIVE, "uses a flat surface instead of the bump map" );
idCVar r_skipDiffuse( "r_skipDiffuse", "0", CVAR_RENDERER | CVAR_BOOL, "use black for diffuse" );
idCVar r_skipSubviews( "r_skipSubviews", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = don't render any gui elements on surfaces" );
idCVar r_skipGuiShaders( "r_skipGuiShaders", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = skip all gui elements on surfaces, 2 = skip drawing but still handle events, 3 = draw but skip events", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );
idCVar r_skipParticles( "r_skipParticles", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = skip all particle systems", 0, 1, idCmdSystem::ArgCompletion_Integer<0,1> );
idCVar r_skipShadows( "r_skipShadows", "0", CVAR_RENDERER | CVAR_BOOL  | CVAR_ARCHIVE, "disable shadows" );

idCVar r_useLightPortalCulling( "r_useLightPortalCulling", "1", CVAR_RENDERER | CVAR_INTEGER, "0 = none, 1 = cull frustum corners to plane, 2 = exact clip the frustum faces", 0, 2, idCmdSystem::ArgCompletion_Integer<0,2> );
idCVar r_useLightAreaCulling( "r_useLightAreaCulling", "1", CVAR_RENDERER | CVAR_BOOL, "0 = off, 1 = on" );
idCVar r_useLightScissors( "r_useLightScissors", "3", CVAR_RENDERER | CVAR_INTEGER, "0 = no scissor, 1 = non-clipped scissor, 2 = near-clipped scissor, 3 = fully-clipped scissor", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );
idCVar r_useEntityPortalCulling( "r_useEntityPortalCulling", "1", CVAR_RENDERER | CVAR_INTEGER, "0 = none, 1 = cull frustum corners to plane, 2 = exact clip the frustum faces", 0, 2, idCmdSystem::ArgCompletion_Integer<0,2> );
idCVar r_logFile( "r_logFile", "0", CVAR_RENDERER | CVAR_INTEGER, "number of frames to emit GL logs" );
idCVar r_clear( "r_clear", "2", CVAR_RENDERER, "force screen clear every frame, 1 = purple, 2 = black, 'r g b' = custom" );

idCVar r_offsetFactor( "r_offsetfactor", "0", CVAR_RENDERER | CVAR_FLOAT, "polygon offset parameter" );
idCVar r_offsetUnits( "r_offsetunits", "-600", CVAR_RENDERER | CVAR_FLOAT, "polygon offset parameter" );

idCVar r_shadowPolygonOffset( "r_shadowPolygonOffset", "-1", CVAR_RENDERER | CVAR_FLOAT, "bias value added to depth test for stencil shadow drawing" );
idCVar r_shadowPolygonFactor( "r_shadowPolygonFactor", "0", CVAR_RENDERER | CVAR_FLOAT, "scale value for stencil shadow drawing" );
idCVar r_subviewOnly( "r_subviewOnly", "0", CVAR_RENDERER | CVAR_BOOL, "1 = don't render main view, allowing subviews to be debugged" );
idCVar r_testGamma( "r_testGamma", "0", CVAR_RENDERER | CVAR_FLOAT, "if > 0 draw a grid pattern to test gamma levels", 0, 195 );
idCVar r_testGammaBias( "r_testGammaBias", "0", CVAR_RENDERER | CVAR_FLOAT, "if > 0 draw a grid pattern to test gamma levels" );
idCVar r_lightScale( "r_lightScale", "3", CVAR_ARCHIVE | CVAR_RENDERER | CVAR_FLOAT, "all light intensities are multiplied by this" );
idCVar r_flareSize( "r_flareSize", "1", CVAR_RENDERER | CVAR_FLOAT, "scale the flare deforms from the material def" ); 

idCVar r_skipPrelightShadows( "r_skipPrelightShadows", "0", CVAR_RENDERER | CVAR_BOOL, "skip the dmap generated static shadow volumes" );
idCVar r_useScissor( "r_useScissor", "1", CVAR_RENDERER | CVAR_BOOL, "scissor clip as portals and lights are processed" );
idCVar r_useLightDepthBounds( "r_useLightDepthBounds", "1", CVAR_RENDERER | CVAR_BOOL, "use depth bounds test on lights to reduce both shadow and interaction fill" );
idCVar r_useShadowDepthBounds( "r_useShadowDepthBounds", "1", CVAR_RENDERER | CVAR_BOOL, "use depth bounds test on individual shadow volumes to reduce shadow fill" );

idCVar r_screenFraction( "r_screenFraction", "100", CVAR_RENDERER | CVAR_INTEGER, "for testing fill rate, the resolution of the entire screen can be changed" );
idCVar r_usePortals( "r_usePortals", "1", CVAR_RENDERER | CVAR_BOOL, " 1 = use portals to perform area culling, otherwise draw everything" );
idCVar r_singleLight( "r_singleLight", "-1", CVAR_RENDERER | CVAR_INTEGER, "suppress all but one light" );
idCVar r_singleEntity( "r_singleEntity", "-1", CVAR_RENDERER | CVAR_INTEGER, "suppress all but one entity" );
idCVar r_singleSurface( "r_singleSurface", "-1", CVAR_RENDERER | CVAR_INTEGER, "suppress all but one surface on each entity" );
idCVar r_singleArea( "r_singleArea", "0", CVAR_RENDERER | CVAR_BOOL, "only draw the portal area the view is actually in" );
idCVar r_orderIndexes( "r_orderIndexes", "1", CVAR_RENDERER | CVAR_BOOL, "perform index reorganization to optimize vertex use" );
idCVar r_lightAllBackFaces( "r_lightAllBackFaces", "0", CVAR_RENDERER | CVAR_BOOL, "light all the back faces, even when they would be shadowed" );

// visual debugging info
idCVar r_showPortals( "r_showPortals", "0", CVAR_RENDERER | CVAR_BOOL, "draw portal outlines in color based on passed / not passed" );
idCVar r_showUnsmoothedTangents( "r_showUnsmoothedTangents", "0", CVAR_RENDERER | CVAR_BOOL, "if 1, put all nvidia register combiner programming in display lists" );
idCVar r_showSilhouette( "r_showSilhouette", "0", CVAR_RENDERER | CVAR_BOOL, "highlight edges that are casting shadow planes" );
idCVar r_showVertexColor( "r_showVertexColor", "0", CVAR_RENDERER | CVAR_BOOL, "draws all triangles with the solid vertex color" );
idCVar r_showUpdates( "r_showUpdates", "0", CVAR_RENDERER | CVAR_BOOL, "report entity and light updates and ref counts" );
idCVar r_showDemo( "r_showDemo", "0", CVAR_RENDERER | CVAR_BOOL, "report reads and writes to the demo file" );
idCVar r_showDynamic( "r_showDynamic", "0", CVAR_RENDERER | CVAR_BOOL, "report stats on dynamic surface generation" );
idCVar r_showTrace( "r_showTrace", "0", CVAR_RENDERER | CVAR_INTEGER, "show the intersection of an eye trace with the world", idCmdSystem::ArgCompletion_Integer<0,2> );
idCVar r_showIntensity( "r_showIntensity", "0", CVAR_RENDERER | CVAR_BOOL, "draw the screen colors based on intensity, red = 0, green = 128, blue = 255" );
idCVar r_showLights( "r_showLights", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = just print volumes numbers, highlighting ones covering the view, 2 = also draw planes of each volume, 3 = also draw edges of each volume", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );
idCVar r_showShadows( "r_showShadows", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = visualize the stencil shadow volumes, 2 = draw filled in", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );
idCVar r_showLightScissors( "r_showLightScissors", "0", CVAR_RENDERER | CVAR_BOOL, "show light scissor rectangles" );
idCVar r_showLightCount( "r_showLightCount", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = colors surfaces based on light count, 2 = also count everything through walls, 3 = also print overdraw", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );
idCVar r_showViewEntitys( "r_showViewEntitys", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = displays the bounding boxes of all view models, 2 = print index numbers" );
idCVar r_showTris( "r_showTris", "0", CVAR_RENDERER | CVAR_INTEGER, "enables wireframe rendering of the world, 1 = only draw visible ones, 2 = draw all front facing, 3 = draw all, 4 = draw with alpha", 0, 4, idCmdSystem::ArgCompletion_Integer<0,4> );
idCVar r_showSurfaceInfo( "r_showSurfaceInfo", "0", CVAR_RENDERER | CVAR_BOOL, "show surface material name under crosshair" );
idCVar r_showNormals( "r_showNormals", "0", CVAR_RENDERER | CVAR_FLOAT, "draws wireframe normals" );
idCVar r_showMemory( "r_showMemory", "0", CVAR_RENDERER | CVAR_BOOL, "print frame memory utilization" );
idCVar r_showCull( "r_showCull", "0", CVAR_RENDERER | CVAR_BOOL, "report sphere and box culling stats" );
idCVar r_showAddModel( "r_showAddModel", "0", CVAR_RENDERER | CVAR_BOOL, "report stats from tr_addModel" );
idCVar r_showDepth( "r_showDepth", "0", CVAR_RENDERER | CVAR_BOOL, "display the contents of the depth buffer and the depth range" );
idCVar r_showSurfaces( "r_showSurfaces", "0", CVAR_RENDERER | CVAR_BOOL, "report surface/light/shadow counts" );
idCVar r_showPrimitives( "r_showPrimitives", "0", CVAR_RENDERER | CVAR_INTEGER, "report drawsurf/index/vertex counts" );
idCVar r_showEdges( "r_showEdges", "0", CVAR_RENDERER | CVAR_BOOL, "draw the sil edges" );
idCVar r_showTexturePolarity( "r_showTexturePolarity", "0", CVAR_RENDERER | CVAR_BOOL, "shade triangles by texture area polarity" );
idCVar r_showTangentSpace( "r_showTangentSpace", "0", CVAR_RENDERER | CVAR_INTEGER, "shade triangles by tangent space, 1 = use 1st tangent vector, 2 = use 2nd tangent vector, 3 = use normal vector", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );
idCVar r_showDominantTri( "r_showDominantTri", "0", CVAR_RENDERER | CVAR_BOOL, "draw lines from vertexes to center of dominant triangles" );
idCVar r_showTextureVectors( "r_showTextureVectors", "0", CVAR_RENDERER | CVAR_FLOAT, " if > 0 draw each triangles texture (tangent) vectors" );
idCVar r_showOverDraw( "r_showOverDraw", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = geometry overdraw, 2 = light interaction overdraw, 3 = geometry and light interaction overdraw", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );

idCVar r_useEntityCallbacks( "r_useEntityCallbacks", "1", CVAR_RENDERER | CVAR_BOOL, "if 0, issue the callback immediately at update time, rather than defering" );

idCVar r_showSkel( "r_showSkel", "0", CVAR_RENDERER | CVAR_INTEGER, "draw the skeleton when model animates, 1 = draw model with skeleton, 2 = draw skeleton only", 0, 2, idCmdSystem::ArgCompletion_Integer<0,2> );
idCVar r_jointNameScale( "r_jointNameScale", "0.02", CVAR_RENDERER | CVAR_FLOAT, "size of joint names when r_showskel is set to 1" );
idCVar r_jointNameOffset( "r_jointNameOffset", "0.5", CVAR_RENDERER | CVAR_FLOAT, "offset of joint names when r_showskel is set to 1" );

idCVar r_debugLineDepthTest( "r_debugLineDepthTest", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "perform depth test on debug lines" );
idCVar r_debugLineWidth( "r_debugLineWidth", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "width of debug lines" );
idCVar r_debugArrowStep( "r_debugArrowStep", "120", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "step size of arrow cone line rotation in degrees", 0, 120 );
idCVar r_debugPolygonFilled( "r_debugPolygonFilled", "1", CVAR_RENDERER | CVAR_BOOL, "draw a filled polygon" );

idCVar r_materialOverride( "r_materialOverride", "", CVAR_RENDERER, "overrides all materials", idCmdSystem::ArgCompletion_Decl<DECL_MATERIAL> );

//#define TRACK_FRAME_ALLOCS

static const unsigned int FRAME_ALLOC_ALIGNMENT = 128;
static const unsigned int MAX_FRAME_MEMORY = 64 * 1024 * 1024;	// larger so that we can noclip on PC for dev purposes

#if defined( TRACK_FRAME_ALLOCS )
idSysInterlockedInteger frameAllocTypeCount[ FRAME_ALLOC_MAX ];
int frameHighWaterTypeCount[ FRAME_ALLOC_MAX ];
#endif


/*
=============
R_MakeFullScreenTris
=============
*/
static srfTriangles_t * R_MakeFullScreenTris() {
	// copy verts and indexes
	srfTriangles_t * tri = (srfTriangles_t *)Mem_ClearedAlloc( sizeof( *tri ), TAG_RENDER_TOOLS );

	tri->numIndexes = 6;
	tri->numVerts = 4;

	int indexSize = tri->numIndexes * sizeof( tri->indexes[0] );
	int allocatedIndexBytes = ALIGN( indexSize, 16 );
	tri->indexes = (triIndex_t *)Mem_Alloc( allocatedIndexBytes, TAG_RENDER_TOOLS );

	int vertexSize = tri->numVerts * sizeof( tri->verts[0] );
	int allocatedVertexBytes =  ALIGN( vertexSize, 16 );
	tri->verts = (idDrawVert *)Mem_ClearedAlloc( allocatedVertexBytes, TAG_RENDER_TOOLS );

	idDrawVert * verts = tri->verts;

	triIndex_t tempIndexes[6] = { 3, 0, 2, 2, 0, 1 };
	memcpy( tri->indexes, tempIndexes, indexSize );
	
	verts[0].xyz[0] = -1.0f;
	verts[0].xyz[1] = 1.0f;
	verts[0].SetTexCoord( 0.0f, 1.0f );

	verts[1].xyz[0] = 1.0f;
	verts[1].xyz[1] = 1.0f;
	verts[1].SetTexCoord( 1.0f, 1.0f );

	verts[2].xyz[0] = 1.0f;
	verts[2].xyz[1] = -1.0f;
	verts[2].SetTexCoord( 1.0f, 0.0f );

	verts[3].xyz[0] = -1.0f;
	verts[3].xyz[1] = -1.0f;
	verts[3].SetTexCoord( 0.0f, 0.0f );

	for ( int i = 0 ; i < 4 ; i++ ) {
		verts[i].SetColor( 0xffffffff );
	}

	return tri;
}

/*
=============
R_MakeZeroOneCubeTris
=============
*/
static srfTriangles_t * R_MakeZeroOneCubeTris() {
	srfTriangles_t * tri = (srfTriangles_t *)Mem_ClearedAlloc( sizeof( *tri ), TAG_RENDER_TOOLS );

	tri->numVerts = 8;
	tri->numIndexes = 36;

	const int indexSize = tri->numIndexes * sizeof( tri->indexes[0] );
	const int allocatedIndexBytes = ALIGN( indexSize, 16 );
	tri->indexes = (triIndex_t *)Mem_Alloc( allocatedIndexBytes, TAG_RENDER_TOOLS );

	const int vertexSize = tri->numVerts * sizeof( tri->verts[0] );
	const int allocatedVertexBytes =  ALIGN( vertexSize, 16 );
	tri->verts = (idDrawVert *)Mem_ClearedAlloc( allocatedVertexBytes, TAG_RENDER_TOOLS );

	idDrawVert * verts = tri->verts;

	const float low = 0.0f;
	const float high = 1.0f;

	idVec3 center( 0.0f );
	idVec3 mx(  low, 0.0f, 0.0f );
	idVec3 px( high, 0.0f, 0.0f );
	idVec3 my( 0.0f,  low, 0.0f );
	idVec3 py( 0.0f, high, 0.0f );
	idVec3 mz( 0.0f, 0.0f,  low );
	idVec3 pz( 0.0f, 0.0f, high );

	verts[0].xyz = center + mx + my + mz;
	verts[1].xyz = center + px + my + mz;
	verts[2].xyz = center + px + py + mz;
	verts[3].xyz = center + mx + py + mz;
	verts[4].xyz = center + mx + my + pz;
	verts[5].xyz = center + px + my + pz;
	verts[6].xyz = center + px + py + pz;
	verts[7].xyz = center + mx + py + pz;

	// bottom
	tri->indexes[ 0*3+0] = 2;
	tri->indexes[ 0*3+1] = 3;
	tri->indexes[ 0*3+2] = 0;
	tri->indexes[ 1*3+0] = 1;
	tri->indexes[ 1*3+1] = 2;
	tri->indexes[ 1*3+2] = 0;
	// back
	tri->indexes[ 2*3+0] = 5;
	tri->indexes[ 2*3+1] = 1;
	tri->indexes[ 2*3+2] = 0;
	tri->indexes[ 3*3+0] = 4;
	tri->indexes[ 3*3+1] = 5;
	tri->indexes[ 3*3+2] = 0;
	// left
	tri->indexes[ 4*3+0] = 7;
	tri->indexes[ 4*3+1] = 4;
	tri->indexes[ 4*3+2] = 0;
	tri->indexes[ 5*3+0] = 3;
	tri->indexes[ 5*3+1] = 7;
	tri->indexes[ 5*3+2] = 0;
	// right
	tri->indexes[ 6*3+0] = 1;
	tri->indexes[ 6*3+1] = 5;
	tri->indexes[ 6*3+2] = 6;
	tri->indexes[ 7*3+0] = 2;
	tri->indexes[ 7*3+1] = 1;
	tri->indexes[ 7*3+2] = 6;
	// front
	tri->indexes[ 8*3+0] = 3;
	tri->indexes[ 8*3+1] = 2;
	tri->indexes[ 8*3+2] = 6;
	tri->indexes[ 9*3+0] = 7;
	tri->indexes[ 9*3+1] = 3;
	tri->indexes[ 9*3+2] = 6;
	// top
	tri->indexes[10*3+0] = 4;
	tri->indexes[10*3+1] = 7;
	tri->indexes[10*3+2] = 6;
	tri->indexes[11*3+0] = 5;
	tri->indexes[11*3+1] = 4;
	tri->indexes[11*3+2] = 6;

	for ( int i = 0 ; i < 4 ; i++ ) {
		verts[i].SetColor( 0xffffffff );
	}

	return tri;
}

/*
================
R_MakeTestImageTriangles

Initializes the Test Image Triangles
================
*/
srfTriangles_t* R_MakeTestImageTriangles() {
	srfTriangles_t * tri = (srfTriangles_t *)Mem_ClearedAlloc( sizeof( *tri ), TAG_RENDER_TOOLS );

	tri->numIndexes = 6;
	tri->numVerts = 4;

	int indexSize = tri->numIndexes * sizeof( tri->indexes[0] );
	int allocatedIndexBytes = ALIGN( indexSize, 16 );
	tri->indexes = (triIndex_t *)Mem_Alloc( allocatedIndexBytes, TAG_RENDER_TOOLS );

	int vertexSize = tri->numVerts * sizeof( tri->verts[0] );
	int allocatedVertexBytes =  ALIGN( vertexSize, 16 );
	tri->verts = (idDrawVert *)Mem_ClearedAlloc( allocatedVertexBytes, TAG_RENDER_TOOLS );

	ALIGNTYPE16 triIndex_t tempIndexes[6] = { 3, 0, 2, 2, 0, 1 };
	memcpy( tri->indexes, tempIndexes, indexSize );

	idDrawVert* tempVerts = tri->verts;
	tempVerts[0].xyz[0] = 0.0f;
	tempVerts[0].xyz[1] = 0.0f;
	tempVerts[0].xyz[2] = 0;
	tempVerts[0].SetTexCoord( 0.0, 0.0f );

	tempVerts[1].xyz[0] = 1.0f;
	tempVerts[1].xyz[1] = 0.0f;
	tempVerts[1].xyz[2] = 0;
	tempVerts[1].SetTexCoord( 1.0f, 0.0f );

	tempVerts[2].xyz[0] = 1.0f;
	tempVerts[2].xyz[1] = 1.0f;
	tempVerts[2].xyz[2] = 0;
	tempVerts[2].SetTexCoord( 1.0f, 1.0f );

	tempVerts[3].xyz[0] = 0.0f;
	tempVerts[3].xyz[1] = 1.0f;
	tempVerts[3].xyz[2] = 0;
	tempVerts[3].SetTexCoord( 0.0f, 1.0f );

	for ( int i = 0; i < 4; i++ ) {
		tempVerts[i].SetColor( 0xFFFFFFFF );
	}
	return tri;
}

/*
=============================
SetNewMode

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
bool R_GetModeListForDisplay( const int requestedDisplayNum, idList<vidMode_t> & modeList );
bool SetScreenParms( gfxImpParms_t parms );
static void SetNewMode() {
	// try up to three different configurations

	for ( int i = 0 ; i < 3 ; i++ ) {
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
			if ( !R_GetModeListForDisplay( r_fullscreen.GetInteger() - 1, modeList ) ) {
				idLib::Printf( "r_fullscreen reset from %i to 1 because mode list failed.", r_fullscreen.GetInteger() );
				r_fullscreen.SetInteger( 1 );
				R_GetModeListForDisplay( r_fullscreen.GetInteger() - 1, modeList );
			}
			if ( modeList.Num() < 1 ) {
				idLib::Printf( "Going to safe mode because mode list failed." );
				goto safeMode;
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

		// rebuild the window
		if ( SetScreenParms( parms ) ) {
			// it worked
			break;
		}

		if ( i == 2 ) {
			idLib::FatalError( "Unable to initialize new mode." );
		}

		if ( i == 0 ) {
			// same settings, no stereo
			continue;
		}

safeMode:
		// if we failed, set everything back to "safe mode"
		// and try again
		r_vidMode.SetInteger( 0 );
		r_fullscreen.SetInteger( 1 );
		r_displayRefresh.SetInteger( 0 );
		r_multiSamples.SetInteger( 0 );
	}
}

/*
=============
idRenderSystemLocal::idRenderSystemLocal
=============
*/
idRenderSystemLocal::idRenderSystemLocal() :
	m_bInitialized( false ),
	m_unitSquareTriangles( NULL ),
	m_zeroOneCubeTriangles( NULL ),
	m_testImageTriangles( NULL ) {

	Clear();
}

/*
=============
idRenderSystemLocal::~idRenderSystemLocal
=============
*/
idRenderSystemLocal::~idRenderSystemLocal() {

}

/*
===============
idRenderSystemLocal::Init
===============
*/
void idRenderSystemLocal::Init() {	
	if ( m_bInitialized ) {
		idLib::Warning( "RenderSystem already initialized." );
		return;
	}

	idLib::Printf( "------- Initializing renderSystem --------\n" );

	InitFrameData();

	// Start Renderer Backend [ OpenGL, Vulkan, etc ]
	m_backend.Init();

	// clear all our internal state
	viewCount = 1;		// so cleared structures never match viewCount
	// we used to memset tr, but now that it is a class, we can't, so
	// there may be other state we need to reset

	m_guiModel = new (TAG_RENDER) idGuiModel;
	m_guiModel->Clear();
	tr_guiModel = m_guiModel;	// for DeviceContext fast path

	globalImages->Init();

	idCinematic::InitCinematic();

	InitMaterials();

	renderModelManager->Init();

	// make sure the m_unitSquareTriangles data is current in the vertex / index cache
	if ( m_unitSquareTriangles == NULL ) {
		m_unitSquareTriangles = R_MakeFullScreenTris();
	}
	// make sure the zeroOneCubeTriangles data is current in the vertex / index cache
	if ( m_zeroOneCubeTriangles == NULL ) {
		m_zeroOneCubeTriangles = R_MakeZeroOneCubeTris();
	}
	// make sure the m_testImageTriangles data is current in the vertex / index cache
	if ( m_testImageTriangles == NULL )  {
		m_testImageTriangles = R_MakeTestImageTriangles();
	}

	m_frontEndJobList = parallelJobManager->AllocJobList( JOBLIST_RENDERER_FRONTEND, JOBLIST_PRIORITY_MEDIUM, 2048, 0, NULL );

	m_bInitialized = true;

	// make sure the command buffers are ready to accept the first screen update
	SwapCommandBuffers( NULL, NULL, NULL, NULL );

	idLib::Printf( "renderSystem initialized.\n" );
	idLib::Printf( "--------------------------------------\n" );
}

/*
===============
idRenderSystemLocal::Shutdown
===============
*/
void RB_ShutdownDebugTools();
void idRenderSystemLocal::Shutdown() {	
	idLib::Printf( "idRenderSystem::Shutdown()\n" );

	m_fonts.DeleteContents();

	if ( m_bInitialized ) {
		globalImages->PurgeAllImages();
	}

	renderModelManager->Shutdown();

	idCinematic::ShutdownCinematic();

	globalImages->Shutdown();

	// free frame memory
	ShutdownFrameData();

	UnbindBufferObjects();

	// free the vertex cache, which should have nothing allocated now
	vertexCache.Shutdown();

	RB_ShutdownDebugTools();

	delete m_guiModel;
	m_guiModel = NULL;

	parallelJobManager->FreeJobList( m_frontEndJobList );

	m_backend.Shutdown();

	Clear();

	// free the context and close the window
	ShutdownFrameData();
	
	m_bInitialized = false;
}

/*
=================
idRenderSystemLocal::VidRestart
=================
*/
void idRenderSystemLocal::VidRestart() {
	// if OpenGL isn't started, do nothing
	if ( !m_bInitialized ) {
		return;
	}

	// set the mode without re-initializing the context
	SetNewMode();

#if 0
	// this could take a while, so give them the cursor back ASAP
	Sys_GrabMouseCursor( false );

	// dump ambient caches
	renderModelManager->FreeModelVertexCaches();

	// free any current world interaction surfaces and vertex caches
	FreeWorldDerivedData();

	// make sure the defered frees are actually freed
	ToggleSmpFrame();
	ToggleSmpFrame();

	// free the vertex caches so they will be regenerated again
	vertexCache.PurgeAll();

	// sound and input are tied to the window we are about to destroy

	// free all of our texture numbers
	Sys_ShutdownInput();
	globalImages->PurgeAllImages();
	// free the context and close the window
	Shutdown();

	// create the new context and vertex cache
	Init();

	// regenerate all images
	globalImages->ReloadImages( true );

	// make sure the regeneration doesn't use anything no longer valid
	viewCount++;
	m_viewDef = NULL;
#endif
}

/*
=================
idRenderSystemLocal::InitMaterials
=================
*/
void idRenderSystemLocal::InitMaterials() {
	defaultMaterial = declManager->FindMaterial( "_default", false );
	if ( !defaultMaterial ) {
		common->FatalError( "_default material not found" );
	}
	defaultPointLight = declManager->FindMaterial( "lights/defaultPointLight" );
	defaultProjectedLight = declManager->FindMaterial( "lights/defaultProjectedLight" );
	whiteMaterial = declManager->FindMaterial( "_white" );
	charSetMaterial = declManager->FindMaterial( "textures/bigchars" );
}

/*
===============
idRenderSystemLocal::Clear
===============
*/
void idRenderSystemLocal::Clear() {
	frameCount = 0;
	viewCount = 0;
	m_worlds.Clear();
	primaryWorld = NULL;
	memset( &m_primaryRenderView, 0, sizeof( m_primaryRenderView ) );
	primaryView = NULL;
	defaultMaterial = NULL;
	m_viewDef = NULL;
	memset( &pc, 0, sizeof( pc ) );
	memset( m_renderCrops, 0, sizeof( m_renderCrops ) );
	m_currentRenderCrop = 0;
	m_currentColorNativeBytesOrder = 0xFFFFFFFF;
	m_currentGLState = 0;
	m_guiRecursionLevel = 0;
	m_guiModel = NULL;
	m_takingScreenshot = false;
	memset( &m_smpFrameData, 0, sizeof( m_frameData ) );

	if ( m_unitSquareTriangles != NULL ) {
		Mem_Free( m_unitSquareTriangles );
		m_unitSquareTriangles = NULL;
	}

	if ( m_zeroOneCubeTriangles != NULL ) {
		Mem_Free( m_zeroOneCubeTriangles );
		m_zeroOneCubeTriangles = NULL;
	}

	if ( m_testImageTriangles != NULL ) {
		Mem_Free( m_testImageTriangles );
		m_testImageTriangles = NULL;
	}

	m_frontEndJobList = NULL;
}

/*
========================
idRenderSystemLocal::BeginLevelLoad
========================
*/
void idRenderSystemLocal::BeginLevelLoad() {
	globalImages->BeginLevelLoad();
	renderModelManager->BeginLevelLoad();

	// Re-Initialize the Default Materials if needed. 
	InitMaterials();
}

/*
========================
idRenderSystemLocal::LoadLevelImages
========================
*/
void idRenderSystemLocal::LoadLevelImages() {
	globalImages->LoadLevelImages( false );
}

/*
========================
idRenderSystemLocal::Preload
========================
*/
void idRenderSystemLocal::Preload( const idPreloadManifest &manifest, const char *mapName ) {
	globalImages->Preload( manifest, true );
	uiManager->Preload( mapName );
	renderModelManager->Preload( manifest );
}

/*
========================
idRenderSystemLocal::EndLevelLoad
========================
*/
void idRenderSystemLocal::EndLevelLoad() {
	renderModelManager->EndLevelLoad();
	globalImages->EndLevelLoad();
}

/*
============
idRenderSystemLocal::RegisterFont
============
*/
idFont * idRenderSystemLocal::RegisterFont( const char * fontName ) {

	idStrStatic< MAX_OSPATH > baseFontName = fontName;
	baseFontName.Replace( "fonts/", "" );
	for ( int i = 0; i < m_fonts.Num(); i++ ) {
		if ( idStr::Icmp( m_fonts[i]->GetName(), baseFontName ) == 0 ) {
			m_fonts[i]->Touch();
			return m_fonts[i];
		}
	}
	idFont * newFont = new (TAG_FONT) idFont( baseFontName );
	m_fonts.Append( newFont );
	return newFont;
}

/*
========================
idRenderSystemLocal::IsFullScreen
========================
*/
bool idRenderSystemLocal::IsFullScreen() const {
	return win32.isFullscreen != 0;
}

/*
========================
idRenderSystemLocal::GetWidth
========================
*/
int idRenderSystemLocal::GetWidth() const {
	return win32.nativeScreenWidth;
}

/*
========================
idRenderSystemLocal::GetHeight
========================
*/
int idRenderSystemLocal::GetHeight() const {
	return win32.nativeScreenHeight;
}

/*
========================
idRenderSystemLocal::GetPixelAspect
========================
*/
float idRenderSystemLocal::GetPixelAspect() const {
	return win32.pixelAspect;
}

/*
========================
idRenderSystemLocal::FrameAlloc

This data will be automatically freed when the
current frame's back end completes.

This should only be called by the front end.  The
back end shouldn't need to allocate memory.

All temporary data, like dynamic tesselations
and local spaces are allocated here.

All memory is cache-line-cleared for the best performance.
========================
*/
void * idRenderSystemLocal::FrameAlloc( int bytes, frameAllocType_t type ) {
#if defined( TRACK_FRAME_ALLOCS )
	m_frameData->frameMemoryUsed.Add( bytes );
	frameAllocTypeCount[ type ].Add( bytes );
#endif

	bytes = ( bytes + FRAME_ALLOC_ALIGNMENT - 1 ) & ~ ( FRAME_ALLOC_ALIGNMENT - 1 );

	// thread safe add
	int end = m_frameData->frameMemoryAllocated.Add( bytes );
	if ( end > MAX_FRAME_MEMORY ) {
		idLib::Error( "idRenderSystemLocal::FrameAlloc ran out of memory. bytes = %d, end = %d, highWaterAllocated = %d\n", bytes, end, m_frameData->highWaterAllocated );
	}

	byte * ptr = m_frameData->frameMemory + end - bytes;

	// cache line clear the memory
	for ( int offset = 0; offset < bytes; offset += CACHE_LINE_SIZE ) {
		ZeroCacheLine( ptr, offset );
	}

	return ptr;
}

/*
============
idRenderSystemLocal::ClearedFrameAlloc
============
*/
void * idRenderSystemLocal::ClearedFrameAlloc( int bytes, frameAllocType_t type ) {
	// NOTE: every allocation is cache line cleared
	return FrameAlloc( bytes, type );
}

/*
============
idRenderSystemLocal::ToggleSmpFrame
============
*/
void idRenderSystemLocal::ToggleSmpFrame() {
	// update the highwater mark
	if ( m_frameData->frameMemoryAllocated.GetValue() > m_frameData->highWaterAllocated ) {
		m_frameData->highWaterAllocated = m_frameData->frameMemoryAllocated.GetValue();
#if defined( TRACK_FRAME_ALLOCS )
		m_frameData->highWaterUsed = m_frameData->frameMemoryUsed.GetValue();
		for ( int i = 0; i < FRAME_ALLOC_MAX; i++ ) {
			frameHighWaterTypeCount[i] = frameAllocTypeCount[i].GetValue();
		}
#endif
	}

	// switch to the next frame
	m_smpFrame++;
	m_frameData = &m_smpFrameData[ m_smpFrame % NUM_FRAME_DATA ];

	// reset the memory allocation
	const uint32 bytesNeededForAlignment = FRAME_ALLOC_ALIGNMENT - ( (uint32)m_frameData->frameMemory & ( FRAME_ALLOC_ALIGNMENT - 1 ) );
	m_frameData->frameMemoryAllocated.SetValue( bytesNeededForAlignment );
	m_frameData->frameMemoryUsed.SetValue( 0 );

#if defined( TRACK_FRAME_ALLOCS )
	for ( int i = 0; i < FRAME_ALLOC_MAX; i++ ) {
		frameAllocTypeCount[i].SetValue( 0 );
	}
#endif

	// clear the command chain and make a RC_NOP command the only thing on the list
	m_frameData->cmdHead = m_frameData->cmdTail = (renderCommand_t *)FrameAlloc( sizeof( *m_frameData->cmdHead ), FRAME_ALLOC_DRAW_COMMAND );
	m_frameData->cmdHead->commandId = RC_NOP;
	m_frameData->cmdHead->next = NULL;
}

/*
============
idRenderSystemLocal::InitFrameData
============
*/
void idRenderSystemLocal::InitFrameData() {
	ShutdownFrameData();

	for ( int i = 0; i < NUM_FRAME_DATA; ++i ) {
		m_smpFrameData[ i ].frameMemory = (byte *) Mem_Alloc16( MAX_FRAME_MEMORY, TAG_RENDER );
	}

	// must be set before ToggleSmpFrame()
	m_frameData = &m_smpFrameData[ 0 ];

	ToggleSmpFrame();
}

/*
============
idRenderSystemLocal::ShutdownFrameData
============
*/
void idRenderSystemLocal::ShutdownFrameData() {
	m_frameData = NULL;
	for ( int i = 0; i < NUM_FRAME_DATA; ++i ) {
		Mem_Free16( m_smpFrameData[ i ].frameMemory );
		m_smpFrameData[ i ].frameMemory = NULL;
	}
}

/*
============
idRenderSystemLocal::GetCommandBuffer

Returns memory for a command buffer (stretchPicCommand_t, 
drawSurfsCommand_t, etc) and links it to the end of the
current command chain.
============
*/
void * idRenderSystemLocal::GetCommandBuffer( int bytes ) {
	renderCommand_t	* cmd;

	cmd = (renderCommand_t *)FrameAlloc( bytes, FRAME_ALLOC_DRAW_COMMAND );
	cmd->next = NULL;
	m_frameData->cmdTail->next = &cmd->commandId;
	m_frameData->cmdTail = cmd;

	return (void *)cmd;
}

/*
=============
idRenderSystemLocal::AddDrawViewCmd

This is the main 3D rendering command.  A single scene may
have multiple views if a mirror, portal, or dynamic texture is present.
=============
*/
void idRenderSystemLocal::AddDrawViewCmd( viewDef_t *parms, bool guiOnly ) {
	drawSurfsCommand_t	*cmd;

	cmd = (drawSurfsCommand_t *)GetCommandBuffer( sizeof( *cmd ) );
	cmd->commandId = ( guiOnly ) ? RC_DRAW_VIEW_GUI : RC_DRAW_VIEW_3D;

	cmd->viewDef = parms;

	pc.c_numViews++;

	// report statistics about this view
	if ( r_showSurfaces.GetBool() ) {
		idLib::Printf( "view:%p surfs:%i\n", parms, parms->numDrawSurfs );
	}
}

/*
=============
idRenderSystemLocal::EmitFullscreenGuis
=============
*/
void idRenderSystemLocal::EmitFullscreenGui() {
	viewDef_t * guiViewDef = m_guiModel->EmitFullScreen();
	if ( guiViewDef ) {
		// add the command to draw this view
		AddDrawViewCmd( guiViewDef, true );
	}
	m_guiModel->Clear();
}

/*
=============
idRenderSystemLocal::SetColor
=============
*/
void idRenderSystemLocal::SetColor( const idVec4 & rgba ) {
	m_currentColorNativeBytesOrder = LittleLong( PackColor( rgba ) );
}

/*
=============
idRenderSystemLocal::GetColor
=============
*/
uint32 idRenderSystemLocal::GetColor() {
	return LittleLong( m_currentColorNativeBytesOrder );
}

/*
=============
idRenderSystemLocal::SetGLState
=============
*/
void idRenderSystemLocal::SetGLState( const uint64 glState ) {
	m_currentGLState = glState;
}

/*
=============
idRenderSystemLocal::DrawFilled
=============
*/
void idRenderSystemLocal::DrawFilled( const idVec4 & color, float x, float y, float w, float h ) {
	SetColor( color );
	DrawStretchPic( x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, whiteMaterial );
}

/*
=============
idRenderSystemLocal::DrawStretchPic
=============
*/
void idRenderSystemLocal::DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, const idMaterial *material ) {
	DrawStretchPic( idVec4( x, y, s1, t1 ), idVec4( x+w, y, s2, t1 ), idVec4( x+w, y+h, s2, t2 ), idVec4( x, y+h, s1, t2 ), material );
}

/*
=============
idRenderSystemLocal::DrawStretchFX
=============
*/
void idRenderSystemLocal::DrawStretchFX( float x, float y, float w, float h, float s1, float t1, float s2, float t2, const idMaterial *material ) {
#if defined( ID_VULKAN )
	SwapValues( t1, t2 );
	DrawStretchPic( idVec4( x, y, s1, t1 ), idVec4( x+w, y, s2, t1 ), idVec4( x+w, y+h, s2, t2 ), idVec4( x, y+h, s1, t2 ), material );
#else
	DrawStretchPic( idVec4( x, y, s1, t1 ), idVec4( x+w, y, s2, t1 ), idVec4( x+w, y+h, s2, t2 ), idVec4( x, y+h, s1, t2 ), material );
#endif
}

/*
=============
idRenderSystemLocal::DrawStretchPic
=============
*/
static triIndex_t quadPicIndexes[6] = { 3, 0, 2, 2, 0, 1 };
void idRenderSystemLocal::DrawStretchPic( const idVec4 & topLeft, const idVec4 & topRight, const idVec4 & bottomRight, const idVec4 & bottomLeft, const idMaterial * material ) {
	if ( !m_bInitialized ) {
		return;
	}
	if ( material == NULL ) {
		return;
	}

	idDrawVert * verts = m_guiModel->AllocTris( 4, quadPicIndexes, 6, material, m_currentGLState );
	if ( verts == NULL ) {
		return;
	}

	ALIGNTYPE16 idDrawVert localVerts[4];

	localVerts[0].Clear();
	localVerts[0].xyz[0] = topLeft.x;
	localVerts[0].xyz[1] = topLeft.y;
	localVerts[0].SetTexCoord( topLeft.z, topLeft.w );
	localVerts[0].SetNativeOrderColor( m_currentColorNativeBytesOrder );
	localVerts[0].ClearColor2();

	localVerts[1].Clear();
	localVerts[1].xyz[0] = topRight.x;
	localVerts[1].xyz[1] = topRight.y;
	localVerts[1].SetTexCoord( topRight.z, topRight.w );
	localVerts[1].SetNativeOrderColor( m_currentColorNativeBytesOrder );
	localVerts[1].ClearColor2();

	localVerts[2].Clear();
	localVerts[2].xyz[0] = bottomRight.x;
	localVerts[2].xyz[1] = bottomRight.y;
	localVerts[2].SetTexCoord( bottomRight.z, bottomRight.w );
	localVerts[2].SetNativeOrderColor( m_currentColorNativeBytesOrder );
	localVerts[2].ClearColor2();

	localVerts[3].Clear();
	localVerts[3].xyz[0] = bottomLeft.x;
	localVerts[3].xyz[1] = bottomLeft.y;
	localVerts[3].SetTexCoord( bottomLeft.z, bottomLeft.w );
	localVerts[3].SetNativeOrderColor( m_currentColorNativeBytesOrder );
	localVerts[3].ClearColor2();

	WriteDrawVerts16( verts, localVerts, 4 );
}

/*
=============
idRenderSystemLocal::DrawStretchTri
=============
*/
void idRenderSystemLocal::DrawStretchTri( const idVec2 & p1, const idVec2 & p2, const idVec2 & p3, const idVec2 & t1, const idVec2 & t2, const idVec2 & t3, const idMaterial *material ) {
	if ( !m_bInitialized ) {
		return;
	}
	if ( material == NULL ) {
		return;
	}

	triIndex_t tempIndexes[3] = { 1, 0, 2 };

	idDrawVert * verts = m_guiModel->AllocTris( 3, tempIndexes, 3, material, m_currentGLState );
	if ( verts == NULL ) {
		return;
	}

	ALIGNTYPE16 idDrawVert localVerts[3];

	localVerts[0].Clear();
	localVerts[0].xyz[0] = p1.x;
	localVerts[0].xyz[1] = p1.y;
	localVerts[0].SetTexCoord( t1 );
	localVerts[0].SetNativeOrderColor( m_currentColorNativeBytesOrder );
	localVerts[0].ClearColor2();

	localVerts[1].Clear();
	localVerts[1].xyz[0] = p2.x;
	localVerts[1].xyz[1] = p2.y;
	localVerts[1].SetTexCoord( t2 );
	localVerts[1].SetNativeOrderColor( m_currentColorNativeBytesOrder );
	localVerts[1].ClearColor2();

	localVerts[2].Clear();
	localVerts[2].xyz[0] = p3.x;
	localVerts[2].xyz[1] = p3.y;
	localVerts[2].SetTexCoord( t3 );
	localVerts[2].SetNativeOrderColor( m_currentColorNativeBytesOrder );
	localVerts[2].ClearColor2();

	WriteDrawVerts16( verts, localVerts, 3 );
}

/*
=============
idRenderSystemLocal::AllocTris
=============
*/
idDrawVert * idRenderSystemLocal::AllocTris( int numVerts, const triIndex_t * indexes, int numIndexes, const idMaterial * material ) {
	return m_guiModel->AllocTris( numVerts, indexes, numIndexes, material, m_currentGLState );
}

/*
=====================
idRenderSystemLocal::DrawSmallChar

small chars are drawn at native screen resolution
=====================
*/
void idRenderSystemLocal::DrawSmallChar( int x, int y, int ch ) {
	int row, col;
	float frow, fcol;
	float size;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	if ( y < -SMALLCHAR_HEIGHT ) {
		return;
	}

	row = ch >> 4;
	col = ch & 15;

	frow = row * 0.0625f;
	fcol = col * 0.0625f;
	size = 0.0625f;

	DrawStretchPic( x, y, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT,
					   fcol, frow, 
					   fcol + size, frow + size, 
					   charSetMaterial );
}

/*
==================
idRenderSystemLocal::DrawSmallStringExt

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void idRenderSystemLocal::DrawSmallStringExt( int x, int y, const char *string, const idVec4 &setColor, bool forceColor ) {
	idVec4		color;
	const unsigned char	*s;
	int			xx;

	// draw the colored text
	s = (const unsigned char*)string;
	xx = x;
	SetColor( setColor );
	while ( *s ) {
		if ( idStr::IsColor( (const char*)s ) ) {
			if ( !forceColor ) {
				if ( *(s+1) == C_COLOR_DEFAULT ) {
					SetColor( setColor );
				} else {
					color = idStr::ColorForIndex( *(s+1) );
					color[3] = setColor[3];
					SetColor( color );
				}
			}
			s += 2;
			continue;
		}
		DrawSmallChar( xx, y, *s );
		xx += SMALLCHAR_WIDTH;
		s++;
	}
	SetColor( colorWhite );
}

/*
=====================
idRenderSystemLocal::DrawBigChar
=====================
*/
void idRenderSystemLocal::DrawBigChar( int x, int y, int ch ) {
	int row, col;
	float frow, fcol;
	float size;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	if ( y < -BIGCHAR_HEIGHT ) {
		return;
	}

	row = ch >> 4;
	col = ch & 15;

	frow = row * 0.0625f;
	fcol = col * 0.0625f;
	size = 0.0625f;

	DrawStretchPic( x, y, BIGCHAR_WIDTH, BIGCHAR_HEIGHT,
					   fcol, frow, 
					   fcol + size, frow + size, 
					   charSetMaterial );
}

/*
==================
idRenderSystemLocal::DrawBigStringExt

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void idRenderSystemLocal::DrawBigStringExt( int x, int y, const char *string, const idVec4 &setColor, bool forceColor ) {
	idVec4		color;
	const char	*s;
	int			xx;

	// draw the colored text
	s = string;
	xx = x;
	SetColor( setColor );
	while ( *s ) {
		if ( idStr::IsColor( s ) ) {
			if ( !forceColor ) {
				if ( *(s+1) == C_COLOR_DEFAULT ) {
					SetColor( setColor );
				} else {
					color = idStr::ColorForIndex( *(s+1) );
					color[3] = setColor[3];
					SetColor( color );
				}
			}
			s += 2;
			continue;
		}
		DrawBigChar( xx, y, *s );
		xx += BIGCHAR_WIDTH;
		s++;
	}
	SetColor( colorWhite );
}

/*
====================
idRenderSystemLocal::SwapCommandBuffers

Performs final closeout of any gui models being defined.

Waits for the previous GPU rendering to complete and vsync.

Returns the head of the linked command list that was just closed off.

Returns timing information from the previous frame.

After this is called, new command buffers can be built up in parallel
with the rendering of the closed off command buffers by RenderCommandBuffers()
====================
*/
const renderCommand_t * idRenderSystemLocal::SwapCommandBuffers( 
													uint64 * frontEndMicroSec,
													uint64 * backEndMicroSec,
													uint64 * shadowMicroSec,
													uint64 * gpuMicroSec )  {

	SwapCommandBuffers_FinishRendering( frontEndMicroSec, backEndMicroSec, shadowMicroSec, gpuMicroSec );

	return SwapCommandBuffers_FinishCommandBuffers();
}

/*
=====================
idRenderSystemLocal::SwapCommandBuffers_FinishRendering
=====================
*/
void idRenderSystemLocal::SwapCommandBuffers_FinishRendering( 
												uint64 * frontEndMicroSec,
												uint64 * backEndMicroSec,
												uint64 * shadowMicroSec,
												uint64 * gpuMicroSec )  {
	SCOPED_PROFILE_EVENT( "SwapCommandBuffers_FinishRendering" );

	if ( gpuMicroSec != NULL ) {
		*gpuMicroSec = 0;		// until shown otherwise
	}

	if ( !m_bInitialized ) {
		return;
	}

	// wait for our fence to hit, which means the swap has actually happened
	// We must do this before clearing any resources the GPU may be using
	m_backend.BlockingSwapBuffers();

	//------------------------------

	// save out timing information
	if ( frontEndMicroSec != NULL ) {
		*frontEndMicroSec = pc.frontEndMicroSec;
	}
	if ( backEndMicroSec != NULL ) {
		*backEndMicroSec = m_backend.m_pc.totalMicroSec;
	}
	if ( shadowMicroSec != NULL ) {
		*shadowMicroSec = m_backend.m_pc.shadowMicroSec;
	}

	// print any other statistics and clear all of them
	PrintPerformanceCounters();
}

/*
=====================
idRenderSystemLocal::SwapCommandBuffers_FinishCommandBuffers
=====================
*/
void R_InitDrawSurfFromTri( drawSurf_t & ds, srfTriangles_t & tri );
const renderCommand_t * idRenderSystemLocal::SwapCommandBuffers_FinishCommandBuffers() {
	if ( !m_bInitialized ) {
		return NULL;
	}

	// close any gui drawing
	EmitFullscreenGui();

	// unmap the buffer objects so they can be used by the GPU
	vertexCache.BeginBackEnd();

	// save off this command buffer
	const renderCommand_t * commandBufferHead = m_frameData->cmdHead;

	// copy the code-used drawsurfs that were
	// allocated at the start of the buffer memory to the m_backend referenced locations
	m_backend.m_unitSquareSurface = m_unitSquareSurface;
	m_backend.m_zeroOneCubeSurface = m_zeroOneCubeSurface;
	m_backend.m_testImageSurface = m_testImageSurface;

	// use the other buffers next frame, because another CPU
	// may still be rendering into the current buffers
	ToggleSmpFrame();

	// prepare the new command buffer
	m_guiModel->BeginFrame();

	//------------------------------
	// Make sure that geometry used by code is present in the buffer cache.
	// These use frame buffer cache (not static) because they may be used during
	// map loads.
	//
	// It is important to do this first, so if the buffers overflow during
	// scene generation, the basic surfaces needed for drawing the buffers will
	// always be present.
	//------------------------------
	R_InitDrawSurfFromTri( m_unitSquareSurface, *m_unitSquareTriangles );
	R_InitDrawSurfFromTri( m_zeroOneCubeSurface, *m_zeroOneCubeTriangles );
	R_InitDrawSurfFromTri( m_testImageSurface, *m_testImageTriangles );

	// Reset render crop to be the full screen
	m_renderCrops[0].x1 = 0;
	m_renderCrops[0].y1 = 0;
	m_renderCrops[0].x2 = GetWidth() - 1;
	m_renderCrops[0].y2 = GetHeight() - 1;
	m_currentRenderCrop = 0;

	// this is the ONLY place this is modified
	frameCount++;

	// just in case we did a idLib::Error while this
	// was set
	m_guiRecursionLevel = 0;

	// the old command buffer can now be rendered, while the new one can
	// be built in parallel
	return commandBufferHead;
}

/*
====================
idRenderSystemLocal::RenderCommandBuffers
====================
*/
void idRenderSystemLocal::RenderCommandBuffers( const renderCommand_t * const cmdHead ) {
	// if there isn't a draw view command, do nothing to avoid swapping a bad frame
	bool hasView = false;
	for ( const renderCommand_t * cmd = cmdHead ; cmd ; cmd = (const renderCommand_t *)cmd->next ) {
		if ( cmd->commandId == RC_DRAW_VIEW_3D || cmd->commandId == RC_DRAW_VIEW_GUI ) {
			hasView = true;
			break;
		}
	}
	if ( !hasView ) {
		return;
	}

	// r_skipBackEnd allows the entire time of the back end
	// to be removed from performance measurements, although
	// nothing will be drawn to the screen.  If the prints
	// are going to a file, or r_skipBackEnd is later disabled,
	// usefull data can be received.

	// r_skipRender is usually more usefull, because it will still
	// draw 2D graphics
	if ( !r_skipBackEnd.GetBool() ) {
		m_backend.ExecuteBackEndCommands( cmdHead );
	}

	// pass in null for now - we may need to do some map specific hackery in the future
	resolutionScale.InitForMap( NULL );
}

/*
=====================
idRenderSystemLocal::GetCroppedViewport

Returns the current cropped pixel coordinates
=====================
*/
void idRenderSystemLocal::GetCroppedViewport( idScreenRect * viewport ) {
	*viewport = m_renderCrops[ m_currentRenderCrop ];
}

/*
========================
idRenderSystemLocal::PerformResolutionScaling

The 3D rendering size can be smaller than the full window resolution to reduce
fill rate requirements while still allowing the GUIs to be full resolution.
In split screen mode the rendering size is also smaller.
========================
*/
void idRenderSystemLocal::PerformResolutionScaling( int& newWidth, int& newHeight ) {

	float xScale = 1.0f;
	float yScale = 1.0f;
	resolutionScale.GetCurrentResolutionScale( xScale, yScale );

	newWidth = idMath::Ftoi( GetWidth() * xScale );
	newHeight = idMath::Ftoi( GetHeight() * yScale );
}

/*
================
idRenderSystemLocal::CropRenderSize
================
*/
void idRenderSystemLocal::CropRenderSize( int width, int height ) {
	if ( !m_bInitialized ) {
		return;
	}

	// close any gui drawing before changing the size
	EmitFullscreenGui();

	if ( width < 1 || height < 1 ) {
		idLib::Error( "CropRenderSize: bad sizes" );
	}

	idScreenRect & previous = m_renderCrops[ m_currentRenderCrop ];

	m_currentRenderCrop++;

	idScreenRect & current = m_renderCrops[ m_currentRenderCrop ];

	current.x1 = previous.x1;
	current.x2 = previous.x1 + width - 1;
	current.y1 = previous.y2 - height + 1;
	current.y2 = previous.y2;
}

/*
================
idRenderSystemLocal::UnCrop
================
*/
void idRenderSystemLocal::UnCrop() {
	if ( !m_bInitialized ) {
		return;
	}

	if ( m_currentRenderCrop < 1 ) {
		idLib::Error( "idRenderSystemLocal::UnCrop: m_currentRenderCrop < 1" );
	}

	// close any gui drawing
	EmitFullscreenGui();

	m_currentRenderCrop--;
}

/*
================
idRenderSystemLocal::CaptureRenderToImage
================
*/
void idRenderSystemLocal::CaptureRenderToImage( const char *imageName, bool clearColorAfterCopy ) {
	if ( !m_bInitialized ) {
		return;
	}
	EmitFullscreenGui();

	idImage	* image = globalImages->GetImage( imageName );
	if ( image == NULL ) {
		image = globalImages->AllocImage( imageName );
	}

	idScreenRect & rc = m_renderCrops[ m_currentRenderCrop ];

	copyRenderCommand_t *cmd = (copyRenderCommand_t *)GetCommandBuffer( sizeof( *cmd ) );
	cmd->commandId = RC_COPY_RENDER;
	cmd->x = rc.x1;
	cmd->y = rc.y1;
	cmd->imageWidth = rc.GetWidth();
	cmd->imageHeight = rc.GetHeight();
	cmd->image = image;
	cmd->clearColorAfterCopy = clearColorAfterCopy;

	m_guiModel->Clear();
}

/*
==============
idRenderSystemLocal::CaptureRenderToFile
==============
*/
void idRenderSystemLocal::CaptureRenderToFile( const char *fileName, bool fixAlpha ) {
	if ( !m_bInitialized ) {
		return;
	}

	// TODO: Refactor for both APIs
#if 0
	idScreenRect & rc = m_renderCrops[ m_currentRenderCrop ];

	EmitFullscreenGui();

	RenderCommandBuffers( m_frameData->cmdHead );

	qglReadBuffer( GL_BACK );

	// include extra space for OpenGL padding to word boundaries
	int	c = ( rc.GetWidth() + 3 ) * rc.GetHeight();
	byte *data = (byte *)R_StaticAlloc( c * 3 );
	
	qglReadPixels( rc.x1, rc.y1, rc.GetWidth(), rc.GetHeight(), GL_RGB, GL_UNSIGNED_BYTE, data ); 

	byte *data2 = (byte *)R_StaticAlloc( c * 4 );

	for ( int i = 0 ; i < c ; i++ ) {
		data2[ i * 4 ] = data[ i * 3 ];
		data2[ i * 4 + 1 ] = data[ i * 3 + 1 ];
		data2[ i * 4 + 2 ] = data[ i * 3 + 2 ];
		data2[ i * 4 + 3 ] = 0xff;
	}

	R_WriteTGA( fileName, data2, rc.GetWidth(), rc.GetHeight(), true );

	R_StaticFree( data );
	R_StaticFree( data2 );
#endif
}


/*
==============
idRenderSystemLocal::AllocRenderWorld
==============
*/
idRenderWorld *idRenderSystemLocal::AllocRenderWorld() {
	idRenderWorld *rw;
	rw = new (TAG_RENDER) idRenderWorld;
	m_worlds.Append( rw );
	return rw;
}

/*
==============
idRenderSystemLocal::ReCreateWorldReferences

ReloadModels and RegenerateWorld call this
==============
*/
void idRenderSystemLocal::ReCreateWorldReferences() {
	// let the interaction generation code know this
	// shouldn't be optimized for a particular view
	m_viewDef = NULL;

	for ( int i = 0; i < m_worlds.Num(); ++i ) {
		m_worlds[ i ]->ReCreateReferences();
	}
}

/*
===================
idRenderSystemLocal::FreeWorldDerivedData

ReloadModels and RegenerateWorld call this
===================
*/
void idRenderSystemLocal::FreeWorldDerivedData() {
	for ( int i = 0; i < m_worlds.Num(); ++i ) {
		m_worlds[ i ]->FreeDerivedData();
	}
}

/*
===================
idRenderSystemLocal::CheckWorldsForEntityDefsUsingModel
===================
*/
void idRenderSystemLocal::CheckWorldsForEntityDefsUsingModel( idRenderModel * model ) {
	for ( int i = 0; i < m_worlds.Num(); ++i ) {
		m_worlds[ i ]->CheckForEntityDefsUsingModel( model );
	}
}

/*
==============
idRenderSystemLocal::FreeRenderWorld
==============
*/
void idRenderSystemLocal::FreeRenderWorld( idRenderWorld *rw ) {
	if ( primaryWorld == rw ) {
		primaryWorld = NULL;
	}
	m_worlds.Remove( rw );
	delete rw;
}

/*
=====================
idRenderSystemLocal::PrintPerformanceCounters

This prints both front and back end counters, so it should
only be called when the back end thread is idle.
=====================
*/
void idRenderSystemLocal::PrintPerformanceCounters() {
	if ( r_showPrimitives.GetInteger() != 0 ) {
		backEndCounters_t & bc = m_backend.m_pc;

		idLib::Printf( "views:%i draws:%i tris:%i (shdw:%i)\n",
			pc.c_numViews,
			bc.c_drawElements + bc.c_shadowElements,
			( bc.c_drawIndexes + bc.c_shadowIndexes ) / 3,
			bc.c_shadowIndexes / 3
			);
	}

	if ( r_showDynamic.GetBool() ) {
		idLib::Printf( "callback:%i md5:%i dfrmVerts:%i dfrmTris:%i tangTris:%i guis:%i\n",
			pc.c_entityDefCallbacks,
			pc.c_generateMd5,
			pc.c_deformedVerts,
			pc.c_deformedIndexes/3,
			pc.c_tangentIndexes/3,
			pc.c_guiSurfs
			); 
	}

	if ( r_showCull.GetBool() ) {
		idLib::Printf( "%i box in %i box out\n",
			pc.c_box_cull_in, pc.c_box_cull_out );
	}
	
	if ( r_showAddModel.GetBool() ) {
		idLib::Printf( "callback:%i createInteractions:%i createShadowVolumes:%i\n",
			pc.c_entityDefCallbacks, pc.c_createInteractions, pc.c_createShadowVolumes );
		idLib::Printf( "viewEntities:%i  shadowEntities:%i  viewLights:%i\n", pc.c_visibleViewEntities,
			pc.c_shadowViewEntities, pc.c_viewLights );
	}
	if ( r_showUpdates.GetBool() ) {
		idLib::Printf( "entityUpdates:%i  entityRefs:%i  lightUpdates:%i  lightRefs:%i\n", 
			pc.c_entityUpdates, pc.c_entityReferences,
			pc.c_lightUpdates, pc.c_lightReferences );
	}
	if ( r_showMemory.GetBool() ) {
		idLib::Printf( "frameData: %i (%i)\n", m_frameData->frameMemoryAllocated.GetValue(), m_frameData->highWaterAllocated );
	}

	memset( &pc, 0, sizeof( pc ) );
	memset( &m_backend.m_pc, 0, sizeof( m_backend.m_pc ) );
}
