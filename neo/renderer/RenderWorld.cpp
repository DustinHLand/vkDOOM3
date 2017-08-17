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
#include "../framework/precompiled.h"
#include "RenderSystem_local.h"
#include "GLMatrix.h"
#include "ModelDecal.h"
#include "ModelOverlay.h"
#include "Interaction.h"

bool R_IssueEntityDefCallback( idRenderEntity *def );
idRenderModel *R_EntityDefDynamicModel( idRenderEntity *def );

void RB_AddDebugText( const char *text, const idVec3 &origin, float scale, const idVec4 &color, const idMat3 &viewAxis, const int align, const int lifetime, const bool depthTest );
void RB_ClearDebugText( int time );
void RB_AddDebugLine( const idVec4 &color, const idVec3 &start, const idVec3 &end, const int lifeTime, const bool depthTest );
void RB_ClearDebugLines( int time );
void RB_AddDebugPolygon( const idVec4 &color, const idWinding &winding, const int lifeTime, const bool depthTest );
void RB_ClearDebugPolygons( int time );
localTrace_t R_LocalTrace( const idVec3 &start, const idVec3 &end, const float radius, const srfTriangles_t *tri );

extern idCVar r_useEntityCallbacks;
extern idCVar r_skipUpdates;
extern idCVar r_useNodeCommonChildren;
extern idCVar r_debugArrowStep;
extern idCVar r_znear;

/*
===============
R_RemapShaderBySkin
===============
*/
const idMaterial *R_RemapShaderBySkin( const idMaterial *shader, const idDeclSkin *skin, const idMaterial *customShader ) {

	if ( !shader ) {
		return NULL;
	}

	// never remap surfaces that were originally nodraw, like collision hulls
	if ( !shader->IsDrawn() ) {
		return shader;
	}

	if ( customShader ) {
		// this is sort of a hack, but cause deformed surfaces to map to empty surfaces,
		// so the item highlight overlay doesn't highlight the autosprite surface
		if ( shader->Deform() ) {
			return NULL;
		}
		return const_cast<idMaterial *>(customShader);
	}

	if ( !skin ) {
		return const_cast<idMaterial *>(shader);
	}

	return skin->RemapShaderBySkin( shader );
}

/*
===================
idRenderWorld::PrintRenderLightDefs
===================
*/
void idRenderWorld::PrintRenderLightDefs() {
	idRenderLight * ldef;

	int active = 0;
	int	totalRef = 0;
	int	totalIntr = 0;

	for ( int i = 0; i < m_lightDefs.Num(); i++ ) {
		ldef = m_lightDefs[i];
		if ( !ldef ) {
			idLib::Printf( "%4i: FREED\n", i );
			continue;
		}

		// count up the interactions
		int	iCount = 0;
		for ( idInteraction *inter = ldef->firstInteraction; inter != NULL; inter = inter->lightNext ) {
			iCount++;
		}
		totalIntr += iCount;

		// count up the references
		int	rCount = 0;
		for ( areaReference_t *ref = ldef->references; ref; ref = ref->ownerNext ) {
			rCount++;
		}
		totalRef += rCount;

		idLib::Printf( "%4i: %3i intr %2i refs %s\n", i, iCount, rCount, ldef->lightShader->GetName());
		active++;
	}

	idLib::Printf( "%i lightDefs, %i interactions, %i areaRefs\n", active, totalIntr, totalRef );
}

/*
===================
idRenderWorld::PrintRenderEntityDefs
===================
*/
void idRenderWorld::PrintRenderEntityDefs() {
	idRenderEntity	*mdef;

	int active = 0;
	int	totalRef = 0;
	int	totalIntr = 0;

	for ( int i = 0; i < m_entityDefs.Num(); i++ ) {
		mdef = m_entityDefs[i];
		if ( !mdef ) {
			idLib::Printf( "%4i: FREED\n", i );
			continue;
		}

		// count up the interactions
		int	iCount = 0;
		for ( idInteraction *inter = mdef->firstInteraction; inter != NULL; inter = inter->entityNext ) {
			iCount++;
		}
		totalIntr += iCount;

		// count up the references
		int	rCount = 0;
		for ( areaReference_t *ref = mdef->entityRefs; ref; ref = ref->ownerNext ) {
			rCount++;
		}
		totalRef += rCount;

		idLib::Printf( "%4i: %3i intr %2i refs %s\n", i, iCount, rCount, mdef->parms.hModel->Name());
		active++;
	}

	idLib::Printf( "total active: %i\n", active );
}

/*
===================
idRenderWorld::idRenderWorld
===================
*/
idRenderWorld::idRenderWorld() {
	m_mapName.Clear();
	m_mapTimeStamp = FILE_NOT_FOUND_TIMESTAMP;

	m_areaNodes = NULL;
	m_numAreaNodes = 0;

	m_portalAreas = NULL;
	m_numPortalAreas = 0;

	m_doublePortals = NULL;
	m_numInterAreaPortals = 0;

	m_interactionTable = 0;
	m_interactionTableWidth = 0;
	m_interactionTableHeight = 0;

	for ( int i = 0; i < m_decals.Num(); i++ ) {
		m_decals[i].entityHandle = -1;
		m_decals[i].lastStartTime = 0;
		m_decals[i].decals = new (TAG_MODEL) idRenderModelDecal();
	}

	for ( int i = 0; i < m_overlays.Num(); i++ ) {
		m_overlays[i].entityHandle = -1;
		m_overlays[i].lastStartTime = 0;
		m_overlays[i].overlays = new (TAG_MODEL) idRenderModelOverlay();
	}
}

/*
===================
idRenderWorld::~idRenderWorld
===================
*/
void RB_ClearDebugText( int time );
idRenderWorld::~idRenderWorld() {
	// free all the entityDefs, lightDefs, portals, etc
	FreeWorld();

	for ( int i = 0; i < m_decals.Num(); i++ ) {
		delete m_decals[i].decals;
	}

	for ( int i = 0; i < m_overlays.Num(); i++ ) {
		delete m_overlays[i].overlays;
	}

	// free up the debug lines, polys, and text
	RB_ClearDebugPolygons( 0 );
	RB_ClearDebugLines( 0 );
	RB_ClearDebugText( 0 );
}

/*
===================
ResizeInteractionTable
===================
*/
void idRenderWorld::ResizeInteractionTable() {
	// we overflowed the interaction table, so make it larger
	idLib::Printf( "idRenderWorld::ResizeInteractionTable: overflowed interactionTable, resizing\n" );

	const int oldInteractionTableWidth = m_interactionTableWidth;
	const int oldIinteractionTableHeight = m_interactionTableHeight;
	idInteraction ** oldInteractionTable = m_interactionTable;

	// build the interaction table
	// this will be dynamically resized if the entity / light counts grow too much
	m_interactionTableWidth = m_entityDefs.Num() + 100;
	m_interactionTableHeight = m_lightDefs.Num() + 100;
	const int	size =  m_interactionTableWidth * m_interactionTableHeight * sizeof( *m_interactionTable );
	m_interactionTable = (idInteraction **)R_ClearedStaticAlloc( size );
	for ( int l = 0; l < oldIinteractionTableHeight; l++ ) {
		for ( int e = 0; e < oldInteractionTableWidth; e++ ) {
			m_interactionTable[ l * m_interactionTableWidth + e ] = oldInteractionTable[ l * oldInteractionTableWidth + e ];
		}
	}

	R_StaticFree( oldInteractionTable );
}

/*
===================
AddEntityDef
===================
*/
qhandle_t idRenderWorld::AddEntityDef( const renderEntity_t *re ){
	// try and reuse a free spot
	int entityHandle = m_entityDefs.FindNull();
	if ( entityHandle == -1 ) {
		entityHandle = m_entityDefs.Append( NULL );

		if ( m_interactionTable && m_entityDefs.Num() > m_interactionTableWidth ) {
			ResizeInteractionTable();
		}
	}

	UpdateEntityDef( entityHandle, re );
	
	return entityHandle;
}

