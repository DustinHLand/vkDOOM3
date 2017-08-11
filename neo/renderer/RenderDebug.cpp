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
#include "../idlib/precompiled.h"
#include "RenderSystem_local.h"
#include "RenderBackend.h"
#include "Interaction.h"
#include "Image.h"

extern idCVar r_screenFraction;

debugLine_t		rb_debugLines[ MAX_DEBUG_LINES ];
int				rb_numDebugLines = 0;
int				rb_debugLineTime = 0;

debugText_t		rb_debugText[ MAX_DEBUG_TEXT ];
int				rb_numDebugText = 0;
int				rb_debugTextTime = 0;

debugPolygon_t	rb_debugPolygons[ MAX_DEBUG_POLYGONS ];
int				rb_numDebugPolygons = 0;
int				rb_debugPolygonTime = 0;

idImage *		testImage = NULL;
idCinematic *	testVideo = NULL;
int				testVideoStartTime = 0;

/*
================
RB_AddDebugText
================
*/
void RB_AddDebugText( const char *text, const idVec3 &origin, float scale, const idVec4 &color, const idMat3 &viewAxis, const int align, const int lifetime, const bool depthTest ) {
	debugText_t *debugText;

	if ( rb_numDebugText < MAX_DEBUG_TEXT ) {
		debugText = &rb_debugText[ rb_numDebugText++ ];
		debugText->text			= text;
		debugText->origin		= origin;
		debugText->scale		= scale;
		debugText->color		= color;
		debugText->viewAxis		= viewAxis;
		debugText->align		= align;
		debugText->lifeTime		= rb_debugTextTime + lifetime;
		debugText->depthTest	= depthTest;
	}
}

/*
================
RB_ClearDebugText
================
*/
void RB_ClearDebugText( int time ) {
	int			i;
	int			num;
	debugText_t	*text;

	rb_debugTextTime = time;

	if ( !time ) {
		// free up our strings
		text = rb_debugText;
		for ( i = 0; i < MAX_DEBUG_TEXT; i++, text++ ) {
			text->text.Clear();
		}
		rb_numDebugText = 0;
		return;
	}

	// copy any text that still needs to be drawn
	num	= 0;
	text = rb_debugText;
	for ( i = 0; i < rb_numDebugText; i++, text++ ) {
		if ( text->lifeTime > time ) {
			if ( num != i ) {
				rb_debugText[ num ] = *text;
			}
			num++;
		}
	}
	rb_numDebugText = num;
}

/*
================
RB_AddDebugLine
================
*/
void RB_AddDebugLine( const idVec4 &color, const idVec3 &start, const idVec3 &end, const int lifeTime, const bool depthTest ) {
	debugLine_t *line;

	if ( rb_numDebugLines < MAX_DEBUG_LINES ) {
		line = &rb_debugLines[ rb_numDebugLines++ ];
		line->rgb		= color;
		line->start		= start;
		line->end		= end;
		line->depthTest = depthTest;
		line->lifeTime	= rb_debugLineTime + lifeTime;
	}
}

/*
================
RB_ClearDebugLines
================
*/
void RB_ClearDebugLines( int time ) {
	int			i;
	int			num;
	debugLine_t	*line;

	rb_debugLineTime = time;

	if ( !time ) {
		rb_numDebugLines = 0;
		return;
	}

	// copy any lines that still need to be drawn
	num	= 0;
	line = rb_debugLines;
	for ( i = 0; i < rb_numDebugLines; i++, line++ ) {
		if ( line->lifeTime > time ) {
			if ( num != i ) {
				rb_debugLines[ num ] = *line;
			}
			num++;
		}
	}
	rb_numDebugLines = num;
}

/*
================
RB_AddDebugPolygon
================
*/
void RB_AddDebugPolygon( const idVec4 &color, const idWinding &winding, const int lifeTime, const bool depthTest ) {
	debugPolygon_t *poly;

	if ( rb_numDebugPolygons < MAX_DEBUG_POLYGONS ) {
		poly = &rb_debugPolygons[ rb_numDebugPolygons++ ];
		poly->rgb		= color;
		poly->winding	= winding;
		poly->depthTest = depthTest;
		poly->lifeTime	= rb_debugPolygonTime + lifeTime;
	}
}

