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
#include "Interaction.h"

srfTriangles_t *	R_AllocStaticTriSurf();
void				R_AllocStaticTriSurfVerts( srfTriangles_t *tri, int numVerts );
void				R_AllocStaticTriSurfIndexes( srfTriangles_t *tri, int numIndexes );
void				R_AllocStaticTriSurfPreLightShadowVerts( srfTriangles_t *tri, int numVerts );

/*
================
idRenderWorld::FreeWorld
================
*/
void idRenderWorld::FreeWorld() {
	// this will free all the lightDefs and entityDefs
	FreeDefs();

	// free all the portals and check light/model references
	for ( int i = 0; i < m_numPortalAreas; i++ ) {
		portalArea_t	*area;
		portal_t		*portal, *nextPortal;

		area = &m_portalAreas[i];
		for ( portal = area->portals; portal; portal = nextPortal ) {
			nextPortal = portal->next;
			delete portal->w;
			R_StaticFree( portal );
		}

		// there shouldn't be any remaining lightRefs or entityRefs
		if ( area->lightRefs.areaNext != &area->lightRefs ) {
			idLib::Error( "FreeWorld: unexpected remaining lightRefs" );
		}
		if ( area->entityRefs.areaNext != &area->entityRefs ) {
			idLib::Error( "FreeWorld: unexpected remaining entityRefs" );
		}
	}

	if ( m_portalAreas ) {
		R_StaticFree( m_portalAreas );
		m_portalAreas = NULL;
		m_numPortalAreas = 0;
		R_StaticFree( m_areaScreenRect );
		m_areaScreenRect = NULL;
	}

	if ( m_doublePortals ) {
		R_StaticFree( m_doublePortals );
		m_doublePortals = NULL;
		m_numInterAreaPortals = 0;
	}

	if ( m_areaNodes ) {
		R_StaticFree( m_areaNodes );
		m_areaNodes = NULL;
	}

	// free all the inline idRenderModels 
	for ( int i = 0; i < m_localModels.Num(); i++ ) {
		renderModelManager->RemoveModel( m_localModels[i] );
		delete m_localModels[i];
	}
	m_localModels.Clear();

	m_areaReferenceAllocator.Shutdown();
	m_interactionAllocator.Shutdown();

	m_mapName = "<FREED>";
}

/*
================
idRenderWorld::TouchWorldModels
================
*/
void idRenderWorld::TouchWorldModels() {
	for ( int i = 0; i < m_localModels.Num(); i++ ) {
		renderModelManager->CheckModel( m_localModels[i]->Name() );
	}
}

/*
================
idRenderWorld::ReadBinaryShadowModel
================
*/
idRenderModel *idRenderWorld::ReadBinaryModel( idFile *fileIn ) {
	idStrStatic< MAX_OSPATH > name;
	fileIn->ReadString( name );
	idRenderModel * model = renderModelManager->AllocModel();
	model->InitEmpty( name );
	if ( model->LoadBinaryModel( fileIn, m_mapTimeStamp ) ) {
		return model;
	}
	return NULL;
}

extern idCVar r_binaryLoadRenderModels;

