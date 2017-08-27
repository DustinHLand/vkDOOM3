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

#ifndef __RENDERER_COMMON_H__
#define __RENDERER_COMMON_H__

#include "ScreenRect.h"
#include "jobs/ShadowShared.h"

class idRenderModelDecal;
class idRenderModelOverlay;
class idRenderEntity;
class idRenderLight;
class idInteraction;

// maximum texture units
const int MAX_PROG_TEXTURE_PARMS	= 16;

const int FALLOFF_TEXTURE_SIZE		= 64;

const float	DEFAULT_FOG_DISTANCE	= 500.0f;

// picky to get the bilerp correct at terminator
const int FOG_ENTER_SIZE			= 64;
const float FOG_ENTER				= (FOG_ENTER_SIZE+1.0f)/(FOG_ENTER_SIZE*2);

// we may expand this to six for some subview issuess
const int MAX_CLIP_PLANES			= 1;

// this is the inital allocation for max number of drawsurfs
// in a given view, but it will automatically grow if needed
const int INITIAL_DRAWSURFS			= 2048;

static const int MAX_RENDER_CROPS	= 8;

// Guis
const int MAX_RENDERENTITY_GUI		= 3;
// default size of the drawSurfs list for guis, will
// be automatically expanded as needed
const int MAX_GUI_SURFACES			= 1024;

// Portals
static const int NUM_PORTAL_ATTRIBUTES = 3;
static const int CHILDREN_HAVE_MULTIPLE_AREAS = -2;
static const int AREANUM_SOLID = -1;

#if defined( ID_VULKAN )
static const int MAX_DESC_SETS				= 16384;
static const int MAX_DESC_UNIFORM_BUFFERS	= 8192;
static const int MAX_DESC_IMAGE_SAMPLERS	= 12384;
static const int MAX_DESC_SET_WRITES		= 32;
static const int MAX_DESC_SET_UNIFORMS		= 48;
static const int MAX_IMAGE_PARMS			= 16;
static const int MAX_UBO_PARMS				= 2;
static const int NUM_TIMESTAMP_QUERIES		= 16;
#endif

// vertCacheHandle_t packs size, offset, and frame number into 64 bits
typedef uint64 vertCacheHandle_t;

// How is this texture used?  Determines the storage and color format
typedef enum {
	TD_SPECULAR,			// may be compressed, and always zeros the alpha channel
	TD_DIFFUSE,				// may be compressed
	TD_DEFAULT,				// generic RGBA texture (particles, etc...)
	TD_BUMP,				// may be compressed with 8 bit lookup
	TD_FONT,				// Font image
	TD_LIGHT,				// Light image
	TD_LOOKUP_TABLE_MONO,	// Mono lookup table (including alpha)
	TD_LOOKUP_TABLE_ALPHA,	// Alpha lookup table with a white color channel
	TD_LOOKUP_TABLE_RGB1,	// RGB lookup table with a solid white alpha
	TD_LOOKUP_TABLE_RGBA,	// RGBA lookup table
	TD_COVERAGE,			// coverage map for fill depth pass when YCoCG is used
	TD_DEPTH,				// depth buffer copy for motion blur
} textureUsage_t;

typedef enum {
	CF_2D,			// not a cube map
	CF_NATIVE,		// _px, _nx, _py, etc, directly sent to GL
	CF_CAMERA		// _forward, _back, etc, rotated and flipped as needed before sending to GL
} cubeFiles_t;

void GL_CheckErrors();

struct vidMode_t {
    int width;
	int height;
	int displayHz;

	bool operator==( const vidMode_t & a ) {
		return a.width == width && a.height == height && a.displayHz == displayHz;
	}
};

/*
===========================================================================

SURFACES

drawSurf_t

Command the back end to render surfaces
a given srfTriangles_t may be used with multiple viewEntity_t,
as when viewed in a subview or multiple viewport render, or
with multiple shaders when skinned, or, possibly with multiple
lights, although currently each lighting interaction creates
unique srfTriangles_t
drawSurf_t are always allocated and freed every frame, they are never cached

===========================================================================
*/

struct srfTriangles_t;
struct viewEntity_t;
struct viewLight_t;
class idMaterial;

