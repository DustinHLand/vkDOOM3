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

#ifndef __RENDERER_LOCAL_H__
#define __RENDERER_LOCAL_H__

#include "RenderCommon.h"
#include "RenderWorld.h"
#include "RenderBackend.h"

class idParallelJobList;

/*
===========================================================================

idRenderSystemLocal

Most renderer globals are defined here.
backend functions should never modify any of these fields,
but may read fields that aren't dynamically modified
by the frontend.

===========================================================================
*/
class idRenderSystemLocal : public idRenderSystem {
public:
		// internal functions
							idRenderSystemLocal();
	virtual					~idRenderSystemLocal();

	virtual void			Init();
	virtual void			Shutdown();
	virtual bool			IsInitialized() const { return m_bInitialized; }
	virtual void			VidRestart();

	virtual bool			IsFullScreen() const;
	virtual int				GetWidth() const;
	virtual int				GetHeight() const;
	virtual float			GetPixelAspect() const;

	virtual idRenderWorld *	AllocRenderWorld();
	virtual void			ReCreateWorldReferences();
	virtual void			FreeWorldDerivedData();
	virtual void			CheckWorldsForEntityDefsUsingModel( idRenderModel * model );
	virtual void			FreeRenderWorld( idRenderWorld *rw );

	virtual void			BeginLevelLoad();
	virtual void			EndLevelLoad();
	virtual void			Preload( const idPreloadManifest &manifest, const char *mapName );
	virtual void			LoadLevelImages();

	virtual void *			FrameAlloc( int bytes, frameAllocType_t type = FRAME_ALLOC_UNKNOWN );
	virtual void *			ClearedFrameAlloc( int bytes, frameAllocType_t type = FRAME_ALLOC_UNKNOWN );

	// Render
	virtual void			SetRenderView( const renderView_t * renderView ) { m_primaryRenderView = *renderView; }
	virtual void			RenderScene( idRenderWorld * world, const renderView_t *renderView );

	virtual idFont *		RegisterFont( const char * fontName );

	virtual void			SetColor( const idVec4 & color );
	virtual uint32			GetColor();

	virtual void			SetGLState( const uint64 glState ) ;

	virtual void			DrawFilled( const idVec4 & color, float x, float y, float w, float h );
	virtual void			DrawStretchPic ( float x, float y, float w, float h, float s1, float t1, float s2, float t2, const idMaterial *material );
	virtual void			DrawStretchPic( const idVec4 & topLeft, const idVec4 & topRight, const idVec4 & bottomRight, const idVec4 & bottomLeft, const idMaterial * material );
	virtual void			DrawStretchTri( const idVec2 & p1, const idVec2 & p2, const idVec2 & p3, const idVec2 & t1, const idVec2 & t2, const idVec2 & t3, const idMaterial *material );
	virtual void			DrawStretchFX( float x, float y, float w, float h, float s1, float t1, float s2, float t2, const idMaterial *material );
	virtual idDrawVert *	AllocTris( int numVerts, const triIndex_t * indexes, int numIndexes, const idMaterial * material );

	virtual void			DrawSmallChar( int x, int y, int ch );
	virtual void			DrawSmallStringExt( int x, int y, const char *string, const idVec4 &setColor, bool forceColor );
	virtual void			DrawBigChar( int x, int y, int ch );
	virtual void			DrawBigStringExt( int x, int y, const char *string, const idVec4 &setColor, bool forceColor );

	virtual const renderCommand_t *	SwapCommandBuffers( frameTiming_t * frameTiming );
	virtual void			SwapCommandBuffers_FinishRendering( frameTiming_t * frameTiming );
	virtual const renderCommand_t *	SwapCommandBuffers_FinishCommandBuffers();
	virtual void			RenderCommandBuffers( const renderCommand_t * commandBuffers );

	virtual void			TakeScreenshot( int width, int height, const char *fileName, int downSample, renderView_t *ref );
	virtual void			CropRenderSize( int width, int height );
	virtual void			CaptureRenderToImage( const char *imageName, bool clearColorAfterCopy = false );
	virtual void			CaptureRenderToFile( const char *fileName, bool fixAlpha );
	virtual void			UnCrop();

	virtual void			GetCroppedViewport( idScreenRect * viewport );
	virtual void			PerformResolutionScaling( int & newWidth, int & newHeight );

	virtual void			ReloadSurface();

	virtual void			PrintMemInfo( MemInfo_t *mi );
	virtual void			PrintRenderEntityDefs();
	virtual void			PrintRenderLightDefs();

private:
	void					Clear();

	// Setup
	void					InitMaterials();
	void					InitFrameData();
	void					ShutdownFrameData();
	void					ToggleSmpFrame();

	void *					GetCommandBuffer( int bytes );
	void					AddDrawViewCmd( viewDef_t * parms, bool guiOnly );

	// Render
	void					RenderView( viewDef_t * parms );
	void					RenderGuiSurf( idUserInterface * gui, const drawSurf_t * drawSurf );

	// Lights
	void					AddLights();

	// Models
	void					AddModels();