/*
================
idRenderWorld::ParseModel
================
*/
idRenderModel *idRenderWorld::ParseModel( idLexer *src, const char *mapName, ID_TIME_T mapTimeStamp, idFile *fileOut ) {
	idToken token;

	src->ExpectTokenString( "{" );

	// parse the name
	src->ExpectAnyToken( &token );

	idRenderModel * model = renderModelManager->AllocModel();
	model->InitEmpty( token );

	if ( fileOut != NULL ) {
		// write out the type so the binary reader knows what to instantiate
		fileOut->WriteString( "shadowmodel" );
		fileOut->WriteString( token );
	}

	int numSurfaces = src->ParseInt();
	if ( numSurfaces < 0 ) {
		src->Error( "R_ParseModel: bad numSurfaces" );
	}

	for ( int i = 0; i < numSurfaces; i++ ) {
		src->ExpectTokenString( "{" );

		src->ExpectAnyToken( &token );

		modelSurface_t surf;
		surf.shader = declManager->FindMaterial( token );

		((idMaterial*)surf.shader)->AddReference();

		srfTriangles_t * tri = R_AllocStaticTriSurf();
		surf.geometry = tri;

		tri->numVerts = src->ParseInt();
		tri->numIndexes = src->ParseInt();

		// parse the vertices
		idTempArray<float> verts( tri->numVerts * 8 );
		for ( int j = 0; j < tri->numVerts; j++ ) {
			src->Parse1DMatrix( 8, &verts[j * 8] );
		}

		// parse the indices
		idTempArray<triIndex_t> indexes( tri->numIndexes );
		for ( int j = 0; j < tri->numIndexes; j++ ) {
			indexes[j] = src->ParseInt();
		}

#if 1
		// find the island that each vertex belongs to
		idTempArray<int> vertIslands( tri->numVerts );
		idTempArray<bool> trisVisited( tri->numIndexes );
		vertIslands.Zero();
		trisVisited.Zero();
		int numIslands = 0;
		for ( int j = 0; j < tri->numIndexes; j += 3 ) {
			if ( trisVisited[j] ) {
				continue;
			}

			int islandNum = ++numIslands;
			vertIslands[indexes[j + 0]] = islandNum;
			vertIslands[indexes[j + 1]] = islandNum;
			vertIslands[indexes[j + 2]] = islandNum;
			trisVisited[j] = true;

			idList<int> queue;
			queue.Append( j );
			for ( int n = 0; n < queue.Num(); n++ ) {
				int t = queue[n];
				for ( int k = 0; k < tri->numIndexes; k += 3 ) {
					if ( trisVisited[k] ) {
						continue;
					}
					bool connected =	indexes[t + 0] == indexes[k + 0] || indexes[t + 0] == indexes[k + 1] || indexes[t + 0] == indexes[k + 2] ||
										indexes[t + 1] == indexes[k + 0] || indexes[t + 1] == indexes[k + 1] || indexes[t + 1] == indexes[k + 2] ||
										indexes[t + 2] == indexes[k + 0] || indexes[t + 2] == indexes[k + 1] || indexes[t + 2] == indexes[k + 2];
					if ( connected ) {
						vertIslands[indexes[k + 0]] = islandNum;
						vertIslands[indexes[k + 1]] = islandNum;
						vertIslands[indexes[k + 2]] = islandNum;
						trisVisited[k] = true;
						queue.Append( k );
					}
				}
			}
		}

		// center the texture coordinates for each island for maximum 16-bit precision
		for ( int j = 1; j <= numIslands; j++ ) {
			float minS = idMath::INFINITY;
			float minT = idMath::INFINITY;
			float maxS = -idMath::INFINITY;
			float maxT = -idMath::INFINITY;
			for ( int k = 0; k < tri->numVerts; k++ ) {
				if ( vertIslands[k] == j ) {
					minS = Min( minS, verts[k * 8 + 3] );
					maxS = Max( maxS, verts[k * 8 + 3] );
					minT = Min( minT, verts[k * 8 + 4] );
					maxT = Max( maxT, verts[k * 8 + 4] );
				}
			}
			const float averageS = idMath::Ftoi( ( minS + maxS ) * 0.5f );
			const float averageT = idMath::Ftoi( ( minT + maxT ) * 0.5f );
			for ( int k = 0; k < tri->numVerts; k++ ) {
				if ( vertIslands[k] == j ) {
					verts[k * 8 + 3] -= averageS;
					verts[k * 8 + 4] -= averageT;
				}
			}
		}
#endif

		R_AllocStaticTriSurfVerts( tri, tri->numVerts );
		for ( int j = 0; j < tri->numVerts; j++ ) {
			tri->verts[j].xyz[0] = verts[j * 8 + 0];
			tri->verts[j].xyz[1] = verts[j * 8 + 1];
			tri->verts[j].xyz[2] = verts[j * 8 + 2];
			tri->verts[j].SetTexCoord( verts[j * 8 + 3], verts[j * 8 + 4] );
			tri->verts[j].SetNormal( verts[j * 8 + 5], verts[j * 8 + 6], verts[j * 8 + 7] );
		}

		R_AllocStaticTriSurfIndexes( tri, tri->numIndexes );
		for ( int j = 0; j < tri->numIndexes; j++ ) {
			tri->indexes[j] = indexes[j];
		}
		src->ExpectTokenString( "}" );

		// add the completed surface to the model
		model->AddSurface( surf );
	}

	src->ExpectTokenString( "}" );

	model->FinishSurfaces();

	if ( fileOut != NULL && model->SupportsBinaryModel() && r_binaryLoadRenderModels.GetBool() ) {
		model->WriteBinaryModel( fileOut, &mapTimeStamp );
	}

	return model;
}