/*
==============
UpdateEntityDef

Does not write to the demo file, which will only be updated for
visible entities
==============
*/
void R_ClearEntityDefDynamicModel( idRenderEntity *def );
void idRenderWorld::UpdateEntityDef( qhandle_t entityHandle, const renderEntity_t *re ) {
	if ( r_skipUpdates.GetBool() ) {
		return;
	}

	tr.pc.c_entityUpdates++;

	if ( !re->hModel && !re->callback ) {
		idLib::Error( "idRenderWorld::UpdateEntityDef: NULL hModel" );
	}

	// create new slots if needed
	if ( entityHandle < 0 || entityHandle > LUDICROUS_INDEX ) {
		idLib::Error( "idRenderWorld::UpdateEntityDef: index = %i", entityHandle );
	}
	while ( entityHandle >= m_entityDefs.Num() ) {
		m_entityDefs.Append( NULL );
	}

	idRenderEntity	*def = m_entityDefs[entityHandle];
	if ( def != NULL ) {

		if ( !re->forceUpdate ) {

			// check for exact match (OPTIMIZE: check through pointers more)
			if ( !re->joints && !re->callbackData && !def->dynamicModel && !memcmp( re, &def->parms, sizeof( *re ) ) ) {
				return;
			}

			// if the only thing that changed was shaderparms, we can just leave things as they are
			// after updating parms

			// if we have a callback function and the bounds, origin, axis and model match,
			// then we can leave the references as they are
			if ( re->callback ) {

				bool axisMatch = ( re->axis == def->parms.axis );
				bool originMatch = ( re->origin == def->parms.origin );
				bool boundsMatch = ( re->bounds == def->localReferenceBounds );
				bool modelMatch = ( re->hModel == def->parms.hModel );

				if ( boundsMatch && originMatch && axisMatch && modelMatch ) {
					// only clear the dynamic model and interaction surfaces if they exist
					R_ClearEntityDefDynamicModel( def );
					def->parms = *re;
					return;
				}
			}
		}

		// save any decals if the model is the same, allowing marks to move with entities
		if ( def->parms.hModel == re->hModel ) {
			FreeEntityDefDerivedData( def, true, true );
		} else {
			FreeEntityDefDerivedData( def, false, false );
		}
	} else {
		// creating a new one
		def = new (TAG_RENDER_ENTITY) idRenderEntity;
		m_entityDefs[entityHandle] = def;

		def->world = this;
		def->index = entityHandle;
	}

	def->parms = *re;

	def->lastModifiedFrameNum = tr.frameCount;

	// optionally immediately issue any callbacks
	if ( !r_useEntityCallbacks.GetBool() && def->parms.callback != NULL ) {
		R_IssueEntityDefCallback( def );
	}

	// trigger entities don't need to get linked in and processed,
	// they only exist for editor use
	if ( def->parms.hModel != NULL && !def->parms.hModel->ModelHasDrawingSurfaces() ) {
		return;
	}

	// based on the model bounds, add references in each area
	// that may contain the updated surface
	CreateEntityRefs( def );
}

/*
===================
FreeEntityDef

Frees all references and lit surfaces from the model, and
NULL's out it's entry in the world list
===================
*/
void idRenderWorld::FreeEntityDef( qhandle_t entityHandle ) {
	idRenderEntity	*def;

	if ( entityHandle < 0 || entityHandle >= m_entityDefs.Num() ) {
		idLib::Printf( "idRenderWorld::FreeEntityDef: handle %i > %i\n", entityHandle, m_entityDefs.Num() );
		return;
	}

	def = m_entityDefs[entityHandle];
	if ( !def ) {
		idLib::Printf( "idRenderWorld::FreeEntityDef: handle %i is NULL\n", entityHandle );
		return;
	}

	FreeEntityDefDerivedData( def, false, false );

	// if we are playing a demo, these will have been freed
	// in FreeEntityDefDerivedData(), otherwise the gui
	// object still exists in the game

	def->parms.gui[ 0 ] = NULL;
	def->parms.gui[ 1 ] = NULL;
	def->parms.gui[ 2 ] = NULL;

	delete def;
	m_entityDefs[ entityHandle ] = NULL;
}

/*
==================
GetRenderEntity
==================
*/
const renderEntity_t *idRenderWorld::GetRenderEntity( qhandle_t entityHandle ) const {
	idRenderEntity	*def;

	if ( entityHandle < 0 || entityHandle >= m_entityDefs.Num() ) {
		idLib::Printf( "idRenderWorld::GetRenderEntity: invalid handle %i [0, %i]\n", entityHandle, m_entityDefs.Num() );
		return NULL;
	}

	def = m_entityDefs[entityHandle];
	if ( !def ) {
		idLib::Printf( "idRenderWorld::GetRenderEntity: handle %i is NULL\n", entityHandle );
		return NULL;
	}

	return &def->parms;
}

/*
==================
AddLightDef
==================
*/
qhandle_t idRenderWorld::AddLightDef( const renderLight_t *rlight ) {
	// try and reuse a free spot
	int lightHandle = m_lightDefs.FindNull();

	if ( lightHandle == -1 ) {
		lightHandle = m_lightDefs.Append( NULL );
		if ( m_interactionTable && m_lightDefs.Num() > m_interactionTableHeight ) {
			ResizeInteractionTable();
		}
	}
	UpdateLightDef( lightHandle, rlight );

	return lightHandle;
}

/*
=================
UpdateLightDef

The generation of all the derived interaction data will
usually be deferred until it is visible in a scene

Does not write to the demo file, which will only be done for visible lights
=================
*/
void idRenderWorld::UpdateLightDef( qhandle_t lightHandle, const renderLight_t *rlight ) {
	if ( r_skipUpdates.GetBool() ) {
		return;
	}

	tr.pc.c_lightUpdates++;

	// create new slots if needed
	if ( lightHandle < 0 || lightHandle > LUDICROUS_INDEX ) {
		idLib::Error( "idRenderWorld::UpdateLightDef: index = %i", lightHandle );
	}
	while ( lightHandle >= m_lightDefs.Num() ) {
		m_lightDefs.Append( NULL );
	}

	bool justUpdate = false;
	idRenderLight *light = m_lightDefs[lightHandle];
	if ( light ) {
		// if the shape of the light stays the same, we don't need to dump
		// any of our derived data, because shader parms are calculated every frame
		if ( rlight->axis == light->parms.axis && rlight->end == light->parms.end &&
			 rlight->lightCenter == light->parms.lightCenter && rlight->lightRadius == light->parms.lightRadius &&
			 rlight->noShadows == light->parms.noShadows && rlight->origin == light->parms.origin &&
			 rlight->parallel == light->parms.parallel && rlight->pointLight == light->parms.pointLight &&
			 rlight->right == light->parms.right && rlight->start == light->parms.start &&
			 rlight->target == light->parms.target && rlight->up == light->parms.up && 
			 rlight->shader == light->lightShader && rlight->prelightModel == light->parms.prelightModel ) {
			justUpdate = true;
		} else {
			// if we are updating shadows, the prelight model is no longer valid
			light->lightHasMoved = true;
			FreeLightDefDerivedData( light );
		}
	} else {
		// create a new one
		light = new (TAG_RENDER_LIGHT) idRenderLight;
		m_lightDefs[lightHandle] = light;

		light->world = this;
		light->index = lightHandle;
	}

	light->parms = *rlight;
	light->lastModifiedFrameNum = tr.frameCount;

	// new for BFG edition: force noShadows on spectrum lights so teleport spawns
	// don't cause such a slowdown.  Hell writing shouldn't be shadowed anyway...
	if ( light->parms.shader && light->parms.shader->Spectrum() ) {
		light->parms.noShadows = true;
	}

	if ( light->lightHasMoved ) {
		light->parms.prelightModel = NULL;
	}

	if ( !justUpdate ) {
		CreateLightRefs( light );
	}
}