struct drawSurf_t {
	const srfTriangles_t *	frontEndGeo;		// don't use on the back end, it may be updated by the front end!
	int						numIndexes;
	vertCacheHandle_t		indexCache;			// triIndex_t
	vertCacheHandle_t		ambientCache;		// idDrawVert
	vertCacheHandle_t		shadowCache;		// idShadowVert / idShadowVertSkinned
	vertCacheHandle_t		jointCache;			// idJointMat
	const viewEntity_t *	space;
	const idMaterial *		material;			// may be NULL for shadow volumes
	uint64					extraGLState;		// Extra GL state |'d with material->stage[].drawStateBits
	float					sort;				// material->sort, modified by gui / entity sort offsets
	const float	 *			shaderRegisters;	// evaluated and adjusted for referenceShaders
	drawSurf_t *			nextOnLight;		// viewLight chains
	drawSurf_t **			linkChain;			// defer linking to lights to a serial section to avoid a mutex
	idScreenRect			scissorRect;		// for scissor clipping, local inside renderView viewport
	int						renderZFail;
	volatile shadowVolumeState_t shadowVolumeState;
};

// areas have references to hold all the lights and entities in them
struct areaReference_t {
	areaReference_t *		areaNext;				// chain in the area
	areaReference_t *		areaPrev;
	areaReference_t *		ownerNext;				// chain on either the entityDef or lightDef
	idRenderEntity *		entity;					// only one of entity / light will be non-NULL
	idRenderLight *			light;					// only one of entity / light will be non-NULL
	struct portalArea_s	*	area;					// so owners can find all the areas they are in
};

/*
===========================================================================

SURFACES

idRenderLight

===========================================================================
*/

class idRenderModel;

typedef struct renderLight_s {
	idMat3					axis;				// rotation vectors, must be unit length
	idVec3					origin;

	// if non-zero, the light will not show up in the specific view,
	// which may be used if we want to have slightly different muzzle
	// flash lights for the player and other views
	int						suppressLightInViewID;

	// if non-zero, the light will only show up in the specific view
	// which can allow player gun gui lights and such to not effect everyone
	int						allowLightInViewID;

	// I am sticking the four bools together so there are no unused gaps in
	// the padded structure, which could confuse the memcmp that checks for redundant
	// updates
	bool					forceShadows;		// Used to override the material parameters
	bool					noShadows;			// (should we replace this with material parameters on the shader?)
	bool					noSpecular;			// (should we replace this with material parameters on the shader?)

	bool					pointLight;			// otherwise a projection light (should probably invert the sense of this, because points are way more common)
	bool					parallel;			// lightCenter gives the direction to the light at infinity
	idVec3					lightRadius;		// xyz radius for point lights
	idVec3					lightCenter;		// offset the lighting direction for shading and
												// shadows, relative to origin

	// frustum definition for projected lights, all reletive to origin
	// FIXME: we should probably have real plane equations here, and offer
	// a helper function for conversion from this format
	idVec3					target;
	idVec3					right;
	idVec3					up;
	idVec3					start;
	idVec3					end;

	// Dmap will generate an optimized shadow volume named _prelight_<lightName>
	// for the light against all the _area* models in the map.  The renderer will
	// ignore this value if the light has been moved after initial creation
	idRenderModel *			prelightModel;

	// muzzle flash lights will not cast shadows from player and weapon world models
	int						lightId;


	const idMaterial *		shader;				// NULL = either lights/defaultPointLight or lights/defaultProjectedLight
	float					shaderParms[ MAX_ENTITY_SHADER_PARMS ];		// can be used in any way by shader
	idSoundEmitter *		referenceSound;		// for shader sound tables, allowing effects to vary with sounds
} renderLight_t;

class idRenderLight {
public:
							idRenderLight();

	void					FreeRenderLight();
	void					UpdateRenderLight( const renderLight_t *re, bool forceUpdate = false );
	void					GetRenderLight( renderLight_t *re );
	void					ForceUpdate();

	bool					LightCastsShadows() const { return parms.forceShadows || ( !parms.noShadows && lightShader->LightCastsShadows() ); }

	renderLight_t			parms;					// specification

	bool					lightHasMoved;			// the light has changed its position since it was
													// first added, so the prelight model is not valid
	idRenderWorld *			world;
	int						index;					// in world lightdefs

	int						areaNum;				// if not -1, we may be able to cull all the light's
													// interactions if !viewDef->connectedAreas[areaNum]