/*
================
idRenderWorld::ReadBinaryShadowModel
================
*/
idRenderModel *idRenderWorld::ReadBinaryShadowModel( idFile *fileIn ) {
	idStrStatic< MAX_OSPATH > name;
	fileIn->ReadString( name );
	idRenderModel * model = renderModelManager->AllocModel();
	model->InitEmpty( name );
	if ( model->LoadBinaryModel( fileIn, m_mapTimeStamp ) ) {
		return model;
	}
	return NULL;
}
/*
================
idRenderWorld::ParseShadowModel
================
*/
idRenderModel *idRenderWorld::ParseShadowModel( idLexer *src, idFile *fileOut ) {
	idToken token;

	src->ExpectTokenString( "{" );

	// parse the name
	src->ExpectAnyToken( &token );

	idRenderModel * model = renderModelManager->AllocModel();
	model->InitEmpty( token );

	if ( fileOut != NULL ) {
		// write out the type so the binary reader knows what to instantiate
		fileOut->WriteString( "shadowmodel" );
		fileOut->WriteString( token );
	}

	srfTriangles_t * tri = R_AllocStaticTriSurf();

	tri->numVerts = src->ParseInt();
	tri->numShadowIndexesNoCaps = src->ParseInt();
	tri->numShadowIndexesNoFrontCaps = src->ParseInt();
	tri->numIndexes = src->ParseInt();
	tri->shadowCapPlaneBits = src->ParseInt();

	assert( ( tri->numVerts & 1 ) == 0 );

	R_AllocStaticTriSurfPreLightShadowVerts( tri, ALIGN( tri->numVerts, 2 ) );
	tri->bounds.Clear();
	for ( int j = 0; j < tri->numVerts; j++ ) {
		float vec[8];

		src->Parse1DMatrix( 3, vec );
		tri->preLightShadowVertexes[j].xyzw[0] = vec[0];
		tri->preLightShadowVertexes[j].xyzw[1] = vec[1];
		tri->preLightShadowVertexes[j].xyzw[2] = vec[2];
		tri->preLightShadowVertexes[j].xyzw[3] = 1.0f;		// no homogenous value

		tri->bounds.AddPoint( tri->preLightShadowVertexes[j].xyzw.ToVec3() );
	}
	// clear the last vertex if it wasn't stored
	if ( ( tri->numVerts & 1 ) != 0 ) {
		tri->preLightShadowVertexes[ALIGN( tri->numVerts, 2 ) - 1].xyzw.Zero();
	}

	// to be consistent set the number of vertices to half the number of shadow vertices
	tri->numVerts = ALIGN( tri->numVerts, 2 ) / 2;

	R_AllocStaticTriSurfIndexes( tri, tri->numIndexes );
	for ( int j = 0; j < tri->numIndexes; j++ ) {
		tri->indexes[j] = src->ParseInt();
	}

	// add the completed surface to the model
	modelSurface_t surf;
	surf.id = 0;
	surf.shader = tr.defaultMaterial;
	surf.geometry = tri;

	model->AddSurface( surf );

	src->ExpectTokenString( "}" );

	// NOTE: we do NOT do a model->FinishSurfaceces, because we don't need sil edges, planes, tangents, etc.

	if ( fileOut != NULL && model->SupportsBinaryModel() && r_binaryLoadRenderModels.GetBool() ) {
		model->WriteBinaryModel( fileOut, &m_mapTimeStamp );
	}

	return model;
}

/*
================
idRenderWorld::SetupAreaRefs
================
*/
void idRenderWorld::SetupAreaRefs() {
	m_connectedAreaNum = 0;
	for ( int i = 0; i < m_numPortalAreas; i++ ) {
		m_portalAreas[i].areaNum = i;
		m_portalAreas[i].lightRefs.areaNext =
		m_portalAreas[i].lightRefs.areaPrev = &m_portalAreas[i].lightRefs;
		m_portalAreas[i].entityRefs.areaNext =
		m_portalAreas[i].entityRefs.areaPrev = &m_portalAreas[i].entityRefs;
	}
}