/*
====================
FreeLightDef

Frees all references and lit surfaces from the light, and
NULL's out it's entry in the world list
====================
*/
void idRenderWorld::FreeLightDef( qhandle_t lightHandle ) {
	idRenderLight	*light;

	if ( lightHandle < 0 || lightHandle >= m_lightDefs.Num() ) {
		idLib::Printf( "idRenderWorld::FreeLightDef: invalid handle %i [0, %i]\n", lightHandle, m_lightDefs.Num() );
		return;
	}

	light = m_lightDefs[lightHandle];
	if ( !light ) {
		idLib::Printf( "idRenderWorld::FreeLightDef: handle %i is NULL\n", lightHandle );
		return;
	}

	FreeLightDefDerivedData( light );

	delete light;
	m_lightDefs[lightHandle] = NULL;
}

/*
==================
GetRenderLight
==================
*/
const renderLight_t *idRenderWorld::GetRenderLight( qhandle_t lightHandle ) const {
	idRenderLight *def;

	if ( lightHandle < 0 || lightHandle >= m_lightDefs.Num() ) {
		idLib::Printf( "idRenderWorld::GetRenderLight: handle %i > %i\n", lightHandle, m_lightDefs.Num() );
		return NULL;
	}

	def = m_lightDefs[lightHandle];
	if ( !def ) {
		idLib::Printf( "idRenderWorld::GetRenderLight: handle %i is NULL\n", lightHandle );
		return NULL;
	}

	return &def->parms;
}

/*
================
idRenderWorld::ProjectDecalOntoWorld
================
*/
void idRenderWorld::ProjectDecalOntoWorld( const idFixedWinding &winding, const idVec3 &projectionOrigin, const bool parallel, const float fadeDepth, const idMaterial *material, const int startTime ) {
	decalProjectionParms_t globalParms;

	if ( !idRenderModelDecal::CreateProjectionParms( globalParms, winding, projectionOrigin, parallel, fadeDepth, material, startTime ) ) {
		return;
	}

	// get the world areas touched by the projection volume
	int areas[10];
	int numAreas = BoundsInAreas( globalParms.projectionBounds, areas, 10 );

	// check all areas for models
	for ( int i = 0; i < numAreas; i++ ) {

		const portalArea_t * area = &m_portalAreas[ areas[i] ];

		// check all models in this area
		for ( const areaReference_t * ref = area->entityRefs.areaNext; ref != &area->entityRefs; ref = ref->areaNext ) {
			idRenderEntity * def = ref->entity;

			if ( def->parms.noOverlays ) {
				continue;
			}

			if ( def->parms.customShader != NULL && !def->parms.customShader->AllowOverlays() ) {
				continue;
			}

			// completely ignore any dynamic or callback models
			const idRenderModel * model = def->parms.hModel;
			if ( def->parms.callback != NULL || model == NULL || model->IsDynamicModel() != DM_STATIC ) {
				continue;
			}

			idBounds bounds;
			bounds.FromTransformedBounds( model->Bounds( &def->parms ), def->parms.origin, def->parms.axis );

			// if the model bounds do not overlap with the projection bounds
			decalProjectionParms_t localParms;
			if ( !globalParms.projectionBounds.IntersectsBounds( bounds ) ) {
				continue;
			}

			// transform the bounding planes, fade planes and texture axis into local space
			idRenderModelDecal::GlobalProjectionParmsToLocal( localParms, globalParms, def->parms.origin, def->parms.axis );
			localParms.force = ( def->parms.customShader != NULL );

			if ( def->decals == NULL ) {
				def->decals = AllocDecal( def->index, startTime );
			}
			def->decals->AddDeferredDecal( localParms );
		}
	}
}

/*
====================
idRenderWorld::ProjectDecal
====================
*/
void idRenderWorld::ProjectDecal( qhandle_t entityHandle, const idFixedWinding &winding, const idVec3 &projectionOrigin, const bool parallel, const float fadeDepth, const idMaterial *material, const int startTime ) {
	if ( entityHandle < 0 || entityHandle >= m_entityDefs.Num() ) {
		idLib::Error( "idRenderWorld::ProjectOverlay: index = %i", entityHandle );
		return;
	}

	idRenderEntity	*def = m_entityDefs[ entityHandle ];
	if ( def == NULL ) {
		return;
	}

	const idRenderModel *model = def->parms.hModel;

	if ( model == NULL || model->IsDynamicModel() != DM_STATIC || def->parms.callback != NULL ) {
		return;
	}

	decalProjectionParms_t globalParms;
	if ( !idRenderModelDecal::CreateProjectionParms( globalParms, winding, projectionOrigin, parallel, fadeDepth, material, startTime ) ) {
		return;
	}

	idBounds bounds;
	bounds.FromTransformedBounds( model->Bounds( &def->parms ), def->parms.origin, def->parms.axis );

	// if the model bounds do not overlap with the projection bounds
	if ( !globalParms.projectionBounds.IntersectsBounds( bounds ) ) {
		return;
	}

	// transform the bounding planes, fade planes and texture axis into local space
	decalProjectionParms_t localParms;
	idRenderModelDecal::GlobalProjectionParmsToLocal( localParms, globalParms, def->parms.origin, def->parms.axis );
	localParms.force = ( def->parms.customShader != NULL );

	if ( def->decals == NULL ) {
		def->decals = AllocDecal( def->index, startTime );
	}
	def->decals->AddDeferredDecal( localParms );
}

/*
====================
idRenderWorld::ProjectOverlay
====================
*/
void idRenderWorld::ProjectOverlay( qhandle_t entityHandle, const idPlane localTextureAxis[2], const idMaterial *material, const int startTime ) {
	if ( entityHandle < 0 || entityHandle >= m_entityDefs.Num() ) {
		idLib::Error( "idRenderWorld::ProjectOverlay: index = %i", entityHandle );
		return;
	}

	idRenderEntity	*def = m_entityDefs[ entityHandle ];
	if ( def == NULL ) {
		return;
	}

	const idRenderModel * model = def->parms.hModel;
	if ( model->IsDynamicModel() != DM_CACHED ) {	// FIXME: probably should be MD5 only
		return;
	}

	overlayProjectionParms_t localParms;
	localParms.localTextureAxis[0] = localTextureAxis[0];
	localParms.localTextureAxis[1] = localTextureAxis[1];
	localParms.material = material;
	localParms.startTime = startTime;

	if ( def->overlays == NULL ) {
		def->overlays = AllocOverlay( def->index, startTime );
	}
	def->overlays->AddDeferredOverlay( localParms );
}

/*
====================
idRenderWorld::AllocDecal
====================
*/
idRenderModelDecal * idRenderWorld::AllocDecal( qhandle_t newEntityHandle, int startTime ) {
	int oldest = 0;
	int oldestTime = MAX_TYPE( oldestTime );
	for ( int i = 0; i < m_decals.Num(); i++ ) {
		if ( m_decals[i].lastStartTime < oldestTime ) {
			oldestTime = m_decals[i].lastStartTime;
			oldest = i;
		}
	}

	// remove any reference another model may still have to this decal
	if ( m_decals[oldest].entityHandle >= 0 && m_decals[ oldest ].entityHandle < m_entityDefs.Num() ) {
		idRenderEntity	*def = m_entityDefs[ m_decals[ oldest ].entityHandle ];
		if ( def != NULL && def->decals == m_decals[ oldest ].decals ) {
			def->decals = NULL;
		}
	}

	m_decals[ oldest ].entityHandle = newEntityHandle;
	m_decals[ oldest ].lastStartTime = startTime;
	m_decals[ oldest ].decals->ReUse();

	return m_decals[ oldest ].decals;
}

