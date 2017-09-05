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
#include "RenderLog.h"
#include "Image.h"

static const char * const formatStrings[] = {
	ASSERT_ENUM_STRING( FMT_NONE, 0 ),
	ASSERT_ENUM_STRING( FMT_RGBA8, 1 ),
	ASSERT_ENUM_STRING( FMT_XRGB8, 2 ),
	ASSERT_ENUM_STRING( FMT_ALPHA, 3 ),
	ASSERT_ENUM_STRING( FMT_L8A8, 4 ),
	ASSERT_ENUM_STRING( FMT_LUM8, 5 ),
	ASSERT_ENUM_STRING( FMT_INT8, 6 ),
	ASSERT_ENUM_STRING( FMT_DXT1, 7 ),
	ASSERT_ENUM_STRING( FMT_DXT5, 8 ),
	ASSERT_ENUM_STRING( FMT_DEPTH, 9 ),
	ASSERT_ENUM_STRING( FMT_X16, 10 ),
	ASSERT_ENUM_STRING( FMT_Y16_X16, 11 ),
	ASSERT_ENUM_STRING( FMT_RGB565, 12 ),
};

/*
================
BitsForFormat
================
*/
int BitsForFormat( textureFormat_t format ) {
	switch ( format ) {
		case FMT_NONE:		return 0;
		case FMT_RGBA8:		return 32;
		case FMT_XRGB8:		return 32;
		case FMT_RGB565:	return 16;
		case FMT_L8A8:		return 16;
		case FMT_ALPHA:		return 8;
		case FMT_LUM8:		return 8;
		case FMT_INT8:		return 8;
		case FMT_DXT1:		return 4;
		case FMT_DXT5:		return 8;
		case FMT_DEPTH:		return 32;
		case FMT_X16:		return 16;
		case FMT_Y16_X16:	return 32;
		default:
			assert( 0 );
			return 0;
	}
}

/*
========================
idImage::DeriveOpts
========================
*/
ID_INLINE void idImage::DeriveOpts() {

	if ( m_opts.format == FMT_NONE ) {
		m_opts.colorFormat = CFM_DEFAULT;

		switch ( m_opts.usage ) {
			case TD_COVERAGE:
				m_opts.format = FMT_DXT1;
				m_opts.colorFormat = CFM_GREEN_ALPHA;
				break;
			case TD_DEPTH:
				m_opts.format = FMT_DEPTH;
				break;
			case TD_TARGET:
				m_opts.format = FMT_RGBA8;
				break;
			case TD_DIFFUSE: 
				// TD_DIFFUSE gets only set to when its a diffuse texture for an interaction
				m_opts.gammaMips = true;
				m_opts.format = FMT_DXT5;
				m_opts.colorFormat = CFM_YCOCG_DXT5;
				break;
			case TD_SPECULAR:
				m_opts.gammaMips = true;
				m_opts.format = FMT_DXT1;
				m_opts.colorFormat = CFM_DEFAULT;
				break;
			case TD_DEFAULT:
				m_opts.gammaMips = true;
				m_opts.format = FMT_DXT5;
				m_opts.colorFormat = CFM_DEFAULT;
				break;
			case TD_BUMP:
				m_opts.format = FMT_DXT5;
				m_opts.colorFormat = CFM_NORMAL_DXT5;
				break;
			case TD_FONT:
				m_opts.format = FMT_DXT1;
				m_opts.colorFormat = CFM_GREEN_ALPHA;
				m_opts.numLevels = 4; // We only support 4 levels because we align to 16 in the exporter
				m_opts.gammaMips = true;
				break;
			case TD_LIGHT:
				m_opts.format = FMT_RGB565;
				m_opts.gammaMips = true;
				break;
			case TD_LOOKUP_TABLE_MONO:
				m_opts.format = FMT_INT8;
				break;
			case TD_LOOKUP_TABLE_ALPHA:
				m_opts.format = FMT_ALPHA;
				break;
			case TD_LOOKUP_TABLE_RGB1:
			case TD_LOOKUP_TABLE_RGBA:
				m_opts.format = FMT_RGBA8;
				break;
			default:
				assert( false );
				m_opts.format = FMT_RGBA8;
		}
	}

	if ( m_opts.numLevels == 0 ) {
		m_opts.numLevels = 1;

		if ( m_filter == TF_LINEAR || m_filter == TF_NEAREST ) {
			// don't create mip maps if we aren't going to be using them
		} else {
			int	temp_width = m_opts.width;
			int	temp_height = m_opts.height;
			while ( temp_width > 1 || temp_height > 1 ) {
				temp_width >>= 1;
				temp_height >>= 1;
				if ( ( m_opts.format == FMT_DXT1 || m_opts.format == FMT_DXT5 ) &&
					( ( temp_width & 0x3 ) != 0 || ( temp_height & 0x3 ) != 0 ) ) {
						break;
				}
				m_opts.numLevels++;
			}
		}
	}
}

