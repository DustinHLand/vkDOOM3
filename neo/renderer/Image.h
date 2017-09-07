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

enum textureType_t {
	TT_DISABLED,
	TT_2D,
	TT_CUBIC
};

/*
================================================
The internal *Texture Format Types*, ::textureFormat_t, are:
================================================
*/
enum textureFormat_t {
	FMT_NONE,

	//------------------------
	// Standard color image formats
	//------------------------

	FMT_RGBA8,			// 32 bpp
	FMT_XRGB8,			// 32 bpp

	//------------------------
	// Alpha channel only
	//------------------------

	// Alpha ends up being the same as L8A8 in our current implementation, because straight 
	// alpha gives 0 for color, but we want 1.
	FMT_ALPHA,		

	//------------------------
	// Luminance replicates the value across RGB with a constant A of 255
	// Intensity replicates the value across RGBA
	//------------------------

	FMT_L8A8,			// 16 bpp
	FMT_LUM8,			//  8 bpp
	FMT_INT8,			//  8 bpp

	//------------------------
	// Compressed texture formats
	//------------------------

	FMT_DXT1,			// 4 bpp
	FMT_DXT5,			// 8 bpp

	//------------------------
	// Depth buffer formats
	//------------------------

	FMT_DEPTH,			// 24 bpp

	//------------------------
	//
	//------------------------

	FMT_X16,			// 16 bpp
	FMT_Y16_X16,		// 32 bpp
	FMT_RGB565,			// 16 bpp
};

int BitsForFormat( textureFormat_t format );

enum textureSamples_t {
	SAMPLE_1	= BIT( 0 ),
	SAMPLE_2	= BIT( 1 ),
	SAMPLE_4	= BIT( 2 ),
	SAMPLE_8	= BIT( 3 ),
	SAMPLE_16	= BIT( 4 )
};

/*
================================================
DXT5 color formats
================================================
*/
enum textureColor_t {
	CFM_DEFAULT,			// RGBA
	CFM_NORMAL_DXT5,		// XY format and use the fast DXT5 compressor
	CFM_YCOCG_DXT5,			// convert RGBA to CoCg_Y format
	CFM_GREEN_ALPHA			// Copy the alpha channel to green
};

/*
================================================
idImageOpts hold parameters for texture operations.
================================================
*/
class idImageOpts {
public:
	idImageOpts();

	bool	operator==( const idImageOpts & opts );

	//---------------------------------------------------
	// these determine the physical memory size and layout
	//---------------------------------------------------

	textureType_t		textureType;
	textureFormat_t		format;
	textureColor_t		colorFormat;
	textureSamples_t	samples;
	textureUsage_t		usage;			// Used to determine the type of compression to use
	int					width;
	int					height;			// not needed for cube maps
	int					numLevels;		// if 0, will be 1 for NEAREST / LINEAR filters, otherwise based on size
	bool				gammaMips;		// if true, mips will be generated with gamma correction
};

/*
========================
idImageOpts::idImageOpts
========================
*/
ID_INLINE idImageOpts::idImageOpts() {
	format			= FMT_NONE;
	colorFormat		= CFM_DEFAULT;
	samples			= SAMPLE_1;
	usage			= TD_DEFAULT;
	width			= 0;
	height			= 0;
	numLevels		= 0;
	textureType		= TT_2D;
	gammaMips		= false;

};

/*
========================
idImageOpts::operator==
========================
*/
ID_INLINE bool idImageOpts::operator==( const idImageOpts & opts ) {
	return ( memcmp( this, &opts, sizeof( *this ) ) == 0 );
}

/*
====================================================================

IMAGE

idImage have a one to one correspondance with GL/DX/GCM textures.

No texture is ever used that does not have a corresponding idImage.

====================================================================
*/

#include "BinaryImage.h"

#define	MAX_IMAGE_NAME	256

class idImage {
public:
	idImage( const char * name );
	~idImage();

	const char *	GetName() const { return m_imgName; }

	// estimates size of the GL image based on dimensions and storage type
	int			StorageSize() const;

	// print a one line summary of the image
	void		Print() const;

	// check for changed timestamp on disk and reload if necessary
	void		Reload( bool force );

	void		AddReference() { m_refCount++; };

	const idImageOpts &	GetOpts() const { return m_opts; }
	int			GetUploadWidth() const { return m_opts.width; }
	int			GetUploadHeight() const { return m_opts.height; }

	void		SetReferencedOutsideLevelLoad() { m_referencedOutsideLevelLoad = true; }
	void		SetReferencedInsideLevelLoad() { m_levelLoadReferenced = true; }
	void		ActuallyLoadImage( bool fromBackEnd );
	//---------------------------------------------
	// Platform specific implementations
	//---------------------------------------------

#if defined( ID_VULKAN )
	VkImage			GetImage() const { return m_image; }
	VkImageView		GetView() const { return m_view; }
	VkImageLayout	GetLayout() const { return m_layout; }
	VkSampler		GetSampler() const { return m_sampler; }
	VkRenderPass	GetRenderPass() const { return m_renderPass; }
	VkFramebuffer	GetFrameBuffer() const { return m_frameBuffer; }