/*
====================
idRenderWorld::AllocOverlay
====================
*/
idRenderModelOverlay * idRenderWorld::AllocOverlay( qhandle_t newEntityHandle, int startTime ) {
	int oldest = 0;
	int oldestTime = MAX_TYPE( oldestTime );
	for ( int i = 0; i < m_overlays.Num(); i++ ) {
		if ( m_overlays[i].lastStartTime < oldestTime ) {
			oldestTime = m_overlays[i].lastStartTime;
			oldest = i;
		}
	}

	// remove any reference another model may still have to this overlay
	if ( m_overlays[oldest].entityHandle >= 0 && m_overlays[ oldest ].entityHandle < m_entityDefs.Num() ) {
		idRenderEntity	*def = m_entityDefs[ m_overlays[ oldest ].entityHandle ];
		if ( def != NULL && def->overlays == m_overlays[ oldest ].overlays ) {
			def->overlays = NULL;
		}
	}

	m_overlays[ oldest ].entityHandle = newEntityHandle;
	m_overlays[ oldest ].lastStartTime = startTime;
	m_overlays[ oldest ].overlays->ReUse();

	return m_overlays[ oldest ].overlays;
}

/*
====================
idRenderWorld::RemoveDecals
====================
*/
void idRenderWorld::RemoveDecals( qhandle_t entityHandle ) {
	if ( entityHandle < 0 || entityHandle >= m_entityDefs.Num() ) {
		idLib::Error( "idRenderWorld::ProjectOverlay: index = %i", entityHandle );
		return;
	}

	idRenderEntity	*def = m_entityDefs[ entityHandle ];
	if ( !def ) {
		return;
	}

	FreeEntityDefDecals( def );
	FreeEntityDefOverlay( def );
}

/*
===================
idRenderWorld::NumAreas
===================
*/
int idRenderWorld::NumAreas() const {
	return m_numPortalAreas;
}

/*
===================
idRenderWorld::NumPortalsInArea
===================
*/
int idRenderWorld::NumPortalsInArea( int areaNum ) {
	portalArea_t	*area;
	int				count;
	portal_t		*portal;

	if ( areaNum >= m_numPortalAreas || areaNum < 0 ) {
		idLib::Error( "idRenderWorld::NumPortalsInArea: bad areanum %i", areaNum );
	}
	area = &m_portalAreas[areaNum];

	count = 0;
	for ( portal = area->portals; portal; portal = portal->next ) {
		count++;
	}
	return count;
}

/*
===================
idRenderWorld::GetPortal
===================
*/
exitPortal_t idRenderWorld::GetPortal( int areaNum, int portalNum ) {
	portalArea_t	*area;
	int				count;
	portal_t		*portal;
	exitPortal_t	ret;

	if ( areaNum > m_numPortalAreas ) {
		idLib::Error( "idRenderWorld::GetPortal: areaNum > numAreas" );
	}
	area = &m_portalAreas[areaNum];

	count = 0;
	for ( portal = area->portals; portal; portal = portal->next ) {
		if ( count == portalNum ) {
			ret.areas[0] = areaNum;
			ret.areas[1] = portal->intoArea;
			ret.w = portal->w;
			ret.blockingBits = portal->doublePortal->blockingBits;
			ret.portalHandle = portal->doublePortal - m_doublePortals + 1;
			return ret;
		}
		count++;
	}

	idLib::Error( "idRenderWorld::GetPortal: portalNum > numPortals" );

	memset( &ret, 0, sizeof( ret ) );
	return ret;
}

/*
===============
idRenderWorld::PointInAreaNum

Will return -1 if the point is not in an area, otherwise
it will return 0 <= value < tr.world->numPortalAreas
===============
*/
int idRenderWorld::PointInArea( const idVec3 &point ) const {
	areaNode_t	*node;
	int			nodeNum;
	float		d;
	
	node = m_areaNodes;
	if ( !node ) {
		return -1;
	}
	while( 1 ) {
		d = point * node->plane.Normal() + node->plane[3];
		if (d > 0) {
			nodeNum = node->children[0];
		} else {
			nodeNum = node->children[1];
		}
		if ( nodeNum == 0 ) {
			return -1;		// in solid
		}
		if ( nodeNum < 0 ) {
			nodeNum = -1 - nodeNum;
			if ( nodeNum >= m_numPortalAreas ) {
				idLib::Error( "idRenderWorld::PointInArea: area out of range" );
			}
			return nodeNum;
		}
		node = m_areaNodes + nodeNum;
	}
	
	return -1;
}

/*
===================
idRenderWorld::BoundsInAreas_r
===================
*/
void idRenderWorld::BoundsInAreas_r( int nodeNum, const idBounds &bounds, int *areas, int *numAreas, int maxAreas ) const {
	int side, i;
	areaNode_t *node;

	do {
		if ( nodeNum < 0 ) {
			nodeNum = -1 - nodeNum;

			for ( i = 0; i < (*numAreas); i++ ) {
				if ( areas[i] == nodeNum ) {
					break;
				}
			}
			if ( i >= (*numAreas) && (*numAreas) < maxAreas ) {
				areas[(*numAreas)++] = nodeNum;
			}
			return;
		}

		node = m_areaNodes + nodeNum;

		side = bounds.PlaneSide( node->plane );
		if ( side == PLANESIDE_FRONT ) {
			nodeNum = node->children[0];
		}
		else if ( side == PLANESIDE_BACK ) {
			nodeNum = node->children[1];
		}
		else {
			if ( node->children[1] != 0 ) {
				BoundsInAreas_r( node->children[1], bounds, areas, numAreas, maxAreas );
				if ( (*numAreas) >= maxAreas ) {
					return;
				}
			}
			nodeNum = node->children[0];
		}
	} while( nodeNum != 0 );

	return;
}

/*
===================
idRenderWorld::BoundsInAreas

  fills the *areas array with the number of the areas the bounds are in
  returns the total number of areas the bounds are in
===================
*/
int idRenderWorld::BoundsInAreas( const idBounds &bounds, int *areas, int maxAreas ) const {
	int numAreas = 0;

	assert( areas );
	assert( bounds[0][0] <= bounds[1][0] && bounds[0][1] <= bounds[1][1] && bounds[0][2] <= bounds[1][2] );
	assert( bounds[1][0] - bounds[0][0] < 1e4f && bounds[1][1] - bounds[0][1] < 1e4f && bounds[1][2] - bounds[0][2] < 1e4f );

	if ( !m_areaNodes ) {
		return numAreas;
	}
	BoundsInAreas_r( 0, bounds, areas, &numAreas, maxAreas );
	return numAreas;
}

/*
================
idRenderWorld::GuiTrace

checks a ray trace against any gui surfaces in an entity, returning the
fraction location of the trace on the gui surface, or -1,-1 if no hit.
this doesn't do any occlusion testing, simply ignoring non-gui surfaces.
start / end are in global world coordinates.
================
*/
void R_SurfaceToTextureAxis( const srfTriangles_t *tri, idVec3 &origin, idVec3 axis[3] );
guiPoint_t idRenderWorld::GuiTrace( qhandle_t entityHandle, const idVec3 start, const idVec3 end ) const {
	guiPoint_t	pt;
	pt.x = pt.y = -1;
	pt.guiId = 0;

	if ( ( entityHandle < 0 ) || ( entityHandle >= m_entityDefs.Num() ) ) {
		idLib::Printf( "idRenderWorld::GuiTrace: invalid handle %i\n", entityHandle );
		return pt;
	}

	idRenderEntity * def = m_entityDefs[ entityHandle ];	
	if ( def == NULL ) {
		idLib::Printf( "idRenderWorld::GuiTrace: handle %i is NULL\n", entityHandle );
		return pt;
	}

	idRenderModel * model = def->parms.hModel;
	if ( model == NULL || model->IsDynamicModel() != DM_STATIC || def->parms.callback != NULL ) {
		return pt;
	}

	// transform the points into local space
	idVec3 localStart, localEnd;
	R_GlobalPointToLocal( def->modelMatrix, start, localStart );
	R_GlobalPointToLocal( def->modelMatrix, end, localEnd );

	for ( int i = 0; i < model->NumSurfaces(); i++ ) {
		const modelSurface_t *surf = model->Surface( i );

		const srfTriangles_t * tri = surf->geometry;
		if ( tri == NULL ) {
			continue;
		}

		const idMaterial * shader = R_RemapShaderBySkin( surf->shader, def->parms.customSkin, def->parms.customShader );
		if ( shader == NULL ) {
			continue;
		}
		// only trace against gui surfaces
		if ( !shader->HasGui() ) {
			continue;
		}

		localTrace_t local = R_LocalTrace( localStart, localEnd, 0.0f, tri );
		if ( local.fraction < 1.0f ) {
			idVec3 origin, axis[3];

			R_SurfaceToTextureAxis( tri, origin, axis );
			const idVec3 cursor = local.point - origin;

			float axisLen[2];
			axisLen[0] = axis[0].Length();
			axisLen[1] = axis[1].Length();

			pt.x = ( cursor * axis[0] ) / ( axisLen[0] * axisLen[0] );
			pt.y = ( cursor * axis[1] ) / ( axisLen[1] * axisLen[1] );
			pt.guiId = shader->GetEntityGui();

			return pt;
		}
	}

	return pt;
}

