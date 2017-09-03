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

#ifndef __RENDERER_H__
#define __RENDERER_H__

class idMaterial;
class idRenderModel;
struct renderView_t;
struct renderCommand_t;
struct frameTiming_t;

/*
===============================================================================

	idRenderSystem is responsible for managing the screen, which can have
	multiple idRenderWorld and 2D drawing done on it.

===============================================================================
*/

enum graphicsVendor_t {
	VENDOR_NVIDIA,
	VENDOR_AMD,
	VENDOR_INTEL
};

enum frameAllocType_t {
	FRAME_ALLOC_VIEW_DEF,
	FRAME_ALLOC_VIEW_ENTITY,
	FRAME_ALLOC_VIEW_LIGHT,
	FRAME_ALLOC_SURFACE_TRIANGLES,
	FRAME_ALLOC_DRAW_SURFACE,
	FRAME_ALLOC_INTERACTION_STATE,
	FRAME_ALLOC_SHADOW_ONLY_ENTITY,
	FRAME_ALLOC_SHADOW_VOLUME_PARMS,
	FRAME_ALLOC_SHADER_REGISTER,
	FRAME_ALLOC_DRAW_SURFACE_POINTER,
	FRAME_ALLOC_DRAW_COMMAND,
	FRAME_ALLOC_UNKNOWN,
	FRAME_ALLOC_MAX
};

const int SMALLCHAR_WIDTH		= 8;
const int SMALLCHAR_HEIGHT		= 16;
const int BIGCHAR_WIDTH			= 16;
const int BIGCHAR_HEIGHT		= 16;

// all drawing is done to a 640 x 480 virtual screen size
// and will be automatically scaled to the real resolution
const int SCREEN_WIDTH			= 640;
const int SCREEN_HEIGHT			= 480;

const int TITLESAFE_LEFT		= 32;
const int TITLESAFE_RIGHT		= 608;
const int TITLESAFE_TOP			= 24;
const int TITLESAFE_BOTTOM		= 456;
const int TITLESAFE_WIDTH		= TITLESAFE_RIGHT - TITLESAFE_LEFT;
const int TITLESAFE_HEIGHT		= TITLESAFE_BOTTOM - TITLESAFE_TOP;

class idRenderWorld;


class idRenderSystem {
public:

	virtual					~idRenderSystem() {}

	// set up cvars and basic data structures, but don't
	// init OpenGL, so it can also be used for dedicated servers
	virtual void			Init() = 0;
	virtual void			Shutdown() = 0;
	virtual bool			IsInitialized() const = 0;
	virtual void			VidRestart() = 0;

	virtual bool			IsFullScreen() const = 0;
	virtual int				GetWidth() const = 0;
	virtual int				GetHeight() const = 0;

	// return w/h of a single pixel. This will be 1.0 for normal cases.
	// A side-by-side stereo 3D frame will have a pixel aspect of 0.5.
	// A top-and-bottom stereo 3D frame will have a pixel aspect of 2.0
	virtual float			GetPixelAspect() const = 0;

	// allocate a renderWorld to be used for drawing
	virtual idRenderWorld *	AllocRenderWorld() = 0;
	virtual void			ReCreateWorldReferences() = 0;
	virtual void			FreeWorldDerivedData() = 0;
	virtual void			CheckWorldsForEntityDefsUsingModel( idRenderModel * model ) = 0;
	virtual	void			FreeRenderWorld( idRenderWorld * rw ) = 0;

	virtual void *			FrameAlloc( int bytes, frameAllocType_t type = FRAME_ALLOC_UNKNOWN ) = 0;
	virtual void *			ClearedFrameAlloc( int bytes, frameAllocType_t type = FRAME_ALLOC_UNKNOWN ) = 0;

	// rendering a scene may actually render multiple subviews for mirrors and portals, and
	// may render composite textures for gui console screens and light projections
	// It would also be acceptable to render a scene multiple times, for "rear view mirrors", etc
	virtual void			SetRenderView( const renderView_t * renderView ) = 0;
	virtual void			RenderScene( idRenderWorld * world, const renderView_t *renderView ) = 0;

	// All data that will be used in a level should be
	// registered before rendering any frames to prevent disk hits,
	// but they can still be registered at a later time
	// if necessary.
	virtual void			BeginLevelLoad() = 0;
	virtual void			EndLevelLoad() = 0;
	virtual void			Preload( const idPreloadManifest &manifest, const char *mapName ) = 0;
	virtual void			LoadLevelImages() = 0;

	// font support
	virtual class idFont *	RegisterFont( const char * fontName ) = 0;