/*
========================
idImage::AllocImage
========================
*/
void idImage::AllocImage( const idImageOpts & imgOpts, textureFilter_t tf, textureRepeat_t tr ) {
	m_filter = tf;
	m_repeat = tr;
	m_opts = imgOpts;
	DeriveOpts();
	AllocImage();
}

/*
===============
GetGeneratedName

name contains GetName() upon entry
===============
*/
 void idImage::GetGeneratedName( idStr &_name, const textureUsage_t &_usage, const cubeFiles_t &_cube ) {
	idStrStatic< 64 > extension;

	_name.ExtractFileExtension( extension );
	_name.StripFileExtension();

	_name += va( "#__%02d%02d", (int)_usage, (int)_cube );
	if ( extension.Length() > 0 ) {
		_name.SetFileExtension( extension );
	}
}


/*
===============
ActuallyLoadImage

Absolutely every image goes through this path
On exit, the idImage will have a valid OpenGL texture number that can be bound
===============
*/
void idImage::ActuallyLoadImage( bool fromBackEnd ) {
	// this is the ONLY place m_generatorFunction will ever be called
	if ( m_generatorFunction ) {
		m_generatorFunction( this );
		return;
	}

	if ( com_productionMode.GetInteger() != 0 ) {
		m_sourceFileTime = FILE_NOT_FOUND_TIMESTAMP;
		if ( m_cubeFiles != CF_2D ) {
			m_opts.textureType = TT_CUBIC;
			m_repeat = TR_CLAMP;
		}
	} else {
		if ( m_cubeFiles != CF_2D ) {
			m_opts.textureType = TT_CUBIC;
			m_repeat = TR_CLAMP;
			R_LoadCubeImages( GetName(), m_cubeFiles, NULL, NULL, &m_sourceFileTime );
		} else {
			m_opts.textureType = TT_2D;
			R_LoadImageProgram( GetName(), NULL, NULL, NULL, &m_sourceFileTime, &m_opts.usage );
		}
	}

	// Figure out m_opts.colorFormat and m_opts.format so we can make sure the binary image is up to date
	DeriveOpts();

	idStrStatic< MAX_OSPATH > generatedName = GetName();
	GetGeneratedName( generatedName, m_opts.usage, m_cubeFiles );

	idBinaryImage im( generatedName );
	m_binaryFileTime = im.LoadFromGeneratedFile( m_sourceFileTime );

	// BFHACK, do not want to tweak on buildgame so catch these images here
	if ( m_binaryFileTime == FILE_NOT_FOUND_TIMESTAMP && fileSystem->UsingResourceFiles() ) {
		int c = 1;
		while ( c-- > 0 ) {
			if ( generatedName.Find( "guis/assets/white#__0000", false ) >= 0 ) {
				generatedName.Replace( "white#__0000", "white#__0200" );
				im.SetName( generatedName );
				m_binaryFileTime = im.LoadFromGeneratedFile( m_sourceFileTime );
				break;
			}
			if ( generatedName.Find( "guis/assets/white#__0100", false ) >= 0 ) {
				generatedName.Replace( "white#__0100", "white#__0200" );
				im.SetName( generatedName );
				m_binaryFileTime = im.LoadFromGeneratedFile( m_sourceFileTime );
				break;
			}
			if ( generatedName.Find( "textures/black#__0100", false ) >= 0 ) {
				generatedName.Replace( "black#__0100", "black#__0200" );
				im.SetName( generatedName );
				m_binaryFileTime = im.LoadFromGeneratedFile( m_sourceFileTime );
				break;
			}
			if ( generatedName.Find( "textures/decals/bulletglass1_d#__0100", false ) >= 0 ) {
				generatedName.Replace( "bulletglass1_d#__0100", "bulletglass1_d#__0200" );
				im.SetName( generatedName );
				m_binaryFileTime = im.LoadFromGeneratedFile( m_sourceFileTime );
				break;
			}
			if ( generatedName.Find( "models/monsters/skeleton/skeleton01_d#__1000", false ) >= 0 ) {
				generatedName.Replace( "skeleton01_d#__1000", "skeleton01_d#__0100" );
				im.SetName( generatedName );
				m_binaryFileTime = im.LoadFromGeneratedFile( m_sourceFileTime );
				break;
			}
		}
	}
	const bimageFile_t & header = im.GetFileHeader();

	if ( ( fileSystem->InProductionMode() && m_binaryFileTime != FILE_NOT_FOUND_TIMESTAMP ) || ( ( m_binaryFileTime != FILE_NOT_FOUND_TIMESTAMP )
		&& ( header.colorFormat == m_opts.colorFormat )
		&& ( header.format == m_opts.format )
		&& ( header.textureType == m_opts.textureType )
		) ) {
		m_opts.width = header.width;
		m_opts.height = header.height;
		m_opts.numLevels = header.numLevels;
		m_opts.colorFormat = (textureColor_t)header.colorFormat;
		m_opts.format = (textureFormat_t)header.format;
		m_opts.textureType = (textureType_t)header.textureType;
		if ( cvarSystem->GetCVarBool( "fs_buildresources" ) ) {
			// for resource gathering write this image to the preload file for this map
			fileSystem->AddImagePreload( GetName(), m_filter, m_repeat, m_opts.usage, m_cubeFiles );
		}
	} else {
		if ( m_cubeFiles != CF_2D ) {
			int size;
			byte * pics[6];

			if ( !R_LoadCubeImages( GetName(), m_cubeFiles, pics, &size, &m_sourceFileTime ) || size == 0 ) {
				idLib::Warning( "Couldn't load cube image: %s", GetName() );
				return;
			}

			m_opts.textureType = TT_CUBIC;
			m_repeat = TR_CLAMP;
			m_opts.width = size;
			m_opts.height = size;
			m_opts.numLevels = 0;
			DeriveOpts();
			im.LoadCubeFromMemory( size, (const byte **)pics, m_opts.numLevels, m_opts.format, m_opts.gammaMips );
			m_repeat = TR_CLAMP;

			for ( int i = 0; i < 6; i++ ) {
				if ( pics[i] ) {
					Mem_Free( pics[i] );
				}
			}
		} else {
			int width, height;
			byte * pic;

			// load the full specification, and perform any image program calculations
			R_LoadImageProgram( GetName(), &pic, &width, &height, &m_sourceFileTime, &m_opts.usage );

			if ( pic == NULL ) {
				idLib::Warning( "Couldn't load image: %s : %s", GetName(), generatedName.c_str() );
				// create a default so it doesn't get continuously reloaded
				m_opts.width = 8;
				m_opts.height = 8;
				m_opts.numLevels = 1;
				DeriveOpts();
				AllocImage();
				
				// clear the data so it's not left uninitialized
				idTempArray<byte> clear( m_opts.width * m_opts.height * 4 );
				memset( clear.Ptr(), 0, clear.Size() );
				for ( int level = 0; level < m_opts.numLevels; level++ ) {
					SubImageUpload( level, 0, 0, 0, m_opts.width >> level, m_opts.height >> level, clear.Ptr() );
				}

				return;
			}

			m_opts.width = width;
			m_opts.height = height;
			m_opts.numLevels = 0;
			DeriveOpts();
			im.Load2DFromMemory( m_opts.width, m_opts.height, pic, m_opts.numLevels, m_opts.format, m_opts.colorFormat, m_opts.gammaMips );

			Mem_Free( pic );
		}
		m_binaryFileTime = im.WriteGeneratedFile( m_sourceFileTime );
	}

	AllocImage();

	for ( int i = 0; i < im.NumImages(); i++ ) {
		const bimageImage_t & img = im.GetImageHeader( i );
		const byte * data = im.GetImageData( i );
		SubImageUpload( img.level, 0, 0, img.destZ, img.width, img.height, data );
	}
}