/*
===================
idRenderWorld::ModelTrace
===================
*/
bool idRenderWorld::ModelTrace( modelTrace_t &trace, qhandle_t entityHandle, const idVec3 &start, const idVec3 &end, const float radius ) const {

	memset( &trace, 0, sizeof( trace ) );
	trace.fraction = 1.0f;
	trace.point = end;

	if ( entityHandle < 0 || entityHandle >= m_entityDefs.Num() ) {
		return false;
	}

	idRenderEntity	*def = m_entityDefs[ entityHandle ];
	if ( def == NULL ) {
		return false;
	}

	renderEntity_t *refEnt = &def->parms;

	idRenderModel *model = R_EntityDefDynamicModel( def );
	if ( model == NULL ) {
		return false;
	}

	// transform the points into local space
	float modelMatrix[16];
	idVec3 localStart;
	idVec3 localEnd;
	R_AxisToModelMatrix( refEnt->axis, refEnt->origin, modelMatrix );
	R_GlobalPointToLocal( modelMatrix, start, localStart );
	R_GlobalPointToLocal( modelMatrix, end, localEnd );

	// if we have explicit collision surfaces, only collide against them
	// (FIXME, should probably have a parm to control this)
	bool collisionSurface = false;
	for ( int i = 0; i < model->NumBaseSurfaces(); i++ ) {
		const modelSurface_t *surf = model->Surface( i );

		const idMaterial *shader = R_RemapShaderBySkin( surf->shader, def->parms.customSkin, def->parms.customShader );

		if ( shader->GetSurfaceFlags() & SURF_COLLISION ) {
			collisionSurface = true;
			break;
		}
	}

	// only use baseSurfaces, not any overlays
	for ( int i = 0; i < model->NumBaseSurfaces(); i++ ) {
		const modelSurface_t *surf = model->Surface( i );

		const idMaterial *shader = R_RemapShaderBySkin( surf->shader, def->parms.customSkin, def->parms.customShader );

		if ( surf->geometry == NULL || shader == NULL ) {
			continue;
		}

		if ( collisionSurface ) {
			// only trace vs collision surfaces
			if ( ( shader->GetSurfaceFlags() & SURF_COLLISION ) == 0 ) {
				continue;
			}
		} else {
			// skip if not drawn or translucent
			if ( !shader->IsDrawn() || ( shader->Coverage() != MC_OPAQUE && shader->Coverage() != MC_PERFORATED ) ) {
				continue;
			}
		}

		localTrace_t localTrace = R_LocalTrace( localStart, localEnd, radius, surf->geometry );

		if ( localTrace.fraction < trace.fraction ) {
			trace.fraction = localTrace.fraction;
			R_LocalPointToGlobal( modelMatrix, localTrace.point, trace.point );
			trace.normal = localTrace.normal * refEnt->axis;
			trace.material = shader;
			trace.entity = &def->parms;
			trace.jointNumber = refEnt->hModel->NearestJoint( i, localTrace.indexes[0], localTrace.indexes[1], localTrace.indexes[2] );
		}
	}

	return ( trace.fraction < 1.0f );
}

/*
===================
idRenderWorld::Trace
===================
*/
// FIXME: _D3XP added those.
const char * playerModelExcludeList[] = {
	"models/md5/characters/player/d3xp_spplayer.md5mesh",
	"models/md5/characters/player/head/d3xp_head.md5mesh",
	"models/md5/weapons/pistol_world/worldpistol.md5mesh",
	NULL
};

const char * playerMaterialExcludeList[] = {
	"muzzlesmokepuff",
	NULL
};

bool idRenderWorld::Trace( modelTrace_t &trace, const idVec3 &start, const idVec3 &end, const float radius, bool skipDynamic, bool skipPlayer /*_D3XP*/ ) const {
	trace.fraction = 1.0f;
	trace.point = end;

	// bounds for the whole trace
	idBounds traceBounds;
	traceBounds.Clear();
	traceBounds.AddPoint( start );
	traceBounds.AddPoint( end );

	// get the world areas the trace is in
	int areas[128];
	int numAreas = BoundsInAreas( traceBounds, areas, 128 );

	int numSurfaces = 0;

	// check all areas for models
	for ( int i = 0; i < numAreas; i++ ) {

		portalArea_t * area = &m_portalAreas[ areas[i] ];

		// check all models in this area
		for ( areaReference_t * ref = area->entityRefs.areaNext; ref != &area->entityRefs; ref = ref->areaNext ) {
			idRenderEntity * def = ref->entity;

			idRenderModel * model = def->parms.hModel;
			if ( model == NULL ) {
				continue;
			}

			if ( model->IsDynamicModel() != DM_STATIC ) {
				if ( skipDynamic ) {
					continue;
				}

#if 1	/* _D3XP addition. could use a cleaner approach */
				if ( skipPlayer ) {
					bool exclude = false;
					for ( int k = 0; playerModelExcludeList[k] != NULL; k++ ) {
						if ( idStr::Cmp( model->Name(), playerModelExcludeList[k] ) == 0 ) {
							exclude = true;
							break;
						}
					}
					if ( exclude ) {
						continue;
					}
				}
#endif

				model = R_EntityDefDynamicModel( def );
				if ( !model ) {
					continue;	// can happen with particle systems, which don't instantiate without a valid view
				}
			}

			idBounds bounds;
			bounds.FromTransformedBounds( model->Bounds( &def->parms ), def->parms.origin, def->parms.axis );

			// if the model bounds do not overlap with the trace bounds
			if ( !traceBounds.IntersectsBounds( bounds ) || !bounds.LineIntersection( start, trace.point ) ) {
				continue;
			}

			// check all model surfaces
			for ( int j = 0; j < model->NumSurfaces(); j++ ) {
				const modelSurface_t *surf = model->Surface( j );

				const idMaterial * shader = R_RemapShaderBySkin( surf->shader, def->parms.customSkin, def->parms.customShader );

				// if no geometry or no shader
				if ( surf->geometry == NULL || shader == NULL ) {
					continue;
				}

#if 1 /* _D3XP addition. could use a cleaner approach */
				if ( skipPlayer ) {
					bool exclude = false;
					for ( int k = 0; playerMaterialExcludeList[k] != NULL; k++ ) {
						if ( idStr::Cmp( shader->GetName(), playerMaterialExcludeList[k] ) == 0 ) {
							exclude = true;
							break;
						}
					}
					if ( exclude ) {
						continue;
					}
				}
#endif

				const srfTriangles_t * tri = surf->geometry;

				bounds.FromTransformedBounds( tri->bounds, def->parms.origin, def->parms.axis );

				// if triangle bounds do not overlap with the trace bounds
				if ( !traceBounds.IntersectsBounds( bounds ) || !bounds.LineIntersection( start, trace.point ) ) {
					continue;
				}

				numSurfaces++;

				// transform the points into local space
				float modelMatrix[16];
				idVec3 localStart, localEnd;
				R_AxisToModelMatrix( def->parms.axis, def->parms.origin, modelMatrix );
				R_GlobalPointToLocal( modelMatrix, start, localStart );
				R_GlobalPointToLocal( modelMatrix, end, localEnd );

				localTrace_t localTrace = R_LocalTrace( localStart, localEnd, radius, surf->geometry );

				if ( localTrace.fraction < trace.fraction ) {
					trace.fraction = localTrace.fraction;
					R_LocalPointToGlobal( modelMatrix, localTrace.point, trace.point );
					trace.normal = localTrace.normal * def->parms.axis;
					trace.material = shader;
					trace.entity = &def->parms;
					trace.jointNumber = model->NearestJoint( j, localTrace.indexes[0], localTrace.indexes[1], localTrace.indexes[2] );

					traceBounds.Clear();
					traceBounds.AddPoint( start );
					traceBounds.AddPoint( start + trace.fraction * (end - start) );
				}
			}
		}
	}
	return ( trace.fraction < 1.0f );
}