/*
================
idRenderWorld::ParseInterAreaPortals
================
*/
void idRenderWorld::ParseInterAreaPortals( idLexer *src, idFile *fileOut ) {
	src->ExpectTokenString( "{" );

	m_numPortalAreas = src->ParseInt();
	if ( m_numPortalAreas < 0 ) {
		src->Error( "R_ParseInterAreaPortals: bad m_numPortalAreas" );
		return;
	}

	if ( fileOut != NULL ) {
		// write out the type so the binary reader knows what to instantiate
		fileOut->WriteString( "interAreaPortals" );
	}


	m_portalAreas = (portalArea_t *)R_ClearedStaticAlloc( m_numPortalAreas * sizeof( m_portalAreas[0] ) );
	m_areaScreenRect = (idScreenRect *) R_ClearedStaticAlloc( m_numPortalAreas * sizeof( idScreenRect ) );

	// set the doubly linked lists
	SetupAreaRefs();

	m_numInterAreaPortals = src->ParseInt();
	if ( m_numInterAreaPortals < 0 ) {
		src->Error(  "R_ParseInterAreaPortals: bad m_numInterAreaPortals" );
		return;
	}

	if ( fileOut != NULL ) {
		fileOut->WriteBig( m_numPortalAreas );
		fileOut->WriteBig( m_numInterAreaPortals );
	}

	m_doublePortals = (doublePortal_t *)R_ClearedStaticAlloc( m_numInterAreaPortals * 
		sizeof( m_doublePortals [0] ) );

	for ( int i = 0; i < m_numInterAreaPortals; i++ ) {
		int		numPoints, a1, a2;
		idWinding	*w;
		portal_t	*p;

		numPoints = src->ParseInt();
		a1 = src->ParseInt();
		a2 = src->ParseInt();

		if ( fileOut != NULL ) {
			fileOut->WriteBig( numPoints );
			fileOut->WriteBig( a1 );
			fileOut->WriteBig( a2 );
		}

		w = new (TAG_RENDER_WINDING) idWinding( numPoints );
		w->SetNumPoints( numPoints );
		for ( int j = 0; j < numPoints; j++ ) {
			src->Parse1DMatrix( 3, (*w)[j].ToFloatPtr() );

			if ( fileOut != NULL ) {
				fileOut->WriteBig( (*w)[j].x );
				fileOut->WriteBig( (*w)[j].y );
				fileOut->WriteBig( (*w)[j].z );
			}
			// no texture coordinates
			(*w)[j][3] = 0;
			(*w)[j][4] = 0;
		}

		// add the portal to a1
		p = (portal_t *)R_ClearedStaticAlloc( sizeof( *p ) );
		p->intoArea = a2;
		p->doublePortal = &m_doublePortals[i];
		p->w = w;
		p->w->GetPlane( p->plane );

		p->next = m_portalAreas[a1].portals;
		m_portalAreas[a1].portals = p;

		m_doublePortals[i].portals[0] = p;

		// reverse it for a2
		p = (portal_t *)R_ClearedStaticAlloc( sizeof( *p ) );
		p->intoArea = a1;
		p->doublePortal = &m_doublePortals[i];
		p->w = w->Reverse();
		p->w->GetPlane( p->plane );

		p->next = m_portalAreas[a2].portals;
		m_portalAreas[a2].portals = p;

		m_doublePortals[i].portals[1] = p;
	}

	src->ExpectTokenString( "}" );
}

/*
================
idRenderWorld::ParseInterAreaPortals
================
*/
void idRenderWorld::ReadBinaryAreaPortals( idFile *file ) {

	file->ReadBig( m_numPortalAreas );
	file->ReadBig( m_numInterAreaPortals );

	m_portalAreas = (portalArea_t *)R_ClearedStaticAlloc( m_numPortalAreas * sizeof( m_portalAreas[0] ) );
	m_areaScreenRect = (idScreenRect *) R_ClearedStaticAlloc( m_numPortalAreas * sizeof( idScreenRect ) );

	// set the doubly linked lists
	SetupAreaRefs();

	m_doublePortals = (doublePortal_t *)R_ClearedStaticAlloc( m_numInterAreaPortals * sizeof( m_doublePortals [0] ) );

	for ( int i = 0; i < m_numInterAreaPortals; i++ ) {
		int		numPoints, a1, a2;
		idWinding	*w;
		portal_t	*p;

		file->ReadBig( numPoints );
		file->ReadBig( a1 );
		file->ReadBig( a2 );
		w = new (TAG_RENDER_WINDING) idWinding( numPoints );
		w->SetNumPoints( numPoints );
		for ( int j = 0; j < numPoints; j++ ) {
			file->ReadBig( (*w)[ j ][ 0 ] );
			file->ReadBig( (*w)[ j ][ 1 ] );
			file->ReadBig( (*w)[ j ][ 2 ] );
			// no texture coordinates
			(*w)[ j ][ 3 ] = 0;
			(*w)[ j ][ 4 ] = 0;
		}

		// add the portal to a1
		p = (portal_t *)R_ClearedStaticAlloc( sizeof( *p ) );
		p->intoArea = a2;
		p->doublePortal = &m_doublePortals[i];
		p->w = w;
		p->w->GetPlane( p->plane );

		p->next = m_portalAreas[a1].portals;
		m_portalAreas[a1].portals = p;

		m_doublePortals[i].portals[0] = p;

		// reverse it for a2
		p = (portal_t *)R_ClearedStaticAlloc( sizeof( *p ) );
		p->intoArea = a1;
		p->doublePortal = &m_doublePortals[i];
		p->w = w->Reverse();
		p->w->GetPlane( p->plane );

		p->next = m_portalAreas[a2].portals;
		m_portalAreas[a2].portals = p;

		m_doublePortals[i].portals[1] = p;
	}
}


