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

#ifndef __RENDERWORLD_H__
#define __RENDERWORLD_H__

class idRenderModelDecal;

/*
===============================================================================

	Render World

===============================================================================
*/

#define PROC_FILE_EXT				"proc"
#define	PROC_FILE_ID				"mapProcFile003"

// shader parms
const int SHADERPARM_RED			= 0;
const int SHADERPARM_GREEN			= 1;
const int SHADERPARM_BLUE			= 2;
const int SHADERPARM_ALPHA			= 3;
const int SHADERPARM_TIMESCALE		= 3;
const int SHADERPARM_TIMEOFFSET		= 4;
const int SHADERPARM_DIVERSITY		= 5;	// random between 0.0 and 1.0 for some effects (muzzle flashes, etc)
const int SHADERPARM_MODE			= 7;	// for selecting which shader passes to enable
const int SHADERPARM_TIME_OF_DEATH	= 7;	// for the monster skin-burn-away effect enable and time offset

// model parms
const int SHADERPARM_MD3_FRAME		= 8;
const int SHADERPARM_MD3_LASTFRAME	= 9;
const int SHADERPARM_MD3_BACKLERP	= 10;

const int SHADERPARM_BEAM_END_X		= 8;	// for _beam models
const int SHADERPARM_BEAM_END_Y		= 9;
const int SHADERPARM_BEAM_END_Z		= 10;
const int SHADERPARM_BEAM_WIDTH		= 11;

const int SHADERPARM_SPRITE_WIDTH	= 8;
const int SHADERPARM_SPRITE_HEIGHT	= 9;

const int SHADERPARM_PARTICLE_STOPTIME = 8;	// don't spawn any more particles after this time

// assume any lightDef or entityDef index above this is an internal error
const int LUDICROUS_INDEX			= 10000;

// the renderEntity_s::joints array needs to point at enough memory to store the number of joints rounded up to two for SIMD
ID_INLINE int SIMD_ROUND_JOINTS( int numJoints )							{ return ( ( numJoints + 1 ) & ~1 ); }
ID_INLINE void SIMD_INIT_LAST_JOINT( idJointMat * joints, int numJoints )	{ if ( numJoints & 1 ) { joints[numJoints] = joints[numJoints - 1]; } }


struct reusableDecal_t {
	qhandle_t				entityHandle;
	int						lastStartTime;
	idRenderModelDecal *	decals;
};


struct reusableOverlay_t {
	qhandle_t				entityHandle;
	int						lastStartTime;
	idRenderModelOverlay *	overlays;
};

struct portalStack_t;

class idRenderWorld {
public:
							idRenderWorld();
							~idRenderWorld();

	// The same render world can be reinitialized as often as desired
	// a NULL or empty mapName will create an empty, single area world
	bool					InitFromMap( const char *mapName );

	void					ReCreateReferences();
	void					FreeDerivedData();
	void					CheckForEntityDefsUsingModel( idRenderModel * model );

	//-------------- Entity and Light Defs -----------------

	// entityDefs and lightDefs are added to a given world to determine
	// what will be drawn for a rendered scene.  Most update work is defered
	// until it is determined that it is actually needed for a given view.
	qhandle_t				AddEntityDef( const renderEntity_t *re );
	void					UpdateEntityDef( qhandle_t entityHandle, const renderEntity_t *re );
	void					FreeEntityDef( qhandle_t entityHandle );
	const renderEntity_t *	GetRenderEntity( qhandle_t entityHandle ) const;

	qhandle_t				AddLightDef( const renderLight_t *rlight );
	void					UpdateLightDef( qhandle_t lightHandle, const renderLight_t *rlight );
	void					FreeLightDef( qhandle_t lightHandle );
	const renderLight_t *	GetRenderLight( qhandle_t lightHandle ) const;

	// Force the generation of all light / surface interactions at the start of a level
	// If this isn't called, they will all be dynamically generated
	void					GenerateAllInteractions();

	// returns true if this area model needs portal sky to draw
	bool					CheckAreaForPortalSky( int areaNum );

