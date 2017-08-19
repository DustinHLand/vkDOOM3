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
#include "../Image.h"
#include "../RenderBackend.h"
#include "Allocator_VK.h"
#include "Staging_VK.h"

int						idImage::m_garbageIndex = 0;
#if defined( ID_USE_AMD_ALLOCATOR )
idList< VmaAllocation > idImage::m_allocationGarbage[ NUM_FRAME_DATA ];
#else
idList< vulkanAllocation_t > idImage::m_allocationGarbage[ NUM_FRAME_DATA ];
#endif
idList< VkImage >		idImage::m_imageGarbage[ NUM_FRAME_DATA ];
idList< VkImageView >	idImage::m_viewGarbage[ NUM_FRAME_DATA ];
idList< VkSampler >		idImage::m_samplerGarbage[ NUM_FRAME_DATA ];

/*
====================
VK_GetFormatFromTextureFormat
====================
*/
VkFormat VK_GetFormatFromTextureFormat( const textureFormat_t format ) {
	switch ( format ) {
		case FMT_RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
		case FMT_XRGB8: return VK_FORMAT_R8G8B8_UNORM;
		case FMT_ALPHA: return VK_FORMAT_R8_UNORM;
		case FMT_L8A8: return VK_FORMAT_R8G8_UNORM;
		case FMT_LUM8: return VK_FORMAT_R8_UNORM;
		case FMT_INT8: return VK_FORMAT_R8_UNORM;
		case FMT_DXT1: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
		case FMT_DXT5: return VK_FORMAT_BC3_UNORM_BLOCK;
		case FMT_DEPTH: return vkcontext.depthFormat;
		case FMT_X16: return VK_FORMAT_R16_UNORM;
		case FMT_Y16_X16: return VK_FORMAT_R16G16_UNORM;
		case FMT_RGB565: return VK_FORMAT_R5G6B5_UNORM_PACK16;
		default:
			return VK_FORMAT_UNDEFINED;
	}
}

/*
====================
VK_GetComponentMappingFromTextureFormat
====================
*/
VkComponentMapping VK_GetComponentMappingFromTextureFormat( const textureFormat_t format, textureColor_t color ) {
	VkComponentMapping componentMapping = {
		VK_COMPONENT_SWIZZLE_ZERO,
		VK_COMPONENT_SWIZZLE_ZERO,
		VK_COMPONENT_SWIZZLE_ZERO,
		VK_COMPONENT_SWIZZLE_ZERO
	};

	if ( color == CFM_GREEN_ALPHA ) {
		componentMapping.r = VK_COMPONENT_SWIZZLE_ONE;
		componentMapping.g = VK_COMPONENT_SWIZZLE_ONE;
		componentMapping.b = VK_COMPONENT_SWIZZLE_ONE;
		componentMapping.a = VK_COMPONENT_SWIZZLE_G;
		return componentMapping;
	}

	switch ( format ) {
		case FMT_LUM8:
			componentMapping.r = VK_COMPONENT_SWIZZLE_R;
			componentMapping.g = VK_COMPONENT_SWIZZLE_R;
			componentMapping.b = VK_COMPONENT_SWIZZLE_R;
			componentMapping.a = VK_COMPONENT_SWIZZLE_ONE;
			break;
		case FMT_L8A8:
			componentMapping.r = VK_COMPONENT_SWIZZLE_R;
			componentMapping.g = VK_COMPONENT_SWIZZLE_R;
			componentMapping.b = VK_COMPONENT_SWIZZLE_R;
			componentMapping.a = VK_COMPONENT_SWIZZLE_G;
			break;
		case FMT_ALPHA:
			componentMapping.r = VK_COMPONENT_SWIZZLE_ONE;
			componentMapping.g = VK_COMPONENT_SWIZZLE_ONE;
			componentMapping.b = VK_COMPONENT_SWIZZLE_ONE;
			componentMapping.a = VK_COMPONENT_SWIZZLE_R;
			break;
		case FMT_INT8:
			componentMapping.r = VK_COMPONENT_SWIZZLE_R;
			componentMapping.g = VK_COMPONENT_SWIZZLE_R;
			componentMapping.b = VK_COMPONENT_SWIZZLE_R;
			componentMapping.a = VK_COMPONENT_SWIZZLE_R;
			break;
		default:
			componentMapping.r = VK_COMPONENT_SWIZZLE_R;
			componentMapping.g = VK_COMPONENT_SWIZZLE_G;
			componentMapping.b = VK_COMPONENT_SWIZZLE_B;
			componentMapping.a = VK_COMPONENT_SWIZZLE_A;
			break;
	}

	return componentMapping;
}