/*
================
RB_ClearDebugPolygons
================
*/
void RB_ClearDebugPolygons( int time ) {
	int				i;
	int				num;
	debugPolygon_t	*poly;

	rb_debugPolygonTime = time;

	if ( !time ) {
		rb_numDebugPolygons = 0;
		return;
	}

	// copy any polygons that still need to be drawn
	num	= 0;

	poly = rb_debugPolygons;
	for ( i = 0; i < rb_numDebugPolygons; i++, poly++ ) {
		if ( poly->lifeTime > time ) {
			if ( num != i ) {
				rb_debugPolygons[ num ] = *poly;
			}
			num++;
		}
	}
	rb_numDebugPolygons = num;
}

/*
=================
RB_ShutdownDebugTools
=================
*/
void RB_ShutdownDebugTools() {
	for ( int i = 0; i < MAX_DEBUG_POLYGONS; i++ ) {
		rb_debugPolygons[i].winding.Clear();
	}
}

/*
================== 
idRenderSystemLocal::TakeScreenshot

Move to tr_imagefiles.c...

Downsample is the number of steps to mipmap the image before saving it
If ref == NULL, common->UpdateScreen will be used
================== 
*/  
void idRenderSystemLocal::TakeScreenshot( int width, int height, const char *fileName, int blends, renderView_t *ref ) {
	byte		*buffer;
	int			i, j, c, temp;

	m_takingScreenshot = true;

	const int pix = width * height;
	const int bufferSize = pix * 3 + 18;

	buffer = (byte *)R_StaticAlloc( bufferSize );
	memset( buffer, 0, bufferSize );

	if ( blends <= 1 ) {
		ReadTiledPixels( width, height, buffer + 18, ref );
	} else {
		unsigned short *shortBuffer = (unsigned short *)R_StaticAlloc(pix*2*3);
		memset (shortBuffer, 0, pix*2*3);

		for ( i = 0 ; i < blends ; i++ ) {
			ReadTiledPixels( width, height, buffer + 18, ref );

			for ( j = 0 ; j < pix*3 ; j++ ) {
				shortBuffer[j] += buffer[18+j];
			}
		}

		// divide back to bytes
		for ( i = 0 ; i < pix*3 ; i++ ) {
			buffer[18+i] = shortBuffer[i] / blends;
		}

		R_StaticFree( shortBuffer );
	}

	// fill in the header (this is vertically flipped, which qglReadPixels emits)
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;	// pixel size

	// swap rgb to bgr
	c = 18 + width * height * 3;
	for (i=18 ; i<c ; i+=3) {
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}

	fileSystem->WriteFile( fileName, buffer, c );

	R_StaticFree( buffer );

	m_takingScreenshot = false;
}

/*
=================
idRenderSystemLocal::ReloadSurface
=================
*/
void idRenderSystemLocal::ReloadSurface() {
	modelTrace_t mt;
	idVec3 start, end;
	
	// start far enough away that we don't hit the player model
	start = primaryView->renderView.vieworg + primaryView->renderView.viewaxis[0] * 16;
	end = start + primaryView->renderView.viewaxis[0] * 1000.0f;
	if ( !primaryWorld->Trace( mt, start, end, 0.0f, false ) ) {
		return;
	}

	idLib::Printf( "Reloading %s\n", mt.material->GetName() );

	// reload the decl
	mt.material->base->Reload();

	// reload any images used by the decl
	mt.material->ReloadImages( false );
}

/*
==============
idRenderSystemLocal::PrintMemInfo
==============
*/
void idRenderSystemLocal::PrintMemInfo( MemInfo_t *mi ) {
	// sum up image totals
	globalImages->PrintMemInfo( mi );

	// sum up model totals
	renderModelManager->PrintMemInfo( mi );

	// compute render totals

}

/*
=================
idRenderSystemLocal::PrintRenderEntityDefs
=================
*/
void idRenderSystemLocal::PrintRenderEntityDefs() {
	if ( primaryWorld ) {
		primaryWorld->PrintRenderEntityDefs();
	}
}

/*
=================
idRenderSystemLocal::PrintRenderLightDefs
=================
*/
void idRenderSystemLocal::PrintRenderLightDefs() {
	if ( primaryWorld ) {
		primaryWorld->PrintRenderLightDefs();
	}
}

