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
#include "../RenderBackend.h"
#include "Allocator_VK.h"

idCVar r_vkDeviceLocalMemoryMB( "r_vkDeviceLocalMemoryMB", "128", CVAR_INTEGER | CVAR_INIT, "" );
idCVar r_vkHostVisibleMemoryMB( "r_vkHostVisibleMemoryMB", "64", CVAR_INTEGER | CVAR_INIT, "" );

static const char * memoryUsageStrings[ VULKAN_MEMORY_USAGES ] = {
	"VULKAN_MEMORY_USAGE_UNKNOWN",
	"VULKAN_MEMORY_USAGE_GPU_ONLY",
	"VULKAN_MEMORY_USAGE_CPU_ONLY",
	"VULKAN_MEMORY_USAGE_CPU_TO_GPU",
	"VULKAN_MEMORY_USAGE_GPU_TO_CPU",
};

static const char * allocationTypeStrings[ VULKAN_ALLOCATION_TYPES ] = {
	"VULKAN_ALLOCATION_TYPE_FREE",
	"VULKAN_ALLOCATION_TYPE_BUFFER",
	"VULKAN_ALLOCATION_TYPE_IMAGE",
	"VULKAN_ALLOCATION_TYPE_IMAGE_LINEAR",
	"VULKAN_ALLOCATION_TYPE_IMAGE_OPTIMAL",
};

/*
=============
FindMemoryTypeIndex
=============
*/
uint32 FindMemoryTypeIndex( const uint32 memoryTypeBits, const vulkanMemoryUsage_t usage ) {
	VkPhysicalDeviceMemoryProperties & physicalMemoryProperties = vkcontext.gpu.memProps;

	VkMemoryPropertyFlags required = 0;
	VkMemoryPropertyFlags preferred = 0;

	switch ( usage ) {
	case VULKAN_MEMORY_USAGE_GPU_ONLY:
		preferred |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		break;
	case VULKAN_MEMORY_USAGE_CPU_ONLY:
		required |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		break;
	case VULKAN_MEMORY_USAGE_CPU_TO_GPU:
		required |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		preferred |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		break;
	case VULKAN_MEMORY_USAGE_GPU_TO_CPU:
		required |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		preferred |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		break;
	default:
		idLib::FatalError( "idVulkanAllocator::AllocateFromPools: Unknown memory usage." );
	}

	for ( uint32 i = 0; i < physicalMemoryProperties.memoryTypeCount; ++i ) {
		if ( ( ( memoryTypeBits >> i ) & 1 ) == 0 ) {
			continue;
		}
		
		const VkMemoryPropertyFlags properties = physicalMemoryProperties.memoryTypes[ i ].propertyFlags;
		if ( ( properties & required ) != required ) {
			continue;
		}

		if ( ( properties & preferred ) != preferred ) {
			continue;
		}

		return i;
	}

	for ( uint32 i = 0; i < physicalMemoryProperties.memoryTypeCount; ++i ) {
		if ( ( ( memoryTypeBits >> i ) & 1 ) == 0 ) {
			continue;
		}
			
		const VkMemoryPropertyFlags properties = physicalMemoryProperties.memoryTypes[ i ].propertyFlags;
		if ( ( properties & required ) != required ) {
			continue;
		}

		return i;
	}

	return UINT32_MAX;
}

/*
================================================================================================

idVulkanAllocator

================================================================================================
*/

/*
=============
idVulkanBlock::idVulkanBlock
=============
*/
idVulkanBlock::idVulkanBlock( const uint32 memoryTypeIndex, const VkDeviceSize size, vulkanMemoryUsage_t usage ) : 
	m_nextBlockId( 0 ),
	m_size( size ),
	m_allocated( 0 ),
	m_memoryTypeIndex( memoryTypeIndex ),
	m_usage( usage ),
	m_deviceMemory( VK_NULL_HANDLE ) {
	
}

/*
=============
idVulkanBlock::idVulkanBlock
=============
*/
idVulkanBlock::~idVulkanBlock() {
	Shutdown();
}

