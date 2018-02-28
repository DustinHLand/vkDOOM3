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
#include "../GLState.h"
#include "../RenderProgs.h"
#include "../RenderBackend.h"
#include "../RenderLog.h"
#include "../Image.h"

void RpPrintState( uint64 stateBits );

struct vertexLayout_t {
	VkPipelineVertexInputStateCreateInfo inputState;
	idList< VkVertexInputBindingDescription > bindingDesc;
	idList< VkVertexInputAttributeDescription > attributeDesc;
};

static vertexLayout_t vertexLayouts[ NUM_VERTEX_LAYOUTS ];

static shader_t defaultShader;
static idUniformBuffer emptyUBO;

static const char * renderProgBindingStrings[ BINDING_TYPE_MAX ] = {
	"ubo",
	"sampler"
};

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
		uint64 stateBits ) {

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
		if ( vkcontext.gpu.features.depthBounds ) {
			depthStencilState.depthBoundsTestEnable = ( stateBits & GLS_DEPTH_TEST_MASK ) != 0;
			depthStencilState.minDepthBounds = 0.0f;
			depthStencilState.maxDepthBounds = 1.0f;
		}
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

	if ( vkcontext.gpu.features.depthBounds && ( stateBits & GLS_DEPTH_TEST_MASK ) ) {
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
	createInfo.renderPass = vkcontext.renderPass;
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
VkPipeline renderProg_t::GetPipeline( uint64 stateBits, VkShaderModule vertexShader, VkShaderModule fragmentShader ) {
	for ( int i = 0; i < pipelines.Num(); ++i ) {
		if ( stateBits == pipelines[ i ].stateBits ) {
			return pipelines[ i ].pipeline;
		}
	}

	VkPipeline pipeline = CreateGraphicsPipeline( vertexLayoutType, vertexShader, fragmentShader, pipelineLayout, stateBits );

	pipelineState_t pipelineState;
	pipelineState.pipeline = pipeline;
	pipelineState.stateBits = stateBits;
	pipelines.Append( pipelineState );

	return pipeline;
}

/*
========================
idRenderProgManager::idRenderProgManager
========================
*/
idRenderProgManager::idRenderProgManager() : 
	m_current( 0 ),
	m_counter( 0 ),
	m_currentData( 0 ),
	m_currentDescSet( 0 ),
	m_currentParmBufferOffset( 0 ) {
	
	memset( m_parmBuffers, 0, sizeof( m_parmBuffers ) );
}

/*
========================
idRenderProgManager::Init
========================
*/
void idRenderProgManager::Init() {
	idLib::Printf( "----- Initializing Render Shaders -----\n" );

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
========================
idRenderProgManager::Shutdown
========================
*/
void idRenderProgManager::Shutdown() {
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

	m_counter = 0;
	m_currentData = 0;
	m_currentDescSet = 0;
}

/*
========================
idRenderProgManager::StartFrame
========================
*/
void idRenderProgManager::StartFrame() {
	m_counter++;
	m_currentData = m_counter % NUM_FRAME_DATA;
	m_currentDescSet = 0;
	m_currentParmBufferOffset = 0;

	vkResetDescriptorPool( vkcontext.device, m_descriptorPools[ m_currentData ], 0 );
}

/*
========================
idRenderProgManager::BindProgram
========================
*/
void idRenderProgManager::BindProgram( int index ) {
	if ( m_current == index ) {
		return;
	}

	m_current = index;
	RENDERLOG_PRINTF( "Binding SPIRV Program %s\n", m_renderProgs[ index ].name.c_str() );
}

/*
========================
idRenderProgManager::AllocParmBlockBuffer
========================
*/
void idRenderProgManager::AllocParmBlockBuffer( const idList< int > & parmIndices, idUniformBuffer & ubo ) {
	const int numParms = parmIndices.Num();
	const int bytes = ALIGN( numParms * sizeof( idVec4 ), vkcontext.gpu.props.limits.minUniformBufferOffsetAlignment );

	ubo.Reference( *m_parmBuffers[ m_currentData ], m_currentParmBufferOffset, bytes );

	idVec4 * uniforms = (idVec4 *)ubo.MapBuffer( BM_WRITE );
	
	for ( int i = 0; i < numParms; ++i ) {
		uniforms[ i ] = renderProgManager.GetRenderParm( static_cast< renderParm_t >( parmIndices[ i ] ) );
	}

	ubo.UnmapBuffer();

	m_currentParmBufferOffset += bytes;
}

/*
========================
idRenderProgManager::CommitCurrent
========================
*/
void idRenderProgManager::CommitCurrent( uint64 stateBits, VkCommandBuffer commandBuffer ) {
	renderProg_t & prog = m_renderProgs[ m_current ];

	VkPipeline pipeline = prog.GetPipeline( 
		stateBits,
		m_shaders[ prog.vertexShaderIndex ].module,
		prog.fragmentShaderIndex != -1 ? m_shaders[ prog.fragmentShaderIndex ].module : VK_NULL_HANDLE );

	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.pNext = NULL;
	setAllocInfo.descriptorPool = m_descriptorPools[ m_currentData ];
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &prog.descriptorSetLayout;

	ID_VK_CHECK( vkAllocateDescriptorSets( vkcontext.device, &setAllocInfo, &m_descriptorSets[ m_currentData ][ m_currentDescSet ] ) );

	VkDescriptorSet descSet = m_descriptorSets[ m_currentData ][ m_currentDescSet ];
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
		assert( ( jointBuffer.GetOffset() & ( vkcontext.gpu.props.limits.minUniformBufferOffsetAlignment - 1 ) ) == 0 );

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

	vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, prog.pipelineLayout, 0, 1, &descSet, 0, NULL );
	vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
}

/*
========================
idRenderProgManager::FindProgram
========================
*/
int idRenderProgManager::FindProgram( const char * name, int vIndex, int fIndex ) {
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
idRenderProgManager::LoadShader
========================
*/
void idRenderProgManager::LoadShader( int index ) {
	if ( m_shaders[ index ].module != VK_NULL_HANDLE ) {
		return; // Already loaded
	}

	LoadShader( m_shaders[ index ] );
}

/*
========================
idRenderProgManager::LoadShader
========================
*/
void idRenderProgManager::LoadShader( shader_t & shader ) {
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
		idLib::Error( "idRenderProgManager::LoadShader: Unable to load SPIRV shader file %s.", spirvPath.c_str() );
	}

	void * layoutBuffer = NULL;
	int layoutLen = fileSystem->ReadFile( layoutPath.c_str(), &layoutBuffer );
	if ( layoutLen <= 0 ) {
		idLib::Error( "idRenderProgManager::LoadShader: Unable to load layout file %s.", layoutPath.c_str() );
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

CONSOLE_COMMAND( Vulkan_ClearPipelines, "Clear all existing pipelines, forcing them to be recreated.", 0 ) {
	for ( int i = 0; i < renderProgManager.m_renderProgs.Num(); ++i ) {
		renderProg_t & prog = renderProgManager.m_renderProgs[ i ];
		for ( int j = 0; j < prog.pipelines.Num(); ++j ) {
			vkDestroyPipeline( vkcontext.device, prog.pipelines[ j ].pipeline, NULL );
		}
		prog.pipelines.Clear();
	}
}

CONSOLE_COMMAND( Vulkan_PrintNumPipelines, "Print the number of pipelines available.", 0 ) {
	int totalPipelines = 0;
	for ( int i = 0; i < renderProgManager.m_renderProgs.Num(); ++i ) {
		renderProg_t & prog = renderProgManager.m_renderProgs[ i ];
		int progPipelines = prog.pipelines.Num();
		totalPipelines += progPipelines;
		idLib::Printf( "%s: %d\n", prog.name.c_str(), progPipelines );
	}
	idLib::Printf( "TOTAL: %d\n", totalPipelines );
}

CONSOLE_COMMAND( Vulkan_PrintPipelineStates, "Print the GLState bits associated with each pipeline.", 0 ) {
	for ( int i = 0; i < renderProgManager.m_renderProgs.Num(); ++i ) {
		renderProg_t & prog = renderProgManager.m_renderProgs[ i ];
		for ( int j = 0; j < prog.pipelines.Num(); ++j ) {
			idLib::Printf( "%s: %llu\n", prog.name.c_str(), prog.pipelines[ j ].stateBits );
			idLib::Printf( "------------------------------------------\n" );
			RpPrintState( prog.pipelines[ j ].stateBits );
			idLib::Printf( "\n" );
		}
	}
}