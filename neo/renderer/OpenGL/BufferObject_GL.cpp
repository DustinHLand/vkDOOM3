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
#include "../../idlib/precompiled.h"
#include "../BufferObject.h"

extern idCVar r_showBuffers;

//static const GLenum bufferUsage = GL_STATIC_DRAW_ARB;
static const GLenum bufferUsage = GL_DYNAMIC_DRAW_ARB;

/*
========================
UnbindBufferObjects
========================
*/
void UnbindBufferObjects() {
	qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
}

/*
================================================================================================

idVertexBuffer

================================================================================================
*/

/*
========================
idVertexBuffer::idVertexBuffer
========================
*/
idVertexBuffer::idVertexBuffer() {
	m_size = 0;
	m_offsetInOtherBuffer = OWNS_BUFFER_FLAG;
	m_apiObject = 0xFFFF;
	SetUnmapped();
}

/*
========================
idVertexBuffer::AllocBufferObject
========================
*/
bool idVertexBuffer::AllocBufferObject( const void * data, int allocSize, bufferUsageType_t usage ) {
	assert( m_apiObject == 0xFFFF );
	assert_16_byte_aligned( data );

	if ( allocSize <= 0 ) {
		idLib::Error( "idVertexBuffer::AllocBufferObject: allocSize = %i", allocSize );
	}

	m_size = allocSize;
	m_usage = usage;

	bool allocationFailed = false;

	int numBytes = GetAllocedSize();

	// clear out any previous error
	GL_CheckErrors();

	qglGenBuffersARB( 1, &m_apiObject );
	if ( m_apiObject == 0xFFFF ) {
		idLib::FatalError( "idVertexBuffer::AllocBufferObject: failed" );
	}
	qglBindBufferARB( GL_ARRAY_BUFFER_ARB, m_apiObject );

	// these are rewritten every frame
	qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numBytes, NULL, bufferUsage );

	GLenum err = qglGetError();
	if ( err == GL_OUT_OF_MEMORY ) {
		idLib::Warning( "idVertexBuffer::AllocBufferObject: allocation failed" );
		allocationFailed = true;
	}

	if ( r_showBuffers.GetBool() ) {
		idLib::Printf( "vertex buffer alloc %p, (%i bytes)\n", this, GetSize() );
	}

	// copy the data
	if ( data != NULL ) {
		Update( data, allocSize );
	}

	return !allocationFailed;
}

/*
========================
idVertexBuffer::FreeBufferObject
========================
*/
void idVertexBuffer::FreeBufferObject() {
	if ( IsMapped() ) {
		UnmapBuffer();
	}

	// if this is a sub-allocation inside a larger buffer, don't actually free anything.
	if ( OwnsBuffer() == false ) {
		ClearWithoutFreeing();
		return;
	}

	if ( m_apiObject == 0xFFFF ) {
		return;
	}

	if ( r_showBuffers.GetBool() ) {
		idLib::Printf( "vertex buffer free %p, (%i bytes)\n", this, GetSize() );
	}

	qglDeleteBuffersARB( 1, &m_apiObject );

	ClearWithoutFreeing();
}

/*
========================
idVertexBuffer::Update
========================
*/
void idVertexBuffer::Update( const void * data, int updateSize, int offset ) const {
	assert( m_apiObject != 0xFFFF );
	assert_16_byte_aligned( data );
	assert( ( GetOffset() & 15 ) == 0 );

	if ( updateSize > GetSize() ) {
		idLib::FatalError( "idVertexBuffer::Update: size overrun, %i > %i\n", updateSize, GetSize() );
	}

	int numBytes = ( updateSize + 15 ) & ~15;

	if ( m_usage == BU_DYNAMIC ) {
		CopyBuffer( (byte *)m_buffer + offset, (const byte*)data, numBytes );
	} else {
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, m_apiObject );
		qglBufferSubDataARB( GL_ARRAY_BUFFER_ARB, GetOffset() + offset, (GLsizeiptrARB)numBytes, data );
	}
}