/*
================
MakePowerOfTwo
================
*/
int MakePowerOfTwo( int num ) {
	int	pot;
	for ( pot = 1; pot < num; pot <<= 1 ) {
	}
	return pot;
}

/*
==================
StorageSize
==================
*/
int idImage::StorageSize() const {

	if ( !IsLoaded() ) {
		return 0;
	}
	int baseSize = m_opts.width * m_opts.height;
	if ( m_opts.numLevels > 1 ) {
		baseSize *= 4;
		baseSize /= 3;
	}
	baseSize *= BitsForFormat( m_opts.format );
	baseSize /= 8;
	return baseSize;
}

/*
==================
Print
==================
*/
void idImage::Print() const {
	if ( m_generatorFunction ) {
		idLib::Printf( "F" );
	} else {
		idLib::Printf( " " );
	}

	switch ( m_opts.textureType ) {
		case TT_2D:
			idLib::Printf( " " );
			break;
		case TT_CUBIC:
			idLib::Printf( "C" );
			break;
		default:
			idLib::Printf( "<BAD TYPE:%i>", m_opts.textureType );
			break;
	}

	idLib::Printf( "%4i %4i ",	m_opts.width, m_opts.height );

	switch ( m_opts.format ) {
#define NAME_FORMAT( x ) case FMT_##x: idLib::Printf( "%-6s ", #x ); break;
		NAME_FORMAT( NONE );
		NAME_FORMAT( RGBA8 );
		NAME_FORMAT( XRGB8 );
		NAME_FORMAT( RGB565 );
		NAME_FORMAT( L8A8 );
		NAME_FORMAT( ALPHA );
		NAME_FORMAT( LUM8 );
		NAME_FORMAT( INT8 );
		NAME_FORMAT( DXT1 );
		NAME_FORMAT( DXT5 );
		NAME_FORMAT( DEPTH );
		NAME_FORMAT( X16 );
		NAME_FORMAT( Y16_X16 );
		default:
			idLib::Printf( "<%3i>", m_opts.format );
			break;
	}

	switch( m_filter ) {
		case TF_DEFAULT:
			idLib::Printf( "mip  " );
			break;
		case TF_LINEAR:
			idLib::Printf( "linr " );
			break;
		case TF_NEAREST:
			idLib::Printf( "nrst " );
			break;
		default:
			idLib::Printf( "<BAD FILTER:%i>", m_filter );
			break;
	}

	switch ( m_repeat ) {
		case TR_REPEAT:
			idLib::Printf( "rept " );
			break;
		case TR_CLAMP_TO_ZERO:
			idLib::Printf( "zero " );
			break;
		case TR_CLAMP_TO_ZERO_ALPHA:
			idLib::Printf( "azro " );
			break;
		case TR_CLAMP:
			idLib::Printf( "clmp " );
			break;
		default:
			idLib::Printf( "<BAD REPEAT:%i>", m_repeat );
			break;
	}

	idLib::Printf( "%4ik ", StorageSize() / 1024 );

	idLib::Printf( " %s\n", GetName() );
}