	//-------------- Decals and Overlays  -----------------

	// Creates decals on all world surfaces that the winding projects onto.
	// The projection origin should be infront of the winding plane.
	// The decals are projected onto world geometry between the winding plane and the projection origin.
	// The decals are depth faded from the winding plane to a certain distance infront of the
	// winding plane and the same distance from the projection origin towards the winding.
	void					ProjectDecalOntoWorld( const idFixedWinding &winding, const idVec3 &projectionOrigin, const bool parallel, const float fadeDepth, const idMaterial *material, const int startTime );

	// Creates decals on static models.
	void					ProjectDecal( qhandle_t entityHandle, const idFixedWinding &winding, const idVec3 &projectionOrigin, const bool parallel, const float fadeDepth, const idMaterial *material, const int startTime );

	// Creates overlays on dynamic models.
	void					ProjectOverlay( qhandle_t entityHandle, const idPlane localTextureAxis[2], const idMaterial *material, const int startTime );

	// Removes all decals and overlays from the given entity def.
	void					RemoveDecals( qhandle_t entityHandle );

	//-------------- Portal Area Information -----------------

	// returns the number of portal areas in a map, so game code can build information
	// tables for the different areas
	int						NumAreas() const;

	// Will return -1 if the point is not in an area, otherwise
	// it will return 0 <= value < NumAreas()
	int						PointInArea( const idVec3 &point ) const;

	// fills the *areas array with the numbers of the areas the bounds cover
	// returns the total number of areas the bounds cover
	int						BoundsInAreas( const idBounds &bounds, int *areas, int maxAreas ) const;

	// Used by the sound system to do area flowing
	int						NumPortalsInArea( int areaNum );

	// returns one portal from an area
	exitPortal_t			GetPortal( int areaNum, int portalNum );

	//-------------- Tracing  -----------------

	// Checks a ray trace against any gui surfaces in an entity, returning the
	// fraction location of the trace on the gui surface, or -1,-1 if no hit.
	// This doesn't do any occlusion testing, simply ignoring non-gui surfaces.
	// start / end are in global world coordinates.
	guiPoint_t				GuiTrace( qhandle_t entityHandle, const idVec3 start, const idVec3 end ) const;

	// Traces vs the render model, possibly instantiating a dynamic version, and returns true if something was hit
	bool					ModelTrace( modelTrace_t &trace, qhandle_t entityHandle, const idVec3 &start, const idVec3 &end, const float radius ) const;

	// Traces vs the whole rendered world. FIXME: we need some kind of material flags.
	bool					Trace( modelTrace_t &trace, const idVec3 &start, const idVec3 &end, const float radius, bool skipDynamic = true, bool skipPlayer = false ) const;

	// Traces vs the world model bsp tree.
	bool					FastWorldTrace( modelTrace_t &trace, const idVec3 &start, const idVec3 &end ) const;

	// this is used to regenerate all interactions ( which is currently only done during influences ), there may be a less 
	// expensive way to do it
	void					RegenerateWorld();

	//-------------- Debug Visualization  -----------------

	// Line drawing for debug visualization
	void					DebugClearLines( int time );		// a time of 0 will clear all lines and text
	void					DebugLine( const idVec4 &color, const idVec3 &start, const idVec3 &end, const int lifetime = 0, const bool depthTest = false );
	void					DebugArrow( const idVec4 &color, const idVec3 &start, const idVec3 &end, int size, const int lifetime = 0 );
	void					DebugWinding( const idVec4 &color, const idWinding &w, const idVec3 &origin, const idMat3 &axis, const int lifetime = 0, const bool depthTest = false );
	void					DebugCircle( const idVec4 &color, const idVec3 &origin, const idVec3 &dir, const float radius, const int numSteps, const int lifetime = 0, const bool depthTest = false );
	void					DebugSphere( const idVec4 &color, const idSphere &sphere, const int lifetime = 0, bool depthTest = false );
	void					DebugBounds( const idVec4 &color, const idBounds &bounds, const idVec3 &org = vec3_origin, const int lifetime = 0 );
	void					DebugBox( const idVec4 &color, const idBox &box, const int lifetime = 0 );
	void					DebugCone( const idVec4 &color, const idVec3 &apex, const idVec3 &dir, float radius1, float radius2, const int lifetime = 0 );
	void					DebugScreenRect( const idVec4 &color, const idScreenRect &rect, const viewDef_t *viewDef, const int lifetime = 0 );
	void					DebugAxis( const idVec3 &origin, const idMat3 &axis );

