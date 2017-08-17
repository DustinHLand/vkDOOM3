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
#ifndef __RENDERPROGS_H__
#define __RENDERPROGS_H__


#define VERTEX_UNIFORM_ARRAY_NAME			"_va_"
#define FRAGMENT_UNIFORM_ARRAY_NAME			"_fa_"

static const int PC_ATTRIB_INDEX_VERTEX		= 0;
static const int PC_ATTRIB_INDEX_NORMAL		= 2;
static const int PC_ATTRIB_INDEX_COLOR		= 3;
static const int PC_ATTRIB_INDEX_COLOR2		= 4;
static const int PC_ATTRIB_INDEX_ST			= 8;
static const int PC_ATTRIB_INDEX_TANGENT	= 9;

enum vertexMask_t {
	VERTEX_MASK_XYZ			= BIT( PC_ATTRIB_INDEX_VERTEX ),
	VERTEX_MASK_ST			= BIT( PC_ATTRIB_INDEX_ST ),
	VERTEX_MASK_NORMAL		= BIT( PC_ATTRIB_INDEX_NORMAL ),
	VERTEX_MASK_COLOR		= BIT( PC_ATTRIB_INDEX_COLOR ),
	VERTEX_MASK_TANGENT		= BIT( PC_ATTRIB_INDEX_TANGENT ),
	VERTEX_MASK_COLOR2		= BIT( PC_ATTRIB_INDEX_COLOR2 ),
};

enum vertexLayoutType_t {
	LAYOUT_UNKNOWN = -1,
	LAYOUT_DRAW_VERT,
	LAYOUT_DRAW_SHADOW_VERT,
	LAYOUT_DRAW_SHADOW_VERT_SKINNED,
	NUM_VERTEX_LAYOUTS
};

// This enum list corresponds to the global constant register indecies as defined in global.inc for all
// shaders.  We used a shared pool to keeps things simple.  If something changes here then it also
// needs to change in global.inc and vice versa
enum renderParm_t {
	// For backwards compatibility, do not change the order of the first 17 items
	RENDERPARM_SCREENCORRECTIONFACTOR = 0,
	RENDERPARM_WINDOWCOORD,
	RENDERPARM_DIFFUSEMODIFIER,
	RENDERPARM_SPECULARMODIFIER,

	RENDERPARM_LOCALLIGHTORIGIN,
	RENDERPARM_LOCALVIEWORIGIN,

	RENDERPARM_LIGHTPROJECTION_S,
	RENDERPARM_LIGHTPROJECTION_T,
	RENDERPARM_LIGHTPROJECTION_Q,
	RENDERPARM_LIGHTFALLOFF_S,

	RENDERPARM_BUMPMATRIX_S,
	RENDERPARM_BUMPMATRIX_T,

	RENDERPARM_DIFFUSEMATRIX_S,
	RENDERPARM_DIFFUSEMATRIX_T,

	RENDERPARM_SPECULARMATRIX_S,
	RENDERPARM_SPECULARMATRIX_T,

	RENDERPARM_VERTEXCOLOR_MODULATE,
	RENDERPARM_VERTEXCOLOR_ADD,

	// The following are new and can be in any order
	
	RENDERPARM_COLOR,
	RENDERPARM_VIEWORIGIN,
	RENDERPARM_GLOBALEYEPOS,

	RENDERPARM_MVPMATRIX_X,
	RENDERPARM_MVPMATRIX_Y,
	RENDERPARM_MVPMATRIX_Z,
	RENDERPARM_MVPMATRIX_W,

	RENDERPARM_MODELMATRIX_X,
	RENDERPARM_MODELMATRIX_Y,
	RENDERPARM_MODELMATRIX_Z,
	RENDERPARM_MODELMATRIX_W,

	RENDERPARM_PROJMATRIX_X,
	RENDERPARM_PROJMATRIX_Y,
	RENDERPARM_PROJMATRIX_Z,
	RENDERPARM_PROJMATRIX_W,

	RENDERPARM_MODELVIEWMATRIX_X,
	RENDERPARM_MODELVIEWMATRIX_Y,
	RENDERPARM_MODELVIEWMATRIX_Z,
	RENDERPARM_MODELVIEWMATRIX_W,

	RENDERPARM_TEXTUREMATRIX_S,
	RENDERPARM_TEXTUREMATRIX_T,

	RENDERPARM_TEXGEN_0_S,
	RENDERPARM_TEXGEN_0_T,
	RENDERPARM_TEXGEN_0_Q,
	RENDERPARM_TEXGEN_0_ENABLED,

	RENDERPARM_TEXGEN_1_S,
	RENDERPARM_TEXGEN_1_T,
	RENDERPARM_TEXGEN_1_Q,
	RENDERPARM_TEXGEN_1_ENABLED,

	RENDERPARM_WOBBLESKY_X,
	RENDERPARM_WOBBLESKY_Y,
	RENDERPARM_WOBBLESKY_Z,

	RENDERPARM_OVERBRIGHT,
	RENDERPARM_ENABLE_SKINNING,
	RENDERPARM_ALPHA_TEST,

	RENDERPARM_USER0,
	RENDERPARM_USER1,
	RENDERPARM_USER2,
	RENDERPARM_USER3,
	RENDERPARM_USER4,
	RENDERPARM_USER5,
	RENDERPARM_USER6,
	RENDERPARM_USER7,

