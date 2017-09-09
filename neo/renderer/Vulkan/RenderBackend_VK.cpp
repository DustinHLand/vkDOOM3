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
#include "../../framework/Common_local.h"
#include "../../sys/win32/rc/doom_resource.h"
#include "../GLState.h"
#include "../GLMatrix.h"
#include "../RenderBackend.h"
#include "../Image.h"
#include "../ResolutionScale.h"
#include "../RenderLog.h"
#include "../../sys/win32/win_local.h"
#include "Allocator_VK.h"
#include "Staging_VK.h"

vulkanContext_t vkcontext;

idCVar r_vkEnableValidationLayers( "r_vkEnableValidationLayers", "0", CVAR_BOOL | CVAR_INIT, "" );

extern idCVar r_multiSamples;
extern idCVar r_skipRender;
extern idCVar r_skipShadows;
extern idCVar r_showShadows;
extern idCVar r_shadowPolygonFactor;
extern idCVar r_shadowPolygonOffset;
extern idCVar r_useScissor;
extern idCVar r_useShadowDepthBounds;
extern idCVar r_forceZPassStencilShadows;
extern idCVar r_useStencilShadowPreload;
extern idCVar r_singleTriangle;
extern idCVar r_useLightDepthBounds;
extern idCVar r_swapInterval;

void PrintState( uint64 stateBits );
void RpPrintState( uint64 stateBits );

static const int g_numInstanceExtensions = 2;
static const char * g_instanceExtensions[ g_numInstanceExtensions ] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME
};

static const int g_numDebugInstanceExtensions = 1;
static const char * g_debugInstanceExtensions[ g_numDebugInstanceExtensions ] = {
	VK_EXT_DEBUG_REPORT_EXTENSION_NAME
};

static const int g_numDeviceExtensions = 1;
static const char * g_deviceExtensions[ g_numDeviceExtensions ] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static const int g_numValidationLayers = 1;
static const char * g_validationLayers[ g_numValidationLayers ] = {
	"VK_LAYER_LUNARG_standard_validation"
};

static const char * renderProgBindingStrings[ BINDING_TYPE_MAX ] = {
	"ubo",
	"sampler"
};

#define ID_VK_ERROR_STRING( x ) case static_cast< int >( x ): return #x

/*
=============
VK_ErrorToString
=============
*/
const char * VK_ErrorToString( VkResult result ) {
	switch ( result ) {
		ID_VK_ERROR_STRING( VK_SUCCESS );
		ID_VK_ERROR_STRING( VK_NOT_READY );
		ID_VK_ERROR_STRING( VK_TIMEOUT );
		ID_VK_ERROR_STRING( VK_EVENT_SET );
		ID_VK_ERROR_STRING( VK_EVENT_RESET );
		ID_VK_ERROR_STRING( VK_INCOMPLETE );
		ID_VK_ERROR_STRING( VK_ERROR_OUT_OF_HOST_MEMORY );
		ID_VK_ERROR_STRING( VK_ERROR_OUT_OF_DEVICE_MEMORY );
		ID_VK_ERROR_STRING( VK_ERROR_INITIALIZATION_FAILED );
		ID_VK_ERROR_STRING( VK_ERROR_DEVICE_LOST );
		ID_VK_ERROR_STRING( VK_ERROR_MEMORY_MAP_FAILED );
		ID_VK_ERROR_STRING( VK_ERROR_LAYER_NOT_PRESENT );
		ID_VK_ERROR_STRING( VK_ERROR_EXTENSION_NOT_PRESENT );
		ID_VK_ERROR_STRING( VK_ERROR_FEATURE_NOT_PRESENT );
		ID_VK_ERROR_STRING( VK_ERROR_INCOMPATIBLE_DRIVER );
		ID_VK_ERROR_STRING( VK_ERROR_TOO_MANY_OBJECTS );
		ID_VK_ERROR_STRING( VK_ERROR_FORMAT_NOT_SUPPORTED );
		ID_VK_ERROR_STRING( VK_ERROR_SURFACE_LOST_KHR );
		ID_VK_ERROR_STRING( VK_ERROR_NATIVE_WINDOW_IN_USE_KHR );
		ID_VK_ERROR_STRING( VK_SUBOPTIMAL_KHR );
		ID_VK_ERROR_STRING( VK_ERROR_OUT_OF_DATE_KHR );
		ID_VK_ERROR_STRING( VK_ERROR_INCOMPATIBLE_DISPLAY_KHR );
		ID_VK_ERROR_STRING( VK_ERROR_VALIDATION_FAILED_EXT );
		ID_VK_ERROR_STRING( VK_ERROR_INVALID_SHADER_NV );
		ID_VK_ERROR_STRING( VK_RESULT_BEGIN_RANGE );
		ID_VK_ERROR_STRING( VK_RESULT_RANGE_SIZE );
		default: return "UNKNOWN";
	};
}

/*
===========================================================================

idRenderBackend

===========================================================================
*/

static VkDebugReportCallbackEXT debugReportCallback = VK_NULL_HANDLE;

/*
=============
DebugCallback
=============
*/
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback( 
	VkDebugReportFlagsEXT flags, 
	VkDebugReportObjectTypeEXT objType, 
	uint64 obj, size_t location, int32 code,
	const char * layerPrefix, const char * msg, void * userData ) {

	idLib::Printf( "VK_DEBUG::%s: %s flags=%d, objType=%d, obj=%llu, location=%lld, code=%d\n", 
		layerPrefix, msg, flags, objType, obj, location, code );

	return VK_FALSE;
}

/*
=============
CreateDebugReportCallback
=============
*/
static void CreateDebugReportCallback( VkInstance instance ) {
	VkDebugReportCallbackCreateInfoEXT callbackInfo = {};
	callbackInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	callbackInfo.flags = VK_DEBUG_REPORT_DEBUG_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;
	callbackInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT) DebugCallback;

	PFN_vkCreateDebugReportCallbackEXT func = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr( instance, "vkCreateDebugReportCallbackEXT" );
	ID_VK_VALIDATE( func != NULL, "Could not find vkCreateDebugReportCallbackEXT" );
	ID_VK_CHECK( func( instance, &callbackInfo, NULL, &debugReportCallback ) );
}

/*
=============
DestroyDebugReportCallback
=============
*/
static void DestroyDebugReportCallback( VkInstance instance ) {
	PFN_vkDestroyDebugReportCallbackEXT func = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr( instance, "vkDestroyDebugReportCallbackEXT" );
	ID_VK_VALIDATE( func != NULL, "Could not find vkDestroyDebugReportCallbackEXT" );
	func( instance, debugReportCallback, NULL );
}

/*
=============
ValidateValidationLayers
=============
*/
static void ValidateValidationLayers() {
	uint32 instanceLayerCount = 0;
	vkEnumerateInstanceLayerProperties( &instanceLayerCount, NULL );

	idList< VkLayerProperties > instanceLayers;
	instanceLayers.SetNum( instanceLayerCount );
	vkEnumerateInstanceLayerProperties( &instanceLayerCount, instanceLayers.Ptr() );

	bool found = false;
	for ( uint32 i = 0; i < g_numValidationLayers; ++i ) {
		for ( uint32 j = 0; j < instanceLayerCount; ++j ) {
			if ( idStr::Icmp( g_validationLayers[i], instanceLayers[j].layerName ) == 0 ) {
				found = true;
				break;
			}
		}
		if ( !found ) {
			idLib::FatalError( "Cannot find validation layer: %s.\n", g_validationLayers[ i ] );
		}
	}
}

/*
=============
ChooseSupportedFormat
=============
*/
static VkFormat ChooseSupportedFormat( VkPhysicalDevice physicalDevice, VkFormat * formats, int numFormats, VkImageTiling tiling, VkFormatFeatureFlags features ) {
	for ( int i = 0; i < numFormats; ++i ) {
		VkFormat format = formats[ i ];

		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties( physicalDevice, format, &props );

		if ( tiling == VK_IMAGE_TILING_LINEAR && ( props.linearTilingFeatures & features ) == features ) {
			return format;
		} else if ( tiling == VK_IMAGE_TILING_OPTIMAL && ( props.optimalTilingFeatures & features ) == features ) {
			return format;
		}
	}

	idLib::FatalError( "Failed to find a supported format." );

	return VK_FORMAT_UNDEFINED;
}

/*
=============
idRenderBackend::CreateInstance
=============
*/
void idRenderBackend::CreateInstance() {
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "DOOM";
	appInfo.applicationVersion = 1;
	appInfo.pEngineName = "idTech 4.5";
	appInfo.engineVersion = 1;
	appInfo.apiVersion = VK_MAKE_VERSION( 1, 0, VK_HEADER_VERSION );

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	const bool enableLayers = r_vkEnableValidationLayers.GetBool();

	m_instanceExtensions.Clear();
	m_deviceExtensions.Clear();
	m_validationLayers.Clear();

	for ( int i = 0; i < g_numInstanceExtensions; ++i ) {
		m_instanceExtensions.Append( g_instanceExtensions[ i ] );
	}

	for ( int i = 0; i < g_numDeviceExtensions; ++i ) {
		m_deviceExtensions.Append( g_deviceExtensions[ i ] );
	}

	if ( enableLayers ) {
		for ( int i = 0; i < g_numDebugInstanceExtensions; ++i ) {
			m_instanceExtensions.Append( g_debugInstanceExtensions[ i ] );
		}

		for ( int i = 0; i < g_numValidationLayers; ++i ) {
			m_validationLayers.Append( g_validationLayers[ i ] );
		}

		ValidateValidationLayers();
	}

	createInfo.enabledExtensionCount = m_instanceExtensions.Num();
	createInfo.ppEnabledExtensionNames = m_instanceExtensions.Ptr();
	createInfo.enabledLayerCount = m_validationLayers.Num();
	createInfo.ppEnabledLayerNames = m_validationLayers.Ptr();

	ID_VK_CHECK( vkCreateInstance( &createInfo, NULL, &m_instance ) );

	if ( enableLayers ) {
		CreateDebugReportCallback( m_instance );
	}
}

/*
=============
idRenderBackend::EnumeratePhysicalDevices
=============
*/
void idRenderBackend::EnumeratePhysicalDevices() {
	uint32 numDevices = 0;
	ID_VK_CHECK( vkEnumeratePhysicalDevices( m_instance, &numDevices, NULL ) );
	ID_VK_VALIDATE( numDevices > 0, "vkEnumeratePhysicalDevices returned zero devices." );

	idList< VkPhysicalDevice > devices;
	devices.SetNum( numDevices );
	
	ID_VK_CHECK( vkEnumeratePhysicalDevices( m_instance, &numDevices, devices.Ptr() ) );
	ID_VK_VALIDATE( numDevices > 0, "vkEnumeratePhysicalDevices returned zero devices." );

	vkcontext.gpus.SetNum( numDevices );

	for ( uint32 i = 0; i < numDevices; ++i ) {
		gpuInfo_t & gpu = vkcontext.gpus[ i ];
		gpu.device = devices[ i ];

		{
			uint32 numQueues = 0;
			vkGetPhysicalDeviceQueueFamilyProperties( gpu.device, &numQueues, NULL );
			ID_VK_VALIDATE( numQueues > 0, "vkGetPhysicalDeviceQueueFamilyProperties returned zero queues." );

			gpu.queueFamilyProps.SetNum( numQueues );
			vkGetPhysicalDeviceQueueFamilyProperties( gpu.device, &numQueues, gpu.queueFamilyProps.Ptr() );
			ID_VK_VALIDATE( numQueues > 0, "vkGetPhysicalDeviceQueueFamilyProperties returned zero queues." );
		}

		{
			uint32 numExtension;
			ID_VK_CHECK( vkEnumerateDeviceExtensionProperties( gpu.device, NULL, &numExtension, NULL ) );
			ID_VK_VALIDATE( numExtension > 0, "vkEnumerateDeviceExtensionProperties returned zero extensions." );

			gpu.extensionProps.SetNum( numExtension );
			ID_VK_CHECK( vkEnumerateDeviceExtensionProperties( gpu.device, NULL, &numExtension, gpu.extensionProps.Ptr() ) );
			ID_VK_VALIDATE( numExtension > 0, "vkEnumerateDeviceExtensionProperties returned zero extensions." );
		}

		ID_VK_CHECK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR( gpu.device, m_surface, &gpu.surfaceCaps ) );

		{
			uint32 numFormats;
			ID_VK_CHECK( vkGetPhysicalDeviceSurfaceFormatsKHR( gpu.device, m_surface, &numFormats, NULL ) );
			ID_VK_VALIDATE( numFormats > 0, "vkGetPhysicalDeviceSurfaceFormatsKHR returned zero surface formats." );

			gpu.surfaceFormats.SetNum( numFormats );
			ID_VK_CHECK( vkGetPhysicalDeviceSurfaceFormatsKHR( gpu.device, m_surface, &numFormats, gpu.surfaceFormats.Ptr() ) );
			ID_VK_VALIDATE( numFormats > 0, "vkGetPhysicalDeviceSurfaceFormatsKHR returned zero surface formats." );
		}

		{
			uint32 numPresentModes;
			ID_VK_CHECK( vkGetPhysicalDeviceSurfacePresentModesKHR( gpu.device, m_surface, &numPresentModes, NULL ) );
			ID_VK_VALIDATE( numPresentModes > 0, "vkGetPhysicalDeviceSurfacePresentModesKHR returned zero present modes." );

			gpu.presentModes.SetNum( numPresentModes );
			ID_VK_CHECK( vkGetPhysicalDeviceSurfacePresentModesKHR( gpu.device, m_surface, &numPresentModes, gpu.presentModes.Ptr() ) );
			ID_VK_VALIDATE( numPresentModes > 0, "vkGetPhysicalDeviceSurfacePresentModesKHR returned zero present modes." );
		}

		vkGetPhysicalDeviceMemoryProperties( gpu.device, &gpu.memProps );
		vkGetPhysicalDeviceProperties( gpu.device, &gpu.props );
	}
}