/*
================
idRenderWorld::ParseNodes
================
*/
void idRenderWorld::ParseNodes( idLexer *src, idFile *fileOut ) {
	src->ExpectTokenString( "{" );

	m_numAreaNodes = src->ParseInt();
	if ( m_numAreaNodes < 0 ) {
		src->Error( "R_ParseNodes: bad numAreaNodes" );
	}
	m_areaNodes = (areaNode_t *)R_ClearedStaticAlloc( m_numAreaNodes * sizeof( m_areaNodes[0] ) );

	if ( fileOut != NULL ) {
		// write out the type so the binary reader knows what to instantiate
		fileOut->WriteString( "nodes" );
	}

	if ( fileOut != NULL ) {
		fileOut->WriteBig( m_numAreaNodes );
	}

	for ( int i = 0; i < m_numAreaNodes; i++ ) {
		areaNode_t	*node;

		node = &m_areaNodes[i];

		src->Parse1DMatrix( 4, node->plane.ToFloatPtr() );

		node->children[0] = src->ParseInt();
		node->children[1] = src->ParseInt();

		if ( fileOut != NULL ) {
			fileOut->WriteBig( node->plane[ 0 ] );
			fileOut->WriteBig( node->plane[ 1 ] );
			fileOut->WriteBig( node->plane[ 2 ] );
			fileOut->WriteBig( node->plane[ 3 ] );
			fileOut->WriteBig( node->children[ 0 ] );
			fileOut->WriteBig( node->children[ 1 ] );
		}

	}

	src->ExpectTokenString( "}" );
}

/*
================
idRenderWorld::ReadBinaryNodes
================
*/
void idRenderWorld::ReadBinaryNodes( idFile * file ) {
	file->ReadBig( m_numAreaNodes );
	m_areaNodes = (areaNode_t *)R_ClearedStaticAlloc( m_numAreaNodes * sizeof( m_areaNodes[0] ) );
	for ( int i = 0; i < m_numAreaNodes; i++ ) {
		areaNode_t * node = &m_areaNodes[ i ];
		file->ReadBig( node->plane[ 0 ] );
		file->ReadBig( node->plane[ 1 ] );
		file->ReadBig( node->plane[ 2 ] );
		file->ReadBig( node->plane[ 3 ] );
		file->ReadBig( node->children[ 0 ] );
		file->ReadBig( node->children[ 1 ] );
	}
}

/*
================
idRenderWorld::CommonChildrenArea_r
================
*/
int idRenderWorld::CommonChildrenArea_r( areaNode_t *node ) {
	int	nums[2];

	for ( int i = 0; i < 2; i++ ) {
		if ( node->children[i] <= 0 ) {
			nums[i] = -1 - node->children[i];
		} else {
			nums[i] = CommonChildrenArea_r( &m_areaNodes[ node->children[i] ] );
		}
	}

	// solid nodes will match any area
	if ( nums[0] == AREANUM_SOLID ) {
		nums[0] = nums[1];
	}
	if ( nums[1] == AREANUM_SOLID ) {
		nums[1] = nums[0];
	}

	int	common;
	if ( nums[0] == nums[1] ) {
		common = nums[0];
	} else {
		common = CHILDREN_HAVE_MULTIPLE_AREAS;
	}

	node->commonChildrenArea = common;

	return common;
}