/*
=============
idVulkanBlock::Init
=============
*/
bool idVulkanBlock::Init() {
	if ( m_memoryTypeIndex == UINT64_MAX ) {
		return false;
	}

	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = m_size;
	memoryAllocateInfo.memoryTypeIndex = m_memoryTypeIndex;

	ID_VK_CHECK( vkAllocateMemory( vkcontext.device, &memoryAllocateInfo, NULL, &m_deviceMemory ) )

	if ( m_deviceMemory == VK_NULL_HANDLE ) {
		return false;
	}

	if ( IsHostVisible() ) {
		ID_VK_CHECK( vkMapMemory( vkcontext.device, m_deviceMemory, 0, m_size, 0, (void **)&m_data ) );
	}

	m_head = new chunk_t();
	m_head->id = m_nextBlockId++;
	m_head->size = m_size;
	m_head->offset = 0;
	m_head->prev = NULL;
	m_head->next = NULL;
	m_head->type = VULKAN_ALLOCATION_TYPE_FREE;

	return true;
}

/*
=============
idVulkanBlock::Shutdown
=============
*/
void idVulkanBlock::Shutdown() {
	// Unmap the memory
	if ( IsHostVisible() ) {
		vkUnmapMemory( vkcontext.device, m_deviceMemory );
	}

	// Free the memory
	vkFreeMemory( vkcontext.device, m_deviceMemory, NULL );
	m_deviceMemory = VK_NULL_HANDLE;

	chunk_t * prev = NULL;
	chunk_t * current = m_head;
	while ( 1 ) {
		if ( current->next == NULL ) {
			delete current;
			break;
		} else {
			prev = current;
			current = current->next;
			delete prev;
		}
	}

	m_head = NULL;
}

/*
=============
IsOnSamePage

Algorithm comes from the Vulkan 1.0.39 spec. "Buffer-Image Granularity"
Also known as "Linear-Optimal Granularity"
=============
*/
static bool IsOnSamePage( 
	VkDeviceSize rAOffset, VkDeviceSize rASize, 
	VkDeviceSize rBOffset, VkDeviceSize pageSize ) {

	assert( rAOffset + rASize <= rBOffset && rASize > 0 && pageSize > 0 );

	VkDeviceSize rAEnd = rAOffset + rASize - 1;
	VkDeviceSize rAEndPage = rAEnd & ~( pageSize -1 );
	VkDeviceSize rBStart = rBOffset;
	VkDeviceSize rBStartPage = rBStart & ~( pageSize - 1 );

	return rAEndPage == rBStartPage;
}

/*
=============
HasGranularityConflict

Check that allocation types obey buffer image granularity.
=============
*/
static bool HasGranularityConflict( vulkanAllocationType_t type1, vulkanAllocationType_t type2 ) {
	if ( type1 > type2 ) {
		SwapValues( type1, type2 );
	}

	switch( type1 ) {
	case VULKAN_ALLOCATION_TYPE_FREE:
		return false;
	case VULKAN_ALLOCATION_TYPE_BUFFER:
		return	type2 == VULKAN_ALLOCATION_TYPE_IMAGE || 
				type2 == VULKAN_ALLOCATION_TYPE_IMAGE_OPTIMAL;
	case VULKAN_ALLOCATION_TYPE_IMAGE:
		return  type2 == VULKAN_ALLOCATION_TYPE_IMAGE ||
				type2 == VULKAN_ALLOCATION_TYPE_IMAGE_LINEAR ||
				type2 == VULKAN_ALLOCATION_TYPE_IMAGE_OPTIMAL;
	case VULKAN_ALLOCATION_TYPE_IMAGE_LINEAR:
		return type2 == VULKAN_ALLOCATION_TYPE_IMAGE_OPTIMAL;
	case VULKAN_ALLOCATION_TYPE_IMAGE_OPTIMAL:
		return false;
	default:
		assert( false );
		return true;
	}
}