	void		CreateFromSwapImage( VkImage image, VkImageView view, VkFormat format, int width, int height );
#endif

	void		AllocImage( const idImageOpts &imgOpts, textureFilter_t filter, textureRepeat_t repeat );
	void		PurgeImage();

	bool		IsCompressed() const { return ( m_opts.format == FMT_DXT1 || m_opts.format == FMT_DXT5 ); }

	bool		IsLoaded() const;

	static void	GetGeneratedName( idStr &_name, const textureUsage_t &_usage, const cubeFiles_t &_cube );

	// used by callback functions to specify the actual data
	// data goes from the bottom to the top line of the image, as OpenGL expects it
	// These perform an implicit Bind() on the current texture unit
	// FIXME: should we implement cinematics this way, instead of with explicit calls?
	void		GenerateImage( 
					const byte * pic, 
					int width, int height, 
					textureFilter_t filter, 
					textureRepeat_t repeat, 
					textureUsage_t usage );
	void		GenerateCubeImage( 
					const byte *pic[6], 
					int size, 
					textureFilter_t filter, 
					textureUsage_t usage );

private:
	friend class idImageManager;
	friend class idRenderBackend;

	void		DeriveOpts();
	void		AllocImage();
	void		SetSamplerState( textureFilter_t filter, textureRepeat_t repeat );
	void		UploadScratchImage( const byte * data, int cols, int rows );

	// z is 0 for 2D textures, 0 - 5 for cube maps, and 0 - uploadDepth for 3D textures. Only 
	// one plane at a time of 3D textures can be uploaded. The data is assumed to be correct for 
	// the format, either bytes, halfFloats, floats, or DXT compressed. The data is assumed to 
	// be in OpenGL RGBA format, the consoles may have to reorganize. pixelPitch is only needed 
	// when updating from a source subrect. Width, height, and dest* are always in pixels, so 
	// they must be a multiple of four for dxt data.
	void		SubImageUpload( 
					int mipLevel, 
					int x, int y, int z, 
					int width, int height, 
					const void * pic, 
					int pixelPitch = 0 );

#if defined( ID_VULKAN )
	void		CreateSampler();
	void		CreateFrameBuffer();

	static void EmptyGarbage();
#endif

private:
	// parameters that define this image
	idStr				m_imgName;				// game path, including extension (except for cube maps), may be an image program
	cubeFiles_t			m_cubeFiles;			// If this is a cube map, and if so, what kind
	void				(*m_generatorFunction)( idImage *image, textureUsage_t usage );	// NULL for files
	idImageOpts			m_opts;					// Parameters that determine the storage method

	// Sampler settings
	textureFilter_t		m_filter;
	textureRepeat_t		m_repeat;

	bool				m_referencedOutsideLevelLoad;
	bool				m_levelLoadReferenced;	// for determining if it needs to be purged
	ID_TIME_T			m_sourceFileTime;		// the most recent of all images used in creation, for reloadImages command
	ID_TIME_T			m_binaryFileTime;		// the time stamp of the binary file

	int					m_refCount;				// overall ref count

#if defined( ID_VULKAN )
	bool				m_bIsSwapimage;
	VkFormat			m_internalFormat;
	VkImage				m_image;
	VkImageView			m_view;
	VkImageLayout		m_layout;
	VkSampler			m_sampler;
	VkRenderPass		m_renderPass;
	VkFramebuffer		m_frameBuffer;

	idImage *			m_depthAttachment;
	idImage *			m_resolveAttachment;
#endif

#if defined( ID_USE_AMD_ALLOCATOR )
	VmaAllocation		m_allocation;
	static idList< VmaAllocation >		m_allocationGarbage[ NUM_FRAME_DATA ];
#else
	vulkanAllocation_t	m_allocation;
	static idList< vulkanAllocation_t > m_allocationGarbage[ NUM_FRAME_DATA ];
#endif

	static int						m_garbageIndex;
	static idList< VkImage >		m_imageGarbage[ NUM_FRAME_DATA ];
	static idList< VkImageView >	m_viewGarbage[ NUM_FRAME_DATA ];
	static idList< VkSampler >		m_samplerGarbage[ NUM_FRAME_DATA ];
};

// data is RGBA
void	R_WriteTGA( const char *filename, const byte *data, int width, int height, bool flipVertical = false, const char * basePath = "fs_savepath" );
// data is in top-to-bottom raster order unless flipVertical is set



class idImageManager {
public:

	idImageManager() 
	{
		m_insideLevelLoad = false;
		m_preloadingMapImages = false;
	}

	void				Init();
	void				Shutdown();