/*
====================
idImage::idImage
====================
*/
idImage::idImage( const char * name ) : m_imgName( name ) {
	m_bIsSwapChainImage = false;
	m_internalFormat = VK_FORMAT_UNDEFINED;
	m_image = VK_NULL_HANDLE;
	m_view = VK_NULL_HANDLE;
	m_layout = VK_IMAGE_LAYOUT_GENERAL;
	m_sampler = VK_NULL_HANDLE;
	m_generatorFunction = NULL;
	m_filter = TF_DEFAULT;
	m_repeat = TR_REPEAT;
	m_usage = TD_DEFAULT;
	m_cubeFiles = CF_2D;

	m_referencedOutsideLevelLoad = false;
	m_levelLoadReferenced = false;
	m_sourceFileTime = FILE_NOT_FOUND_TIMESTAMP;
	m_binaryFileTime = FILE_NOT_FOUND_TIMESTAMP;
	m_refCount = 0;
}

/*
====================
idImage::~idImage
====================
*/
idImage::~idImage() {
	if ( !m_bIsSwapChainImage ) {
		PurgeImage();
	}
}

/*
====================
idImage::IsLoaded
====================
*/
bool idImage::IsLoaded() const { 
	return m_image != VK_NULL_HANDLE; // TODO_VK maybe do something better than this.
}

/*
====================
idImage::CreateFromSwapImage
====================
*/
void idImage::CreateFromSwapImage( VkImage image, VkImageView imageView, VkFormat format, const VkExtent2D & extent ) {
	m_image = image;
	m_view = imageView;
	m_internalFormat = format;
	m_opts.textureType = TT_2D;
	m_opts.format = FMT_RGBA8;
	m_opts.numLevels = 1;
	m_opts.width = extent.width;
	m_opts.height = extent.height;
	m_bIsSwapChainImage = true;

	// TODO_VK may need to setup more state here.
}

/*
====================
idImage::CreateSampler
====================
*/
void idImage::CreateSampler() {
	VkSamplerCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.maxAnisotropy = 1.0f;
	createInfo.anisotropyEnable = VK_FALSE;
	createInfo.compareEnable = ( m_opts.format == FMT_DEPTH );
	createInfo.compareOp = ( m_opts.format == FMT_DEPTH ) ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_NEVER;

	switch ( m_filter ) {
		case TF_DEFAULT:
		case TF_LINEAR:
			createInfo.minFilter = VK_FILTER_LINEAR;
			createInfo.magFilter = VK_FILTER_LINEAR;
			createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			break;
		case TF_NEAREST:
			createInfo.minFilter = VK_FILTER_NEAREST;
			createInfo.magFilter = VK_FILTER_NEAREST;
			createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		default:
			idLib::FatalError( "idImage::CreateSampler: unrecognized texture filter %d", m_filter );
	}

	switch ( m_repeat ) {
		case TR_REPEAT:
			createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			break;
		case TR_CLAMP:
			createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			break;
		case TR_CLAMP_TO_ZERO_ALPHA:
			createInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
			createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			break;
		case TR_CLAMP_TO_ZERO:
			createInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
			createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			break;
		default:
			idLib::FatalError( "idImage::CreateSampler: unrecognized texture repeat mode %d", m_repeat );
	}

	ID_VK_CHECK( vkCreateSampler( vkcontext.device, &createInfo, NULL, &m_sampler ) );
}