/*
========================
idVertexBuffer::MapBuffer
========================
*/
void * idVertexBuffer::MapBuffer( bufferMapType_t mapType ) {
	assert( m_apiObject != 0xFFFF );
	assert( IsMapped() == false );

	m_buffer = NULL;

	qglBindBufferARB( GL_ARRAY_BUFFER_ARB, m_apiObject );
	if ( mapType == BM_READ ) {
		m_buffer = qglMapBufferRange( GL_ARRAY_BUFFER_ARB, 0, GetAllocedSize(), GL_MAP_READ_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT );
		if ( m_buffer != NULL ) {
			m_buffer = (byte *)m_buffer + GetOffset();
		}
	} else if ( mapType == BM_WRITE ) {
		m_buffer = qglMapBufferRange( GL_ARRAY_BUFFER_ARB, 0, GetAllocedSize(), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT );
		if ( m_buffer != NULL ) {
			m_buffer = (byte *)m_buffer + GetOffset();
		}
		assert( IsWriteCombined( m_buffer ) );
	} else {
		assert( false );
	}

	SetMapped();

	if ( m_buffer == NULL ) {
		idLib::FatalError( "idVertexBuffer::MapBuffer: failed" );
	}
	return m_buffer;
}

/*
========================
idVertexBuffer::UnmapBuffer
========================
*/
void idVertexBuffer::UnmapBuffer() {
	assert( m_apiObject != 0xFFFF );
	assert( IsMapped() );

	qglBindBufferARB( GL_ARRAY_BUFFER_ARB, m_apiObject );
	if ( !qglUnmapBufferARB( GL_ARRAY_BUFFER_ARB ) ) {
		idLib::Printf( "idVertexBuffer::UnmapBuffer failed\n" );
	}

	m_buffer = NULL;

	SetUnmapped();
}

/*
========================
idVertexBuffer::ClearWithoutFreeing
========================
*/
void idVertexBuffer::ClearWithoutFreeing() {
	m_size = 0;
	m_offsetInOtherBuffer = OWNS_BUFFER_FLAG;
	m_apiObject = 0xFFFF;
}

/*
================================================================================================

idIndexBuffer

================================================================================================
*/

/*
========================
idIndexBuffer::idIndexBuffer
========================
*/
idIndexBuffer::idIndexBuffer() {
	m_size = 0;
	m_offsetInOtherBuffer = OWNS_BUFFER_FLAG;
	m_apiObject = 0xFFFF;
	SetUnmapped();
}

/*
========================
idIndexBuffer::AllocBufferObject
========================
*/
bool idIndexBuffer::AllocBufferObject( const void * data, int allocSize, bufferUsageType_t usage ) {
	assert( m_apiObject == 0xFFFF );
	assert_16_byte_aligned( data );

	if ( allocSize <= 0 ) {
		idLib::Error( "idIndexBuffer::AllocBufferObject: allocSize = %i", allocSize );
	}

	m_size = allocSize;
	m_usage = usage;

	bool allocationFailed = false;

	int numBytes = GetAllocedSize();


	// clear out any previous error
	GL_CheckErrors();

	qglGenBuffersARB( 1, &m_apiObject );
	if ( m_apiObject == 0xFFFF ) {
		GLenum error = qglGetError();
		idLib::FatalError( "idIndexBuffer::AllocBufferObject: failed - GL_Error %d", error );
	}
	qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, m_apiObject );

	// these are rewritten every frame
	qglBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, numBytes, NULL, bufferUsage );

	GLenum err = qglGetError();
	if ( err == GL_OUT_OF_MEMORY ) {
		idLib::Warning( "idIndexBuffer:AllocBufferObject: allocation failed" );
		allocationFailed = true;
	}

	if ( r_showBuffers.GetBool() ) {
		idLib::Printf( "index buffer alloc %p, (%i bytes)\n", this, GetSize() );
	}

	// copy the data
	if ( data != NULL ) {
		Update( data, allocSize );
	}

	return !allocationFailed;
}