CONSOLE_COMMAND( sizeUp, "makes the rendered view larger", NULL ) {
	if ( r_screenFraction.GetInteger() + 10 > 100 ) {
		r_screenFraction.SetInteger( 100 );
	} else {
		r_screenFraction.SetInteger( r_screenFraction.GetInteger() + 10 );
	}
}

CONSOLE_COMMAND( sizeDown, "makes the rendered view smaller", NULL ) {
	if ( r_screenFraction.GetInteger() - 10 < 10 ) {
		r_screenFraction.SetInteger( 10 );
	} else {
		r_screenFraction.SetInteger( r_screenFraction.GetInteger() - 10 );
	}
}

/*
================,
reloadGuis

Reloads any guis that have had their file timestamps changed.
An optional "all" parameter will cause all models to reload, even
if they are not out of date.

Should we also reload the map models?
================
*/
CONSOLE_COMMAND( reloadGuis, "reloads guis", NULL ) {
	bool all;

	if ( !idStr::Icmp( args.Argv(1), "all" ) ) {
		all = true;
		idLib::Printf( "Reloading all gui files...\n" );
	} else {
		all = false;
		idLib::Printf( "Checking for changed gui files...\n" );
	}

	uiManager->Reload( all );
}

CONSOLE_COMMAND( listGuis, "lists guis", NULL ) {
	uiManager->ListGuis();
}

CONSOLE_COMMAND( touchGui, "touches a gui", NULL ) {
	const char	*gui = args.Argv( 1 );

	if ( !gui[0] ) {
		idLib::Printf( "USAGE: touchGui <guiName>\n" );
		return;
	}

	idLib::Printf( "touchGui %s\n", gui );
	common->UpdateScreen();
	uiManager->Touch( gui );
}

/* 
================== 
R_ScreenshotFilename

Returns a filename with digits appended
if we have saved a previous screenshot, don't scan
from the beginning, because recording demo avis can involve
thousands of shots
================== 
*/  
void R_ScreenshotFilename( int &lastNumber, const char *base, idStr &fileName ) {
	int	a,b,c,d, e;

	lastNumber++;
	if ( lastNumber > 99999 ) {
		lastNumber = 99999;
	}
	for ( ; lastNumber < 99999 ; lastNumber++ ) {
		int	frac = lastNumber;

		a = frac / 10000;
		frac -= a*10000;
		b = frac / 1000;
		frac -= b*1000;
		c = frac / 100;
		frac -= c*100;
		d = frac / 10;
		frac -= d*10;
		e = frac;

		sprintf( fileName, "%s%i%i%i%i%i.tga", base, a, b, c, d, e );
		if ( lastNumber == 99999 ) {
			break;
		}
		int len = fileSystem->ReadFile( fileName, NULL, NULL );
		if ( len <= 0 ) {
			break;
		}
		// check again...
	}
}

/*
================== 
screenshot

screenshot
screenshot [filename]
screenshot [width] [height]
screenshot [width] [height] [samples]
================== 
*/ 
#define	MAX_BLENDS	256	// to keep the accumulation in shorts
CONSOLE_COMMAND( screenshot, "takes a screenshot", NULL ) {
	static int lastNumber = 0;
	idStr checkname;

	int width = renderSystem->GetWidth();
	int height = renderSystem->GetHeight();
	int	blends = 0;

	switch ( args.Argc() ) {
	case 1:
		width = renderSystem->GetWidth();
		height = renderSystem->GetHeight();
		blends = 1;
		R_ScreenshotFilename( lastNumber, "screenshots/shot", checkname );
		break;
	case 2:
		width = renderSystem->GetWidth();
		height = renderSystem->GetHeight();
		blends = 1;
		checkname = args.Argv( 1 );
		break;
	case 3:
		width = atoi( args.Argv( 1 ) );
		height = atoi( args.Argv( 2 ) );
		blends = 1;
		R_ScreenshotFilename( lastNumber, "screenshots/shot", checkname );
		break;
	case 4:
		width = atoi( args.Argv( 1 ) );
		height = atoi( args.Argv( 2 ) );
		blends = atoi( args.Argv( 3 ) );
		if ( blends < 1 ) {
			blends = 1;
		}
		if ( blends > MAX_BLENDS ) {
			blends = MAX_BLENDS;
		}
		R_ScreenshotFilename( lastNumber, "screenshots/shot", checkname );
		break;
	default:
		idLib::Printf( "usage: screenshot\n       screenshot <filename>\n       screenshot <width> <height>\n       screenshot <width> <height> <blends>\n" );
		return;
	}

	// put the console away
	console->Close();

	tr.TakeScreenshot( width, height, checkname, blends, NULL );

	idLib::Printf( "Wrote %s\n", checkname.c_str() );
}