	// Polygon drawing for debug visualization.
	void					DebugClearPolygons( int time );		// a time of 0 will clear all polygons
	void					DebugPolygon( const idVec4 &color, const idWinding &winding, const int lifeTime = 0, const bool depthTest = false );

	// Text drawing for debug visualization.
	void					DrawText( const char *text, const idVec3 &origin, float scale, const idVec4 &color, const idMat3 &viewAxis, const int align = 1, const int lifetime = 0, bool depthTest = false );

	void					PrintRenderEntityDefs();
	void					PrintRenderLightDefs();

	//-----------------------
	// RenderWorld_load.cpp

	idRenderModel *			ParseModel( idLexer *src, const char *mapName, ID_TIME_T mapTimeStamp, idFile *fileOut );
	idRenderModel *			ParseShadowModel( idLexer *src, idFile *fileOut );
	void					SetupAreaRefs();
	void					ParseInterAreaPortals( idLexer *src, idFile *fileOut );
	void					ParseNodes( idLexer *src, idFile *fileOut );
	int						CommonChildrenArea_r( areaNode_t *node );
	void					FreeWorld();
	void					ClearWorld();
	void					FreeDefs();
	void					TouchWorldModels();
	void					AddWorldModelEntities();
	void					ClearPortalStates();
	void					ReadBinaryAreaPortals( idFile *file );
	void					ReadBinaryNodes( idFile *file );
	idRenderModel *			ReadBinaryModel( idFile *file );
	idRenderModel *			ReadBinaryShadowModel( idFile *file );

	//--------------------------
	// RenderWorld_portals.cpp

	bool					CullEntityByPortals( const idRenderEntity *entity, const portalStack_t *ps );
	void					AddAreaViewEntities( int areaNum, const portalStack_t *ps );
	bool					CullLightByPortals( const idRenderLight *light, const portalStack_t *ps );
	void					AddAreaViewLights( int areaNum, const portalStack_t *ps );
	void					AddAreaToView( int areaNum, const portalStack_t *ps );
	idScreenRect			ScreenRectFromWinding( const idWinding *w, const float modelMatrix[ 16 ] );
	bool					PortalIsFoggedOut( const portal_t *p );
	void					FloodViewThroughArea_r( const idVec3 & origin, int areaNum, const portalStack_t *ps );
	void					FlowViewThroughPortals( const idVec3 & origin, int numPlanes, const idPlane *planes );
	void					BuildConnectedAreas_r( int areaNum );
	void					BuildConnectedAreas();
	void					FindViewLightsAndEntities();

	void					FloodLightThroughArea_r( idRenderLight *light, int areaNum, const portalStack_t *ps );
	void					FlowLightThroughPortals( idRenderLight *light );

	int						NumPortals() const;

	// returns 0 if no portal contacts the bounds
	// This is used by the game to identify portals that are contained
	// inside doors, so the connection between areas can be topologically
	// terminated when the door shuts.
	qhandle_t				FindPortal( const idBounds &b ) const;

	// doors explicitly close off portals when shut
	// multiple bits can be set to block multiple things, ie: ( PS_VIEW | PS_LOCATION | PS_AIR )
	void					SetPortalState( qhandle_t portal, int blockingBits );
	int						GetPortalState( qhandle_t portal );

	// returns true only if a chain of portals without the given connection bits set
	// exists between the two areas (a door doesn't separate them, etc)
	bool					AreasAreConnected( int areaNum1, int areaNum2, portalConnection_t connection ) const;