/*
==================
idRenderWorld::RecurseProcBSP
==================
*/
void idRenderWorld::RecurseProcBSP_r( modelTrace_t *results, int parentNodeNum, int nodeNum, float p1f, float p2f, const idVec3 &p1, const idVec3 &p2 ) const {
	float		t1, t2;
	float		frac;
	idVec3		mid;
	int			side;
	float		midf;
	areaNode_t *node;

	if ( results->fraction <= p1f) {
		return;		// already hit something nearer
	}
	// empty leaf
	if ( nodeNum < 0 ) {
		return;
	}
	// if solid leaf node
	if ( nodeNum == 0 ) {
		if ( parentNodeNum != -1 ) {

			results->fraction = p1f;
			results->point = p1;
			node = &m_areaNodes[parentNodeNum];
			results->normal = node->plane.Normal();
			return;
		}
	}
	node = &m_areaNodes[nodeNum];

	// distance from plane for trace start and end
	t1 = node->plane.Normal() * p1 + node->plane[3];
	t2 = node->plane.Normal() * p2 + node->plane[3];

	if ( t1 >= 0.0f && t2 >= 0.0f ) {
		RecurseProcBSP_r( results, nodeNum, node->children[0], p1f, p2f, p1, p2 );
		return;
	}
	if ( t1 < 0.0f && t2 < 0.0f ) {
		RecurseProcBSP_r( results, nodeNum, node->children[1], p1f, p2f, p1, p2 );
		return;
	}
	side = t1 < t2;
	frac = t1 / (t1 - t2);
	midf = p1f + frac*(p2f - p1f);
	mid[0] = p1[0] + frac*(p2[0] - p1[0]);
	mid[1] = p1[1] + frac*(p2[1] - p1[1]);
	mid[2] = p1[2] + frac*(p2[2] - p1[2]);
	RecurseProcBSP_r( results, nodeNum, node->children[side], p1f, midf, p1, mid );
	RecurseProcBSP_r( results, nodeNum, node->children[side^1], midf, p2f, mid, p2 );
}

/*
==================
idRenderWorld::FastWorldTrace
==================
*/
bool idRenderWorld::FastWorldTrace( modelTrace_t &results, const idVec3 &start, const idVec3 &end ) const {
	memset( &results, 0, sizeof( modelTrace_t ) );
	results.fraction = 1.0f;
	if ( m_areaNodes != NULL ) {
		RecurseProcBSP_r( &results, -1, 0, 0.0f, 1.0f, start, end );
		return ( results.fraction < 1.0f );
	}
	return false;
}

/*
=================================================================================

CREATE MODEL REFS

=================================================================================
*/

/*
=================
idRenderWorld::AddEntityRefToArea

This is called by R_PushVolumeIntoTree and also directly
for the world model references that are precalculated.
=================
*/
void idRenderWorld::AddEntityRefToArea( idRenderEntity *def, portalArea_t *area ) {
	areaReference_t	*ref;

	if ( def == NULL ) {
		idLib::Error( "idRenderWorld::AddEntityRefToArea: NULL def" );
		return;
	}

	for ( ref = def->entityRefs; ref != NULL; ref = ref->ownerNext ) {
		if ( ref->area == area ) {
			return;
		}
	}

	ref = m_areaReferenceAllocator.Alloc();

	tr.pc.c_entityReferences++;

	ref->entity = def;

	// link to entityDef
	ref->ownerNext = def->entityRefs;
	def->entityRefs = ref;

	// link to end of area list
	ref->area = area;
	ref->areaNext = &area->entityRefs;
	ref->areaPrev = area->entityRefs.areaPrev;
	ref->areaNext->areaPrev = ref;
	ref->areaPrev->areaNext = ref;
}

/*
===================
idRenderWorld::AddLightRefToArea
===================
*/
void idRenderWorld::AddLightRefToArea( idRenderLight *light, portalArea_t *area ) {
	areaReference_t	*lref;

	for ( lref = light->references; lref != NULL; lref = lref->ownerNext ) {
		if ( lref->area == area ) {
			return;
		}
	}

	// add a lightref to this area
	lref = m_areaReferenceAllocator.Alloc();
	lref->light = light;
	lref->area = area;
	lref->ownerNext = light->references;
	light->references = lref;
	tr.pc.c_lightReferences++;

	// doubly linked list so we can free them easily later
	area->lightRefs.areaNext->areaPrev = lref;
	lref->areaNext = area->lightRefs.areaNext;
	lref->areaPrev = &area->lightRefs;
	area->lightRefs.areaNext = lref;
}

/*
===================
idRenderWorld::GenerateAllInteractions

Force the generation of all light / surface interactions at the start of a level
If this isn't called, they will all be dynamically generated
===================
*/
void idRenderWorld::GenerateAllInteractions() {
	if ( !renderSystem->IsInitialized() ) {
		return;
	}

	int start = Sys_Milliseconds();

	// let the interaction creation code know that it shouldn't
	// try and do any view specific optimizations
	tr.m_viewDef = NULL;

	// build the interaction table
	// this will be dynamically resized if the entity / light counts grow too much
	m_interactionTableWidth = m_entityDefs.Num() + 100;
	m_interactionTableHeight = m_lightDefs.Num() + 100;
	int	size =  m_interactionTableWidth * m_interactionTableHeight * sizeof( *m_interactionTable );
	m_interactionTable = (idInteraction **)R_ClearedStaticAlloc( size );

	// itterate through all lights
	int	count = 0;
	for ( int i = 0; i < m_lightDefs.Num(); i++ ) {
		idRenderLight	*ldef = m_lightDefs[i];
		if ( ldef == NULL ) {
			continue;
		}

		// check all areas the light touches
		for ( areaReference_t *lref = ldef->references; lref; lref = lref->ownerNext ) {
			portalArea_t *area = lref->area;

			// check all the models in this area
			for ( areaReference_t *eref = area->entityRefs.areaNext; eref != &area->entityRefs; eref = eref->areaNext ) {
				idRenderEntity	 *edef = eref->entity;

				// scan the doubly linked lists, which may have several dozen entries
				idInteraction	*inter;

				// we could check either model refs or light refs for matches, but it is
				// assumed that there will be less lights in an area than models
				// so the entity chains should be somewhat shorter (they tend to be fairly close).
				for ( inter = edef->firstInteraction; inter != NULL; inter = inter->entityNext ) {
					if ( inter->lightDef == ldef ) {
						break;
					}
				}

				// if we already have an interaction, we don't need to do anything
				if ( inter != NULL ) {
					continue;
				}

				// make an interaction for this light / entity pair
				// and add a pointer to it in the table
				inter = idInteraction::AllocAndLink( edef, ldef );
				count++;

				// the interaction may create geometry
				inter->CreateStaticInteraction();
			}
		}

		session->Pump();
	}

	int end = Sys_Milliseconds();
	int	msec = end - start;

	idLib::Printf( "idRenderWorld::GenerateAllInteractions, msec = %i\n", msec );
	idLib::Printf( "interactionTable size: %i bytes\n", size );
	idLib::Printf( "%i interactions take %i bytes\n", count, count * sizeof( idInteraction ) );
}