/*
==================
R_SampleCubeMap
==================
*/
static idMat3 cubeAxis[6];
void R_SampleCubeMap( const idVec3 &dir, int size, byte *buffers[6], byte result[4] ) {
	float	adir[3];
	int		axis, x, y;

	adir[0] = fabs(dir[0]);
	adir[1] = fabs(dir[1]);
	adir[2] = fabs(dir[2]);

	if ( dir[0] >= adir[1] && dir[0] >= adir[2] ) {
		axis = 0;
	} else if ( -dir[0] >= adir[1] && -dir[0] >= adir[2] ) {
		axis = 1;
	} else if ( dir[1] >= adir[0] && dir[1] >= adir[2] ) {
		axis = 2;
	} else if ( -dir[1] >= adir[0] && -dir[1] >= adir[2] ) {
		axis = 3;
	} else if ( dir[2] >= adir[1] && dir[2] >= adir[2] ) {
		axis = 4;
	} else {
		axis = 5;
	}

	float	fx = (dir * cubeAxis[axis][1]) / (dir * cubeAxis[axis][0]);
	float	fy = (dir * cubeAxis[axis][2]) / (dir * cubeAxis[axis][0]);

	fx = -fx;
	fy = -fy;
	x = size * 0.5 * (fx + 1);
	y = size * 0.5 * (fy + 1);
	if ( x < 0 ) {
		x = 0;
	} else if ( x >= size ) {
		x = size-1;
	}
	if ( y < 0 ) {
		y = 0;
	} else if ( y >= size ) {
		y = size-1;
	}

	result[0] = buffers[axis][(y*size+x)*4+0];
	result[1] = buffers[axis][(y*size+x)*4+1];
	result[2] = buffers[axis][(y*size+x)*4+2];
	result[3] = buffers[axis][(y*size+x)*4+3];
}