	void					FloodConnectedAreas( portalArea_t *area, int portalAttributeIndex );
	idScreenRect &			GetAreaScreenRect( int areaNum ) const { return m_areaScreenRect[areaNum]; }

	//--------------------------
	// RenderWorld.cpp

	void					ResizeInteractionTable();

	void					AddEntityRefToArea( idRenderEntity *def, portalArea_t *area );
	void					AddLightRefToArea( idRenderLight *light, portalArea_t *area );

	void					RecurseProcBSP_r( modelTrace_t *results, int parentNodeNum, int nodeNum, float p1f, float p2f, const idVec3 &p1, const idVec3 &p2 ) const;
	void					BoundsInAreas_r( int nodeNum, const idBounds &bounds, int *areas, int *numAreas, int maxAreas ) const;

	void					FreeInteractions();

	void					PushFrustumIntoTree_r( idRenderEntity *def, idRenderLight *light, const frustumCorners_t & corners, int nodeNum );
	void					PushFrustumIntoTree( idRenderEntity *def, idRenderLight *light, const idRenderMatrix & frustumTransform, const idBounds & frustumBounds );

	idRenderModelDecal *	AllocDecal( qhandle_t newEntityHandle, int startTime );
	idRenderModelOverlay *	AllocOverlay( qhandle_t newEntityHandle, int startTime );

private:
	void					CreateEntityRefs( idRenderEntity * entity );
	void					DeriveEntityData( idRenderEntity * entity );
	void					FreeEntityDefDerivedData( idRenderEntity *def, bool keepDecals, bool keepCachedDynamicModel );
	void					FreeEntityDefDecals( idRenderEntity *def );
	void					FreeEntityDefOverlay( idRenderEntity *def );
	void					FreeEntityDefFadedDecals( idRenderEntity *def, int time );

	void					CreateLightRefs( idRenderLight * light );
	void					FreeLightDefDerivedData( idRenderLight *ldef );

public:
	//-----------------------

	idStr					m_mapName;				// ie: maps/tim_dm2.proc, written to demoFile
	ID_TIME_T				m_mapTimeStamp;			// for fast reloads of the same level

	areaNode_t *			m_areaNodes;
	int						m_numAreaNodes;

	portalArea_t *			m_portalAreas;
	int						m_numPortalAreas;
	int						m_connectedAreaNum;		// incremented every time a door portal state changes

	idScreenRect *			m_areaScreenRect;

	doublePortal_t *		m_doublePortals;
	int						m_numInterAreaPortals;

	idList< idRenderModel *, TAG_MODEL >		m_localModels;

	idList< idRenderEntity *, TAG_ENTITY >		m_entityDefs;
	idList< idRenderLight *, TAG_LIGHT >		m_lightDefs;

	idBlockAlloc< areaReference_t, 1024 >		m_areaReferenceAllocator;
	idBlockAlloc< idInteraction, 256 >			m_interactionAllocator;

	static const int MAX_DECAL_SURFACES = 32;

	idArray< reusableDecal_t, MAX_DECAL_SURFACES >		m_decals;
	idArray< reusableOverlay_t, MAX_DECAL_SURFACES >	m_overlays;

	// all light / entity interactions are referenced here for fast lookup without
	// having to crawl the doubly linked lists.  EnntityDefs are sequential for better cache access.
	// Growing this table is time consuming, so we add a pad value to the number
	// of entityDefs and lightDefs
	idInteraction **		m_interactionTable;
	int						m_interactionTableWidth;		// entityDefs
	int						m_interactionTableHeight;		// lightDefs
};

// if an entity / light combination has been evaluated and found to not genrate any surfaces or shadows,
// the constant INTERACTION_EMPTY will be stored in the interaction table, int contrasts to NULL, which
// means that the combination has not yet been tested for having surfaces.
static idInteraction * const INTERACTION_EMPTY = (idInteraction *)1;

#endif /* !__RENDERWORLD_H__ */