/*
=============
idVulkanBlock::Allocate
=============
*/
bool idVulkanBlock::Allocate( 
	const uint32 size, 
	const uint32 align, 
	const VkDeviceSize granularity, 
	const vulkanAllocationType_t allocType, 
	vulkanAllocation_t & allocation ) {

	const VkDeviceSize freeSize = m_size - m_allocated;
	if ( freeSize < size ) {
		return false;
	}

	chunk_t * current = NULL;
	chunk_t * bestFit = NULL;
	chunk_t * previous = NULL;

	VkDeviceSize padding = 0;
	VkDeviceSize offset = 0;
	VkDeviceSize alignedSize = 0;

	for ( current = m_head; current != NULL; previous = current, current = current->next ) {
		if ( current->type != VULKAN_ALLOCATION_TYPE_FREE ) {
			continue;
		}

		if ( size > current->size ) {
			continue;
		}

		offset = ALIGN( current->offset, align );

		// Check for linear/optimal granularity conflict with previous allocation
		if ( previous != NULL && granularity > 1 ) {
			if ( IsOnSamePage( previous->offset, previous->size, offset, granularity ) ) {
				if ( HasGranularityConflict( previous->type, allocType ) ) {
					offset = ALIGN( offset, granularity );
				}
			}
		}

		padding = offset - current->offset;
		alignedSize = padding + size;

		if ( alignedSize > current->size ) {
			continue;
		}

		if ( alignedSize + m_allocated >= m_size ) {
			return false;
		}

		if ( granularity > 1 && current->next != NULL ) {
			chunk_t * next = current->next;
			if ( IsOnSamePage( offset, size, next->offset, granularity ) ) {
				if ( HasGranularityConflict( allocType, next->type ) ) {
					continue;
				}
			}
		}

		bestFit = current;
		break;
	}

	if ( bestFit == NULL ) {
		return false;
	}

	if ( bestFit->size > size ) {
		chunk_t * chunk = new chunk_t();
		chunk_t * next = bestFit->next;

		chunk->id = m_nextBlockId++;
		chunk->prev = bestFit;
		bestFit->next = chunk;

		chunk->next = next;
		if ( next ) {
			next->prev = chunk;
		}

		chunk->size = bestFit->size - alignedSize;
		chunk->offset = offset + size;
		chunk->type = VULKAN_ALLOCATION_TYPE_FREE;
	}

	bestFit->type = allocType;
	bestFit->size = size;

	m_allocated += alignedSize;

	allocation.size = bestFit->size;
	allocation.id = bestFit->id;
	allocation.deviceMemory = m_deviceMemory;
	if ( IsHostVisible() ) {
		allocation.data = m_data + offset;
	}
	allocation.offset = offset;
	allocation.block = this;

	return true;
}

/*
=============
idVulkanBlock::Free
=============
*/
void idVulkanBlock::Free( vulkanAllocation_t & allocation ) {
	chunk_t * current = NULL;
	for ( current = m_head; current != NULL; current = current->next ) {
		if ( current->id == allocation.id ) {
			break;
		}
	}

	if ( current == NULL ) {
		idLib::Warning( "idVulkanBlock::Free: Tried to free an unknown allocation. %p - %lu", this, allocation.id );
		return;
	}

	current->type = VULKAN_ALLOCATION_TYPE_FREE;

	if ( current->prev && current->prev->type == VULKAN_ALLOCATION_TYPE_FREE ) {
		chunk_t * prev = current->prev;

		prev->next = current->next;
		if ( current->next ) {
			current->next->prev = prev;
		}

		prev->size += current->size;

		delete current;
		current = prev;
	}

	if ( current->next && current->next->type == VULKAN_ALLOCATION_TYPE_FREE ) {
		chunk_t * next = current->next;

		if ( next->next ) {
			next->next->prev = current;
		}

		current->next = next->next;

		current->size += next->size;

		delete next;
	}

	m_allocated -= allocation.size;
}