	int						lastModifiedFrameNum;	// to determine if it is constantly changing,
													// and should go in the dynamic frame memory, or kept
													// in the cached memory
	bool					archived;				// for demo writing


	// derived information
	idPlane					lightProject[4];		// old style light projection where Z and W are flipped and projected lights lightProject[3] is divided by ( zNear + zFar )
	idRenderMatrix			baseLightProject;		// global xyz1 to projected light strq
	idRenderMatrix			inverseBaseLightProject;// transforms the zero-to-one cube to exactly cover the light in world space

	const idMaterial *		lightShader;			// guaranteed to be valid, even if parms.shader isn't
	idImage *				falloffImage;

	idVec3					globalLightOrigin;		// accounting for lightCenter and parallel
	idBounds				globalLightBounds;

	int						viewCount;				// if == tr.viewCount, the light is on the viewDef->viewLights list
	viewLight_t *			viewLight;

	areaReference_t *		references;				// each area the light is present in will have a lightRef
	idInteraction *			firstInteraction;		// doubly linked list
	idInteraction *			lastInteraction;

	struct doublePortal_s *	foggedPortals;
};

/*
===========================================================================

SURFACES

idRenderEntity

===========================================================================
*/

typedef bool(*deferredEntityCallback_t)( renderEntity_t *, const renderView_t * );

struct renderEntity_t {
	idRenderModel *			hModel;				// this can only be null if callback is set

	int						entityNum;
	int						bodyId;

	// Entities that are expensive to generate, like skeletal models, can be
	// deferred until their bounds are found to be in view, in the frustum
	// of a shadowing light that is in view, or contacted by a trace / overlay test.
	// This is also used to do visual cueing on items in the view
	// The renderView may be NULL if the callback is being issued for a non-view related
	// source.
	// The callback function should clear renderEntity->callback if it doesn't
	// want to be called again next time the entity is referenced (ie, if the
	// callback has now made the entity valid until the next updateEntity)
	idBounds				bounds;					// only needs to be set for deferred models and md5s
	deferredEntityCallback_t	callback;

	void *					callbackData;			// used for whatever the callback wants

	// player bodies and possibly player shadows should be suppressed in views from
	// that player's eyes, but will show up in mirrors and other subviews
	// security cameras could suppress their model in their subviews if we add a way
	// of specifying a view number for a remoteRenderMap view
	int						suppressSurfaceInViewID;
	int						suppressShadowInViewID;

	// world models for the player and weapons will not cast shadows from view weapon
	// muzzle flashes
	int						suppressShadowInLightID;

	// if non-zero, the surface and shadow (if it casts one)
	// will only show up in the specific view, ie: player weapons
	int						allowSurfaceInViewID;

	// positioning
	// axis rotation vectors must be unit length for many
	// R_LocalToGlobal functions to work, so don't scale models!
	// axis vectors are [0] = forward, [1] = left, [2] = up
	idVec3					origin;
	idMat3					axis;

	// texturing
	const idMaterial *		customShader;			// if non-0, all surfaces will use this
	const idMaterial *		referenceShader;		// used so flares can reference the proper light shader
	const idDeclSkin *		customSkin;				// 0 for no remappings
	class idSoundEmitter *	referenceSound;			// for shader sound tables, allowing effects to vary with sounds
	float					shaderParms[ MAX_ENTITY_SHADER_PARMS ];	// can be used in any way by shader or model generation

	// networking: see WriteGUIToSnapshot / ReadGUIFromSnapshot
	class idUserInterface * gui[ MAX_RENDERENTITY_GUI ];

	struct renderView_t	*	remoteRenderView;		// any remote camera surfaces will use this

	int						numJoints;
	idJointMat *			joints;					// array of joints that will modify vertices.
													// NULL if non-deformable model.  NOT freed by renderer

	float					modelDepthHack;			// squash depth range so particle effects don't clip into walls

	// options to override surface shader flags (replace with material parameters?)
	bool					noSelfShadow;			// cast shadows onto other objects,but not self
	bool					noShadow;				// no shadow at all

	bool					noDynamicInteractions;	// don't create any light / shadow interactions after
													// the level load is completed.  This is a performance hack
													// for the gigantic outdoor meshes in the monorail map, so
													// all the lights in the moving monorail don't touch the meshes

	bool					weaponDepthHack;		// squash depth range so view weapons don't poke into walls
													// this automatically implies noShadow
	bool					noOverlays;				// force no overlays on this model
	int						forceUpdate;			// force an update (NOTE: not a bool to keep this struct a multiple of 4 bytes)
	int						timeGroup;
	int						xrayIndex;
};