/*
===============
idImage::Reload
===============
*/
void idImage::Reload( bool force ) {
	// always regenerate functional images
	if ( m_generatorFunction ) {
		common->DPrintf( "regenerating %s.\n", GetName() );
		m_generatorFunction( this );
		return;
	}

	// check file times
	if ( !force ) {
		ID_TIME_T current;
		if ( m_cubeFiles != CF_2D ) {
			R_LoadCubeImages( m_imgName, m_cubeFiles, NULL, NULL, &current );
		} else {
			// get the current values
			R_LoadImageProgram( m_imgName, NULL, NULL, NULL, &current );
		}
		if ( current <= m_sourceFileTime ) {
			return;
		}
	}

	common->DPrintf( "reloading %s.\n", GetName() );

	PurgeImage();

	// Load is from the front end, so the back end must be synced
	ActuallyLoadImage( false );
}

/*
==================
idImage::GenerateImage
==================
*/
void idImage::GenerateImage( 
		const byte * pic, 
		int width, int height, 
		textureFilter_t filter, 
		textureRepeat_t repeat, 
		textureUsage_t usage ) {
	
	PurgeImage();

	m_filter = filter;
	m_repeat = repeat;
	m_opts.usage = usage;
	m_cubeFiles = CF_2D;

	m_opts.textureType = TT_2D;
	m_opts.width = width;
	m_opts.height = height;
	m_opts.numLevels = 0;
	DeriveOpts();

	idBinaryImage im( GetName() );
	im.Load2DFromMemory( 
		width, height, pic, 
		m_opts.numLevels, 
		m_opts.format, 
		m_opts.colorFormat, 
		m_opts.gammaMips );

	AllocImage();

	for ( int i = 0; i < im.NumImages(); ++i ) {
		const bimageImage_t & img = im.GetImageHeader( i );
		const byte * data = im.GetImageData( i );
		SubImageUpload( img.level, 0, 0, img.destZ, img.width, img.height, data );
	}
}