/*
=============
idRenderBackend::CreateSurface
=============
*/
void idRenderBackend::CreateSurface() {
	VkWin32SurfaceCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.hinstance = win32.hInstance;
	createInfo.hwnd = win32.hWnd;

	ID_VK_CHECK( vkCreateWin32SurfaceKHR( m_instance, &createInfo, NULL, &m_surface ) );
}

/*
=============
CheckPhysicalDeviceExtensionSupport
=============
*/
static bool CheckPhysicalDeviceExtensionSupport( gpuInfo_t & gpu, idList< const char * > & requiredExt ) {
	int required = requiredExt.Num();
	int available = 0;

	for ( int i = 0; i < requiredExt.Num(); ++i ) {
		for ( int j = 0; j < gpu.extensionProps.Num(); ++j ) {
			if ( idStr::Icmp( requiredExt[ i ], gpu.extensionProps[ j ].extensionName ) == 0 ) {
				available++;
				break;
			}
		}
	}

	return available == required;
}

/*
=============
idRenderBackend::SelectPhysicalDevice
=============
*/
void idRenderBackend::SelectPhysicalDevice() {
	for ( int i = 0; i < vkcontext.gpus.Num(); ++i ) {
		gpuInfo_t & gpu = vkcontext.gpus[ i ];

		int graphicsIdx = -1;
		int presentIdx = -1;

		if ( !CheckPhysicalDeviceExtensionSupport( gpu, m_deviceExtensions ) ) {
			continue;
		}

		if ( gpu.surfaceFormats.Num() == 0 ) {
			continue;
		}

		if ( gpu.presentModes.Num() == 0 ) {
			continue;
		}

		// Find graphics queue family
		for ( int j = 0; j < gpu.queueFamilyProps.Num(); ++j ) {
			VkQueueFamilyProperties & props = gpu.queueFamilyProps[ j ];

			if ( props.queueCount == 0 ) {
				continue;
			}

			if ( props.queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
				graphicsIdx = j;
				break;
			}
		}

		// Find present queue family
		for ( int j = 0; j < gpu.queueFamilyProps.Num(); ++j ) {
			VkQueueFamilyProperties & props = gpu.queueFamilyProps[ j ];

			if ( props.queueCount == 0 ) {
				continue;
			}

			VkBool32 supportsPresent = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR( gpu.device, j, m_surface, &supportsPresent );
			if ( supportsPresent ) {
				presentIdx = j;
				break;
			}
		}

		// Did we find a device supporting both graphics and present.
		if ( graphicsIdx >= 0 && presentIdx >= 0 ) {
			vkcontext.graphicsFamilyIdx = graphicsIdx;
			vkcontext.presentFamilyIdx = presentIdx;
			m_physicalDevice = gpu.device;
			vkcontext.gpu = &gpu;

			vkGetPhysicalDeviceFeatures( m_physicalDevice, &m_physicalDeviceFeatures );

			return;
		}
	}

	// If we can't render or present, just bail.
	idLib::FatalError( "Could not find a physical device which fits our desired profile" );
}