/* 
================== 
makeAmbientMap

makeAmbientMap <basename> [size]

Saves out env/<basename>_amb_ft.tga, etc
================== 
*/
CONSOLE_COMMAND( makeAmbientMap, "makes an ambient map", NULL ) {
	idStr fullname;
	const char	*baseName;
	int			i;
	renderView_t	ref;
	viewDef_t	primary;
	int			downSample;
	char	*extensions[6] =  { "_px.tga", "_nx.tga", "_py.tga", "_ny.tga", 
		"_pz.tga", "_nz.tga" };
	int			outSize;
	byte		*buffers[6];
	int			width = 0, height = 0;

	if ( args.Argc() != 2 && args.Argc() != 3 ) {
		idLib::Printf( "USAGE: ambientshot <basename> [size]\n" );
		return;
	}
	baseName = args.Argv( 1 );

	downSample = 0;
	if ( args.Argc() == 3 ) {
		outSize = atoi( args.Argv( 2 ) );
	} else {
		outSize = 32;
	}

	memset( &cubeAxis, 0, sizeof( cubeAxis ) );
	cubeAxis[0][0][0] = 1;
	cubeAxis[0][1][2] = 1;
	cubeAxis[0][2][1] = 1;

	cubeAxis[1][0][0] = -1;
	cubeAxis[1][1][2] = -1;
	cubeAxis[1][2][1] = 1;

	cubeAxis[2][0][1] = 1;
	cubeAxis[2][1][0] = -1;
	cubeAxis[2][2][2] = -1;

	cubeAxis[3][0][1] = -1;
	cubeAxis[3][1][0] = -1;
	cubeAxis[3][2][2] = 1;

	cubeAxis[4][0][2] = 1;
	cubeAxis[4][1][0] = -1;
	cubeAxis[4][2][1] = 1;

	cubeAxis[5][0][2] = -1;
	cubeAxis[5][1][0] = 1;
	cubeAxis[5][2][1] = 1;

	// read all of the images
	for ( i = 0 ; i < 6 ; i++ ) {
		sprintf( fullname, "env/%s%s", baseName, extensions[i] );
		idLib::Printf( "loading %s\n", fullname.c_str() );
		common->UpdateScreen();
		R_LoadImage( fullname, &buffers[i], &width, &height, NULL, true );
		if ( !buffers[i] ) {
			idLib::Printf( "failed.\n" );
			for ( i-- ; i >= 0 ; i-- ) {
				Mem_Free( buffers[i] );
			}
			return;
		}
	}

	// resample with hemispherical blending
	int	samples = 1000;

	byte	*outBuffer = (byte *)_alloca( outSize * outSize * 4 );

	for ( int map = 0 ; map < 2 ; map++ ) {
		for ( i = 0 ; i < 6 ; i++ ) {
			for ( int x = 0 ; x < outSize ; x++ ) {
				for ( int y = 0 ; y < outSize ; y++ ) {
					idVec3	dir;
					float	total[3];

					dir = cubeAxis[i][0] + -( -1 + 2.0*x/(outSize-1) ) * cubeAxis[i][1] + -( -1 + 2.0*y/(outSize-1) ) * cubeAxis[i][2];
					dir.Normalize();
					total[0] = total[1] = total[2] = 0;
	//samples = 1;
					float	limit = map ? 0.95 : 0.25;		// small for specular, almost hemisphere for ambient

					for ( int s = 0 ; s < samples ; s++ ) {
						// pick a random direction vector that is inside the unit sphere but not behind dir,
						// which is a robust way to evenly sample a hemisphere
						idVec3	test;
						while( 1 ) {
							for ( int j = 0 ; j < 3 ; j++ ) {
								test[j] = -1 + 2 * (rand()&0x7fff)/(float)0x7fff;
							}
							if ( test.Length() > 1.0 ) {
								continue;
							}
							test.Normalize();
							if ( test * dir > limit ) {	// don't do a complete hemisphere
								break;
							}
						}
						byte	result[4];
	//test = dir;
						R_SampleCubeMap( test, width, buffers, result );
						total[0] += result[0];
						total[1] += result[1];
						total[2] += result[2];
					}
					outBuffer[(y*outSize+x)*4+0] = total[0] / samples;
					outBuffer[(y*outSize+x)*4+1] = total[1] / samples;
					outBuffer[(y*outSize+x)*4+2] = total[2] / samples;
					outBuffer[(y*outSize+x)*4+3] = 255;
				}
			}

			if ( map == 0 ) {
				sprintf( fullname, "env/%s_amb%s", baseName, extensions[i] );
			} else {
				sprintf( fullname, "env/%s_spec%s", baseName, extensions[i] );
			}
			idLib::Printf( "writing %s\n", fullname.c_str() );
			common->UpdateScreen();
			R_WriteTGA( fullname, outBuffer, outSize, outSize );
		}
	}

	for ( i = 0 ; i < 6 ; i++ ) {
		if ( buffers[i] ) {
			Mem_Free( buffers[i] );
		}
	}
}

/*
====================
modulateLights

Modifies the shaderParms on all the lights so the level
designers can easily test different color schemes
====================
*/
CONSOLE_COMMAND( modulateLights, "modifies shader parms on all lights", NULL ) {
	if ( !tr.primaryWorld ) {
		return;
	}
	if ( args.Argc() != 4 ) {
		idLib::Printf( "usage: modulateLights <redFloat> <greenFloat> <blueFloat>\n" );
		return;
	}

	float modulate[3];
	for ( int i = 0; i < 3; i++ ) {
		modulate[i] = atof( args.Argv( i+1 ) );
	}

	int count = 0;
	for ( int i = 0; i < tr.primaryWorld->m_lightDefs.Num(); i++ ) {
		idRenderLight * light = tr.primaryWorld->m_lightDefs[i];
		if ( light != NULL ) {
			count++;
			for ( int j = 0; j < 3; j++ ) {
				light->parms.shaderParms[j] *= modulate[j];
			}
		}
	}
	idLib::Printf( "modulated %i lights\n", count );
}