/*
==================
idImage::GenerateCubeImage
==================
*/
void idImage::GenerateCubeImage( 
		const byte *pic[6], 
		int size, 
		textureFilter_t filter, 
		textureUsage_t usage ) {

	PurgeImage();

	m_filter = filter;
	m_repeat = TR_CLAMP;
	m_opts.usage = usage;
	m_cubeFiles = CF_NATIVE;

	m_opts.textureType = TT_CUBIC;
	m_opts.width = size;
	m_opts.height = size;
	m_opts.numLevels = 0;
	DeriveOpts();

	idBinaryImage im( GetName() );
	im.LoadCubeFromMemory( 
		size, pic, 
		m_opts.numLevels, 
		m_opts.format, 
		m_opts.gammaMips );

	AllocImage();

	for ( int i = 0; i < im.NumImages(); ++i ) {
		const bimageImage_t & img = im.GetImageHeader( i );
		const byte * data = im.GetImageData( i );
		SubImageUpload( img.level, 0, 0, img.destZ, img.width, img.height, data );
	}
}

/*
====================
idImage::UploadScratchImage

if rows = cols * 6, assume it is a cube map animation
====================
*/
void idImage::UploadScratchImage( const byte * data, int cols, int rows ) {
	// if rows = cols * 6, assume it is a cube map animation
	if ( rows == cols * 6 ) {
		rows /= 6;
		const byte * pic[6];
		for ( int i = 0; i < 6; ++i ) {
			pic[i] = data + cols * rows * 4 * i;
		}

		if ( m_opts.textureType != TT_CUBIC || m_opts.usage != TD_LOOKUP_TABLE_RGBA ) {
			GenerateCubeImage( pic, cols, TF_LINEAR, TD_LOOKUP_TABLE_RGBA );
			return;
		}
		if ( m_opts.width != cols || m_opts.height != rows ) {
			m_opts.width = cols;
			m_opts.height = rows;

			AllocImage();
		}
		SetSamplerState( TF_LINEAR, TR_CLAMP );
		for ( int i = 0; i < 6; ++i ) {
			SubImageUpload( 0, 0, 0, i, m_opts.width, m_opts.height, pic[i] );
		}
	} else {
		if ( m_opts.textureType != TT_2D || m_opts.usage != TD_LOOKUP_TABLE_RGBA ) {
			GenerateImage( data, cols, rows, TF_LINEAR, TR_REPEAT, TD_LOOKUP_TABLE_RGBA );
			return;
		}
		if ( m_opts.width != cols || m_opts.height != rows ) {
			m_opts.width = cols;
			m_opts.height = rows;

			AllocImage();
		}
		SetSamplerState( TF_LINEAR, TR_REPEAT );
		SubImageUpload( 0, 0, 0, 0, m_opts.width, m_opts.height, data );
	}
}