/*
=============
idVulkanBlock::Print
=============
*/
void idVulkanBlock::Print() {
	int count = 0;
	for ( chunk_t * current = m_head; current != NULL; current = current->next ) { count++; }

	idLib::Printf( "Type Index: %lu\n", m_memoryTypeIndex );
	idLib::Printf( "Usage:      %s\n", memoryUsageStrings[ m_usage ] );
	idLib::Printf( "Count:      %d\n", count );
	idLib::Printf( "Size:       %llu\n", m_size );
	idLib::Printf( "Allocated:  %llu\n", m_allocated );
	idLib::Printf( "Next Block: %lu\n", m_nextBlockId );
	idLib::Printf( "------------------------\n" );

	for ( chunk_t * current = m_head; current != NULL; current = current->next ) {
		idLib::Printf( "{\n" );

		idLib::Printf( "\tId:     %lu\n", current->id );
		idLib::Printf( "\tSize:   %llu\n", current->size );
		idLib::Printf( "\tOffset: %llu\n", current->offset );
		idLib::Printf( "\tType:   %s\n", allocationTypeStrings[ current->type ] );

		idLib::Printf( "}\n" );
	}

	idLib::Printf( "\n" );
}

/*
================================================================================================

idVulkanAllocator

================================================================================================
*/

#if defined( ID_USE_AMD_ALLOCATOR )
VmaAllocator vmaAllocator;
#else
idVulkanAllocator vulkanAllocator;
#endif

/*
=============
idVulkanAllocator::idVulkanAllocator
=============
*/
idVulkanAllocator::idVulkanAllocator() : 
	m_garbageIndex( 0 ),
	m_deviceLocalMemoryBytes( 0 ),
	m_hostVisibleMemoryBytes( 0 ),
	m_bufferImageGranularity( 0 ) {
	
}

/*
=============
idVulkanAllocator::Init
=============
*/
void idVulkanAllocator::Init() {
	m_deviceLocalMemoryBytes = r_vkDeviceLocalMemoryMB.GetInteger() * 1024 * 1024;
	m_hostVisibleMemoryBytes = r_vkHostVisibleMemoryMB.GetInteger() * 1024 * 1024;
	m_bufferImageGranularity = vkcontext.gpu.props.limits.bufferImageGranularity;
}

/*
=============
idVulkanAllocator::Shutdown
=============
*/
void idVulkanAllocator::Shutdown() {
	EmptyGarbage();
	for ( int i = 0; i < VK_MAX_MEMORY_TYPES; ++i ) {
		idList< idVulkanBlock * > & blocks = m_blocks[ i ];
		const int numBlocks = blocks.Num();
		for ( int j = 0; j < numBlocks; ++j ) {
			delete blocks[ j ];
		}

		blocks.Clear();
	}
}

/*
=============
idVulkanAllocator::Allocate
=============
*/
vulkanAllocation_t idVulkanAllocator::Allocate( 
		const uint32 size, 
		const uint32 align, 
		const uint32 memoryTypeBits,
		const vulkanMemoryUsage_t usage,
		const vulkanAllocationType_t allocType ) {
	
	vulkanAllocation_t allocation;

	uint32 memoryTypeIndex = FindMemoryTypeIndex( memoryTypeBits, usage );
	if ( memoryTypeIndex == UINT32_MAX ) {
		idLib::FatalError( "idVulkanAllocator::Allocate: Unable to find a memoryTypeIndex for allocation request." );
	}

	idList< idVulkanBlock * > & blocks = m_blocks[ memoryTypeIndex ];
	const int numBlocks = blocks.Num();
	for ( int i = 0; i < numBlocks; ++i ) {
		idVulkanBlock * block = blocks[ i ];

		if ( block->m_memoryTypeIndex != memoryTypeIndex ) {
			continue;
		}

		if ( block->Allocate( size, align, m_bufferImageGranularity, allocType, allocation ) ) {
			return allocation;
		}
	}

	VkDeviceSize blockSize = ( usage == VULKAN_MEMORY_USAGE_GPU_ONLY ) ? m_deviceLocalMemoryBytes : m_hostVisibleMemoryBytes;

	idVulkanBlock * block = new idVulkanBlock( memoryTypeIndex, blockSize, usage );
	if ( block->Init() ) {
		blocks.Append( block );
	} else {
		idLib::FatalError( "idVulkanAllocator::Allocate: Could not allocate new memory block." );
	}

	block->Allocate( size, align, m_bufferImageGranularity, allocType, allocation );

	return allocation;
}