class idRenderEntity {
public:
							idRenderEntity();

	void					FreeRenderEntity();
	void					UpdateRenderEntity( const renderEntity_t *re, bool forceUpdate = false );
	void					GetRenderEntity( renderEntity_t *re );
	void					ForceUpdate();

	// overlays are extra polygons that deform with animating models for blood and damage marks
	void					ProjectOverlay( const idPlane localTextureAxis[2], const idMaterial *material );
	void					RemoveDecals();

	bool					IsDirectlyVisible() const;

	renderEntity_t			parms;

	float					modelMatrix[16];		// this is just a rearrangement of parms.axis and parms.origin
	idRenderMatrix			modelRenderMatrix;
	idRenderMatrix			inverseBaseModelProject;// transforms the unit cube to exactly cover the model in world space

	idRenderWorld *			world;
	int						index;					// in world entityDefs

	int						lastModifiedFrameNum;	// to determine if it is constantly changing,
													// and should go in the dynamic frame memory, or kept
													// in the cached memory
	bool					archived;				// for demo writing

	idRenderModel *			dynamicModel;			// if parms.model->IsDynamicModel(), this is the generated data
	int						dynamicModelFrameCount;	// continuously animating dynamic models will recreate
													// dynamicModel if this doesn't == tr.viewCount
	idRenderModel *			cachedDynamicModel;


	// the local bounds used to place entityRefs, either from parms for dynamic entities, or a model bounds
	idBounds				localReferenceBounds;	

	// axis aligned bounding box in world space, derived from refernceBounds and
	// modelMatrix in idRenderWorld::CreateEntityRefs()
	idBounds				globalReferenceBounds;

	// a viewEntity_t is created whenever a idRenderEntity is considered for inclusion
	// in a given view, even if it turns out to not be visible
	int						viewCount;				// if tr.viewCount == viewCount, viewEntity is valid,
													// but the entity may still be off screen
	viewEntity_t *			viewEntity;				// in frame temporary memory

	idRenderModelDecal *	decals;					// decals that have been projected on this model
	idRenderModelOverlay *	overlays;				// blood overlays on animated models

	areaReference_t *		entityRefs;				// chain of all references
	idInteraction *			firstInteraction;		// doubly linked list
	idInteraction *			lastInteraction;

	bool					needsPortalSky;
};

/*
===========================================================================

SURFACES

viewLight_t

Are allocated on the frame temporary stack memory
a viewLight contains everything that the back end needs out of an idRenderLight,
which the front end may be modifying simultaniously if running in SMP mode.
a viewLight may exist even without any surfaces, and may be relevent for fogging,
but should never exist if its volume does not intersect the view frustum

===========================================================================
*/

struct preLightShadowVolumeParms_t;

struct shadowOnlyEntity_t {
	shadowOnlyEntity_t *	next;
	idRenderEntity	*		edef;
};

struct viewLight_t {
	viewLight_t *			next;

	// back end should NOT reference the lightDef, because it can change when running SMP
	idRenderLight *			lightDef;

	// for scissor clipping, local inside renderView viewport
	// scissorRect.Empty() is true if the viewEntity_t was never actually
	// seen through any portals
	idScreenRect			scissorRect;

	// R_AddSingleLight() determined that the light isn't actually needed
	bool					removeFromList;

	// R_AddSingleLight builds this list of entities that need to be added
	// to the viewEntities list because they potentially cast shadows into
	// the view, even though the aren't directly visible
	shadowOnlyEntity_t *	shadowOnlyViewEntities;

	enum interactionState_t {
		INTERACTION_UNCHECKED,
		INTERACTION_NO,
		INTERACTION_YES
	};
	byte *					entityInteractionState;		// [numEntities]

	idVec3					globalLightOrigin;			// global light origin used by backend
	idPlane					lightProject[4];			// light project used by backend
	idPlane					fogPlane;					// fog plane for backend fog volume rendering
	idRenderMatrix			inverseBaseLightProject;	// the matrix for deforming the 'zeroOneCubeModel' to exactly cover the light volume in world space
	const idMaterial *		lightShader;				// light shader used by backend
	const float	*			shaderRegisters;			// shader registers used by backend
	idImage *				falloffImage;				// falloff image used by backend