	virtual void			SetColor( const idVec4 & rgba ) = 0;
	virtual void			SetColor4( float r, float g, float b, float a ) { SetColor( idVec4( r, g, b, a ) ); }
	virtual uint32			GetColor() = 0;

	virtual void			SetGLState( const uint64 glState ) = 0;

	virtual void			DrawFilled( const idVec4 & color, float x, float y, float w, float h ) = 0;
	virtual void			DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, const idMaterial *material ) = 0;
			void			DrawStretchPic( const idVec4 & rect, const idVec4 & st, const idMaterial * material ) { DrawStretchPic( rect.x, rect.y, rect.z, rect.w, st.x, st.y, st.z, st.w, material ); }
	virtual void			DrawStretchPic( const idVec4 & topLeft, const idVec4 & topRight, const idVec4 & bottomRight, const idVec4 & bottomLeft, const idMaterial * material ) = 0;
	virtual void			DrawStretchFX( float x, float y, float w, float h, float s1, float t1, float s2, float t2, const idMaterial *material ) = 0;
	virtual void			DrawStretchTri ( const idVec2 & p1, const idVec2 & p2, const idVec2 & p3, const idVec2 & t1, const idVec2 & t2, const idVec2 & t3, const idMaterial *material ) = 0;
	virtual idDrawVert *	AllocTris( int numVerts, const triIndex_t * indexes, int numIndexes, const idMaterial * material ) = 0;

	virtual void			DrawSmallChar( int x, int y, int ch ) = 0;
	virtual void			DrawSmallStringExt( int x, int y, const char *string, const idVec4 &setColor, bool forceColor ) = 0;
	virtual void			DrawBigChar( int x, int y, int ch ) = 0;
	virtual void			DrawBigStringExt( int x, int y, const char *string, const idVec4 &setColor, bool forceColor ) = 0;

	// Performs final closeout of any gui models being defined.
	//
	// Waits for the previous GPU rendering to complete and vsync.
	//
	// Returns the head of the linked command list that was just closed off.
	//
	// Returns timing information from the previous frame.
	//
	// After this is called, new command buffers can be built up in parallel
	// with the rendering of the closed off command buffers by RenderCommandBuffers()
	virtual void			SwapCommandBuffers( frameTiming_t * frameTiming ) = 0;
	virtual void			SwapAndRenderCommandBuffers( frameTiming_t * frameTiming ) = 0;

	// SwapCommandBuffers operation can be split in two parts for non-smp rendering
	// where the GPU is idled intentionally for minimal latency.
	virtual void			SwapCommandBuffers_FinishRendering( frameTiming_t * frameTiming ) = 0;
	virtual void			SwapCommandBuffers_FinishCommandBuffers() = 0;

	// issues GPU commands to render a built up list of command buffers returned
	// by SwapCommandBuffers().  No references should be made to the current frameData,
	// so new scenes and GUIs can be built up in parallel with the rendering.
	virtual void			RenderCommandBuffers() = 0;

	// aviDemo uses this.
	// Will automatically tile render large screen shots if necessary
	// Samples is the number of jittered frames for anti-aliasing
	// If ref == NULL, common->UpdateScreen will be used
	// This will perform swapbuffers, so it is NOT an approppriate way to
	// generate image files that happen during gameplay, as for savegame
	// markers.  Use WriteRender() instead.
	virtual void			TakeScreenshot( int width, int height, const char *fileName, int samples, struct renderView_t *ref ) = 0;

	// the render output can be cropped down to a subset of the real screen, as
	// for save-game reviews and split-screen multiplayer.  Users of the renderer
	// will not know the actual pixel size of the area they are rendering to

	// the x,y,width,height values are in virtual SCREEN_WIDTH / SCREEN_HEIGHT coordinates

	virtual void			CaptureRenderToImage( const char *imageName, bool clearColorAfterCopy = false ) = 0;
	// fixAlpha will set all the alpha channel values to 0xff, which allows screen captures
	// to use the default tga loading code without having dimmed down areas in many places
	virtual void			CaptureRenderToFile( const char *fileName, bool fixAlpha = false ) = 0;

	virtual void			GetDefaultViewport( class idScreenRect & viewport ) const = 0;
	virtual void			PerformResolutionScaling( int & newWidth, int & newHeight ) = 0;

	virtual void			ReloadSurface() = 0;

	virtual void			PrintMemInfo( MemInfo_t * mi ) = 0;
	virtual void			PrintRenderEntityDefs() = 0;
	virtual void			PrintRenderLightDefs() = 0;
};

extern idRenderSystem *			renderSystem;

#endif /* !__RENDERER_H__ */