/*
========================
idIndexBuffer::FreeBufferObject
========================
*/
void idIndexBuffer::FreeBufferObject() {
	if ( IsMapped() ) {
		UnmapBuffer();
	}

	// if this is a sub-allocation inside a larger buffer, don't actually free anything.
	if ( OwnsBuffer() == false ) {
		ClearWithoutFreeing();
		return;
	}

	if ( m_apiObject == 0xFFFF ) {
		return;
	}

	if ( r_showBuffers.GetBool() ) {
		idLib::Printf( "index buffer free %p, (%i bytes)\n", this, GetSize() );
	}

	qglDeleteBuffersARB( 1, &m_apiObject );

	ClearWithoutFreeing();
}

/*
========================
idIndexBuffer::Update
========================
*/
void idIndexBuffer::Update( const void * data, int updateSize, int offset ) const {
	assert( m_apiObject != 0xFFFF );
	assert_16_byte_aligned( data );
	assert( ( GetOffset() & 15 ) == 0 );

	if ( updateSize > GetSize() ) {
		idLib::FatalError( "idIndexBuffer::Update: size overrun, %i > %i\n", updateSize, GetSize() );
	}

	int numBytes = ( updateSize + 15 ) & ~15;

	if ( m_usage == BU_DYNAMIC ) {
		CopyBuffer( (byte *)m_buffer + offset, (const byte*)data, numBytes );
	} else {
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, m_apiObject );
		qglBufferSubDataARB( GL_ARRAY_BUFFER_ARB, GetOffset() + offset, (GLsizeiptrARB)numBytes, data );
	}
}

/*
========================
idIndexBuffer::MapBuffer
========================
*/
void * idIndexBuffer::MapBuffer( bufferMapType_t mapType ) {
	assert( m_apiObject != 0xFFFF );
	assert( IsMapped() == false );

	m_buffer = NULL;

	qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, m_apiObject );
	if ( mapType == BM_READ ) {
		m_buffer = qglMapBufferRange( GL_ELEMENT_ARRAY_BUFFER_ARB, 0, GetAllocedSize(), GL_MAP_READ_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT );
		if ( m_buffer != NULL ) {
			m_buffer = (byte *)m_buffer + GetOffset();
		}
	} else if ( mapType == BM_WRITE ) {
		m_buffer = qglMapBufferRange( GL_ELEMENT_ARRAY_BUFFER_ARB, 0, GetAllocedSize(), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT );
		if ( m_buffer != NULL ) {
			m_buffer = (byte *)m_buffer + GetOffset();
		}
		assert( IsWriteCombined( m_buffer ) );
	} else {
		assert( false );
	}

	SetMapped();

	if ( m_buffer == NULL ) {
		idLib::FatalError( "idIndexBuffer::MapBuffer: failed" );
	}
	return m_buffer;
}

/*
========================
idIndexBuffer::UnmapBuffer
========================
*/
void idIndexBuffer::UnmapBuffer() {
	assert( m_apiObject != 0xFFFF );
	assert( IsMapped() );

	qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, m_apiObject );
	if ( !qglUnmapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB ) ) {
		idLib::Printf( "idIndexBuffer::UnmapBuffer failed\n" );
	}

	m_buffer = NULL;

	SetUnmapped();
}

/*
========================
idIndexBuffer::ClearWithoutFreeing
========================
*/
void idIndexBuffer::ClearWithoutFreeing() {
	m_size = 0;
	m_offsetInOtherBuffer = OWNS_BUFFER_FLAG;
	m_apiObject = 0xFFFF;
}

/*
================================================================================================

idUniformBuffer

================================================================================================
*/

/*
========================
idUniformBuffer::idUniformBuffer
========================
*/
idUniformBuffer::idUniformBuffer() {
	m_size = 0;
	m_offsetInOtherBuffer = OWNS_BUFFER_FLAG;
	m_apiObject = 0xFFFF;
	SetUnmapped();
}