	drawSurf_t *			globalShadows;				// shadow everything
	drawSurf_t *			localInteractions;			// don't get local shadows
	drawSurf_t *			localShadows;				// don't shadow local surfaces
	drawSurf_t *			globalInteractions;			// get shadows from everything
	drawSurf_t *			translucentInteractions;	// translucent interactions don't get shadows

	// R_AddSingleLight will build a chain of parameters here to setup shadow volumes
	preLightShadowVolumeParms_t *	preLightShadowVolumes;
};

/*
===========================================================================

SURFACES

viewEntity_t

Is created whenever a idRenderEntity is considered for inclusion
in the current view, but it may still turn out to be culled.
viewEntity are allocated on the frame temporary stack memory
a viewEntity contains everything that the back end needs out of a idRenderEntity,
which the front end may be modifying simultaneously if running in SMP mode.
A single entityDef can generate multiple viewEntity_t in a single frame, as when seen in a mirrors

===========================================================================
*/

struct staticShadowVolumeParms_t;
struct dynamicShadowVolumeParms_t;

struct viewEntity_t {
	viewEntity_t *			next;

	// back end should NOT reference the entityDef, because it can change when running SMP
	idRenderEntity	*		entityDef;

	// for scissor clipping, local inside renderView viewport
	// scissorRect.Empty() is true if the viewEntity_t was never actually
	// seen through any portals, but was created for shadow casting.
	// a viewEntity can have a non-empty scissorRect, meaning that an area
	// that it is in is visible, and still not be visible.
	idScreenRect			scissorRect;

	bool					isGuiSurface;			// force two sided and vertex colors regardless of material setting

	bool					weaponDepthHack;
	float					modelDepthHack;
		
	float					modelMatrix[16];		// local coords to global coords
	float					modelViewMatrix[16];	// local coords to eye coords

	idRenderMatrix			mvp;

	// parallelAddModels will build a chain of surfaces here that will need to
	// be linked to the lights or added to the drawsurf list in a serial code section
	drawSurf_t *			drawSurfs;

	// R_AddSingleModel will build a chain of parameters here to setup shadow volumes
	staticShadowVolumeParms_t *		staticShadowVolumes;
	dynamicShadowVolumeParms_t *	dynamicShadowVolumes;
};

/*
===========================================================================

SURFACES

viewDef_t

Are allocated on the frame temporary stack memory

===========================================================================
*/

struct renderView_t {
	// player views will set this to a non-zero integer for model suppress / allow
	// subviews (mirrors, cameras, etc) will always clear it to zero
	int						viewID;

	float					fov_x, fov_y;		// in degrees
	idVec3					vieworg;			// has already been adjusted for stereo world seperation
	idVec3					vieworg_weapon;		// has already been adjusted for stereo world seperation
	idMat3					viewaxis;			// transformation matrix, view looks down the positive X axis

	bool					cramZNear;			// for cinematics, we want to set ZNear much lower
	bool					flipProjection;
	bool					forceUpdate;		// for an update 

	// time in milliseconds for shader effects and other time dependent rendering issues
	int						time[2];
	float					shaderParms[MAX_GLOBAL_SHADER_PARMS];		// can be used in any way by shader
	const idMaterial		*globalMaterial;							// used to override everything draw
};

struct viewDef_t {
	// specified in the call to DrawScene()
	renderView_t		renderView;

	float				projectionMatrix[16];
	idRenderMatrix		projectionRenderMatrix;	// tech5 version of projectionMatrix
	viewEntity_t		worldSpace;

	idRenderWorld *renderWorld;

	idVec3				initialViewAreaOrigin;
	// Used to find the portalArea that view flooding will take place from.
	// for a normal view, the initialViewOrigin will be renderView.viewOrg,
	// but a mirror may put the projection origin outside
	// of any valid area, or in an unconnected area of the map, so the view
	// area must be based on a point just off the surface of the mirror / subview.
	// It may be possible to get a failed portal pass if the plane of the
	// mirror intersects a portal, and the initialViewAreaOrigin is on
	// a different side than the renderView.viewOrg is.

	bool				isSubview;				// true if this view is not the main view
	bool				isMirror;				// the portal is a mirror, invert the face culling
	bool				isXraySubview;

	bool				isEditor;
	bool				is2Dgui;

