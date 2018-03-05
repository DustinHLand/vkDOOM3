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

#ifndef __RENDERER_BACKEND_H__
#define __RENDERER_BACKEND_H__

struct tmu_t {
	unsigned int	current2DMap;
	unsigned int	currentCubeMap;
};

const int MAX_MULTITEXTURE_UNITS =	8;

enum stencilFace_t {
	STENCIL_FACE_FRONT,
	STENCIL_FACE_BACK,
	STENCIL_FACE_NUM
};

struct backEndCounters_t {
	backEndCounters_t() :
		c_surfaces( 0 ),
		c_shaders( 0 ),
		c_drawElements( 0 ),
		c_drawIndexes( 0 ),
		c_shadowElements( 0 ),
		c_shadowIndexes( 0 ),
		c_copyFrameBuffer( 0 ),
		c_overDraw( 0 ),
		totalMicroSec( 0 ),
		shadowMicroSec( 0 ),
		interactionMicroSec( 0 ),
		shaderPassMicroSec( 0 ),
		gpuMicroSec( 0 ) {
	}

	int		c_surfaces;
	int		c_shaders;

	int		c_drawElements;
	int		c_drawIndexes;

	int		c_shadowElements;
	int		c_shadowIndexes;

	int		c_copyFrameBuffer;

	float	c_overDraw;

	uint64	totalMicroSec;		// total microseconds for backend run
	uint64	shadowMicroSec;
	uint64	depthMicroSec;
	uint64	interactionMicroSec;
	uint64	shaderPassMicroSec;
	uint64	gpuMicroSec;
};

struct gfxImpParms_t {
	int		x;				// ignored in fullscreen
	int		y;				// ignored in fullscreen
	int		width;
	int		height;
	int		fullScreen;		// 0 = windowed, otherwise 1 based monitor number to go full screen on
							// -1 = borderless window for spanning multiple displays
	int		displayHz;
	int		multiSamples;
};

#define MAX_DEBUG_LINES		16384
#define MAX_DEBUG_TEXT		512
#define MAX_DEBUG_POLYGONS	8192

struct debugLine_t {
	idVec4		rgb;
	idVec3		start;
	idVec3		end;
	bool		depthTest;
	int			lifeTime;
};

struct debugText_t {
	idStr		text;
	idVec3		origin;
	float		scale;
	idVec4		color;
	idMat3		viewAxis;
	int			align;
	int			lifeTime;
	bool		depthTest;
};

struct debugPolygon_t {
	idVec4		rgb;
	idWinding	winding;
	bool		depthTest;
	int			lifeTime;
};

void RB_SetMVP( const idRenderMatrix & mvp );
void RB_SetVertexColorParms( stageVertexColor_t svc );
void RB_GetShaderTextureMatrix( const float *shaderRegisters, const textureStage_t *texture, float matrix[16] );
void RB_LoadShaderTextureMatrix( const float *shaderRegisters, const textureStage_t *texture );
void RB_BakeTextureMatrixIntoTexgen( idPlane lightProject[3], const float *textureMatrix );
void RB_SetupInteractionStage( const shaderStage_t *surfaceStage, const float *surfaceRegs, const float lightColor[4], idVec4 matrix[2], float color[4] );

struct GPUInfo_t {
	VkPhysicalDevice					device;
	VkPhysicalDeviceProperties			props;
	VkPhysicalDeviceMemoryProperties	memProps;
	VkPhysicalDeviceFeatures			features;
	VkSurfaceCapabilitiesKHR			surfaceCaps;
	idList< VkSurfaceFormatKHR >		surfaceFormats;
	idList< VkPresentModeKHR >			presentModes;
	idList< VkQueueFamilyProperties >	queueFamilyProps;
	idList< VkExtensionProperties >		extensionProps;
};

struct vulkanContext_t {
	vertCacheHandle_t				jointCacheHandle;

	GPUInfo_t						gpu;

	VkDevice						device;
	int								graphicsFamilyIdx;
	int								presentFamilyIdx;
	VkQueue							graphicsQueue;
	VkQueue							presentQueue;

	VkFormat						depthFormat;
	VkRenderPass					renderPass;
	VkPipelineCache					pipelineCache;
	VkSampleCountFlagBits			sampleCount;
	bool							supersampling;

	idArray< idImage *, MAX_IMAGE_PARMS > imageParms;
};

extern vulkanContext_t vkcontext;

/*
===========================================================================

idRenderBackend

all state modified by the back end is separated from the front end state

===========================================================================
*/
class idRenderBackend {
public:
	idRenderBackend();
	~idRenderBackend();

	void				Init();
	void				Shutdown();