	RENDERPARM_TOTAL
};

const char * GLSLParmNames[];

enum rpBuiltIn_t {
	BUILTIN_GUI,
	BUILTIN_COLOR,
	BUILTIN_SIMPLESHADE,
	BUILTIN_TEXTURED,
	BUILTIN_TEXTURE_VERTEXCOLOR,
	BUILTIN_TEXTURE_VERTEXCOLOR_SKINNED,
	BUILTIN_TEXTURE_TEXGEN_VERTEXCOLOR,
	BUILTIN_INTERACTION,
	BUILTIN_INTERACTION_SKINNED,
	BUILTIN_INTERACTION_AMBIENT,
	BUILTIN_INTERACTION_AMBIENT_SKINNED,
	BUILTIN_ENVIRONMENT,
	BUILTIN_ENVIRONMENT_SKINNED,
	BUILTIN_BUMPY_ENVIRONMENT,
	BUILTIN_BUMPY_ENVIRONMENT_SKINNED,

	BUILTIN_DEPTH,
	BUILTIN_DEPTH_SKINNED,
	BUILTIN_SHADOW,
	BUILTIN_SHADOW_SKINNED,
	BUILTIN_SHADOW_DEBUG,
	BUILTIN_SHADOW_DEBUG_SKINNED,

	BUILTIN_BLENDLIGHT,
	BUILTIN_FOG,
	BUILTIN_FOG_SKINNED,
	BUILTIN_SKYBOX,
	BUILTIN_WOBBLESKY,
	BUILTIN_BINK,
	BUILTIN_BINK_GUI,

	MAX_BUILTINS
};

enum rpStage_t {
	SHADER_STAGE_VERTEX		= BIT( 0 ),
	SHADER_STAGE_FRAGMENT	= BIT( 1 ),
	SHADER_STAGE_ALL		= SHADER_STAGE_VERTEX | SHADER_STAGE_FRAGMENT
};

enum rpBinding_t {
	BINDING_TYPE_UNIFORM_BUFFER,
	BINDING_TYPE_SAMPLER,
	BINDING_TYPE_MAX
};

struct shader_t {
	shader_t() : module( VK_NULL_HANDLE ) {}

	idStr					name;
	rpStage_t				stage;
	VkShaderModule			module;
	idList< rpBinding_t >	bindings;
	idList< int >			parmIndices;
};

struct renderProg_t {
	renderProg_t() :
					usesJoints( false ),
					optionalSkinning( false ),
					vertexShaderIndex( -1 ),
					fragmentShaderIndex( -1 ),
					vertexLayoutType( LAYOUT_DRAW_VERT ),
					pipelineLayout( VK_NULL_HANDLE ),
					descriptorSetLayout( VK_NULL_HANDLE ) {}

	struct pipelineState_t {
		pipelineState_t() : 
					stateBits( 0 ),
					pipeline( VK_NULL_HANDLE ) {
			memset( stencilOperations, 0xFF, sizeof( stencilOperations ) );
		}

		uint64		stateBits;
		VkPipeline	pipeline;
		uint64		stencilOperations[ 2 ];
	};

	VkPipeline GetPipeline( uint64 stateBits, VkShaderModule vertexShader, VkShaderModule fragmentShader );

	idStr						name;
	bool						usesJoints;
	bool						optionalSkinning;
	int							vertexShaderIndex;
	int							fragmentShaderIndex;
	vertexLayoutType_t			vertexLayoutType;
	VkPipelineLayout			pipelineLayout;
	VkDescriptorSetLayout		descriptorSetLayout;
	idList< rpBinding_t >		bindings;
	idList< pipelineState_t >	pipelines;
};

/*
===========================================================================

idRenderProgManager

===========================================================================
*/
class idRenderProgManager {
public:
	idRenderProgManager();

	void	Init();
	void	Shutdown();

	void	StartFrame();

	const idVec4 & GetRenderParm( renderParm_t rp );
	void	SetRenderParm( renderParm_t rp, const float * value );
	void	SetRenderParms( renderParm_t rp, const float * values, int numValues );
	
	const renderProg_t & GetCurrentRenderProg() const { return m_renderProgs[ m_current ]; }
	int		FindShader( const char * name, rpStage_t stage );
	void	BindProgram( int index );
	void	Unbind();

	void	CommitCurrent( uint64 stateBits );
	int		FindProgram( const char * name, int vIndex, int fIndex );

private:
	void	LoadShader( int index );
	void	LoadShader( shader_t & shader );

	void	AllocParmBlockBuffer( const idList< int > & parmIndices, idUniformBuffer & ubo );

public:
	idList< renderProg_t, TAG_RENDER > m_renderProgs;

private:
	int	m_current;
	idStaticList< idVec4, RENDERPARM_TOTAL > m_uniforms;

	int	m_builtinShaders[ MAX_BUILTINS ];
	idList< shader_t, TAG_RENDER >	m_shaders;

	int					m_counter;
	int					m_currentData;
	int					m_currentDescSet;
	int					m_currentParmBufferOffset;
	VkDescriptorPool	m_descriptorPools[ NUM_FRAME_DATA ];
	VkDescriptorSet		m_descriptorSets[ NUM_FRAME_DATA ][ MAX_DESC_SETS ];

	idUniformBuffer *	m_parmBuffers[ NUM_FRAME_DATA ];
};

extern idRenderProgManager renderProgManager;

#endif