	int					numClipPlanes;			// mirrors will often use a single clip plane
	idPlane				clipPlanes[MAX_CLIP_PLANES];		// in world space, the positive side
												// of the plane is the visible side
	idScreenRect		viewport;				// in real pixels and proper Y flip

	idScreenRect		scissor;
	// for scissor clipping, local inside renderView viewport
	// subviews may only be rendering part of the main view
	// these are real physical pixel values, possibly scaled and offset from the
	// renderView x/y/width/height

	viewDef_t *			superView;				// never go into an infinite subview loop 
	const drawSurf_t *	subviewSurface;

	// drawSurfs are the visible surfaces of the viewEntities, sorted
	// by the material sort parameter
	drawSurf_t **		drawSurfs;				// we don't use an idList for this, because
	int					numDrawSurfs;			// it is allocated in frame temporary memory
	int					maxDrawSurfs;			// may be resized

	viewLight_t	*		viewLights;			// chain of all viewLights effecting view
	viewEntity_t *		viewEntitys;			// chain of all viewEntities effecting view, including off screen ones casting shadows
	// we use viewEntities as a check to see if a given view consists solely
	// of 2D rendering, which we can optimize in certain ways.  A 2D view will
	// not have any viewEntities

	idPlane				frustum[6];				// positive sides face outward, [4] is the front clip plane

	int					areaNum;				// -1 = not in a valid area

	// An array in frame temporary memory that lists if an area can be reached without
	// crossing a closed door.  This is used to avoid drawing interactions
	// when the light is behind a closed door.
	bool *				connectedAreas;
};

/*
===========================================================================

SURFACES

drawInteraction_t

complex light / surface interactions are broken up into multiple passes of a
simple interaction shader

===========================================================================
*/

struct drawInteraction_t {
	const drawSurf_t *	surf;

	idImage *			bumpImage;
	idImage *			diffuseImage;
	idImage *			specularImage;

	idVec4				diffuseColor;	// may have a light color baked into it
	idVec4				specularColor;	// may have a light color baked into it
	stageVertexColor_t	vertexColor;	// applies to both diffuse and specular

	int					ambientLight;	// use tr.ambientNormalMap instead of normalization cube map 

	// these are loaded into the vertex program
	idVec4				bumpMatrix[2];
	idVec4				diffuseMatrix[2];
	idVec4				specularMatrix[2];
};

/*
===========================================================================

SURFACES

deformInfo_t

deformable meshes precalculate as much as possible from a base frame, then generate
complete srfTriangles_t from just a new set of vertexes

===========================================================================
*/

struct silEdge_t;

struct deformInfo_t {
	int					numSourceVerts;

	// numOutputVerts may be smaller if the input had duplicated or degenerate triangles
	// it will often be larger if the input had mirrored texture seams that needed
	// to be busted for proper tangent spaces
	int					numOutputVerts;
	idDrawVert *		verts;

	int					numIndexes;
	triIndex_t *		indexes;

	triIndex_t *		silIndexes;				// indexes changed to be the first vertex with same XYZ, ignoring normal and texcoords

	int					numMirroredVerts;		// this many verts at the end of the vert list are tangent mirrors
	int *				mirroredVerts;			// tri->mirroredVerts[0] is the mirror of tri->numVerts - tri->numMirroredVerts + 0

	int					numDupVerts;			// number of duplicate vertexes
	int *				dupVerts;				// pairs of the number of the first vertex and the number of the duplicate vertex

	int					numSilEdges;			// number of silhouette edges
	silEdge_t *			silEdges;				// silhouette edges

	vertCacheHandle_t	staticIndexCache;		// GL_INDEX_TYPE
	vertCacheHandle_t	staticAmbientCache;		// idDrawVert
	vertCacheHandle_t	staticShadowCache;		// idShadowCacheSkinned
};

/*
===========================================================================

RENDERER BACK END COMMAND QUEUE

TR_CMDS

===========================================================================
*/

enum rcmd_t {
	RC_NOP,
	RC_DRAW_VIEW,
	RC_COPY_RENDER,
};

struct renderCommand_t {
	rcmd_t		commandId;
	rcmd_t *	next;
};

struct drawSurfsCommand_t {
	rcmd_t		commandId;
	rcmd_t *	next;
	viewDef_t *	viewDef;
};