/*
=============
testImage

Display the given image centered on the screen.
testimage <number>
testimage <filename>
=============
*/
CONSOLE_COMMAND( testImage, "displays the given image centered on screen", idCmdSystem::ArgCompletion_ImageName ) {
	int imageNum;

	if ( testVideo ) {
		delete testVideo;
		testVideo = NULL;
	}
	testImage = NULL;

	if ( args.Argc() != 2 ) {
		return;
	}

	if ( idStr::IsNumeric( args.Argv(1) ) ) {
		imageNum = atoi( args.Argv(1) );
		if ( imageNum >= 0 && imageNum < globalImages->m_images.Num() ) {
			testImage = globalImages->m_images[imageNum];
		}
	} else {
		testImage = globalImages->ImageFromFile( args.Argv( 1 ), TF_DEFAULT, TR_REPEAT, TD_DEFAULT );
	}
}

/*
=============
testVideo

Plays the cinematic file in a testImage
=============
*/
CONSOLE_COMMAND( testVideo, "displays the given cinematic", idCmdSystem::ArgCompletion_VideoName ) {
	if ( testVideo ) {
		delete testVideo;
		testVideo = NULL;
	}
	testImage = NULL;

	if ( args.Argc() < 2 ) {
		return;
	}

	testImage = globalImages->ImageFromFile( "_scratch", TF_DEFAULT, TR_REPEAT, TD_DEFAULT );
	testVideo = idCinematic::Alloc();
	testVideo->InitFromFile( args.Argv( 1 ), true );

	cinData_t	cin;
	cin = testVideo->ImageForTime( 0 );
	if ( cin.imageY == NULL ) {
		delete testVideo;
		testVideo = NULL;
		testImage = NULL;
		return;
	}

	idLib::Printf( "%i x %i images\n", cin.imageWidth, cin.imageHeight );

	int	len = testVideo->AnimationLength();
	idLib::Printf( "%5.1f seconds of video\n", len * 0.001 );

	testVideoStartTime = tr.m_primaryRenderView.time[1];

	// try to play the matching wav file
	idStr	wavString = args.Argv( ( args.Argc() == 2 ) ? 1 : 2 );
	wavString.StripFileExtension();
	wavString = wavString + ".wav";
	common->SW()->PlayShaderDirectly( wavString.c_str() );
}


/*
===================
R_QsortSurfaceAreas
===================
*/
static int R_QsortSurfaceAreas( const void *a, const void *b ) {
	const idMaterial	*ea, *eb;
	int	ac, bc;

	ea = *(idMaterial **)a;
	if ( !ea->EverReferenced() ) {
		ac = 0;
	} else {
		ac = ea->GetSurfaceArea();
	}
	eb = *(idMaterial **)b;
	if ( !eb->EverReferenced() ) {
		bc = 0;
	} else {
		bc = eb->GetSurfaceArea();
	}

	if ( ac < bc ) {
		return -1;
	}
	if ( ac > bc ) {
		return 1;
	}

	return idStr::Icmp( ea->GetName(), eb->GetName() );
}

/*
===================
reportSurfaceAreas

Prints a list of the materials sorted by surface area
===================
*/
CONSOLE_COMMAND( reportSurfaceAreas, "lists all used materials sorted by surface area", NULL ) {
	unsigned int		i;
	idMaterial	**list;

	const unsigned int count = declManager->GetNumDecls( DECL_MATERIAL );
	if ( count == 0 ) {
		return;
	}

	list = (idMaterial **)_alloca( count * sizeof( *list ) );

	for ( i = 0 ; i < count ; i++ ) {
		list[i] = (idMaterial *)declManager->DeclByIndex( DECL_MATERIAL, i, false );
	}

	qsort( list, count, sizeof( list[0] ), R_QsortSurfaceAreas );

	// skip over ones with 0 area
	for ( i = 0 ; i < count ; i++ ) {
		if ( list[i]->GetSurfaceArea() > 0 ) {
			break;
		}
	}

	for ( ; i < count ; i++ ) {
		// report size in "editor blocks"
		int	blocks = list[i]->GetSurfaceArea() / 4096.0;
		idLib::Printf( "%7i %s\n", blocks, list[i]->GetName() );
	}
}