	void				Execute( const int numCmds, const idArray< renderCommand_t, 16 > & renderCommands );
	void				BlockingSwapBuffers();

private:
	void				DrawElementsWithCounters( const drawSurf_t * surf );
	void				DrawStencilShadowPass( const drawSurf_t * drawSurf, const bool renderZPass );

	void				SetColorMappings();
	void				CheckCVars();
	void				ResizeImages();

	void				DrawView( const renderCommand_t & cmd );
	void				CopyRender( const renderCommand_t & cmd );

	void				BindVariableStageImage( const textureStage_t * texture, const float * shaderRegisters );
	void				PrepareStageTexturing( const shaderStage_t * pStage, const drawSurf_t * surf );

	void				FillDepthBufferGeneric( const drawSurf_t * const * drawSurfs, int numDrawSurfs );
	void				FillDepthBufferFast( drawSurf_t ** drawSurfs, int numDrawSurfs );

	void				T_BlendLight( const drawSurf_t * drawSurfs, const viewLight_t * vLight );
	void				BlendLight( const drawSurf_t * drawSurfs, const drawSurf_t * drawSurfs2, const viewLight_t * vLight );
	void				T_BasicFog( const drawSurf_t * drawSurfs, const idPlane fogPlanes[ 4 ], const idRenderMatrix * inverseBaseLightProject );
	void				FogPass( const drawSurf_t * drawSurfs,  const drawSurf_t * drawSurfs2, const viewLight_t * vLight );
	void				FogAllLights();

	void				DrawInteractions();
	void				DrawSingleInteraction( drawInteraction_t * din );
	int					DrawShaderPasses( const drawSurf_t * const * const drawSurfs, const int numDrawSurfs );

	void				RenderInteractions( const drawSurf_t * surfList, const viewLight_t * vLight, int depthFunc, bool performStencilTest, bool useLightDepthBounds );

	void				StencilShadowPass( const drawSurf_t * drawSurfs, const viewLight_t * vLight );
	void				StencilSelectLight( const viewLight_t * vLight );

private:
	void				GL_StartFrame();
	void				GL_EndFrame();

	uint64				GL_GetCurrentStateMinusStencil() const;
	void				GL_SetDefaultState();
	void				GL_State( uint64 stateBits, bool forceGlState = false );

	void				GL_BindTexture( int index, idImage * image );

	void				GL_CopyFrameBuffer( idImage * image, int x, int y, int imageWidth, int imageHeight );
	void				GL_Clear( bool color, bool depth, bool stencil, byte stencilValue, float r, float g, float b, float a );

	void				GL_DepthBoundsTest( const float zmin, const float zmax );
	void				GL_PolygonOffset( float scale, float bias );

	void				GL_Scissor( int x /* left*/, int y /* bottom */, int w, int h );
	void				GL_Viewport( int x /* left */, int y /* bottom */, int w, int h );
	ID_INLINE void		GL_Scissor( const idScreenRect & rect ) { GL_Scissor( rect.x1, rect.y1, rect.x2 - rect.x1 + 1, rect.y2 - rect.y1 + 1 ); }
	ID_INLINE void		GL_Viewport( const idScreenRect & rect ) { GL_Viewport( rect.x1, rect.y1, rect.x2 - rect.x1 + 1, rect.y2 - rect.y1 + 1 ); }

	void				GL_Color( float r, float g, float b, float a );
	ID_INLINE void		GL_Color( float r, float g, float b ) { GL_Color( r, g, b, 1.0f ); }
	void				GL_Color( float * color );

private:
	void				Clear();

	void				CreateInstance();

	void				SelectSuitablePhysicalDevice();

	void				CreateLogicalDeviceAndQueues();

	void				CreateSemaphores();

	void				CreateQueryPool();

	void				CreateSurface();

	void				CreateCommandPool();
	void				CreateCommandBuffer();

	void				CreateSwapChain();
	void				DestroySwapChain();

	void				CreateRenderTargets();
	void				DestroyRenderTargets();

	void				CreateRenderPass();