/*
=================
idRenderWorld::ClearWorld

Sets up for a single area world
=================
*/
void idRenderWorld::ClearWorld() {
	m_numPortalAreas = 1;
	m_portalAreas = (portalArea_t *)R_ClearedStaticAlloc( sizeof( m_portalAreas[0] ) );
	m_areaScreenRect = (idScreenRect *) R_ClearedStaticAlloc( sizeof( idScreenRect ) );

	SetupAreaRefs();

	// even though we only have a single area, create a node
	// that has both children pointing at it so we don't need to
	//
	m_areaNodes = (areaNode_t *)R_ClearedStaticAlloc( sizeof( m_areaNodes[0] ) );
	m_areaNodes[0].plane[3] = 1;
	m_areaNodes[0].children[0] = -1;
	m_areaNodes[0].children[1] = -1;
}

/*
=================
idRenderWorld::FreeDefs

dump all the interactions
=================
*/
void idRenderWorld::FreeDefs() {
	if ( m_interactionTable ) {
		R_StaticFree( m_interactionTable );
		m_interactionTable = NULL;
	}

	// free all lightDefs
	for ( int i = 0; i < m_lightDefs.Num(); i++ ) {
		idRenderLight * light = m_lightDefs[i];
		if ( light != NULL && light->world == this ) {
			FreeLightDef( i );
			m_lightDefs[i] = NULL;
		}
	}

	// free all entityDefs
	for ( int i = 0; i < m_entityDefs.Num(); i++ ) {
		idRenderEntity	* mod = m_entityDefs[i];
		if ( mod != NULL && mod->world == this ) {
			FreeEntityDef( i );
			m_entityDefs[i] = NULL;
		}
	}

	// Reset decals and overlays
	for ( int i = 0; i < m_decals.Num(); i++ ) {
		m_decals[i].entityHandle = -1;
		m_decals[i].lastStartTime = 0;
	}
	for ( int i = 0; i < m_overlays.Num(); i++ ) {
		m_overlays[i].entityHandle = -1;
		m_overlays[i].lastStartTime = 0;
	}
}