	// If the exact combination of parameters has been asked for already, an existing
	// image will be returned, otherwise a new image will be created.
	// Be careful not to use the same image file with different filter / repeat / etc parameters
	// if possible, because it will cause a second copy to be loaded.
	// If the load fails for any reason, the image will be filled in with the default
	// grid pattern.
	// Will automatically execute image programs if needed.
	idImage *			ImageFromFile( const char *name,
							 textureFilter_t filter, textureRepeat_t repeat, textureUsage_t usage, cubeFiles_t cubeMap = CF_2D );

	// These images are for internal renderer use.  Names should start with "_".
	idImage *			ScratchImage( const char * name, const idImageOpts & opts );

	// look for a loaded image, whatever the parameters
	idImage *			GetImage( const char *name ) const;

	// The callback will be issued immediately, and later if images are reloaded or vid_restart
	// The callback function should call one of the idImage::Generate* functions to fill in the data
	idImage *			ImageFromFunction( const char *name, void (*generatorFunction)( idImage *image, textureUsage_t usage ), textureUsage_t usage = TD_DEFAULT );

	// purges all the images before a vid_restart
	void				PurgeAllImages();

	// reloads all appropriate images after a vid_restart
	void				ReloadImages( bool all );

	// reloads all the render targets 
	void				ReloadTargets();

	// Called only by renderSystem::BeginLevelLoad
	void				BeginLevelLoad();

	// Called only by renderSystem::EndLevelLoad
	void				EndLevelLoad();

	void				Preload( const idPreloadManifest &manifest, const bool & mapPreload );

	// Loads unloaded level images
	int					LoadLevelImages( bool pacifier );

	void				PrintMemInfo( MemInfo_t *mi );

	// built-in images
	void				CreateIntrinsicImages();

	idImage *			AllocImage( const char *name );

	// the image has to be already loaded ( most straightforward way would be through a FindMaterial )
	// texture filter / mipmapping / repeat won't be modified by the upload
	// returns false if the image wasn't found
	bool				UploadImage( const char * imageName, const byte * data, int width, int height );

	bool				ExcludePreloadImage( const char *name );

public:
	bool				m_insideLevelLoad;			// don't actually load images now
	bool				m_preloadingMapImages;		// unless this is set

	idImage *			m_defaultImage;
	idImage *			m_flatNormalMap;			// 128 128 255 in all pixels
	idImage *			m_alphaNotchImage;
	idImage *			m_whiteImage;				// full of 0xff
	idImage *			m_blackImage;				// full of 0x00
	idImage *			m_fogImage;					// increasing alpha is denser fog
	idImage *			m_fogEnterImage;			// adjust fogImage alpha based on terminator plane
	idImage *			m_noFalloffImage;
	idImage *			m_quadraticImage;
	idImage *			m_scratchImage;
	idImage *			m_scratchImage2;
	idImage *			m_currentRenderImage;		// for SS_POST_PROCESS shaders
	idImage *			m_accumImage;
	idImage *			m_loadingIconImage;
	idImage *			m_hellLoadingIconImage;

	//--------------------------------------------------------

	idList< idImage *, TAG_IDLIB_LIST_IMAGE > m_images;
	idHashIndex			m_imageHash;
};

extern idImageManager	*globalImages;		// pointer to global list for the rest of the system

int MakePowerOfTwo( int num );

/*
====================================================================

IMAGEPROCESS

FIXME: make an "imageBlock" type to hold byte*,width,height?
====================================================================
*/

byte *R_Dropsample( const byte *in, int inwidth, int inheight, int outwidth, int outheight );
byte *R_ResampleTexture( const byte *in, int inwidth, int inheight, int outwidth, int outheight );
byte *R_MipMapWithAlphaSpecularity( const byte *in, int width, int height );
byte *R_MipMapWithGamma( const byte *in, int width, int height );
byte *R_MipMap( const byte *in, int width, int height );

// these operate in-place on the provided pixels
void R_BlendOverTexture( byte *data, int pixelCount, const byte blend[4] );
void R_HorizontalFlip( byte *data, int width, int height );
void R_VerticalFlip( byte *data, int width, int height );
void R_RotatePic( byte *data, int width );

/*
====================================================================

IMAGEFILES

====================================================================
*/

void R_LoadImage( const char *name, byte **pic, int *width, int *height, ID_TIME_T *timestamp, bool makePowerOf2 );
// pic is in top to bottom raster format
bool R_LoadCubeImages( const char *cname, cubeFiles_t extensions, byte *pic[6], int *size, ID_TIME_T *timestamp );

/*
====================================================================

IMAGEPROGRAM

====================================================================
*/

void R_LoadImageProgram( const char *name, byte **pic, int *width, int *height, ID_TIME_T *timestamp, textureUsage_t * usage = NULL );
const char *R_ParsePastImageProgram( idLexer &src );