/*
====================
idImage::EmptyGarbage
====================
*/
void idImage::EmptyGarbage() {
	m_garbageIndex = ( m_garbageIndex + 1 ) % NUM_FRAME_DATA;

#if defined( ID_USE_AMD_ALLOCATOR )
	idList< VmaAllocation > & allocationsToFree = m_allocationGarbage[ m_garbageIndex ];
#else
	idList< vulkanAllocation_t > & allocationsToFree = m_allocationGarbage[ m_garbageIndex ];
#endif
	idList< VkImage > & imagesToFree = m_imageGarbage[ m_garbageIndex ];
	idList< VkImageView > & viewsToFree = m_viewGarbage[ m_garbageIndex ];
	idList< VkSampler > & samplersToFree = m_samplerGarbage[ m_garbageIndex ];

#if defined( ID_USE_AMD_ALLOCATOR )
	const int numAllocations = allocationsToFree.Num();
	for ( int i = 0; i < numAllocations; ++i ) {
		vmaDestroyImage( vmaAllocator, imagesToFree[ i ], allocationsToFree[ i ] );
	}
#else
	const int numAllocations = allocationsToFree.Num();
	for ( int i = 0; i < numAllocations; ++i ) {
		vulkanAllocator.Free( allocationsToFree[ i ] );
	}

	const int numImages = imagesToFree.Num();
	for ( int i = 0; i < numImages; ++i ) {
		vkDestroyImage( vkcontext.device, imagesToFree[ i ], NULL );
	}
#endif

	const int numViews = viewsToFree.Num();
	for ( int i = 0; i < numViews; ++i ) {
		vkDestroyImageView( vkcontext.device, viewsToFree[ i ], NULL );
	}

	const int numSamplers = samplersToFree.Num();
	for ( int i = 0; i < numSamplers; ++i ) {
		vkDestroySampler( vkcontext.device, samplersToFree[ i ], NULL );
	}

	allocationsToFree.Clear();
	imagesToFree.Clear();
	viewsToFree.Clear();
	samplersToFree.Clear();
}