/*
=================
idRenderWorld::InitFromMap

A NULL or empty name will make a world without a map model, which
is still useful for displaying a bare model
=================
*/
bool idRenderWorld::InitFromMap( const char *name ) {
	idLexer *		src;
	idToken			token;
	idRenderModel *	lastModel;

	// if this is an empty world, initialize manually
	if ( !name || !name[0] ) {
		FreeWorld();
		m_mapName.Clear();
		ClearWorld();
		return true;
	}

	// load it
	idStrStatic< MAX_OSPATH > filename = name;
	filename.SetFileExtension( PROC_FILE_EXT );

	// check for generated file
	idStrStatic< MAX_OSPATH > generatedFileName = filename;
	generatedFileName.Insert( "generated/", 0 );
	generatedFileName.SetFileExtension( "bproc" );

	// if we are reloading the same map, check the timestamp
	// and try to skip all the work
	ID_TIME_T currentTimeStamp = fileSystem->GetTimestamp( filename );

	if ( name == m_mapName ) {
		if ( fileSystem->InProductionMode() || ( currentTimeStamp != FILE_NOT_FOUND_TIMESTAMP && currentTimeStamp == m_mapTimeStamp ) ) {
			idLib::Printf( "idRenderWorld::InitFromMap: retaining existing map\n" );
			FreeDefs();
			TouchWorldModels();
			AddWorldModelEntities();
			ClearPortalStates();
			return true;
		}
		idLib::Printf( "idRenderWorld::InitFromMap: timestamp has changed, reloading.\n" );
	}

	FreeWorld();

	// see if we have a generated version of this 
	static const byte BPROC_VERSION = 1;
	static const unsigned int BPROC_MAGIC = ( 'P' << 24 ) | ( 'R' << 16 ) | ( 'O' << 8 ) | BPROC_VERSION;
	bool loaded = false;
	idFileLocal file( fileSystem->OpenFileReadMemory( generatedFileName ) );
	if ( file != NULL ) {
		int numEntries = 0;
		int magic = 0;
		file->ReadBig( magic );
		if ( magic == BPROC_MAGIC ) {
			file->ReadBig( numEntries );
			file->ReadString( m_mapName );
			file->ReadBig( m_mapTimeStamp );
			loaded = true;
			for ( int i = 0; i < numEntries; i++ ) {
				idStrStatic< MAX_OSPATH > type;
				file->ReadString( type );
				type.ToLower();
				if ( type == "model" ) {
					lastModel = ReadBinaryModel( file );
					if ( lastModel == NULL ) {
						loaded = false;
						break;
					}
					renderModelManager->AddModel( lastModel );
					m_localModels.Append( lastModel );
				} else if ( type == "shadowmodel" ) {
					lastModel = ReadBinaryModel( file );
					if ( lastModel == NULL ) {
						loaded = false;
						break;
					}
					renderModelManager->AddModel( lastModel );
					m_localModels.Append( lastModel );
				} else if ( type == "interareaportals" ) {
					ReadBinaryAreaPortals( file );
				} else if ( type == "nodes" ) {
					ReadBinaryNodes( file );
				} else {
					idLib::Error( "Binary proc file failed, unexpected type %s\n", type.c_str() );
				}
			}
		}
	}

	if ( !loaded ) {

		src = new (TAG_RENDER) idLexer( filename, LEXFL_NOSTRINGCONCAT | LEXFL_NODOLLARPRECOMPILE );
		if ( !src->IsLoaded() ) {
			idLib::Printf( "idRenderWorld::InitFromMap: %s not found\n", filename.c_str() );
			ClearWorld();
			return false;
		}


		m_mapName = name;
		m_mapTimeStamp = currentTimeStamp;

		if ( !src->ReadToken( &token ) || token.Icmp( PROC_FILE_ID ) ) {
			idLib::Printf( "idRenderWorld::InitFromMap: bad id '%s' instead of '%s'\n", token.c_str(), PROC_FILE_ID );
			delete src;
			return false;
		}
			
		int numEntries = 0;
		idFileLocal outputFile( fileSystem->OpenFileWrite( generatedFileName, "fs_basepath" ) );
		if ( outputFile != NULL ) {
			int magic = BPROC_MAGIC;
			outputFile->WriteBig( magic );
			outputFile->WriteBig( numEntries );
			outputFile->WriteString( m_mapName );
			outputFile->WriteBig( m_mapTimeStamp );
		}

		// parse the file
		while ( 1 ) {
			if ( !src->ReadToken( &token ) ) {
				break;
			}

			common->UpdateLevelLoadPacifier();


			if ( token == "model" ) {
				lastModel = ParseModel( src, name, currentTimeStamp, outputFile );

				// add it to the model manager list
				renderModelManager->AddModel( lastModel );

				// save it in the list to free when clearing this map
				m_localModels.Append( lastModel );

				numEntries++;

				continue;
			}

			if ( token == "shadowModel" ) {
				lastModel = ParseShadowModel( src, outputFile );

				// add it to the model manager list
				renderModelManager->AddModel( lastModel );

				// save it in the list to free when clearing this map
				m_localModels.Append( lastModel );

				numEntries++;
				continue;
			}

			if ( token == "interAreaPortals" ) {
				ParseInterAreaPortals( src, outputFile );

				numEntries++;
				continue;
			}

			if ( token == "nodes" ) {
				ParseNodes( src, outputFile );

				numEntries++;
				continue;
			}

			src->Error( "idRenderWorld::InitFromMap: bad token \"%s\"", token.c_str() );
		}

		delete src;

		if ( outputFile != NULL ) {
			outputFile->Seek( 0, FS_SEEK_SET );
			int magic = BPROC_MAGIC;
			outputFile->WriteBig( magic );
			outputFile->WriteBig( numEntries );
		}

	}



	// if it was a trivial map without any areas, create a single area
	if ( !m_numPortalAreas ) {
		ClearWorld();
	}

	// find the points where we can early-our of reference pushing into the BSP tree
	CommonChildrenArea_r( &m_areaNodes[0] );

	AddWorldModelEntities();
	ClearPortalStates();

	// done!
	return true;
}

/*
=====================
idRenderWorld::ClearPortalStates
=====================
*/
void idRenderWorld::ClearPortalStates() {
	// all portals start off open
	for ( int i = 0; i < m_numInterAreaPortals; i++ ) {
		m_doublePortals[i].blockingBits = PS_BLOCK_NONE;
	}

	// flood fill all area connections
	for ( int i = 0; i < m_numPortalAreas; i++ ) {
		for ( int j = 0; j < NUM_PORTAL_ATTRIBUTES; j++ ) {
			m_connectedAreaNum++;
			FloodConnectedAreas( &m_portalAreas[i], j );
		}
	}
}