CONSOLE_COMMAND( showInteractionMemory, "shows memory used by interactions", NULL ) {
	int entities = 0;
	int interactions = 0;
	int deferredInteractions = 0;
	int emptyInteractions = 0;
	int lightTris = 0;
	int lightTriIndexes = 0;
	int shadowTris = 0;
	int shadowTriIndexes = 0;
	int maxInteractionsForEntity = 0;
	int maxInteractionsForLight = 0;

	for ( int i = 0; i < tr.primaryWorld->m_lightDefs.Num(); i++ ) {
		idRenderLight * light = tr.primaryWorld->m_lightDefs[i];
		if ( light == NULL ) {
			continue;
		}
		int numInteractionsForLight = 0;
		for ( idInteraction *inter = light->firstInteraction; inter != NULL; inter = inter->lightNext ) {
			if ( !inter->IsEmpty() ) {
				numInteractionsForLight++;
			}
		}
		if ( numInteractionsForLight > maxInteractionsForLight ) {
			maxInteractionsForLight = numInteractionsForLight;
		}
	}

	for ( int i = 0; i < tr.primaryWorld->m_entityDefs.Num(); i++ ) {
		idRenderEntity	*def = tr.primaryWorld->m_entityDefs[i];
		if ( def == NULL ) {
			continue;
		}
		if ( def->firstInteraction == NULL ) {
			continue;
		}
		entities++;

		int numInteractionsForEntity = 0;
		for ( idInteraction *inter = def->firstInteraction; inter != NULL; inter = inter->entityNext ) {
			interactions++;

			if ( !inter->IsEmpty() ) {
				numInteractionsForEntity++;
			}

			if ( inter->IsDeferred() ) {
				deferredInteractions++;
				continue;
			}
			if ( inter->IsEmpty() ) {
				emptyInteractions++;
				continue;
			}

			for ( int j = 0; j < inter->numSurfaces; j++ ) {
				surfaceInteraction_t *srf = &inter->surfaces[j];

				if ( srf->numLightTrisIndexes ) {
					lightTris++;
					lightTriIndexes += srf->numLightTrisIndexes;
				}

				if ( srf->numShadowIndexes ) {
					shadowTris++;
					shadowTriIndexes += srf->numShadowIndexes;
				}
			}
		}
		if ( numInteractionsForEntity > maxInteractionsForEntity ) {
			maxInteractionsForEntity = numInteractionsForEntity;
		}
	}

	idLib::Printf( "%i entities with %i total interactions\n", entities, interactions );
	idLib::Printf( "%i deferred interactions, %i empty interactions\n", deferredInteractions, emptyInteractions );
	idLib::Printf( "%5i indexes in %5i light tris\n", lightTriIndexes, lightTris );
	idLib::Printf( "%5i indexes in %5i shadow tris\n", shadowTriIndexes, shadowTris );
	idLib::Printf( "%i maxInteractionsForEntity\n", maxInteractionsForEntity );
	idLib::Printf( "%i maxInteractionsForLight\n", maxInteractionsForLight );
}

CONSOLE_COMMAND( vid_restart, "restarts renderSystem", NULL ) {
	renderSystem->VidRestart();
}

CONSOLE_COMMAND( listRenderEntityDefs, "lists the entity defs", NULL ) {
	renderSystem->PrintRenderEntityDefs();
}

CONSOLE_COMMAND( listRenderLightDefs, "lists the light defs", NULL ) {
	renderSystem->PrintRenderLightDefs();
}

bool R_GetModeListForDisplay( const int displayNum, idList<vidMode_t> & modeList );
CONSOLE_COMMAND( listModes, "lists all video modes", NULL ) {
	for ( int displayNum = 0 ; ; displayNum++ ) {
		idList<vidMode_t> modeList;
		if ( !R_GetModeListForDisplay( displayNum, modeList ) ) {
			break;
		}
		for ( int i = 0; i < modeList.Num() ; i++ ) {
			idLib::Printf( "Monitor %i, mode %3i: %4i x %4i @ %ihz\n", displayNum+1, i, modeList[i].width, modeList[i].height, modeList[i].displayHz );
		}
	}
}

/*
=====================
reloadSurface

Reload the material displayed by r_showSurfaceInfo
=====================
*/
CONSOLE_COMMAND( reloadSurface, "reloads the decl and images for selected surface", NULL ) {
	renderSystem->ReloadSurface();
}