/*
====================
idImage::AllocImage
====================
*/
void idImage::AllocImage() {
	PurgeImage();

	m_internalFormat = VK_GetFormatFromTextureFormat( m_opts.format );

	// Create Sampler
	CreateSampler();

	VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;
	if ( m_opts.format == FMT_DEPTH ) {
		usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	} else {
		usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	// Create Image
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.flags = ( m_opts.textureType == TT_CUBIC ) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT: 0;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = m_internalFormat;
	imageCreateInfo.extent.width = m_opts.width;
	imageCreateInfo.extent.height = m_opts.height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = m_opts.numLevels;
	imageCreateInfo.arrayLayers = ( m_opts.textureType == TT_CUBIC ) ? 6 : 1;
	imageCreateInfo.samples = static_cast< VkSampleCountFlagBits >( m_opts.samples );
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = usageFlags;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

#if defined( ID_USE_AMD_ALLOCATOR )
	VmaMemoryRequirements vmaReq = {};
	vmaReq.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	ID_VK_CHECK( vmaCreateImage( vmaAllocator, &imageCreateInfo, &vmaReq, &m_image, &m_allocation, NULL ) );
#else
	ID_VK_CHECK( vkCreateImage( vkcontext.device, &imageCreateInfo, NULL, &m_image ) );

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements( vkcontext.device, m_image, &memoryRequirements );

	m_allocation = vulkanAllocator.Allocate( 
		memoryRequirements.size,
		memoryRequirements.alignment,
		memoryRequirements.memoryTypeBits, 
		VULKAN_MEMORY_USAGE_GPU_ONLY,
		VULKAN_ALLOCATION_TYPE_IMAGE_OPTIMAL );

	ID_VK_CHECK( vkBindImageMemory( vkcontext.device, m_image, m_allocation.deviceMemory, m_allocation.offset ) );
#endif

	// Create Image View
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = m_image;
	viewCreateInfo.viewType = ( m_opts.textureType == TT_CUBIC ) ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = m_internalFormat;
	viewCreateInfo.components = VK_GetComponentMappingFromTextureFormat( m_opts.format, m_opts.colorFormat );
	viewCreateInfo.subresourceRange.aspectMask = ( m_opts.format == FMT_DEPTH ) ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	viewCreateInfo.subresourceRange.levelCount = m_opts.numLevels;
	viewCreateInfo.subresourceRange.layerCount = ( m_opts.textureType == TT_CUBIC ) ? 6 : 1;
	viewCreateInfo.subresourceRange.baseMipLevel = 0;
	
	ID_VK_CHECK( vkCreateImageView( vkcontext.device, &viewCreateInfo, NULL, &m_view ) );
}

/*
====================
idImage::PurgeImage
====================
*/
void idImage::PurgeImage() {
	if ( m_sampler != VK_NULL_HANDLE ) {
		m_samplerGarbage[ m_garbageIndex ].Append( m_sampler );
		m_sampler = VK_NULL_HANDLE;
	}

	if ( m_image != VK_NULL_HANDLE ) {
		m_allocationGarbage[ m_garbageIndex ].Append( m_allocation );
		m_viewGarbage[ m_garbageIndex ].Append( m_view );
		m_imageGarbage[ m_garbageIndex ].Append( m_image );

#if defined( ID_USE_AMD_ALLOCATOR )
		m_allocation = NULL;
#else
		m_allocation = vulkanAllocation_t();
#endif

		m_view = VK_NULL_HANDLE;
		m_image = VK_NULL_HANDLE;
	}
}

/*
====================
idImage::SetSamplerState
====================
*/
void idImage::SetSamplerState( textureFilter_t filter, textureRepeat_t repeat ) {

}

/*
====================
idImage::SubImageUpload
====================
*/
void idImage::SubImageUpload( int mipLevel, int x, int y, int z, int width, int height, const void * pic, int pixelPitch ) {
	assert( x >= 0 && y >= 0 && mipLevel >= 0 && width >= 0 && height >= 0 && mipLevel < m_opts.numLevels );

	if ( IsCompressed() ) {
		width = ( width + 3 ) & ~3;
		height = ( height + 3 ) & ~3;
	}

	int size = width * height * BitsForFormat( m_opts.format ) / 8;

	VkBuffer buffer;
	VkCommandBuffer commandBuffer;
	int offset = 0;
	byte * data = stagingManager.Stage( size, 16, commandBuffer, buffer, offset );
	if ( m_opts.format == FMT_RGB565 ) {
		byte * imgData = (byte *)pic;
		for ( int i = 0; i < size; i += 2 ) {
			data[ i ] = imgData[ i + 1 ];
			data[ i + 1 ] = imgData[ i ];
		}
	} else {
		memcpy( data, pic, size );
	}

	VkBufferImageCopy imgCopy = {};
	imgCopy.bufferOffset = offset;
	imgCopy.bufferRowLength = pixelPitch;
	imgCopy.bufferImageHeight = height;
	imgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgCopy.imageSubresource.layerCount = 1;
	imgCopy.imageSubresource.mipLevel = mipLevel;
	imgCopy.imageSubresource.baseArrayLayer = z;
	imgCopy.imageOffset.x = x;
	imgCopy.imageOffset.y = y;
	imgCopy.imageOffset.z = 0;
	imgCopy.imageExtent.width = width;
	imgCopy.imageExtent.height = height;
	imgCopy.imageExtent.depth = 1;

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = m_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = m_opts.numLevels;
	barrier.subresourceRange.baseArrayLayer = z;
	barrier.subresourceRange.layerCount = 1;
	
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier );

	vkCmdCopyBufferToImage( commandBuffer, buffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imgCopy );

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, NULL, 0, NULL, 1, &barrier );

	m_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}