/*
=====================
idRenderWorld::AddWorldModelEntities
=====================
*/
void idRenderWorld::AddWorldModelEntities() {
	// add the world model for each portal area
	// we can't just call AddEntityDef, because that would place the references
	// based on the bounding box, rather than explicitly into the correct area
	for ( int i = 0; i < m_numPortalAreas; i++ ) {
		common->UpdateLevelLoadPacifier();


		idRenderEntity	* def = new (TAG_RENDER_ENTITY) idRenderEntity;

		// try and reuse a free spot
		int index = m_entityDefs.FindNull();
		if ( index == -1 ) {
			index = m_entityDefs.Append(def);
		} else {
			m_entityDefs[index] = def;
		}

		def->index = index;
		def->world = this;

		def->parms.hModel = renderModelManager->FindModel( va("_area%i", i ) );
		if ( def->parms.hModel->IsDefaultModel() || !def->parms.hModel->IsStaticWorldModel() ) {
			idLib::Error( "idRenderWorld::InitFromMap: bad area model lookup" );
		}

		idRenderModel *hModel = def->parms.hModel;

		for ( int j = 0; j < hModel->NumSurfaces(); j++ ) {
			const modelSurface_t *surf = hModel->Surface( j );

			if ( surf->shader->GetName() == idStr( "textures/smf/portal_sky" ) ) {
				def->needsPortalSky = true;
			}
		}

		// the local and global reference bounds are the same for area models
		def->localReferenceBounds = def->parms.hModel->Bounds();
		def->globalReferenceBounds = def->parms.hModel->Bounds();

		def->parms.axis[0][0] = 1.0f;
		def->parms.axis[1][1] = 1.0f;
		def->parms.axis[2][2] = 1.0f;

		// in case an explicit shader is used on the world, we don't
		// want it to have a 0 alpha or color
		def->parms.shaderParms[0] = 1.0f;
		def->parms.shaderParms[1] = 1.0f;
		def->parms.shaderParms[2] = 1.0f;
		def->parms.shaderParms[3] = 1.0f;

		DeriveEntityData( def );

		AddEntityRefToArea( def, &m_portalAreas[i] );
	}
}

/*
=====================
idRenderWorld::CheckAreaForPortalSky
=====================
*/
bool idRenderWorld::CheckAreaForPortalSky( int areaNum ) {
	assert( areaNum >= 0 && areaNum < m_numPortalAreas );

	for ( areaReference_t * ref = m_portalAreas[areaNum].entityRefs.areaNext; ref->entity; ref = ref->areaNext ) {
		assert( ref->area == &m_portalAreas[areaNum] );

		if ( ref->entity && ref->entity->needsPortalSky ) {
			return true;
		}
	}

	return false;
}

/*
=====================
idRenderWorld::ReCreateReferences
=====================
*/
void idRenderWorld::ReCreateReferences() {
	for ( int i = 0; i < m_entityDefs.Num(); ++i ) {
		idRenderEntity * def = m_entityDefs[ i ];
		if ( def == NULL ) {
			continue;
		}
		// the world model entities are put specifically in a single
		// area, instead of just pushing their bounds into the tree
		if ( i < m_numPortalAreas ) {
			AddEntityRefToArea( def, &m_portalAreas[ i ] );
		} else {
			CreateEntityRefs( def );
		}
	}

	for ( int i = 0; i < m_lightDefs.Num(); ++i ) {
		idRenderLight * light = m_lightDefs[ i ];
		if ( light == NULL ) {
			continue;
		}
		renderLight_t parms = light->parms;

		light->world->FreeLightDef( i );
		UpdateLightDef( i, &parms );
	}
}

/*
=====================
idRenderWorld::FreeDerivedData
=====================
*/
void idRenderWorld::FreeDerivedData() {
	for ( int i = 0; i < m_entityDefs.Num(); ++i ) {
		idRenderEntity * def = m_entityDefs[ i ];
		if ( def == NULL ) {
			continue;
		}
		FreeEntityDefDerivedData( def, false, false );
	}

	for ( int i = 0; i < m_lightDefs.Num(); ++i ) {
		idRenderLight * light = m_lightDefs[ i ];
		if ( light == NULL ) {
			continue;
		}
		FreeLightDefDerivedData( light );
	}
}

/*
=====================
idRenderWorld::CheckForEntityDefsUsingModel
=====================
*/
void idRenderWorld::CheckForEntityDefsUsingModel( idRenderModel * model ) {
	for ( int i = 0; i < m_entityDefs.Num(); ++i ) {
		idRenderEntity * def = m_entityDefs[ i ];
		if ( !def ) {
			continue;
		}
		if ( def->parms.hModel == model ) {
			FreeEntityDefDerivedData( def, false, false );
		}
	}
}