/*
========================
idUniformBuffer::AllocBufferObject
========================
*/
bool idUniformBuffer::AllocBufferObject( const void * data, int allocSize, bufferUsageType_t usage ) {
	assert( m_apiObject == 0xFFFF );
	assert_16_byte_aligned( data );

	if ( allocSize <= 0 ) {
		idLib::Error( "idUniformBuffer::AllocBufferObject: allocSize = %i", allocSize );
	}

	m_size = allocSize;
	m_usage = usage;

	bool allocationFailed = false;

	const int numBytes = GetAllocedSize();

	qglGenBuffersARB( 1, &m_apiObject );
	qglBindBufferARB( GL_UNIFORM_BUFFER, m_apiObject );
	qglBufferDataARB( GL_UNIFORM_BUFFER, numBytes, NULL, GL_STREAM_DRAW_ARB );
	qglBindBufferARB( GL_UNIFORM_BUFFER, 0);

	if ( r_showBuffers.GetBool() ) {
		idLib::Printf( "joint buffer alloc %p, (%i bytes)\n", this, GetSize() );
	}

	// copy the data
	if ( data != NULL ) {
		Update( data, allocSize );
	}

	return !allocationFailed;
}

/*
========================
idUniformBuffer::FreeBufferObject
========================
*/
void idUniformBuffer::FreeBufferObject() {
	if ( IsMapped() ) {
		UnmapBuffer();
	}

	// if this is a sub-allocation inside a larger buffer, don't actually free anything.
	if ( OwnsBuffer() == false ) {
		ClearWithoutFreeing();
		return;
	}

	if ( m_apiObject == 0xFFFF ) {
		return;
	}

	if ( r_showBuffers.GetBool() ) {
		idLib::Printf( "joint buffer free %p, (%i bytes)\n", this, GetSize() );
	}

	qglBindBufferARB( GL_UNIFORM_BUFFER, 0 );
	qglDeleteBuffersARB( 1, &m_apiObject );

	ClearWithoutFreeing();
}

/*
========================
idUniformBuffer::Update
========================
*/
void idUniformBuffer::Update( const void * data, int updateSize, int offset ) const {
	assert( m_apiObject != 0xFFFF );
	assert_16_byte_aligned( data );
	assert( ( GetOffset() & 15 ) == 0 );

	if ( updateSize > GetSize() ) {
		idLib::FatalError( "idUniformBuffer::Update: size overrun, %i > %i\n", updateSize, GetSize() );
	}

	const int numBytes = ( updateSize + 15 ) & ~15;

	if ( m_usage == BU_DYNAMIC ) {
		CopyBuffer( (byte *)m_buffer + offset, (const byte*)data, numBytes );
	} else {
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, m_apiObject );
		qglBufferSubDataARB( GL_ARRAY_BUFFER_ARB, GetOffset() + offset, (GLsizeiptrARB)numBytes, data );
	}
}

/*
========================
idUniformBuffer::MapBuffer
========================
*/
void * idUniformBuffer::MapBuffer( bufferMapType_t mapType ) {
	assert( IsMapped() == false );
	assert( mapType == BM_WRITE );
	assert( m_apiObject != 0xFFFF );

	int numBytes = GetAllocedSize();

	m_buffer = NULL;

	qglBindBufferARB( GL_UNIFORM_BUFFER, m_apiObject );
	numBytes = numBytes;
	assert( GetOffset() == 0 );
	m_buffer = qglMapBufferRange( GL_UNIFORM_BUFFER, 0, GetAllocedSize(), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT );
	if ( m_buffer != NULL ) {
		m_buffer = (byte *)m_buffer + GetOffset();
	}

	SetMapped();

	if ( m_buffer == NULL ) {
		idLib::FatalError( "idUniformBuffer::MapBuffer: failed" );
	}
	return (float *) m_buffer;
}

/*
========================
idUniformBuffer::UnmapBuffer
========================
*/
void idUniformBuffer::UnmapBuffer() {
	assert( m_apiObject != 0xFFFF );
	assert( IsMapped() );

	qglBindBufferARB( GL_UNIFORM_BUFFER, m_apiObject );
	if ( !qglUnmapBufferARB( GL_UNIFORM_BUFFER ) ) {
		idLib::Printf( "idUniformBuffer::UnmapBuffer failed\n" );
	}

	m_buffer = NULL;

	SetUnmapped();
}

/*
========================
idUniformBuffer::ClearWithoutFreeing
========================
*/
void idUniformBuffer::ClearWithoutFreeing() {
	m_size = 0;
	m_offsetInOtherBuffer = OWNS_BUFFER_FLAG;
	m_apiObject = 0xFFFF;
}