/*
===================
idRenderWorld::FreeInteractions
===================
*/
void idRenderWorld::FreeInteractions() {
	int			i;
	idRenderEntity	*def;

	for ( i = 0; i < m_entityDefs.Num(); i++ ) {
		def = m_entityDefs[i];
		if ( !def ) {
			continue;
		}
		// free all the interactions
		while ( def->firstInteraction != NULL ) {
			def->firstInteraction->UnlinkAndFree();
		}
	}
}

/*
==================
idRenderWorld::PushFrustumIntoTree_r

Used for both light volumes and model volumes.

This does not clip the points by the planes, so some slop
occurs.

tr.viewCount should be bumped before calling, allowing it
to prevent double checking areas.

We might alternatively choose to do this with an area flow.
==================
*/
void idRenderWorld::PushFrustumIntoTree_r( idRenderEntity *def, idRenderLight *light,
												const frustumCorners_t & corners, int nodeNum ) {
	if ( nodeNum < 0 ) {
		int areaNum = -1 - nodeNum;
		portalArea_t * area = &m_portalAreas[ areaNum ];
		if ( area->viewCount == tr.viewCount ) {
			return;	// already added a reference here
		}
		area->viewCount = tr.viewCount;

		if ( def != NULL ) {
			AddEntityRefToArea( def, area );
		}
		if ( light != NULL ) {
			AddLightRefToArea( light, area );
		}

		return;
	}

	areaNode_t * node = m_areaNodes + nodeNum;

	// if we know that all possible children nodes only touch an area
	// we have already marked, we can early out
	if ( node->commonChildrenArea != CHILDREN_HAVE_MULTIPLE_AREAS && r_useNodeCommonChildren.GetBool() ) {
		// note that we do NOT try to set a reference in this area
		// yet, because the test volume may yet wind up being in the
		// solid part, which would cause bounds slightly poked into
		// a wall to show up in the next room
		if ( m_portalAreas[ node->commonChildrenArea ].viewCount == tr.viewCount ) {
			return;
		}
	}

	// exact check all the corners against the node plane
	frustumCull_t cull = idRenderMatrix::CullFrustumCornersToPlane( corners, node->plane );

	if ( cull != FRUSTUM_CULL_BACK ) {
		nodeNum = node->children[0];
		if ( nodeNum != 0 ) {	// 0 = solid
			PushFrustumIntoTree_r( def, light, corners, nodeNum );
		}
	}

	if ( cull != FRUSTUM_CULL_FRONT ) {
		nodeNum = node->children[1];
		if ( nodeNum != 0 ) {	// 0 = solid
			PushFrustumIntoTree_r( def, light, corners, nodeNum );
		}
	}
}

/*
==============
idRenderWorld::PushFrustumIntoTree
==============
*/
void idRenderWorld::PushFrustumIntoTree( idRenderEntity *def, idRenderLight *light, const idRenderMatrix & frustumTransform, const idBounds & frustumBounds ) {
	if ( m_areaNodes == NULL ) {
		return;
	}

	// calculate the corners of the frustum in word space
	ALIGNTYPE16 frustumCorners_t corners;
	idRenderMatrix::GetFrustumCorners( corners, frustumTransform, frustumBounds );

	PushFrustumIntoTree_r( def, light, corners, 0 );
}

//===================================================================

/*
====================
idRenderWorld::DebugClearLines
====================
*/
void idRenderWorld::DebugClearLines( int time ) {
	RB_ClearDebugLines( time );
	RB_ClearDebugText( time );
}

/*
====================
idRenderWorld::DebugLine
====================
*/
void idRenderWorld::DebugLine( const idVec4 &color, const idVec3 &start, const idVec3 &end, const int lifetime, const bool depthTest ) {
	RB_AddDebugLine( color, start, end, lifetime, depthTest );
}

/*
================
idRenderWorld::DebugArrow
================
*/
void idRenderWorld::DebugArrow( const idVec4 &color, const idVec3 &start, const idVec3 &end, int size, const int lifetime ) {
	idVec3 forward, right, up, v1, v2;
	float a, s;
	int i;
	static float arrowCos[40];
	static float arrowSin[40];
	static int arrowStep;

	DebugLine( color, start, end, lifetime );

	if ( r_debugArrowStep.GetInteger() <= 10 ) {
		return;
	}
	// calculate sine and cosine when step size changes
	if ( arrowStep != r_debugArrowStep.GetInteger() ) {
		arrowStep = r_debugArrowStep.GetInteger();
		for ( i = 0, a = 0; a < 360.0f; a += arrowStep, i++ ) {
			arrowCos[i] = idMath::Cos16( DEG2RAD( a ) );
			arrowSin[i] = idMath::Sin16( DEG2RAD( a ) );
		}
		arrowCos[i] = arrowCos[0];
		arrowSin[i] = arrowSin[0];
	}
	// draw a nice arrow
	forward = end - start;
	forward.Normalize();
	forward.NormalVectors( right, up);
	for ( i = 0, a = 0; a < 360.0f; a += arrowStep, i++ ) {
		s = 0.5f * size * arrowCos[i];
		v1 = end - size * forward;
		v1 = v1 + s * right;
		s = 0.5f * size * arrowSin[i];
		v1 = v1 + s * up;

		s = 0.5f * size * arrowCos[i+1];
		v2 = end - size * forward;
		v2 = v2 + s * right;
		s = 0.5f * size * arrowSin[i+1];
		v2 = v2 + s * up;

		DebugLine( color, v1, end, lifetime );
		DebugLine( color, v1, v2, lifetime );
	}
}

/*
====================
idRenderWorld::DebugWinding
====================
*/
void idRenderWorld::DebugWinding( const idVec4 &color, const idWinding &w, const idVec3 &origin, const idMat3 &axis, const int lifetime, const bool depthTest ) {
	int i;
	idVec3 point, lastPoint;

	if ( w.GetNumPoints() < 2 ) {
		return;
	}

	lastPoint = origin + w[w.GetNumPoints()-1].ToVec3() * axis;
	for ( i = 0; i < w.GetNumPoints(); i++ ) {
		point = origin + w[i].ToVec3() * axis;
		DebugLine( color, lastPoint, point, lifetime, depthTest );
		lastPoint = point;
	}
}

/*
====================
idRenderWorld::DebugCircle
====================
*/
void idRenderWorld::DebugCircle( const idVec4 &color, const idVec3 &origin, const idVec3 &dir, const float radius, const int numSteps, const int lifetime, const bool depthTest ) {
	int i;
	float a;
	idVec3 left, up, point, lastPoint;

	dir.OrthogonalBasis( left, up );
	left *= radius;
	up *= radius;
	lastPoint = origin + up;
	for ( i = 1; i <= numSteps; i++ ) {
		a = idMath::TWO_PI * i / numSteps;
		point = origin + idMath::Sin16( a ) * left + idMath::Cos16( a ) * up;
		DebugLine( color, lastPoint, point, lifetime, depthTest );
		lastPoint = point;
	}
}