/*
=============
idVulkanAllocator::Free
=============
*/
void idVulkanAllocator::Free( const vulkanAllocation_t allocation ) {
	m_garbage[ m_garbageIndex ].Append( allocation );
}

/*
=============
idVulkanAllocator::EmptyGarbage
=============
*/
void idVulkanAllocator::EmptyGarbage() {
	m_garbageIndex = ( m_garbageIndex + 1 ) % NUM_FRAME_DATA;

	idList< vulkanAllocation_t > & garbage = m_garbage[ m_garbageIndex ];
	
	const int numAllocations = garbage.Num();
	for ( int i = 0; i < numAllocations; ++i ) {
		vulkanAllocation_t allocation = garbage[ i ];

		allocation.block->Free( allocation );

		if ( allocation.block->m_allocated == 0 ) {
			m_blocks[ allocation.block->m_memoryTypeIndex ].Remove( allocation.block );
			delete allocation.block;
			allocation.block = NULL;
		}
	}

	garbage.Clear();
}

/*
=============
idVulkanAllocator::Print
=============
*/
void idVulkanAllocator::Print() {
	idLib::Printf( "Device Local MB: %d\n", int( m_deviceLocalMemoryBytes / 1024 * 1024 ) );
	idLib::Printf( "Host Visible MB: %d\n", int( m_hostVisibleMemoryBytes / 1024 * 1024 ) );
	idLib::Printf( "Buffer Granularity: %llu\n", m_bufferImageGranularity );
	idLib::Printf( "\n" );

	for ( int i = 0; i < VK_MAX_MEMORY_TYPES; ++i ) {
		idList< idVulkanBlock * > & blocksByType = m_blocks[ i ];

		const int numBlocks = blocksByType.Num();
		for ( int j = 0; j < numBlocks; ++j ) {
			blocksByType[ j ]->Print();
		}
	}
}

CONSOLE_COMMAND( Vulkan_PrintHeapInfo, "Print out the heap information for this hardware.", 0 ) {
	VkPhysicalDeviceMemoryProperties & props = vkcontext.gpu.memProps;

	idLib::Printf( "Heaps %lu\n------------------------\n", props.memoryHeapCount );
	for ( uint32 i = 0; i < props.memoryHeapCount; ++i ) {
		VkMemoryHeap heap = props.memoryHeaps[ i ];
		idLib::Printf( "id=%d, size=%llu, flags=", i, heap.size );
		if ( heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
			idLib::Printf( "DEVICE_LOCAL" );
		} else {
			idLib::Printf( "HOST_VISIBLE" );
		}
		idLib::Printf( "\n" );

		for ( uint32 j = 0; j < props.memoryTypeCount; ++j ) {
			VkMemoryType type = props.memoryTypes[ j ];
			if ( type.heapIndex != i ) {
				continue;
			}

			idStr properties;
			if ( type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) {
				properties += "\tDEVICE_LOCAL\n";
			}
			if ( type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) {
				properties += "\tHOST_VISIBLE\n";
			}
			if ( type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) {
				properties += "\tHOST_COHERENT\n";
			}
			if ( type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT ) {
				properties += "\tHOST_CACHED\n";
			}
			if ( type.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT ) {
				properties += "\tLAZILY_ALLOCATED\n";
			}

			if ( properties.Length() > 0 ) {
				idLib::Printf( "memory_type=%lu\n", j );
				idLib::Printf( properties.c_str() );
			}
		}

		idLib::Printf( "\n" );
	}
}

CONSOLE_COMMAND( Vulkan_PrintAllocations, "Print out all the current allocations.", 0 ) {
#if defined( ID_USE_AMD_ALLOCATOR )
	// TODO
#else
	vulkanAllocator.Print();
#endif
}