struct copyRenderCommand_t {
	rcmd_t		commandId;
	rcmd_t *	next;
	int			x;
	int			y;
	int			imageWidth;
	int			imageHeight;
	idImage	*	image;
	int			cubeFace;					// when copying to a cubeMap
	bool		clearColorAfterCopy;
};

/*
===========================================================================

idFrameData

all of the information needed by the back end must be
contained in a idFrameData.  This entire structure is
duplicated so the front and back end can run in parallel
on an SMP machine.

===========================================================================
*/

class idFrameData {
public:
	idSysInterlockedInteger	frameMemoryAllocated;
	idSysInterlockedInteger	frameMemoryUsed;
	byte *					frameMemory;

	int						highWaterAllocated;	// max used on any frame
	int						highWaterUsed;

	// the currently building command list commands can be inserted
	// at the front if needed, as required for dynamically generated textures
	renderCommand_t *		cmdHead;	// may be of other command type based on commandId
	renderCommand_t *		cmdTail;
};

/*
===========================================================================

PORTALS

===========================================================================
*/

// exitPortal_t is returned by idRenderWorld::GetPortal()
typedef struct {
	int					areas[2];		// areas connected by this portal
	const idWinding	*	w;				// winding points have counter clockwise ordering seen from areas[0]
	int					blockingBits;	// PS_BLOCK_VIEW, PS_BLOCK_AIR, etc
	qhandle_t			portalHandle;
} exitPortal_t;


// guiPoint_t is returned by idRenderWorld::GuiTrace()
typedef struct {
	float				x, y;			// 0.0 to 1.0 range if trace hit a gui, otherwise -1
	int					guiId;			// id of gui ( 0, 1, or 2 ) that the trace happened against
} guiPoint_t;


// modelTrace_t is for tracing vs. visual geometry
typedef struct modelTrace_s {
	float					fraction;			// fraction of trace completed
	idVec3					point;				// end point of trace in global space
	idVec3					normal;				// hit triangle normal vector in global space
	const idMaterial *		material;			// material of hit surface
	const renderEntity_t *	entity;				// render entity that was hit
	int						jointNumber;		// md5 joint nearest to the hit triangle
} modelTrace_t;


typedef enum {
	PS_BLOCK_NONE = 0,

	PS_BLOCK_VIEW = 1,
	PS_BLOCK_LOCATION = 2,		// game map location strings often stop in hallways
	PS_BLOCK_AIR = 4,			// windows between pressurized and unpresurized areas

	PS_BLOCK_ALL = (1<<NUM_PORTAL_ATTRIBUTES)-1
} portalConnection_t;


typedef struct portal_s {
	int						intoArea;		// area this portal leads to
	idWinding *				w;				// winding points have counter clockwise ordering seen this area
	idPlane					plane;			// view must be on the positive side of the plane to cross
	struct portal_s *		next;			// next portal of the area
	struct doublePortal_s *	doublePortal;
} portal_t;


typedef struct doublePortal_s {
	struct portal_s	*		portals[2];
	int						blockingBits;	// PS_BLOCK_VIEW, PS_BLOCK_AIR, etc, set by doors that shut them off

	// A portal will be considered closed if it is past the
	// fog-out point in a fog volume.  We only support a single
	// fog volume over each portal.
	idRenderLight *			fogLight;
	struct doublePortal_s *	nextFoggedPortal;
} doublePortal_t;


typedef struct portalArea_s {
	int				areaNum;
	int				connectedAreaNum[NUM_PORTAL_ATTRIBUTES];	// if two areas have matching connectedAreaNum, they are
									// not separated by a portal with the apropriate PS_BLOCK_* blockingBits
	int				viewCount;		// set by R_FindViewLightsAndEntities
	portal_t *		portals;		// never changes after load
	areaReference_t	entityRefs;		// head/tail of doubly linked list, may change
	areaReference_t	lightRefs;		// head/tail of doubly linked list, may change
} portalArea_t;


typedef struct {
	idPlane			plane;
	int				children[2];		// negative numbers are (-1 - areaNumber), 0 = solid
	int				commonChildrenArea;	// if all children are either solid or a single area,
										// this is the area number, else CHILDREN_HAVE_MULTIPLE_AREAS
} areaNode_t;

/*
===========================================================================

TRACE

===========================================================================
*/

struct localTrace_t {
	float		fraction;
	// only valid if fraction < 1.0
	idVec3		point;
	idVec3		normal;
	int			indexes[3];
};

#endif