/*
============
idRenderWorld::DebugSphere
============
*/
void idRenderWorld::DebugSphere( const idVec4 &color, const idSphere &sphere, const int lifetime, const bool depthTest /*_D3XP*/ ) {
	int i, j, n, num;
	float s, c;
	idVec3 p, lastp, *lastArray;

	num = 360 / 15;
	lastArray = (idVec3 *) _alloca16( num * sizeof( idVec3 ) );
	lastArray[0] = sphere.GetOrigin() + idVec3( 0, 0, sphere.GetRadius() );
	for ( n = 1; n < num; n++ ) {
		lastArray[n] = lastArray[0];
	}

	for ( i = 15; i <= 360; i += 15 ) {
		s = idMath::Sin16( DEG2RAD(i) );
		c = idMath::Cos16( DEG2RAD(i) );
		lastp[0] = sphere.GetOrigin()[0];
		lastp[1] = sphere.GetOrigin()[1] + sphere.GetRadius() * s;
		lastp[2] = sphere.GetOrigin()[2] + sphere.GetRadius() * c;
		for ( n = 0, j = 15; j <= 360; j += 15, n++ ) {
			p[0] = sphere.GetOrigin()[0] + idMath::Sin16( DEG2RAD(j) ) * sphere.GetRadius() * s;
			p[1] = sphere.GetOrigin()[1] + idMath::Cos16( DEG2RAD(j) ) * sphere.GetRadius() * s;
			p[2] = lastp[2];

			DebugLine( color, lastp, p, lifetime,depthTest );
			DebugLine( color, lastp, lastArray[n], lifetime, depthTest );

			lastArray[n] = lastp;
			lastp = p;
		}
	}
}

/*
====================
idRenderWorld::DebugBounds
====================
*/
void idRenderWorld::DebugBounds( const idVec4 &color, const idBounds &bounds, const idVec3 &org, const int lifetime ) {
	int i;
	idVec3 v[8];

	if ( bounds.IsCleared() ) {
		return;
	}

	for ( i = 0; i < 8; i++ ) {
		v[i][0] = org[0] + bounds[(i^(i>>1))&1][0];
		v[i][1] = org[1] + bounds[(i>>1)&1][1];
		v[i][2] = org[2] + bounds[(i>>2)&1][2];
	}
	for ( i = 0; i < 4; i++ ) {
		DebugLine( color, v[i], v[(i+1)&3], lifetime );
		DebugLine( color, v[4+i], v[4+((i+1)&3)], lifetime );
		DebugLine( color, v[i], v[4+i], lifetime );
	}
}

/*
====================
idRenderWorld::DebugBox
====================
*/
void idRenderWorld::DebugBox( const idVec4 &color, const idBox &box, const int lifetime ) {
	int i;
	idVec3 v[8];

	box.ToPoints( v );
	for ( i = 0; i < 4; i++ ) {
		DebugLine( color, v[i], v[(i+1)&3], lifetime );
		DebugLine( color, v[4+i], v[4+((i+1)&3)], lifetime );
		DebugLine( color, v[i], v[4+i], lifetime );
	}
}

/*
============
idRenderWorld::DebugCone

  dir is the cone axis
  radius1 is the radius at the apex
  radius2 is the radius at apex+dir
============
*/
void idRenderWorld::DebugCone( const idVec4 &color, const idVec3 &apex, const idVec3 &dir, float radius1, float radius2, const int lifetime ) {
	int i;
	idMat3 axis;
	idVec3 top, p1, p2, lastp1, lastp2, d;

	axis[2] = dir;
	axis[2].Normalize();
	axis[2].NormalVectors( axis[0], axis[1] );
	axis[1] = -axis[1];

	top = apex + dir;
	lastp2 = top + radius2 * axis[1];

	if ( radius1 == 0.0f ) {
		for ( i = 20; i <= 360; i += 20 ) {
			d = idMath::Sin16( DEG2RAD(i) ) * axis[0] + idMath::Cos16( DEG2RAD(i) ) * axis[1];
			p2 = top + d * radius2;
			DebugLine( color, lastp2, p2, lifetime );
			DebugLine( color, p2, apex, lifetime );
			lastp2 = p2;
		}
	} else {
		lastp1 = apex + radius1 * axis[1];
		for ( i = 20; i <= 360; i += 20 ) {
			d = idMath::Sin16( DEG2RAD(i) ) * axis[0] + idMath::Cos16( DEG2RAD(i) ) * axis[1];
			p1 = apex + d * radius1;
			p2 = top + d * radius2;
			DebugLine( color, lastp1, p1, lifetime );
			DebugLine( color, lastp2, p2, lifetime );
			DebugLine( color, p1, p2, lifetime );
			lastp1 = p1;
			lastp2 = p2;
		}
	}
}

/*
================
idRenderWorld::DebugAxis
================
*/
void idRenderWorld::DebugAxis( const idVec3 &origin, const idMat3 &axis ) {
	idVec3 start = origin;
	idVec3 end = start + axis[0] * 20.0f;
	DebugArrow( colorWhite, start, end, 2 );
	end = start + axis[0] * -20.0f;
	DebugArrow( colorWhite, start, end, 2 );
	end = start + axis[1] * +20.0f;
	DebugArrow( colorGreen, start, end, 2 );
	end = start + axis[1] * -20.0f;
	DebugArrow( colorGreen, start, end, 2 );
	end = start + axis[2] * +20.0f;
	DebugArrow( colorBlue, start, end, 2 );
	end = start + axis[2] * -20.0f;
	DebugArrow( colorBlue, start, end, 2 );
}

/*
====================
idRenderWorld::DebugClearPolygons
====================
*/
void idRenderWorld::DebugClearPolygons( int time ) {
	RB_ClearDebugPolygons( time );
}

/*
====================
idRenderWorld::DebugPolygon
====================
*/
void idRenderWorld::DebugPolygon( const idVec4 &color, const idWinding &winding, const int lifeTime, const bool depthTest ) {
	RB_AddDebugPolygon( color, winding, lifeTime, depthTest );
}

/*
================
idRenderWorld::DebugScreenRect
================
*/
void idRenderWorld::DebugScreenRect( const idVec4 &color, const idScreenRect &rect, const viewDef_t *viewDef, const int lifetime ) {
	int i;
	float centerx, centery, dScale, hScale, vScale;
	idBounds bounds;
	idVec3 p[4];

	centerx = ( viewDef->viewport.x2 - viewDef->viewport.x1 ) * 0.5f;
	centery = ( viewDef->viewport.y2 - viewDef->viewport.y1 ) * 0.5f;

	dScale = r_znear.GetFloat() + 1.0f;
	hScale = dScale * idMath::Tan16( DEG2RAD( viewDef->renderView.fov_x * 0.5f ) );
	vScale = dScale * idMath::Tan16( DEG2RAD( viewDef->renderView.fov_y * 0.5f ) );

	bounds[0][0] = bounds[1][0] = dScale;
	bounds[0][1] = -( rect.x1 - centerx ) / centerx * hScale;
	bounds[1][1] = -( rect.x2 - centerx ) / centerx * hScale;
	bounds[0][2] = ( rect.y1 - centery ) / centery * vScale;
	bounds[1][2] = ( rect.y2 - centery ) / centery * vScale;

	for ( i = 0; i < 4; i++ ) {
		p[i].x = bounds[0][0];
		p[i].y = bounds[(i^(i>>1))&1].y;
		p[i].z = bounds[(i>>1)&1].z;
		p[i] = viewDef->renderView.vieworg + p[i] * viewDef->renderView.viewaxis;
	}
	for ( i = 0; i < 4; i++ ) {
		DebugLine( color, p[i], p[(i+1)&3], false );
	}
}

/*
================
idRenderWorld::DrawText

  oriented on the viewaxis
  align can be 0-left, 1-center (default), 2-right
================
*/
void idRenderWorld::DrawText( const char *text, const idVec3 &origin, float scale, const idVec4 &color, const idMat3 &viewAxis, const int align, const int lifetime, const bool depthTest ) {
	RB_AddDebugText( text, origin, scale, color, viewAxis, align, lifetime, depthTest );
}

/*
===============
idRenderWorld::RegenerateWorld
===============
*/
void idRenderWorld::RegenerateWorld() {
	FreeDerivedData();
	ReCreateReferences();
}