	void				CreateFrameBuffers();
	void				DestroyFrameBuffers();

private:
	void				DBG_SimpleSurfaceSetup( const drawSurf_t * drawSurf );
	void				DBG_SimpleWorldSetup();
	void				DBG_ShowDestinationAlpha();
	void				DBG_ColorByStencilBuffer();
	void				DBG_ShowOverdraw();
	void				DBG_ShowIntensity();
	void				DBG_ShowDepthBuffer();
	void				DBG_ShowLightCount();
	void				DBG_EnterWeaponDepthHack();
	void				DBG_EnterModelDepthHack( float depth );
	void				DBG_LeaveDepthHack();
	void				DBG_RenderDrawSurfListWithFunction( drawSurf_t **drawSurfs, int numDrawSurfs );
	void				DBG_ShowSilhouette();
	void				DBG_ShowTris( drawSurf_t **drawSurfs, int numDrawSurfs );
	void				DBG_ShowSurfaceInfo( drawSurf_t **drawSurfs, int numDrawSurfs );
	void				DBG_ShowViewEntitys( viewEntity_t *vModels );
	void				DBG_ShowTexturePolarity( drawSurf_t **drawSurfs, int numDrawSurfs );
	void				DBG_ShowUnsmoothedTangents( drawSurf_t **drawSurfs, int numDrawSurfs );
	void				DBG_ShowTangentSpace( drawSurf_t **drawSurfs, int numDrawSurfs );
	void				DBG_ShowVertexColor( drawSurf_t **drawSurfs, int numDrawSurfs );
	void				DBG_ShowNormals( drawSurf_t **drawSurfs, int numDrawSurfs );
	void				DBG_ShowTextureVectors( drawSurf_t **drawSurfs, int numDrawSurfs );
	void				DBG_ShowDominantTris( drawSurf_t **drawSurfs, int numDrawSurfs );
	void				DBG_ShowEdges( drawSurf_t **drawSurfs, int numDrawSurfs );
	void				DBG_ShowLights();
	void				DBG_ShowPortals();
	void				DBG_ShowDebugText();
	void				DBG_ShowDebugLines();
	void				DBG_ShowDebugPolygons();
	void				DBG_ShowCenterOfProjection();
	void				DBG_TestGamma();
	void				DBG_TestGammaBias();
	void				DBG_TestImage();
	void				DBG_ShowTrace( drawSurf_t **drawSurfs, int numDrawSurfs );
	void				DBG_RenderDebugTools( drawSurf_t **drawSurfs, int numDrawSurfs );

public:
	backEndCounters_t	m_pc;

	// surfaces used for code-based drawing
	drawSurf_t			m_unitSquareSurface;
	drawSurf_t			m_zeroOneCubeSurface;
	drawSurf_t			m_testImageSurface;

private:
	uint64				m_glStateBits;

	const viewDef_t *	m_viewDef;
	
	const viewEntity_t*	m_currentSpace;			// for detecting when a matrix must change
	idScreenRect		m_currentScissor;		// for scissor clipping, local inside renderView viewport

	bool				m_currentRenderCopied;	// true if any material has already referenced _currentRender

	idRenderMatrix		m_prevMVP;				// world MVP from previous frame for motion blur

	unsigned short		m_gammaTable[ 256 ];	// brightness / gamma modify this

private:
	uint64							m_counter;
	uint32							m_currentFrameData;

	VkInstance						m_instance;
	VkPhysicalDevice				m_physicalDevice;

	idList< const char * >			m_instanceExtensions;
	idList< const char * >			m_deviceExtensions;
	idList< const char * >			m_validationLayers;

	VkSurfaceKHR					m_surface;
	VkPresentModeKHR				m_presentMode;

	int								m_fullscreen;
	VkSwapchainKHR					m_swapchain;
	VkFormat						m_swapchainFormat;
	VkExtent2D						m_swapchainExtent;
	uint32							m_currentSwapIndex;
	VkImage							m_msaaImage;
	VkImageView						m_msaaImageView;
#if defined( ID_USE_AMD_ALLOCATOR )
	VmaAllocation					m_msaaVmaAllocation;
	VmaAllocationInfo				m_msaaAllocation;
#else
	vulkanAllocation_t				m_msaaAllocation;
#endif
	VkCommandPool					m_commandPool;

	idArray< VkImage, NUM_FRAME_DATA >			m_swapchainImages;
	idArray< VkImageView, NUM_FRAME_DATA >		m_swapchainViews;
	idArray< VkFramebuffer, NUM_FRAME_DATA >	m_frameBuffers;
	
	idArray< VkCommandBuffer, NUM_FRAME_DATA >	m_commandBuffers;
	idArray< VkFence, NUM_FRAME_DATA >			m_commandBufferFences;
	idArray< bool, NUM_FRAME_DATA >				m_commandBufferRecorded;
	idArray< VkSemaphore, NUM_FRAME_DATA >		m_acquireSemaphores;
	idArray< VkSemaphore, NUM_FRAME_DATA >		m_renderCompleteSemaphores;

	idArray< uint32, NUM_FRAME_DATA >			m_queryIndex;
	idArray< idArray< uint64, NUM_TIMESTAMP_QUERIES >, NUM_FRAME_DATA >	m_queryResults;
	idArray< VkQueryPool, NUM_FRAME_DATA >		m_queryPools;
};

#endif