/*
=============
idRenderBackend::CreateLogicalDeviceAndQueues
=============
*/
void idRenderBackend::CreateLogicalDeviceAndQueues() {
	idList< int > uniqueIdx;
	uniqueIdx.AddUnique( vkcontext.graphicsFamilyIdx );
	uniqueIdx.AddUnique( vkcontext.presentFamilyIdx );
	
	idList< VkDeviceQueueCreateInfo > devqInfo;

	const float priority = 1.0f;
	for ( int i = 0; i < uniqueIdx.Num(); ++i ) {
		VkDeviceQueueCreateInfo qinfo = {};
		qinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qinfo.queueFamilyIndex = uniqueIdx[ i ];
		qinfo.queueCount = 1;
		qinfo.pQueuePriorities = &priority;

		devqInfo.Append( qinfo );
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.textureCompressionBC = VK_TRUE;
	deviceFeatures.imageCubeArray = VK_TRUE;
	deviceFeatures.depthClamp = VK_TRUE;
	deviceFeatures.depthBiasClamp = VK_TRUE;
	deviceFeatures.depthBounds = VK_TRUE;
	deviceFeatures.fillModeNonSolid = VK_TRUE;

	VkDeviceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	info.queueCreateInfoCount = devqInfo.Num();
	info.pQueueCreateInfos = devqInfo.Ptr();
	info.pEnabledFeatures = &deviceFeatures;
	info.enabledExtensionCount = m_deviceExtensions.Num();
	info.ppEnabledExtensionNames = m_deviceExtensions.Ptr();

	if ( r_vkEnableValidationLayers.GetBool() ) {
		info.enabledLayerCount = m_validationLayers.Num();
		info.ppEnabledLayerNames = m_validationLayers.Ptr();
	} else {
		info.enabledLayerCount = 0;
	}

	ID_VK_CHECK( vkCreateDevice( m_physicalDevice, &info, NULL, &vkcontext.device ) );

	vkGetDeviceQueue( vkcontext.device, vkcontext.graphicsFamilyIdx, 0, &vkcontext.graphicsQueue );
	vkGetDeviceQueue( vkcontext.device, vkcontext.presentFamilyIdx, 0, &vkcontext.presentQueue );
}

/*
=============
ChooseSurfaceFormat
=============
*/
VkSurfaceFormatKHR ChooseSurfaceFormat( idList< VkSurfaceFormatKHR > & formats ) {
	VkSurfaceFormatKHR result;
	
	if ( formats.Num() == 1 && formats[ 0 ].format == VK_FORMAT_UNDEFINED ) {
		result.format = VK_FORMAT_B8G8R8A8_UNORM;
		result.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		return result;
	}

	for ( int i = 0; i < formats.Num(); ++i ) {
		VkSurfaceFormatKHR & fmt = formats[ i ];
		if ( fmt.format == VK_FORMAT_B8G8R8A8_UNORM && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
			return fmt;
		}
	}

	return formats[ 0 ];
}

/*
=============
ChoosePresentMode
=============
*/
VkPresentModeKHR ChoosePresentMode( idList< VkPresentModeKHR > & modes ) {
	VkPresentModeKHR desiredMode = VK_PRESENT_MODE_FIFO_KHR;

	if (r_swapInterval.GetInteger() < 1) {
		for (int i = 0; i < modes.Num(); i++) {
			if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
				return VK_PRESENT_MODE_MAILBOX_KHR;
			}
			if ((modes[i] != VK_PRESENT_MODE_MAILBOX_KHR) && (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)) {
				return VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
		}
	}

	for (int i = 0; i < modes.Num(); ++i) {
		if (modes[i] == desiredMode) {
			return desiredMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

/*
=============
ChooseSurfaceExtent
=============
*/
VkExtent2D ChooseSurfaceExtent( VkSurfaceCapabilitiesKHR & caps ) {
	VkExtent2D extent;

	if ( caps.currentExtent.width == -1 ) {
		extent.width = win32.nativeScreenWidth;
		extent.height = win32.nativeScreenHeight;
	} else {
		extent = caps.currentExtent;
	}

	return extent;
}

/*
=============
idRenderBackend::CreateSwapChain
=============
*/
void idRenderBackend::CreateSwapChain() {
	gpuInfo_t & gpu = *vkcontext.gpu;

	VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat( gpu.surfaceFormats );
	VkPresentModeKHR presentMode = ChoosePresentMode( gpu.presentModes );
	VkExtent2D extent = ChooseSurfaceExtent( gpu.surfaceCaps );

	VkSwapchainCreateInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = m_surface;
	info.minImageCount = NUM_FRAME_DATA;
	info.imageFormat = surfaceFormat.format;
	info.imageColorSpace = surfaceFormat.colorSpace;
	info.imageExtent = extent;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	if ( vkcontext.graphicsFamilyIdx != vkcontext.presentFamilyIdx ) {
		uint32 indices[] = { (uint32)vkcontext.graphicsFamilyIdx, (uint32)vkcontext.presentFamilyIdx };

		info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		info.queueFamilyIndexCount = 2;
		info.pQueueFamilyIndices = indices;
	} else {
		info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.presentMode = presentMode;
	info.clipped = VK_TRUE;

	ID_VK_CHECK( vkCreateSwapchainKHR( vkcontext.device, &info, NULL, &m_swapchain ) );

	m_swapchainFormat = surfaceFormat.format;
	m_presentMode = presentMode;
	m_swapchainExtent = extent;
	m_fullscreen = win32.isFullscreen;

	uint32 numImages = 0;
	ID_VK_CHECK( vkGetSwapchainImagesKHR( vkcontext.device, m_swapchain, &numImages, NULL ) );
	ID_VK_VALIDATE( numImages > 0, "vkGetSwapchainImagesKHR returned a zero image count." );

	idList< VkImage > swapImages;
	swapImages.SetNum( numImages );
	ID_VK_CHECK( vkGetSwapchainImagesKHR( vkcontext.device, m_swapchain, &numImages, swapImages.Ptr() ) );
	ID_VK_VALIDATE( numImages > 0, "vkGetSwapchainImagesKHR returned a zero image count." );

	for ( uint32 i = 0; i < numImages; ++i ) {
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = swapImages[ i ];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = m_swapchainFormat;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		imageViewCreateInfo.flags = 0;

		VkImageView swapImageView = VK_NULL_HANDLE;
		ID_VK_CHECK( vkCreateImageView( vkcontext.device, &imageViewCreateInfo, NULL, &swapImageView ) );

		idImage * swapImage = new idImage( va( "_swapchain%d", i ) );
		swapImage->CreateFromSwapImage( 
			swapImages[ i ], 
			swapImageView, 
			m_swapchainFormat, 
			m_swapchainExtent.width, 
			m_swapchainExtent.height );

		m_swapchainImages.Append( swapImage );
	}
}

/*
=============
idRenderBackend::DestroySwapChain
=============
*/
void idRenderBackend::DestroySwapChain() {
	const int numImages = m_swapchainImages.Num();
	for ( int i = 0; i < numImages; ++i ) {
		m_swapchainImages[ i ]->PurgeImage();
		delete m_swapchainImages[ i ];
	}
	m_swapchainImages.Clear();

	vkDestroySwapchainKHR( vkcontext.device, m_swapchain, NULL );
}

/*
=============
CreateCommandPool
=============
*/
static void CreateCommandPool() {
	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = vkcontext.graphicsFamilyIdx;

	ID_VK_CHECK( vkCreateCommandPool( vkcontext.device, &commandPoolCreateInfo, NULL, &vkcontext.commandPool ) );
}

/*
=============
CreateCommandBuffer
=============
*/
static void CreateCommandBuffer() {
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandPool = vkcontext.commandPool;
	commandBufferAllocateInfo.commandBufferCount = NUM_FRAME_DATA;

	ID_VK_CHECK( vkAllocateCommandBuffers( vkcontext.device, &commandBufferAllocateInfo, vkcontext.commandBuffers.Ptr() ) );

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	for ( int i = 0; i < NUM_FRAME_DATA; ++i ) {
		ID_VK_CHECK( vkCreateFence( vkcontext.device, &fenceCreateInfo, NULL, &vkcontext.commandBufferFences[ i ] ) );
	}
}

/*
=============
idRenderBackend::CreateSemaphores
=============
*/
void idRenderBackend::CreateSemaphores() {
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for ( int i = 0; i < NUM_FRAME_DATA; ++i ) {
		ID_VK_CHECK( vkCreateSemaphore( vkcontext.device, &semaphoreCreateInfo, NULL, &m_acquireSemaphores[ i ] ) );
		ID_VK_CHECK( vkCreateSemaphore( vkcontext.device, &semaphoreCreateInfo, NULL, &m_renderCompleteSemaphores[ i ] ) );
	}
}

/*
===============
idRenderBackend::CreateQueryPool
===============
*/
void idRenderBackend::CreateQueryPool() {
	VkQueryPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	createInfo.queryCount = NUM_TIMESTAMP_QUERIES;

	for ( int i = 0; i < NUM_FRAME_DATA; ++i ) {
		ID_VK_CHECK( vkCreateQueryPool( vkcontext.device, &createInfo, NULL, &m_queryPools[ i ] ) );
	}
}

/*
=============
CreatePipelineCache
=============
*/
static void CreatePipelineCache() {
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	ID_VK_CHECK( vkCreatePipelineCache( vkcontext.device, &pipelineCacheCreateInfo, NULL, &vkcontext.pipelineCache ) );
}

/*
=============
ClearContext
=============
*/
static void ClearContext() {
	vkcontext.counter = 0;
	vkcontext.currentFrameData = 0;
	vkcontext.jointCacheHandle = 0;
	vkcontext.gpu = NULL;
	vkcontext.gpus.Clear();
	vkcontext.device = VK_NULL_HANDLE;
	vkcontext.graphicsFamilyIdx = -1;
	vkcontext.presentFamilyIdx = -1;
	vkcontext.graphicsQueue = VK_NULL_HANDLE;
	vkcontext.presentQueue = VK_NULL_HANDLE;
	vkcontext.commandPool = VK_NULL_HANDLE;
	vkcontext.commandBuffer = VK_NULL_HANDLE;
	vkcontext.commandBuffers.Zero();
	vkcontext.commandBufferFences.Zero();
	vkcontext.commandBufferRecorded.Zero();
	vkcontext.depthFormat = VK_FORMAT_UNDEFINED;
	vkcontext.pipelineCache = VK_NULL_HANDLE;
	vkcontext.sampleCount = VK_SAMPLE_COUNT_1_BIT;
	vkcontext.supersampling = false;
	vkcontext.imageParms.Zero();
}

/*
=============
idRenderBackend::idRenderBackend
=============
*/
idRenderBackend::idRenderBackend() {
	ClearContext();
	Clear();

	memset( m_gammaTable, 0, sizeof( m_gammaTable ) );
	memset( m_parmBuffers, 0, sizeof( m_parmBuffers ) );
}

/*
=============
idRenderBackend::~idRenderBackend
=============
*/
idRenderBackend::~idRenderBackend() {

}

/*
=============
idRenderBackend::Clear
=============
*/
void idRenderBackend::Clear() {
	m_currentRp = 0;
	m_currentDescSet = 0;
	m_currentParmBufferOffset = 0;
	m_currentRenderTarget = NULL;

	m_instance = VK_NULL_HANDLE;
	m_physicalDevice = VK_NULL_HANDLE;

	debugReportCallback = VK_NULL_HANDLE;
	m_instanceExtensions.Clear();
	m_deviceExtensions.Clear();
	m_validationLayers.Clear();

	m_surface = VK_NULL_HANDLE;
	m_presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	
	m_fullscreen = 0;
	m_swapchain = VK_NULL_HANDLE;
	m_swapchainFormat = VK_FORMAT_UNDEFINED;
	m_currentSwapIndex = 0;
	m_swapchainImages.Clear();
	m_acquireSemaphores.Zero();
	m_renderCompleteSemaphores.Zero();

	m_queryIndex.Zero();
	for ( int i = 0; i < NUM_FRAME_DATA; ++i ) {
		m_queryResults[ i ].Zero();
	}
	m_queryPools.Zero();
}

/*
=============
idRenderBackend::Print
=============
*/
void idRenderBackend::Print() {
	for ( int i = 0; i < m_renderProgs.Num(); ++i ) {
		renderProg_t & prog = m_renderProgs[ i ];
		for ( int j = 0; j < prog.pipelines.Num(); ++j ) {
			idLib::Printf( "%s: %llu\n", prog.name.c_str(), prog.pipelines[ j ].stateBits );
			idLib::Printf( "------------------------------------------\n" );
			RpPrintState( prog.pipelines[ j ].stateBits );
			idLib::Printf( "\n" );
		}
	}
}

/*
=============
idRenderBackend::Init
=============
*/
void idRenderBackend::Init() {
	idLib::Printf( "----- idRenderBackend::Init -----\n" );

	// Setup the window
	OpenWindow();

	// input and sound systems need to be tied to the new window
	Sys_InitInput();

	// Create the instance
	CreateInstance();

	// Create presentation surface
	CreateSurface();

	// Enumerate physical devices and get their properties
	EnumeratePhysicalDevices();

	// Find queue family/families supporting graphics and present.
	SelectPhysicalDevice();

	// Create logical device and queues
	CreateLogicalDeviceAndQueues();

	// Create semaphores for image acquisition and rendering completion
	CreateSemaphores();

	// Create Query Pool
	CreateQueryPool();

	// Create Command Pool
	CreateCommandPool();

	// Create Command Buffer
	CreateCommandBuffer();

	// Setup the allocator
#if defined( ID_USE_AMD_ALLOCATOR )
	extern idCVar r_vkHostVisibleMemoryMB;
	extern idCVar r_vkDeviceLocalMemoryMB;

	VmaAllocatorCreateInfo createInfo = {};
	createInfo.physicalDevice = m_physicalDevice;
	createInfo.device = vkcontext.device;
	createInfo.preferredSmallHeapBlockSize = r_vkHostVisibleMemoryMB.GetInteger() * 1024 * 1024;
	createInfo.preferredLargeHeapBlockSize = r_vkDeviceLocalMemoryMB.GetInteger() * 1024 * 1024;

	vmaCreateAllocator( &createInfo, &vmaAllocator );
#else
	vulkanAllocator.Init();
#endif

	// Start the Staging Manager
	stagingManager.Init();

	// Determine samples before creating depth
	{
		VkImageFormatProperties fmtProps = {};
		vkGetPhysicalDeviceImageFormatProperties( m_physicalDevice, m_swapchainFormat, 
			VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &fmtProps );

		const int samples = r_multiSamples.GetInteger();

		if ( samples >= 16 && ( fmtProps.sampleCounts & VK_SAMPLE_COUNT_16_BIT ) ) {
			vkcontext.sampleCount = VK_SAMPLE_COUNT_16_BIT;
		} else if ( samples >= 8 && ( fmtProps.sampleCounts & VK_SAMPLE_COUNT_8_BIT ) ) {
			vkcontext.sampleCount = VK_SAMPLE_COUNT_8_BIT;
		} else if ( samples >= 4 && ( fmtProps.sampleCounts & VK_SAMPLE_COUNT_4_BIT ) ) {
			vkcontext.sampleCount = VK_SAMPLE_COUNT_4_BIT;
		} else if ( samples >= 2 && ( fmtProps.sampleCounts & VK_SAMPLE_COUNT_2_BIT ) ) {
			vkcontext.sampleCount = VK_SAMPLE_COUNT_2_BIT;
		}
	}

	// Select Depth Format
	{
		VkFormat formats[] = {  
			VK_FORMAT_D32_SFLOAT_S8_UINT, 
			VK_FORMAT_D24_UNORM_S8_UINT 
		};
		vkcontext.depthFormat = ChooseSupportedFormat( 
			m_physicalDevice,
			formats, 3, 
			VK_IMAGE_TILING_OPTIMAL, 
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );
	}

	// Create Swap Chain
	CreateSwapChain();

	// Create Pipeline Cache
	CreatePipelineCache();

	// Init RenderProg Manager
	CreateRenderProgs();

	// Init Vertex Cache
	vertexCache.Init( vkcontext.gpu->props.limits.minUniformBufferOffsetAlignment );
}

/*
=============
idRenderBackend::Shutdown
=============
*/
void idRenderBackend::Shutdown() {
	// Shutdown input
	Sys_ShutdownInput();

	DestroyRenderProgs();

	// Destroy Pipeline Cache
	vkDestroyPipelineCache( vkcontext.device, vkcontext.pipelineCache, NULL );

	// Destroy Swap Chain
	DestroySwapChain();

	// Stop the Staging Manager
	stagingManager.Shutdown();

	for ( int i = 0; i < NUM_FRAME_DATA; ++i ) {
		idImage::EmptyGarbage();
	}

	// Destroy Command Buffer
	vkFreeCommandBuffers( vkcontext.device, vkcontext.commandPool, NUM_FRAME_DATA, vkcontext.commandBuffers.Ptr() );
	for ( int i = 0; i < NUM_FRAME_DATA; ++i ) {
		vkDestroyFence( vkcontext.device, vkcontext.commandBufferFences[ i ], NULL );
	}

	// Destroy Command Pool
	vkDestroyCommandPool( vkcontext.device, vkcontext.commandPool, NULL );

	// Destroy Query Pools
	for ( int i = 0; i < NUM_FRAME_DATA; ++i ) {
		vkDestroyQueryPool( vkcontext.device, m_queryPools[ i ], NULL );
	}

	// Destroy Semaphores
	for ( int i = 0; i < NUM_FRAME_DATA; ++i ) {
		vkDestroySemaphore( vkcontext.device, m_acquireSemaphores[ i ], NULL );
		vkDestroySemaphore( vkcontext.device, m_renderCompleteSemaphores[ i ], NULL );
	}

	// Destroy Debug Callback
	if ( r_vkEnableValidationLayers.GetBool() ) {
		DestroyDebugReportCallback( m_instance );
	}

	// Dump all our memory
#if defined( ID_USE_AMD_ALLOCATOR )
	vmaDestroyAllocator( vmaAllocator );
#else
	vulkanAllocator.Shutdown();
#endif

	// Destroy Logical Device
	vkDestroyDevice( vkcontext.device, NULL );

	// Destroy Surface
	vkDestroySurfaceKHR( m_instance, m_surface, NULL );

	// Destroy the Instance
	vkDestroyInstance( m_instance, NULL );

	ClearContext();
	Clear();

	CloseWindow();
}

/*
====================
idRenderBackend::ResizeImages
====================
*/
void idRenderBackend::ResizeImages() {
	if ( m_swapchainExtent.width == win32.nativeScreenWidth && 
		m_swapchainExtent.height == win32.nativeScreenHeight &&
		m_fullscreen == win32.isFullscreen ) {
		return;
	}

	stagingManager.Flush();
	
	vkDeviceWaitIdle( vkcontext.device );

	// Destroy Current Swap Chain
	DestroySwapChain();

	// Destroy Current Surface
	vkDestroySurfaceKHR( m_instance, m_surface, NULL );

#if !defined( ID_USE_AMD_ALLOCATOR )
	vulkanAllocator.EmptyGarbage();
#endif

	// Create New Surface
	CreateSurface();

	// Refresh Surface Capabilities
	ID_VK_CHECK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR( m_physicalDevice, m_surface, &vkcontext.gpu->surfaceCaps ) );

	// Recheck presentation support
	VkBool32 supportsPresent = VK_FALSE;
	ID_VK_CHECK( vkGetPhysicalDeviceSurfaceSupportKHR( m_physicalDevice, vkcontext.presentFamilyIdx, m_surface, &supportsPresent ) );
	if ( supportsPresent == VK_FALSE ) {
		idLib::FatalError( "idRenderBackend::ResizeImages: New surface does not support present?" );
	}

	// Create New Swap Chain
	CreateSwapChain();

	// Recreate the targets
	globalImages->ReloadTargets();

	// Clear any image garbage we may have amassed.
	idImage::EmptyGarbage();
}

/*
====================
idRenderBackend::BlockingSwapBuffers
====================
*/
void idRenderBackend::BlockingSwapBuffers() {
	RENDERLOG_PRINTF( "***************** BlockingSwapBuffers *****************\n\n\n" );

	if ( vkcontext.commandBufferRecorded[ vkcontext.currentFrameData ] == false ) {
		return;
	}	

	ID_VK_CHECK( vkWaitForFences( vkcontext.device, 1, &vkcontext.commandBufferFences[ vkcontext.currentFrameData ], VK_TRUE, UINT64_MAX ) );

	ID_VK_CHECK( vkResetFences( vkcontext.device, 1, &vkcontext.commandBufferFences[ vkcontext.currentFrameData ] ) );
	vkcontext.commandBufferRecorded[ vkcontext.currentFrameData ] = false;
		
	VkSemaphore * finished = &m_renderCompleteSemaphores[ vkcontext.currentFrameData ];

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = finished;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapchain;
	presentInfo.pImageIndices = &m_currentSwapIndex;

	ID_VK_CHECK( vkQueuePresentKHR( vkcontext.presentQueue, &presentInfo ) );

	vkcontext.counter++;
	vkcontext.currentFrameData = vkcontext.counter % NUM_FRAME_DATA;
	


	//vkDeviceWaitIdle( vkcontext.device );
}

/*
==================
idRenderBackend::CheckCVars
==================
*/
void idRenderBackend::CheckCVars() {

}

/*
=========================================================================================================

BACKEND COMMANDS

=========================================================================================================
*/

/*
=============
idRenderBackend::DrawElementsWithCounters
=============
*/
void idRenderBackend::DrawElementsWithCounters( const drawSurf_t * surf ) {
	// get vertex buffer
	const vertCacheHandle_t vbHandle = surf->ambientCache;
	idVertexBuffer * vertexBuffer;
	if ( vertexCache.CacheIsStatic( vbHandle ) ) {
		vertexBuffer = &vertexCache.m_staticData.vertexBuffer;
	} else {
		const uint64 frameNum = (int)( vbHandle >> VERTCACHE_FRAME_SHIFT ) & VERTCACHE_FRAME_MASK;
		if ( frameNum != ( ( vertexCache.m_currentFrame - 1 ) & VERTCACHE_FRAME_MASK ) ) {
			idLib::Warning( "idRenderBackend::DrawElementsWithCounters, vertexBuffer == NULL" );
			return;
		}
		vertexBuffer = &vertexCache.m_frameData[ vertexCache.m_drawListNum ].vertexBuffer;
	}
	int vertOffset = (int)( vbHandle >> VERTCACHE_OFFSET_SHIFT ) & VERTCACHE_OFFSET_MASK;

	// get index buffer
	const vertCacheHandle_t ibHandle = surf->indexCache;
	idIndexBuffer * indexBuffer;
	if ( vertexCache.CacheIsStatic( ibHandle ) ) {
		indexBuffer = &vertexCache.m_staticData.indexBuffer;
	} else {
		const uint64 frameNum = (int)( ibHandle >> VERTCACHE_FRAME_SHIFT ) & VERTCACHE_FRAME_MASK;
		if ( frameNum != ( ( vertexCache.m_currentFrame - 1 ) & VERTCACHE_FRAME_MASK ) ) {
			idLib::Warning( "idRenderBackend::DrawElementsWithCounters, indexBuffer == NULL" );
			return;
		}
		indexBuffer = &vertexCache.m_frameData[ vertexCache.m_drawListNum ].indexBuffer;
	}
	int indexOffset = (int)( ibHandle >> VERTCACHE_OFFSET_SHIFT ) & VERTCACHE_OFFSET_MASK;

	RENDERLOG_PRINTF( "Binding Buffers(%d): %p:%i %p:%i\n", surf->numIndexes, vertexBuffer, vertOffset, indexBuffer, indexOffset );

	const renderProg_t & prog = m_renderProgs[ m_currentRp ];

	if ( surf->jointCache ) {
		assert( prog.usesJoints );
		if ( !prog.usesJoints ) {
			return;
		}
	} else {
		assert( !prog.usesJoints || prog.optionalSkinning );
		if ( prog.usesJoints && !prog.optionalSkinning ) {
			return;
		}
	}

	vkcontext.jointCacheHandle = surf->jointCache;

	PrintState( m_glStateBits );
	CommitCurrent( m_glStateBits, m_currentRenderTarget );

	{
		const VkBuffer buffer = indexBuffer->GetAPIObject();
		const VkDeviceSize offset = indexBuffer->GetOffset();
		vkCmdBindIndexBuffer( vkcontext.commandBuffer, buffer, offset, VK_INDEX_TYPE_UINT16 );
	}
	{
		const VkBuffer buffer = vertexBuffer->GetAPIObject();
		const VkDeviceSize offset = vertexBuffer->GetOffset();
		vkCmdBindVertexBuffers( vkcontext.commandBuffer, 0, 1, &buffer, &offset );
	}

	vkCmdDrawIndexed( vkcontext.commandBuffer, surf->numIndexes, 1, ( indexOffset >> 1 ), vertOffset / sizeof( idDrawVert ), 0 );
}

/*
=========================================================================================================

GL COMMANDS

=========================================================================================================
*/

/*
==================
idRenderBackend::GL_StartFrame
==================
*/
void idRenderBackend::GL_StartFrame() {
	vkcontext.commandBuffer = vkcontext.commandBuffers[ vkcontext.currentFrameData ];

	ID_VK_CHECK( vkAcquireNextImageKHR( vkcontext.device, m_swapchain, UINT64_MAX, m_acquireSemaphores[ vkcontext.currentFrameData ], VK_NULL_HANDLE, &m_currentSwapIndex ) );

	idImage::EmptyGarbage();
#if !defined( ID_USE_AMD_ALLOCATOR )
	vulkanAllocator.EmptyGarbage();
#endif
	stagingManager.Flush();

	m_currentDescSet = 0;
	m_currentParmBufferOffset = 0;

	vkResetDescriptorPool( vkcontext.device, m_descriptorPools[ vkcontext.currentFrameData ], 0 );

	VkQueryPool queryPool = m_queryPools[ vkcontext.currentFrameData ];
	idArray< uint64, NUM_TIMESTAMP_QUERIES > & results = m_queryResults[ vkcontext.currentFrameData ];

	if ( m_queryIndex[ vkcontext.currentFrameData ] > 0 ) {
		vkGetQueryPoolResults( vkcontext.device, queryPool, 0, 2, 
			results.ByteSize(), results.Ptr(), sizeof( uint64 ), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT );

		const uint64 gpuStart = results[ 0 ];
		const uint64 gpuEnd = results[ 1 ];
		const uint64 tick = ( 1000 * 1000 * 1000 ) / vkcontext.gpu->props.limits.timestampPeriod;
		m_pc.gpuMicroSec = ( ( gpuEnd - gpuStart ) * 1000 * 1000 ) / tick;

		m_queryIndex[ vkcontext.currentFrameData ] = 0;
	}

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	ID_VK_CHECK( vkBeginCommandBuffer( vkcontext.commandBuffer, &commandBufferBeginInfo ) );

	vkCmdResetQueryPool( vkcontext.commandBuffer, queryPool, 0, NUM_TIMESTAMP_QUERIES );

	vkCmdWriteTimestamp( vkcontext.commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, m_queryIndex[ vkcontext.currentFrameData ]++ );
}

/*
==================
idRenderBackend::GL_EndFrame
==================
*/
void idRenderBackend::GL_EndFrame() {
	vkCmdWriteTimestamp( vkcontext.commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPools[ vkcontext.currentFrameData ], m_queryIndex[ vkcontext.currentFrameData ]++ );

	// Transition our swap image to present.
	// Do this instead of having the renderpass do the transition
	// so we can take advantage of the general layout to avoid 
	// additional image barriers.
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = m_swapchainImages[ m_currentSwapIndex ]->GetImage();
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = 0;

	vkCmdPipelineBarrier( 
		vkcontext.commandBuffer, 
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	ID_VK_CHECK( vkEndCommandBuffer( vkcontext.commandBuffer ) )
	vkcontext.commandBufferRecorded[ vkcontext.currentFrameData ] = true;

	VkSemaphore * acquire = &m_acquireSemaphores[ vkcontext.currentFrameData ];
	VkSemaphore * finished = &m_renderCompleteSemaphores[ vkcontext.currentFrameData ];

	VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vkcontext.commandBuffer;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = acquire;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = finished;
	submitInfo.pWaitDstStageMask = &dstStageMask;

	ID_VK_CHECK( vkQueueSubmit( vkcontext.graphicsQueue, 1, &submitInfo, vkcontext.commandBufferFences[ vkcontext.currentFrameData ] ) );
}

/*
========================
idRenderBackend::GL_StartRenderPass
========================
*/
void idRenderBackend::GL_StartRenderPass() {
	if ( m_viewDef->renderTarget ) {
		// If the view supplied a target, use that.
		m_currentRenderTarget = m_viewDef->renderTarget;
	} else {
		// Use the default target.
		m_currentRenderTarget = m_swapchainImages[ m_currentSwapIndex ];
	}

	VkRenderPassBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.renderPass = m_currentRenderTarget->GetRenderPass();
	beginInfo.framebuffer = m_currentRenderTarget->GetFrameBuffer();
	beginInfo.renderArea.extent.width = m_currentRenderTarget->GetOpts().width;
	beginInfo.renderArea.extent.height = m_currentRenderTarget->GetOpts().height;

	vkCmdBeginRenderPass( vkcontext.commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE );
}

/*
========================
idRenderBackend::GL_EndRenderPass
========================
*/
void idRenderBackend::GL_EndRenderPass() {
	vkCmdEndRenderPass( vkcontext.commandBuffer );
}

/*
========================
idRenderBackend::GL_SetDefaultState

This should initialize all GL state that any part of the entire program
may touch, including the editor.
========================
*/
void idRenderBackend::GL_SetDefaultState() {
	RENDERLOG_PRINTF( "--- GL_SetDefaultState ---\n" );

	m_glStateBits = 0;

	GL_State( 0, true );

	GL_Scissor( 0, 0, renderSystem->GetWidth(), renderSystem->GetHeight() );
}

/*
====================
idRenderBackend::GL_State

This routine is responsible for setting the most commonly changed state
====================
*/
void idRenderBackend::GL_State( uint64 stateBits, bool forceGlState ) {
	m_glStateBits = stateBits | ( m_glStateBits & GLS_KEEP );
	if ( m_viewDef != NULL && m_viewDef->isMirror ) {
		m_glStateBits |= GLS_MIRROR_VIEW;
	}
}

/*
====================
idRenderBackend::GL_BindTexture
====================
*/
void idRenderBackend::GL_BindTexture( int index, idImage * image ) {
	RENDERLOG_PRINTF( "GL_BindTexture( %d, %s )\n", index, image->GetName() );

	vkcontext.imageParms[ index ] = image;
}

/*
====================
idRenderBackend::GL_CopyFrameBuffer
====================
*/
void idRenderBackend::GL_CopyFrameBuffer( idImage * image, int x, int y, int imageWidth, int imageHeight ) {
	//vkCmdEndRenderPass( vkcontext.commandBuffer );

	//VkImageMemoryBarrier dstBarrier = {};
	//dstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	//dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	//dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	//dstBarrier.image = image->GetImage();
	//dstBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//dstBarrier.subresourceRange.baseMipLevel = 0;
	//dstBarrier.subresourceRange.levelCount = 1;
	//dstBarrier.subresourceRange.baseArrayLayer = 0;
	//dstBarrier.subresourceRange.layerCount = 1;

	//// Pre copy transitions
	//{
	//	// Transition the color dst image so we can transfer to it.
	//	dstBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	//	dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	//	dstBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	//	dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	//	vkCmdPipelineBarrier( 
	//		vkcontext.commandBuffer, 
	//		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
	//		VK_PIPELINE_STAGE_TRANSFER_BIT, 
	//		0, 0, NULL, 0, NULL, 1, &dstBarrier );
	//}

	//// Perform the blit/copy
	//{
	//	VkImageBlit region = {};
	//	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//	region.srcSubresource.baseArrayLayer = 0;
	//	region.srcSubresource.mipLevel = 0;
	//	region.srcSubresource.layerCount = 1;
	//	region.srcOffsets[ 1 ] = { imageWidth, imageHeight, 1 };

	//	region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//	region.dstSubresource.baseArrayLayer = 0;
	//	region.dstSubresource.mipLevel = 0;
	//	region.dstSubresource.layerCount = 1;
	//	region.dstOffsets[ 1 ] = { imageWidth, imageHeight, 1 };

	//	vkCmdBlitImage( 
	//		vkcontext.commandBuffer, 
	//		m_swapchainImages[ m_currentSwapIndex ]->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	//		image->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//		1, &region, VK_FILTER_NEAREST );
	//}

	//// Post copy transitions
	//{
	//	// Transition the color dst image so we can transfer to it.
	//	dstBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	//	dstBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	//	dstBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	//	dstBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	//	vkCmdPipelineBarrier( 
	//		vkcontext.commandBuffer, 
	//		VK_PIPELINE_STAGE_TRANSFER_BIT, 
	//		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
	//		0, 0, NULL, 0, NULL, 1, &dstBarrier );
	//}

	//VkRenderPassBeginInfo renderPassBeginInfo = {};
	//renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	//renderPassBeginInfo.renderPass = vkcontext.renderPass;
	//renderPassBeginInfo.framebuffer = m_frameBuffers[ m_currentSwapIndex ];
	//renderPassBeginInfo.renderArea.extent = m_swapchainExtent;

	//vkCmdBeginRenderPass( vkcontext.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE );
}

/*
========================
idRenderBackend::GL_Clear
========================
*/
void idRenderBackend::GL_Clear( bool color, bool depth, bool stencil, byte stencilValue, float r, float g, float b, float a ) {
	RENDERLOG_PRINTF( "GL_Clear( color=%d, depth=%d, stencil=%d, stencil=%d, r=%f, g=%f, b=%f, a=%f )\n", 
		color, depth, stencil, stencilValue, r, g, b, a );

	uint32 numAttachments = 0;
	VkClearAttachment attachments[ 2 ];
	memset( attachments, 0, sizeof( attachments ) );

	if ( color ) {
		VkClearAttachment & attachment = attachments[ numAttachments++ ];
		attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		attachment.colorAttachment = 0;
		VkClearColorValue & color = attachment.clearValue.color;
		color.float32[ 0 ] = r;
		color.float32[ 1 ] = g;
		color.float32[ 2 ] = b;
		color.float32[ 3 ] = a;
	}

	if ( depth || stencil ) {
		VkClearAttachment & attachment = attachments[ numAttachments++ ];
		if ( depth ) {
			attachment.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if ( stencil ) {
			attachment.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		attachment.clearValue.depthStencil.depth = 1.0f;
		attachment.clearValue.depthStencil.stencil = stencilValue;
	}

	VkClearRect clearRect = {};
	clearRect.baseArrayLayer = 0;
	clearRect.layerCount = 1;
	clearRect.rect.extent = m_swapchainExtent;

	vkCmdClearAttachments( vkcontext.commandBuffer, numAttachments, attachments, 1, &clearRect );
}

/*
========================
idRenderBackend::GL_DepthBoundsTest
========================
*/
void idRenderBackend::GL_DepthBoundsTest( const float zmin, const float zmax ) {
	if ( zmin > zmax ) {
		return;
	}

	if ( zmin == 0.0f && zmax == 0.0f ) {
		m_glStateBits = m_glStateBits & ~GLS_DEPTH_TEST_MASK;
	} else {
		m_glStateBits |= GLS_DEPTH_TEST_MASK;
		vkCmdSetDepthBounds( vkcontext.commandBuffer, zmin, zmax );
	}

	RENDERLOG_PRINTF( "GL_DepthBoundsTest( zmin=%f, zmax=%f )\n", zmin, zmax );
}

/*
====================
idRenderBackend::GL_PolygonOffset
====================
*/
void idRenderBackend::GL_PolygonOffset( float scale, float bias ) {
	vkCmdSetDepthBias( vkcontext.commandBuffer, bias, 0.0f, scale );

	RENDERLOG_PRINTF( "GL_PolygonOffset( scale=%f, bias=%f )\n", scale, bias );
}

/*
====================
idRenderBackend::GL_Scissor
====================
*/
void idRenderBackend::GL_Scissor( int x /* left*/, int y /* bottom */, int w, int h ) {
	VkRect2D scissor;
	scissor.offset.x = x;
	scissor.offset.y = y;
	scissor.extent.width = w;
	scissor.extent.height = h;
	vkCmdSetScissor( vkcontext.commandBuffer, 0, 1, &scissor );
}

/*
====================
idRenderBackend::GL_Viewport
====================
*/
void idRenderBackend::GL_Viewport( int x /* left */, int y /* bottom */, int w, int h ) {
	VkViewport viewport;
	viewport.x = x;
	viewport.y = y;
	viewport.width = w;
	viewport.height = h;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport( vkcontext.commandBuffer, 0, 1, &viewport );
}

/*
==============================================================================================

STENCIL SHADOW RENDERING

==============================================================================================
*/

/*
=====================
idRenderBackend::DrawStencilShadowPass
=====================
*/
void idRenderBackend::DrawStencilShadowPass( const drawSurf_t * drawSurf, const bool renderZPass ) {
	if ( renderZPass ) {
		// Z-pass
		uint64 stencil = GLS_STENCIL_OP_FAIL_KEEP | GLS_STENCIL_OP_ZFAIL_KEEP | GLS_STENCIL_OP_PASS_INCR
						| GLS_BACK_STENCIL_OP_FAIL_KEEP | GLS_BACK_STENCIL_OP_ZFAIL_KEEP | GLS_BACK_STENCIL_OP_PASS_DECR;
		GL_State( m_glStateBits & ~GLS_STENCIL_OP_BITS | stencil );
	} else if ( r_useStencilShadowPreload.GetBool() ) {
		// preload + Z-pass
		uint64 stencil = GLS_STENCIL_OP_FAIL_KEEP | GLS_STENCIL_OP_ZFAIL_DECR | GLS_STENCIL_OP_PASS_DECR
						| GLS_BACK_STENCIL_OP_FAIL_KEEP | GLS_BACK_STENCIL_OP_ZFAIL_INCR | GLS_BACK_STENCIL_OP_PASS_INCR;
		GL_State( m_glStateBits & ~GLS_STENCIL_OP_BITS | stencil );
	} else {
		// Z-fail
	}

	// get vertex buffer
	const vertCacheHandle_t vbHandle = drawSurf->shadowCache;
	idVertexBuffer * vertexBuffer;
	if ( vertexCache.CacheIsStatic( vbHandle ) ) {
		vertexBuffer = &vertexCache.m_staticData.vertexBuffer;
	} else {
		const uint64 frameNum = (int)( vbHandle >> VERTCACHE_FRAME_SHIFT ) & VERTCACHE_FRAME_MASK;
		if ( frameNum != ( ( vertexCache.m_currentFrame - 1 ) & VERTCACHE_FRAME_MASK ) ) {
			idLib::Warning( "RB_DrawElementsWithCounters, vertexBuffer == NULL" );
			return;
		}
		vertexBuffer = &vertexCache.m_frameData[ vertexCache.m_drawListNum ].vertexBuffer;
	}
	const int vertOffset = (int)( vbHandle >> VERTCACHE_OFFSET_SHIFT ) & VERTCACHE_OFFSET_MASK;

	// get index buffer
	const vertCacheHandle_t ibHandle = drawSurf->indexCache;
	idIndexBuffer * indexBuffer;
	if ( vertexCache.CacheIsStatic( ibHandle ) ) {
		indexBuffer = &vertexCache.m_staticData.indexBuffer;
	} else {
		const uint64 frameNum = (int)( ibHandle >> VERTCACHE_FRAME_SHIFT ) & VERTCACHE_FRAME_MASK;
		if ( frameNum != ( ( vertexCache.m_currentFrame - 1 ) & VERTCACHE_FRAME_MASK ) ) {
			idLib::Warning( "RB_DrawElementsWithCounters, indexBuffer == NULL" );
			return;
		}
		indexBuffer = &vertexCache.m_frameData[ vertexCache.m_drawListNum ].indexBuffer;
	}
	int indexOffset = (int)( ibHandle >> VERTCACHE_OFFSET_SHIFT ) & VERTCACHE_OFFSET_MASK;

	RENDERLOG_PRINTF( "Binding Buffers(%d): %p:%i %p:%i\n", drawSurf->numIndexes, vertexBuffer, vertOffset, indexBuffer, indexOffset );

	vkcontext.jointCacheHandle = drawSurf->jointCache;

	PrintState( m_glStateBits );
	CommitCurrent( m_glStateBits, m_currentRenderTarget );

	{
		const VkBuffer buffer = indexBuffer->GetAPIObject();
		const VkDeviceSize offset = indexBuffer->GetOffset();
		vkCmdBindIndexBuffer( vkcontext.commandBuffer, buffer, offset, VK_INDEX_TYPE_UINT16 );
	}
	{
		const VkBuffer buffer = vertexBuffer->GetAPIObject();
		const VkDeviceSize offset = vertexBuffer->GetOffset();
		vkCmdBindVertexBuffers( vkcontext.commandBuffer, 0, 1, &buffer, &offset );
	}

	const int baseVertex = vertOffset / ( drawSurf->jointCache ? sizeof( idShadowVertSkinned ) : sizeof( idShadowVert ) );

	vkCmdDrawIndexed( vkcontext.commandBuffer, drawSurf->numIndexes, 1, ( indexOffset >> 1 ), baseVertex, 0 );

	if ( !renderZPass && r_useStencilShadowPreload.GetBool() ) {
		// render again with Z-pass
		uint64 stencil = GLS_STENCIL_OP_FAIL_KEEP | GLS_STENCIL_OP_ZFAIL_KEEP | GLS_STENCIL_OP_PASS_INCR
						| GLS_BACK_STENCIL_OP_FAIL_KEEP | GLS_BACK_STENCIL_OP_ZFAIL_KEEP | GLS_BACK_STENCIL_OP_PASS_DECR;
		GL_State( m_glStateBits & ~GLS_STENCIL_OP_BITS | stencil );

		PrintState( m_glStateBits );
		CommitCurrent( m_glStateBits, m_currentRenderTarget );

		vkCmdDrawIndexed( vkcontext.commandBuffer, drawSurf->numIndexes, 1, ( indexOffset >> 1 ), baseVertex, 0 );
	}
}

/*
==============================================================================================

Render Progs

==============================================================================================
*/

struct vertexLayout_t {
	VkPipelineVertexInputStateCreateInfo inputState;
	idList< VkVertexInputBindingDescription > bindingDesc;
	idList< VkVertexInputAttributeDescription > attributeDesc;
};

static vertexLayout_t vertexLayouts[ NUM_VERTEX_LAYOUTS ];

static shader_t defaultShader;
static idUniformBuffer emptyUBO;

/*
=============
CreateVertexDescriptions
=============
*/
static void CreateVertexDescriptions() {
	VkPipelineVertexInputStateCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	createInfo.pNext = NULL;
	createInfo.flags = 0;

	VkVertexInputBindingDescription binding = {};
	VkVertexInputAttributeDescription attribute = {};

	{
		vertexLayout_t & layout = vertexLayouts[ LAYOUT_DRAW_VERT ];
		layout.inputState = createInfo;

		uint32 locationNo = 0;
		uint32 offset = 0;

		binding.stride = sizeof( idDrawVert );
		binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		layout.bindingDesc.Append( binding );

		// Position
		attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
		attribute.location = locationNo++;
		attribute.offset = offset;
		layout.attributeDesc.Append( attribute );
		offset += sizeof( idDrawVert::xyz );

		// TexCoord
		attribute.format = VK_FORMAT_R16G16_SFLOAT;
		attribute.location = locationNo++;
		attribute.offset = offset;
		layout.attributeDesc.Append( attribute );
		offset += sizeof( idDrawVert::st );

		// Normal
		attribute.format = VK_FORMAT_R8G8B8A8_UNORM;
		attribute.location = locationNo++;
		attribute.offset = offset;
		layout.attributeDesc.Append( attribute );
		offset += sizeof( idDrawVert::normal );

		// Tangent
		attribute.format = VK_FORMAT_R8G8B8A8_UNORM;
		attribute.location = locationNo++;
		attribute.offset = offset;
		layout.attributeDesc.Append( attribute );
		offset += sizeof( idDrawVert::tangent );

		// Color1
		attribute.format = VK_FORMAT_R8G8B8A8_UNORM;
		attribute.location = locationNo++;
		attribute.offset = offset;
		layout.attributeDesc.Append( attribute );
		offset += sizeof( idDrawVert::color );

		// Color2
		attribute.format = VK_FORMAT_R8G8B8A8_UNORM;
		attribute.location = locationNo++;
		attribute.offset = offset;
		layout.attributeDesc.Append( attribute );
	}

	{
		vertexLayout_t & layout = vertexLayouts[ LAYOUT_DRAW_SHADOW_VERT_SKINNED ];
		layout.inputState = createInfo;

		uint32 locationNo = 0;
		uint32 offset = 0;

		binding.stride = sizeof( idShadowVertSkinned );
		binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		layout.bindingDesc.Append( binding );

		// Position
		attribute.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attribute.location = locationNo++;
		attribute.offset = offset;
		layout.attributeDesc.Append( attribute );
		offset += sizeof( idShadowVertSkinned::xyzw );

		// Color1
		attribute.format = VK_FORMAT_R8G8B8A8_UNORM;
		attribute.location = locationNo++;
		attribute.offset = offset;
		layout.attributeDesc.Append( attribute );
		offset += sizeof( idShadowVertSkinned::color );

		// Color2
		attribute.format = VK_FORMAT_R8G8B8A8_UNORM;
		attribute.location = locationNo++;
		attribute.offset = offset;
		layout.attributeDesc.Append( attribute );
	}

	{
		vertexLayout_t & layout = vertexLayouts[ LAYOUT_DRAW_SHADOW_VERT ];
		layout.inputState = createInfo;

		binding.stride = sizeof( idShadowVert );
		binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		layout.bindingDesc.Append( binding );

		// Position
		attribute.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attribute.location = 0;
		attribute.offset = 0;
		layout.attributeDesc.Append( attribute );
	}
}

/*
========================
CreateDescriptorPools
========================
*/
static void CreateDescriptorPools( VkDescriptorPool (&pools)[ NUM_FRAME_DATA ] ) {
	const int numPools = 2;
	VkDescriptorPoolSize poolSizes[ numPools ];
	poolSizes[ 0 ].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[ 0 ].descriptorCount = MAX_DESC_UNIFORM_BUFFERS;
	poolSizes[ 1 ].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[ 1 ].descriptorCount = MAX_DESC_IMAGE_SAMPLERS;

	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.pNext = NULL;
	poolCreateInfo.maxSets = MAX_DESC_SETS;
	poolCreateInfo.poolSizeCount = numPools;
	poolCreateInfo.pPoolSizes = poolSizes;

	for ( int i = 0; i < NUM_FRAME_DATA; ++i ) {
		ID_VK_CHECK( vkCreateDescriptorPool( vkcontext.device, &poolCreateInfo, NULL, &pools[ i ] ) );
	}
}

/*
========================
GetDescriptorType
========================
*/
static VkDescriptorType GetDescriptorType( rpBinding_t type ) {
	switch ( type ) {
	case BINDING_TYPE_UNIFORM_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case BINDING_TYPE_SAMPLER: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	default: 
		idLib::Error( "Unknown rpBinding_t %d", static_cast< int >( type ) );
		return VK_DESCRIPTOR_TYPE_MAX_ENUM;
	}
}

/*
========================
CreateDescriptorSetLayout
========================
*/
static void CreateDescriptorSetLayout( 
		const shader_t & vertexShader, 
		const shader_t & fragmentShader, 
		renderProg_t & renderProg ) {
	
	// Descriptor Set Layout
	{
		idList< VkDescriptorSetLayoutBinding > layoutBindings;
		VkDescriptorSetLayoutBinding binding = {};
		binding.descriptorCount = 1;
		
		uint32 bindingId = 0;

		binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		for ( int i = 0; i < vertexShader.bindings.Num(); ++i ) {
			binding.binding = bindingId++;
			binding.descriptorType = GetDescriptorType( vertexShader.bindings[ i ] );
			renderProg.bindings.Append( vertexShader.bindings[ i ] );

			layoutBindings.Append( binding );
		}

		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		for ( int i = 0; i < fragmentShader.bindings.Num(); ++i ) {
			binding.binding = bindingId++;
			binding.descriptorType = GetDescriptorType( fragmentShader.bindings[ i ] );
			renderProg.bindings.Append( fragmentShader.bindings[ i ] );

			layoutBindings.Append( binding );
		}

		VkDescriptorSetLayoutCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.bindingCount = layoutBindings.Num();
		createInfo.pBindings = layoutBindings.Ptr();

		vkCreateDescriptorSetLayout( vkcontext.device, &createInfo, NULL, &renderProg.descriptorSetLayout );
	}

	// Pipeline Layout
	{
		VkPipelineLayoutCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		createInfo.setLayoutCount = 1;
		createInfo.pSetLayouts = &renderProg.descriptorSetLayout;

		vkCreatePipelineLayout( vkcontext.device, &createInfo, NULL, &renderProg.pipelineLayout );
	}
}

/*
========================
GetStencilOpState
========================
*/
static VkStencilOpState GetStencilOpState( uint64 stencilBits ) {
	VkStencilOpState state = {};

	switch ( stencilBits & GLS_STENCIL_OP_FAIL_BITS ) {
		case GLS_STENCIL_OP_FAIL_KEEP:		state.failOp = VK_STENCIL_OP_KEEP; break;
		case GLS_STENCIL_OP_FAIL_ZERO:		state.failOp = VK_STENCIL_OP_ZERO; break;
		case GLS_STENCIL_OP_FAIL_REPLACE:	state.failOp = VK_STENCIL_OP_REPLACE; break;
		case GLS_STENCIL_OP_FAIL_INCR:		state.failOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP; break;
		case GLS_STENCIL_OP_FAIL_DECR:		state.failOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP; break;
		case GLS_STENCIL_OP_FAIL_INVERT:	state.failOp = VK_STENCIL_OP_INVERT; break;
		case GLS_STENCIL_OP_FAIL_INCR_WRAP: state.failOp = VK_STENCIL_OP_INCREMENT_AND_WRAP; break;
		case GLS_STENCIL_OP_FAIL_DECR_WRAP: state.failOp = VK_STENCIL_OP_DECREMENT_AND_WRAP; break;
	}
	switch ( stencilBits & GLS_STENCIL_OP_ZFAIL_BITS ) {
		case GLS_STENCIL_OP_ZFAIL_KEEP:		state.depthFailOp = VK_STENCIL_OP_KEEP; break;
		case GLS_STENCIL_OP_ZFAIL_ZERO:		state.depthFailOp = VK_STENCIL_OP_ZERO; break;
		case GLS_STENCIL_OP_ZFAIL_REPLACE:	state.depthFailOp = VK_STENCIL_OP_REPLACE; break;
		case GLS_STENCIL_OP_ZFAIL_INCR:		state.depthFailOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP; break;
		case GLS_STENCIL_OP_ZFAIL_DECR:		state.depthFailOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP; break;
		case GLS_STENCIL_OP_ZFAIL_INVERT:	state.depthFailOp = VK_STENCIL_OP_INVERT; break;
		case GLS_STENCIL_OP_ZFAIL_INCR_WRAP:state.depthFailOp = VK_STENCIL_OP_INCREMENT_AND_WRAP; break;
		case GLS_STENCIL_OP_ZFAIL_DECR_WRAP:state.depthFailOp = VK_STENCIL_OP_DECREMENT_AND_WRAP; break;
	}
	switch ( stencilBits & GLS_STENCIL_OP_PASS_BITS ) {
		case GLS_STENCIL_OP_PASS_KEEP:		state.passOp = VK_STENCIL_OP_KEEP; break;
		case GLS_STENCIL_OP_PASS_ZERO:		state.passOp = VK_STENCIL_OP_ZERO; break;
		case GLS_STENCIL_OP_PASS_REPLACE:	state.passOp = VK_STENCIL_OP_REPLACE; break;
		case GLS_STENCIL_OP_PASS_INCR:		state.passOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP; break;
		case GLS_STENCIL_OP_PASS_DECR:		state.passOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP; break;
		case GLS_STENCIL_OP_PASS_INVERT:	state.passOp = VK_STENCIL_OP_INVERT; break;
		case GLS_STENCIL_OP_PASS_INCR_WRAP:	state.passOp = VK_STENCIL_OP_INCREMENT_AND_WRAP; break;
		case GLS_STENCIL_OP_PASS_DECR_WRAP:	state.passOp = VK_STENCIL_OP_DECREMENT_AND_WRAP; break;
	}

	return state;
}

/*
========================
CreateGraphicsPipeline
========================
*/
static VkPipeline CreateGraphicsPipeline(
		vertexLayoutType_t vertexLayoutType,
		VkShaderModule vertexShader,
		VkShaderModule fragmentShader,
		VkPipelineLayout pipelineLayout,
		uint64 stateBits,
		idImage * target ) {

	// Pipeline
	vertexLayout_t & vertexLayout = vertexLayouts[ vertexLayoutType ];

	// Vertex Input
	VkPipelineVertexInputStateCreateInfo vertexInputState = vertexLayout.inputState;
	vertexInputState.vertexBindingDescriptionCount = vertexLayout.bindingDesc.Num();
	vertexInputState.pVertexBindingDescriptions = vertexLayout.bindingDesc.Ptr();
	vertexInputState.vertexAttributeDescriptionCount = vertexLayout.attributeDesc.Num();
	vertexInputState.pVertexAttributeDescriptions = vertexLayout.attributeDesc.Ptr();

	// Input Assembly
	VkPipelineInputAssemblyStateCreateInfo assemblyInputState = {};
	assemblyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assemblyInputState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// Rasterization
	VkPipelineRasterizationStateCreateInfo rasterizationState = {};
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.rasterizerDiscardEnable = VK_FALSE;
	rasterizationState.depthBiasEnable = ( stateBits & GLS_POLYGON_OFFSET ) != 0;
	rasterizationState.depthClampEnable = VK_FALSE;
	rasterizationState.frontFace = ( stateBits & GLS_CLOCKWISE ) ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.lineWidth = 1.0f;
	rasterizationState.polygonMode = ( stateBits & GLS_POLYMODE_LINE ) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;

	switch ( stateBits & GLS_CULL_BITS ) {
	case GLS_CULL_TWOSIDED:
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		break;
	case GLS_CULL_BACKSIDED:
		if ( stateBits & GLS_MIRROR_VIEW ) {
			rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		} else {
			rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		}
		break;
	case GLS_CULL_FRONTSIDED:
	default:
		if ( stateBits & GLS_MIRROR_VIEW ) {
			rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		} else {
			rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		}
		break;
	}

	// Color Blend Attachment
	VkPipelineColorBlendAttachmentState attachmentState = {};
	{
		VkBlendFactor srcFactor = VK_BLEND_FACTOR_ONE;
		switch ( stateBits & GLS_SRCBLEND_BITS ) {
			case GLS_SRCBLEND_ZERO:					srcFactor = VK_BLEND_FACTOR_ZERO; break;
			case GLS_SRCBLEND_ONE:					srcFactor = VK_BLEND_FACTOR_ONE; break;
			case GLS_SRCBLEND_DST_COLOR:			srcFactor = VK_BLEND_FACTOR_DST_COLOR; break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:	srcFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR; break;
			case GLS_SRCBLEND_SRC_ALPHA:			srcFactor = VK_BLEND_FACTOR_SRC_ALPHA; break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:	srcFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; break;
			case GLS_SRCBLEND_DST_ALPHA:			srcFactor = VK_BLEND_FACTOR_DST_ALPHA; break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:	srcFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA; break;
		}

		VkBlendFactor dstFactor = VK_BLEND_FACTOR_ZERO;
		switch ( stateBits & GLS_DSTBLEND_BITS ) {
			case GLS_DSTBLEND_ZERO:					dstFactor = VK_BLEND_FACTOR_ZERO; break;
			case GLS_DSTBLEND_ONE:					dstFactor = VK_BLEND_FACTOR_ONE; break;
			case GLS_DSTBLEND_SRC_COLOR:			dstFactor = VK_BLEND_FACTOR_SRC_COLOR; break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:	dstFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR; break;
			case GLS_DSTBLEND_SRC_ALPHA:			dstFactor = VK_BLEND_FACTOR_SRC_ALPHA; break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:	dstFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; break;
			case GLS_DSTBLEND_DST_ALPHA:			dstFactor = VK_BLEND_FACTOR_DST_ALPHA; break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:	dstFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA; break;
		}

		VkBlendOp blendOp = VK_BLEND_OP_ADD;
		switch ( stateBits & GLS_BLENDOP_BITS ) {
			case GLS_BLENDOP_MIN: blendOp = VK_BLEND_OP_MIN; break;
			case GLS_BLENDOP_MAX: blendOp = VK_BLEND_OP_MAX; break;
			case GLS_BLENDOP_ADD: blendOp = VK_BLEND_OP_ADD; break;
			case GLS_BLENDOP_SUB: blendOp = VK_BLEND_OP_SUBTRACT; break;
		}

		attachmentState.blendEnable = ( srcFactor != VK_BLEND_FACTOR_ONE || dstFactor != VK_BLEND_FACTOR_ZERO );
		attachmentState.colorBlendOp = blendOp;
		attachmentState.srcColorBlendFactor = srcFactor;
		attachmentState.dstColorBlendFactor = dstFactor;
		attachmentState.alphaBlendOp = blendOp;
		attachmentState.srcAlphaBlendFactor = srcFactor;
		attachmentState.dstAlphaBlendFactor = dstFactor;

		// Color Mask
		attachmentState.colorWriteMask = 0;
		attachmentState.colorWriteMask |= ( stateBits & GLS_REDMASK ) ?	0 : VK_COLOR_COMPONENT_R_BIT;
		attachmentState.colorWriteMask |= ( stateBits & GLS_GREENMASK ) ? 0 : VK_COLOR_COMPONENT_G_BIT;
		attachmentState.colorWriteMask |= ( stateBits & GLS_BLUEMASK ) ? 0 : VK_COLOR_COMPONENT_B_BIT;
		attachmentState.colorWriteMask |= ( stateBits & GLS_ALPHAMASK ) ? 0 : VK_COLOR_COMPONENT_A_BIT;
	}

	// Color Blend
	VkPipelineColorBlendStateCreateInfo colorBlendState = {};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &attachmentState;

	// Depth / Stencil
	VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
	{
		VkCompareOp depthCompareOp = VK_COMPARE_OP_ALWAYS;
		switch ( stateBits & GLS_DEPTHFUNC_BITS ) {
			case GLS_DEPTHFUNC_EQUAL:		depthCompareOp = VK_COMPARE_OP_EQUAL; break;
			case GLS_DEPTHFUNC_ALWAYS:		depthCompareOp = VK_COMPARE_OP_ALWAYS; break;
			case GLS_DEPTHFUNC_LESS:		depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; break;
			case GLS_DEPTHFUNC_GREATER:		depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; break;
		}

		VkCompareOp stencilCompareOp = VK_COMPARE_OP_ALWAYS;
		switch ( stateBits & GLS_STENCIL_FUNC_BITS ) {
			case GLS_STENCIL_FUNC_NEVER:	stencilCompareOp = VK_COMPARE_OP_NEVER; break;
			case GLS_STENCIL_FUNC_LESS:		stencilCompareOp = VK_COMPARE_OP_LESS; break;
			case GLS_STENCIL_FUNC_EQUAL:	stencilCompareOp = VK_COMPARE_OP_EQUAL; break;
			case GLS_STENCIL_FUNC_LEQUAL:	stencilCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; break;
			case GLS_STENCIL_FUNC_GREATER:	stencilCompareOp = VK_COMPARE_OP_GREATER; break;
			case GLS_STENCIL_FUNC_NOTEQUAL: stencilCompareOp = VK_COMPARE_OP_NOT_EQUAL; break;
			case GLS_STENCIL_FUNC_GEQUAL:	stencilCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; break;
			case GLS_STENCIL_FUNC_ALWAYS:	stencilCompareOp = VK_COMPARE_OP_ALWAYS; break;
		}

		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.depthTestEnable = VK_TRUE;
		depthStencilState.depthWriteEnable = ( stateBits & GLS_DEPTHMASK ) == 0;
		depthStencilState.depthCompareOp = depthCompareOp;
		depthStencilState.depthBoundsTestEnable = ( stateBits & GLS_DEPTH_TEST_MASK ) != 0;
		depthStencilState.minDepthBounds = 0.0f;
		depthStencilState.maxDepthBounds = 1.0f;
		depthStencilState.stencilTestEnable = ( stateBits & ( GLS_STENCIL_FUNC_BITS | GLS_STENCIL_OP_BITS ) ) != 0;

		uint32 ref = uint32( ( stateBits & GLS_STENCIL_FUNC_REF_BITS ) >> GLS_STENCIL_FUNC_REF_SHIFT );
		uint32 mask = uint32( ( stateBits & GLS_STENCIL_FUNC_MASK_BITS ) >> GLS_STENCIL_FUNC_MASK_SHIFT );

		if ( stateBits & GLS_SEPARATE_STENCIL ) {
			depthStencilState.front = GetStencilOpState( stateBits & GLS_STENCIL_FRONT_OPS );
			depthStencilState.front.writeMask = 0xFFFFFFFF;
			depthStencilState.front.compareOp = stencilCompareOp;
			depthStencilState.front.compareMask = mask;
			depthStencilState.front.reference = ref;

			depthStencilState.back = GetStencilOpState( ( stateBits & GLS_STENCIL_BACK_OPS ) >> 12 );
			depthStencilState.back.writeMask = 0xFFFFFFFF;
			depthStencilState.back.compareOp = stencilCompareOp;
			depthStencilState.back.compareMask = mask;
			depthStencilState.back.reference = ref;
		} else {
			depthStencilState.front = GetStencilOpState( stateBits );
			depthStencilState.front.writeMask = 0xFFFFFFFF;
			depthStencilState.front.compareOp = stencilCompareOp;
			depthStencilState.front.compareMask = mask;
			depthStencilState.front.reference = ref;
			depthStencilState.back = depthStencilState.front;
		}
	}

	// Multisample
	VkPipelineMultisampleStateCreateInfo multisampleState = {};
	multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.rasterizationSamples = vkcontext.sampleCount;
	if ( vkcontext.supersampling ) {
		multisampleState.sampleShadingEnable = VK_TRUE;
		multisampleState.minSampleShading = 1.0f;
	}

	// Shader Stages
	idList< VkPipelineShaderStageCreateInfo > stages;
	VkPipelineShaderStageCreateInfo stage = {};
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.pName = "main";

	{
		stage.module = vertexShader;
		stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages.Append( stage );
	}

	if ( fragmentShader != VK_NULL_HANDLE ) {
		stage.module = fragmentShader;
		stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages.Append( stage );
	}

	// Dynamic
	idList< VkDynamicState > dynamic;
	dynamic.Append( VK_DYNAMIC_STATE_SCISSOR );
	dynamic.Append( VK_DYNAMIC_STATE_VIEWPORT );

	if ( stateBits & GLS_POLYGON_OFFSET ) {
		dynamic.Append( VK_DYNAMIC_STATE_DEPTH_BIAS );
	}

	if ( stateBits & GLS_DEPTH_TEST_MASK ) {
		dynamic.Append( VK_DYNAMIC_STATE_DEPTH_BOUNDS );
	}

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = dynamic.Num();
	dynamicState.pDynamicStates = dynamic.Ptr();

	// Viewport / Scissor
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	// Pipeline Create
	VkGraphicsPipelineCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	createInfo.layout = pipelineLayout;
	createInfo.renderPass = target->GetRenderPass();
	createInfo.pVertexInputState = &vertexInputState;
	createInfo.pInputAssemblyState = &assemblyInputState;
	createInfo.pRasterizationState = &rasterizationState;
	createInfo.pColorBlendState = &colorBlendState;
	createInfo.pDepthStencilState = &depthStencilState;
	createInfo.pMultisampleState = &multisampleState;
	createInfo.pDynamicState = &dynamicState;
	createInfo.pViewportState = &viewportState;
	createInfo.stageCount = stages.Num();
	createInfo.pStages = stages.Ptr();

	VkPipeline pipeline = VK_NULL_HANDLE;

	ID_VK_CHECK( vkCreateGraphicsPipelines( vkcontext.device, vkcontext.pipelineCache, 1, &createInfo, NULL, &pipeline ) );

	return pipeline;
}

/*
========================
renderProg_t::GetPipeline
========================
*/
VkPipeline renderProg_t::GetPipeline( uint64 stateBits, idImage * target, VkShaderModule vertexShader, VkShaderModule fragmentShader ) {
	for ( int i = 0; i < pipelines.Num(); ++i ) {
		if ( stateBits == pipelines[ i ].stateBits && target == pipelines[ i ].target ) {
			return pipelines[ i ].pipeline;
		}
	}

	VkPipeline pipeline = CreateGraphicsPipeline( 
		vertexLayoutType, vertexShader, fragmentShader, pipelineLayout, stateBits, target );

	pipelineState_t pipelineState;
	pipelineState.stateBits = stateBits;
	pipelineState.target = target;
	pipelineState.pipeline = pipeline;
	pipelines.Append( pipelineState );

	return pipeline;
}

/*
===============
idRenderBackend::CreateRenderProgs
===============
*/
void idRenderBackend::CreateRenderProgs() {
	idLib::Printf( "----- Initializing Render Progs -----\n" );

	struct builtinShaders_t {
		int index;
		const char * name;
		rpStage_t stages;
		vertexLayoutType_t layout;
	} builtins[ MAX_BUILTINS ] = {
		{ BUILTIN_GUI, "gui", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_COLOR, "color", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_SIMPLESHADE, "simpleshade", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_TEXTURED, "texture", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_TEXTURE_VERTEXCOLOR, "texture_color", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_TEXTURE_VERTEXCOLOR_SKINNED, "texture_color_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_TEXTURE_TEXGEN_VERTEXCOLOR, "texture_color_texgen", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_INTERACTION, "interaction", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_INTERACTION_SKINNED, "interaction_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_INTERACTION_AMBIENT, "interactionAmbient", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_INTERACTION_AMBIENT_SKINNED, "interactionAmbient_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_ENVIRONMENT, "environment", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_ENVIRONMENT_SKINNED, "environment_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_BUMPY_ENVIRONMENT, "bumpyEnvironment", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_BUMPY_ENVIRONMENT_SKINNED, "bumpyEnvironment_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },

		{ BUILTIN_DEPTH, "depth", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_DEPTH_SKINNED, "depth_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_SHADOW, "shadow", SHADER_STAGE_VERTEX, LAYOUT_DRAW_SHADOW_VERT },
		{ BUILTIN_SHADOW_SKINNED, "shadow_skinned", SHADER_STAGE_VERTEX, LAYOUT_DRAW_SHADOW_VERT_SKINNED },
		{ BUILTIN_SHADOW_DEBUG, "shadowDebug", SHADER_STAGE_ALL, LAYOUT_DRAW_SHADOW_VERT },
		{ BUILTIN_SHADOW_DEBUG_SKINNED, "shadowDebug_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_SHADOW_VERT_SKINNED },

		{ BUILTIN_BLENDLIGHT, "blendlight", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_FOG, "fog", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_FOG_SKINNED, "fog_skinned", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_SKYBOX, "skybox", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_WOBBLESKY, "wobblesky", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_BINK, "bink", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
		{ BUILTIN_BINK_GUI, "bink_gui", SHADER_STAGE_ALL, LAYOUT_DRAW_VERT },
	};
	m_renderProgs.SetNum( MAX_BUILTINS );
	
	for ( int i = 0; i < MAX_BUILTINS; i++ ) {
		
		int vIndex = -1;
		if ( builtins[ i ].stages & SHADER_STAGE_VERTEX ) {
			vIndex = FindShader( builtins[ i ].name, SHADER_STAGE_VERTEX );
		}

		int fIndex = -1;
		if ( builtins[ i ].stages & SHADER_STAGE_FRAGMENT ) {
			fIndex = FindShader( builtins[ i ].name, SHADER_STAGE_FRAGMENT );
		}
		
		renderProg_t & prog = m_renderProgs[ i ];
		prog.name = builtins[ i ].name;
		prog.vertexShaderIndex = vIndex;
		prog.fragmentShaderIndex = fIndex;
		prog.vertexLayoutType = builtins[ i ].layout;

		CreateDescriptorSetLayout( 
			m_shaders[ vIndex ],
			( fIndex > -1 ) ? m_shaders[ fIndex ] : defaultShader,
			prog );
	}

	m_uniforms.SetNum( RENDERPARM_TOTAL, vec4_zero );

	m_renderProgs[ BUILTIN_TEXTURE_VERTEXCOLOR_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_INTERACTION_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_INTERACTION_AMBIENT_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_ENVIRONMENT_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_BUMPY_ENVIRONMENT_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_DEPTH_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_SHADOW_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_SHADOW_DEBUG_SKINNED ].usesJoints = true;
	m_renderProgs[ BUILTIN_FOG_SKINNED ].usesJoints = true;

	// Create Vertex Descriptions
	CreateVertexDescriptions();

	// Create Descriptor Pools
	CreateDescriptorPools( m_descriptorPools );

	for ( int i = 0; i < NUM_FRAME_DATA; ++i ) {
		m_parmBuffers[ i ] = new idUniformBuffer();
		m_parmBuffers[ i ]->AllocBufferObject( NULL, MAX_DESC_SETS * MAX_DESC_SET_UNIFORMS * sizeof( idVec4 ), BU_DYNAMIC );
	}

	// Placeholder: mainly for optionalSkinning
	emptyUBO.AllocBufferObject( NULL, sizeof( idVec4 ), BU_DYNAMIC );
}

/*
===============
idRenderBackend::DestroyRenderProgs
===============
*/
void idRenderBackend::DestroyRenderProgs() {
	// destroy shaders
	for ( int i = 0; i < m_shaders.Num(); ++i ) {
		shader_t & shader = m_shaders[ i ];
		vkDestroyShaderModule( vkcontext.device, shader.module, NULL );
		shader.module = VK_NULL_HANDLE;
	}

	// destroy pipelines
	for ( int i = 0; i < m_renderProgs.Num(); ++i ) {
		renderProg_t & prog = m_renderProgs[ i ];
		
		for ( int j = 0; j < prog.pipelines.Num(); ++j ) {
			vkDestroyPipeline( vkcontext.device, prog.pipelines[ j ].pipeline, NULL );
		}
		prog.pipelines.Clear();

		vkDestroyDescriptorSetLayout( vkcontext.device, prog.descriptorSetLayout, NULL );
		vkDestroyPipelineLayout( vkcontext.device, prog.pipelineLayout, NULL );
	}
	m_renderProgs.Clear();

	for ( int i = 0; i < NUM_FRAME_DATA; ++i ) {
		m_parmBuffers[ i ]->FreeBufferObject();
		delete m_parmBuffers[ i ];
		m_parmBuffers[ i ] = NULL;
	}

	emptyUBO.FreeBufferObject();

	for ( int i = 0; i < NUM_FRAME_DATA; ++i ) {
		//vkFreeDescriptorSets( vkcontext.device, m_descriptorPools[ i ], MAX_DESC_SETS, m_descriptorSets[ i ] );
		vkResetDescriptorPool( vkcontext.device, m_descriptorPools[ i ], 0 );
		vkDestroyDescriptorPool( vkcontext.device, m_descriptorPools[ i ], NULL );
	}

	memset( m_descriptorSets, 0, sizeof( m_descriptorSets ) );
	memset( m_descriptorPools, 0, sizeof( m_descriptorPools ) );

	m_currentDescSet = 0;
}

/*
========================
idRenderBackend::FindProgram
========================
*/
int idRenderBackend::FindProgram( const char * name, int vIndex, int fIndex ) {
	for ( int i = 0; i < m_renderProgs.Num(); ++i ) {
		renderProg_t & prog = m_renderProgs[ i ];
		if ( prog.vertexShaderIndex == vIndex && 
			 prog.fragmentShaderIndex == fIndex ) {
			return i;
		}
	}

	renderProg_t program;
	program.name = name;
	program.vertexShaderIndex = vIndex;
	program.fragmentShaderIndex = fIndex;

	CreateDescriptorSetLayout( m_shaders[ vIndex ], m_shaders[ fIndex ], program );

	// HACK: HeatHaze ( optional skinning )
	{
		static const int heatHazeNameNum = 3;
		static const char * const heatHazeNames[ heatHazeNameNum ] = {
			"heatHaze",
			"heatHazeWithMask",
			"heatHazeWithMaskAndVertex"
		};
		for ( int i = 0; i < heatHazeNameNum; ++i ) {
			// Use the vertex shader name because the renderProg name is more unreliable
			if ( idStr::Icmp( m_shaders[ vIndex ].name.c_str(), heatHazeNames[ i ] ) == 0 ) {
				program.usesJoints = true;
				program.optionalSkinning = true;
				break;
			}
		}
	}

	int index = m_renderProgs.Append( program );
	return index;
}

/*
========================
idRenderBackend::BindProgram
========================
*/
void idRenderBackend::BindProgram( int index ) {
	if ( m_currentRp == index ) {
		return;
	}

	m_currentRp = index;
	RENDERLOG_PRINTF( "Binding SPIRV Program %s\n", m_renderProgs[ index ].name.c_str() );
}

/*
========================
idRenderBackend::CommitCurrent
========================
*/
void idRenderBackend::CommitCurrent( uint64 stateBits, idImage * target ) {
	renderProg_t & prog = m_renderProgs[ m_currentRp ];

	VkPipeline pipeline = prog.GetPipeline( 
		stateBits, target,
		m_shaders[ prog.vertexShaderIndex ].module,
		prog.fragmentShaderIndex != -1 ? m_shaders[ prog.fragmentShaderIndex ].module : VK_NULL_HANDLE );

	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.pNext = NULL;
	setAllocInfo.descriptorPool = m_descriptorPools[ vkcontext.currentFrameData ];
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &prog.descriptorSetLayout;

	ID_VK_CHECK( vkAllocateDescriptorSets( vkcontext.device, &setAllocInfo, &m_descriptorSets[ vkcontext.currentFrameData ][ m_currentDescSet ] ) );

	VkDescriptorSet descSet = m_descriptorSets[ vkcontext.currentFrameData ][ m_currentDescSet ];
	m_currentDescSet++;

	int writeIndex = 0;
	int bufferIndex = 0;
	int	imageIndex = 0;
	int bindingIndex = 0;
	
	VkWriteDescriptorSet writes[ MAX_DESC_SET_WRITES ];
	VkDescriptorBufferInfo bufferInfos[ MAX_DESC_SET_WRITES ];
	VkDescriptorImageInfo imageInfos[ MAX_DESC_SET_WRITES ];

	int uboIndex = 0;
	idUniformBuffer * ubos[ 3 ] = { NULL, NULL, NULL };

	idUniformBuffer vertParms;
	if ( prog.vertexShaderIndex > -1 && m_shaders[ prog.vertexShaderIndex ].parmIndices.Num() > 0 ) {
		AllocParmBlockBuffer( m_shaders[ prog.vertexShaderIndex ].parmIndices, vertParms );

		ubos[ uboIndex++ ] = &vertParms;
	}

	idUniformBuffer jointBuffer;
	if ( prog.usesJoints && vkcontext.jointCacheHandle > 0 ) {
		if ( !vertexCache.GetJointBuffer( vkcontext.jointCacheHandle, &jointBuffer ) ) {
			idLib::Error( "idRenderProgManager::CommitCurrent: jointBuffer == NULL" );
			return;
		}
		assert( ( jointBuffer.GetOffset() & ( vkcontext.gpu->props.limits.minUniformBufferOffsetAlignment - 1 ) ) == 0 );

		ubos[ uboIndex++ ] = &jointBuffer;
	} else if ( prog.optionalSkinning ) {
		ubos[ uboIndex++ ] = &emptyUBO;
	}

	idUniformBuffer fragParms;
	if ( prog.fragmentShaderIndex > -1 && m_shaders[ prog.fragmentShaderIndex ].parmIndices.Num() > 0 ) {
		AllocParmBlockBuffer( m_shaders[ prog.fragmentShaderIndex ].parmIndices, fragParms );

		ubos[ uboIndex++ ] = &fragParms;
	}

	for ( int i = 0; i < prog.bindings.Num(); ++i ) {
		rpBinding_t binding = prog.bindings[ i ];

		switch ( binding ) {
			case BINDING_TYPE_UNIFORM_BUFFER: {
				idUniformBuffer * ubo = ubos[ bufferIndex ];

				VkDescriptorBufferInfo & bufferInfo = bufferInfos[ bufferIndex++ ];
				memset( &bufferInfo, 0, sizeof( VkDescriptorBufferInfo ) );
				bufferInfo.buffer = ubo->GetAPIObject();
				bufferInfo.offset = ubo->GetOffset();
				bufferInfo.range = ubo->GetSize();

				VkWriteDescriptorSet & write = writes[ writeIndex++ ];
				memset( &write, 0, sizeof( VkWriteDescriptorSet ) );
				write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				write.dstSet = descSet;
				write.dstBinding = bindingIndex++;
				write.descriptorCount = 1;
				write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				write.pBufferInfo = &bufferInfo;
				
				break;
			}
			case BINDING_TYPE_SAMPLER: {
				idImage * image = vkcontext.imageParms[ imageIndex ];

				VkDescriptorImageInfo & imageInfo = imageInfos[ imageIndex++ ];
				memset( &imageInfo, 0, sizeof( VkDescriptorImageInfo ) );
				imageInfo.imageLayout = image->GetLayout();
				imageInfo.imageView = image->GetView();
				imageInfo.sampler = image->GetSampler();
				
				assert( image->GetView() != VK_NULL_HANDLE );

				VkWriteDescriptorSet & write = writes[ writeIndex++ ];
				memset( &write, 0, sizeof( VkWriteDescriptorSet ) );
				write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				write.dstSet = descSet;
				write.dstBinding = bindingIndex++;
				write.descriptorCount = 1;
				write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				write.pImageInfo = &imageInfo;
				
				break;
			}
		}
	}

	vkUpdateDescriptorSets( vkcontext.device, writeIndex, writes, 0, NULL );

	vkCmdBindDescriptorSets( vkcontext.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, prog.pipelineLayout, 0, 1, &descSet, 0, NULL );
	vkCmdBindPipeline( vkcontext.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
}

/*
========================
idRenderBackend::LoadShader
========================
*/
void idRenderBackend::LoadShader( int index ) {
	if ( m_shaders[ index ].module != VK_NULL_HANDLE ) {
		return; // Already loaded
	}

	LoadShader( m_shaders[ index ] );
}

/*
========================
idRenderBackend::LoadShader
========================
*/
void idRenderBackend::LoadShader( shader_t & shader ) {
	idStr spirvPath;
	idStr layoutPath;
	spirvPath.Format( "renderprogs\\spirv\\%s", shader.name.c_str() );
	layoutPath.Format( "renderprogs\\vkglsl\\%s", shader.name.c_str() );
	if ( shader.stage == SHADER_STAGE_FRAGMENT ) {
		spirvPath += ".fspv";
		layoutPath += ".frag.layout";
	} else {
		spirvPath += ".vspv";
		layoutPath += ".vert.layout";
	}

	void * spirvBuffer = NULL;
	int sprivLen = fileSystem->ReadFile( spirvPath.c_str(), &spirvBuffer );
	if ( sprivLen <= 0 ) {
		idLib::Error( "idRenderBackend::LoadShader: Unable to load SPIRV shader file %s.", spirvPath.c_str() );
	}

	void * layoutBuffer = NULL;
	int layoutLen = fileSystem->ReadFile( layoutPath.c_str(), &layoutBuffer );
	if ( layoutLen <= 0 ) {
		idLib::Error( "idRenderBackend::LoadShader: Unable to load layout file %s.", layoutPath.c_str() );
	}

	idStr layout = ( const char * )layoutBuffer;

	idLexer src( layout.c_str(), layout.Length(), "layout" );
	idToken token;

	if ( src.ExpectTokenString( "uniforms" ) ) {
		src.ExpectTokenString( "[" );

		while ( !src.CheckTokenString( "]" ) ) {
			src.ReadToken( &token );

			int index = -1;
			for ( int i = 0; i < RENDERPARM_TOTAL && index == -1; ++i ) {
				if ( token == GLSLParmNames[ i ] ) {
					index = i;
				}
			}

			if ( index == -1 ) {
				idLib::Error( "Invalid uniform %s", token.c_str() );
			}

			shader.parmIndices.Append( static_cast< renderParm_t >( index ) );
		}
	}

	if ( src.ExpectTokenString( "bindings" ) ) {
		src.ExpectTokenString( "[" );

		while ( !src.CheckTokenString( "]" ) ) {
			src.ReadToken( &token );

			int index = -1;
			for ( int i = 0; i < BINDING_TYPE_MAX; ++i ) {
				if ( token == renderProgBindingStrings[ i ] ) {
					index = i;
				}
			}

			if ( index == -1 ) {
				idLib::Error( "Invalid binding %s", token.c_str() );
			}

			shader.bindings.Append( static_cast< rpBinding_t >( index ) );
		}
	}

	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = sprivLen;
	shaderModuleCreateInfo.pCode = (uint32 *)spirvBuffer;

	ID_VK_CHECK( vkCreateShaderModule( vkcontext.device, &shaderModuleCreateInfo, NULL, &shader.module ) );

	Mem_Free( layoutBuffer );
	Mem_Free( spirvBuffer );
}

/*
========================
idRenderBackend::AllocParmBlockBuffer
========================
*/
void idRenderBackend::AllocParmBlockBuffer( const idList< int > & parmIndices, idUniformBuffer & ubo ) {
	const int numParms = parmIndices.Num();
	const int bytes = ALIGN( numParms * sizeof( idVec4 ), vkcontext.gpu->props.limits.minUniformBufferOffsetAlignment );

	ubo.Reference( *m_parmBuffers[ vkcontext.currentFrameData ], m_currentParmBufferOffset, bytes );

	idVec4 * uniforms = (idVec4 *)ubo.MapBuffer( BM_WRITE );
	
	for ( int i = 0; i < numParms; ++i ) {
		uniforms[ i ] = GetRenderParm( static_cast< renderParm_t >( parmIndices[ i ] ) );
	}

	ubo.UnmapBuffer();

	m_currentParmBufferOffset += bytes;
}