	// Guis
	void					EmitFullscreenGui();
	void					AddInGameGuis( const drawSurf_t * const drawSurfs[], const int numDrawSurfs );

	// Subview
	bool					GenerateSubViews( const drawSurf_t * const drawSurfs[], const int numDrawSurfs );
	bool					GenerateSurfaceSubview( const drawSurf_t * drawSurf );
	bool					PreciseCullSurface( const drawSurf_t *drawSurf, idBounds &ndcBounds );
	void					RemoteRender( const drawSurf_t * surf, textureStage_t * stage );
	void					MirrorRender( const drawSurf_t * surf, textureStage_t * stage, idScreenRect scissor );
	viewDef_t *				MirrorViewBySurface( const drawSurf_t * drawSurf );
	void					XrayRender( const drawSurf_t * surf, textureStage_t * stage, idScreenRect scissor );
	viewDef_t *				XrayViewBySurface( const drawSurf_t * drawSurf );

	// Screenshot
	void					ReadTiledPixels( int width, int height, byte * buffer, renderView_t * ref = NULL );

	void					PrintPerformanceCounters();

public:
	int						frameCount;			// incremented every frame
	int						viewCount;			// incremented every view (twice a scene if subviewed)
												// and every R_MarkFragments call

	idRenderWorld *			primaryWorld;
	renderView_t			m_primaryRenderView;
	viewDef_t *				primaryView;
	// many console commands need to know which world they should operate on

	const idMaterial *		whiteMaterial;
	const idMaterial *		charSetMaterial;
	const idMaterial *		defaultPointLight;
	const idMaterial *		defaultProjectedLight;
	const idMaterial *		defaultMaterial;

	viewDef_t *				m_viewDef;

	struct performanceCounters_t {
		int		c_box_cull_in;
		int		c_box_cull_out;
		int		c_createInteractions;	// number of calls to idInteraction::CreateInteraction
		int		c_createShadowVolumes;
		int		c_generateMd5;
		int		c_entityDefCallbacks;
		int		c_alloc;				// counts for R_StaticAllc/R_StaticFree
		int		c_free;
		int		c_visibleViewEntities;
		int		c_shadowViewEntities;
		int		c_viewLights;
		int		c_numViews;				// number of total views rendered
		int		c_deformedSurfaces;		// idMD5Mesh::GenerateSurface
		int		c_deformedVerts;		// idMD5Mesh::GenerateSurface
		int		c_deformedIndexes;		// idMD5Mesh::GenerateSurface
		int		c_tangentIndexes;		// R_DeriveTangents()
		int		c_entityUpdates;
		int		c_lightUpdates;
		int		c_entityReferences;
		int		c_lightReferences;
		int		c_guiSurfs;
		int		frontEndMicroSec;		// sum of time in all RE_RenderScene's in a frame
	} pc;

private:
	bool					m_bInitialized;
	bool					m_takingScreenshot;

	idList< idRenderWorld * >	m_worlds;

	srfTriangles_t *		m_unitSquareTriangles;
	srfTriangles_t *		m_zeroOneCubeTriangles;
	srfTriangles_t *		m_testImageTriangles;

	// these are allocated at buffer swap time, but
	// the back end should only use the ones in the backEnd stucture,
	// which are copied over from the frame that was just swapped.
	drawSurf_t				m_unitSquareSurface;
	drawSurf_t				m_zeroOneCubeSurface;
	drawSurf_t				m_testImageSurface;

	idScreenRect			m_renderCrops[ MAX_RENDER_CROPS ];
	int						m_currentRenderCrop;

	// GUI drawing variables for surface creation
	int						m_guiRecursionLevel;	// to prevent infinite overruns
	uint32					m_currentColorNativeBytesOrder;
	uint64					m_currentGLState;
	//

	idList< idFont *, TAG_FONT >	m_fonts;

	// GUI drawing variables for surface creation
	class idGuiModel *		m_guiModel;

	idParallelJobList *		m_frontEndJobList;

	idRenderBackend			m_backend;

	idFrameData				m_smpFrameData[ NUM_FRAME_DATA ];
	idFrameData *			m_frameData;
	uint32					m_smpFrame;
};

extern idRenderSystemLocal	tr;

/*
====================================================================

TR_FRONTEND_MAIN

====================================================================
*/

void *	R_StaticAlloc( int bytes, const memTag_t tag = TAG_RENDER_STATIC );		// just malloc with error checking
void *	R_ClearedStaticAlloc( int bytes );	// with memset
void	R_StaticFree( void *data );

/*
============================================================

TR_FRONTEND_ADDLIGHTS

============================================================
*/

ID_INLINE bool R_CullModelBoundsToLight( const idRenderLight * light, const idBounds & localBounds, const idRenderMatrix & modelRenderMatrix ) {
	idRenderMatrix modelLightProject;
	idRenderMatrix::Multiply( light->baseLightProject, modelRenderMatrix, modelLightProject );
	return idRenderMatrix::CullBoundsToMVP( modelLightProject, localBounds, true );
}

#endif /* !__RENDERER_LOCAL_H__ */