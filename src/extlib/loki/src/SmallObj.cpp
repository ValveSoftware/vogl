////////////////////////////////////////////////////////////////////////////////
// The Loki Library
// Copyright (c) 2001 by Andrei Alexandrescu
// This code accompanies the book:
// Alexandrescu, Andrei. "Modern C++ Design: Generic Programming and Design
//     Patterns Applied". Copyright (c) 2001. Addison-Wesley.
// Permission to use, copy, modify, distribute and sell this software for any
//     purpose is hereby granted without fee, provided that the above  copyright
//     notice appear in all copies and that both that copyright notice and this
//     permission notice appear in supporting documentation.
// The author or Addison-Wesley Longman make no representations about the
//     suitability of this software for any purpose. It is provided "as is"
//     without express or implied warranty.
////////////////////////////////////////////////////////////////////////////////

// $Id: SmallObj.cpp 823 2007-05-08 10:48:40Z lfittl $


#include <loki/SmallObj.h>

#include <cassert>
#include <climits>
#include <vector>
#include <bitset>

//#define DO_EXTRA_LOKI_TESTS
//#define USE_NEW_TO_ALLOCATE
//#define LOKI_CHECK_FOR_CORRUPTION

#ifdef DO_EXTRA_LOKI_TESTS
#include <iostream>
#endif

namespace Loki
{

/** @struct Chunk
    @ingroup SmallObjectGroupInternal
 Contains info about each allocated Chunk - which is a collection of
 contiguous blocks.  Each block is the same size, as specified by the
 FixedAllocator.  The number of blocks in a Chunk depends upon page size.
 This is a POD-style struct with value-semantics.  All functions and data
 are private so that they can not be changed by anything other than the
 FixedAllocator which owns the Chunk.

 @par Minimal Interface
 For the sake of runtime efficiency, no constructor, destructor, or
 copy-assignment operator is defined. The inline functions made by the
 compiler should be sufficient, and perhaps faster than hand-crafted
 functions.  The lack of these functions allows vector to create and copy
 Chunks as needed without overhead.  The Init and Release functions do
 what the default constructor and destructor would do.  A Chunk is not in
 a usable state after it is constructed and before calling Init.  Nor is
 a Chunk usable after Release is called, but before the destructor.

 @par Efficiency
 Down near the lowest level of the allocator, runtime efficiencies trump
 almost all other considerations.  Each function does the minimum required
 of it.  All functions should execute in constant time to prevent higher-
 level code from unwittingly using a version of Shlemiel the Painter's
 Algorithm.

 @par Stealth Indexes
 The first char of each empty block contains the index of the next empty
 block.  These stealth indexes form a singly-linked list within the blocks.
 A Chunk is corrupt if this singly-linked list has a loop or is shorter
 than blocksAvailable_.  Much of the allocator's time and space efficiency
 comes from how these stealth indexes are implemented.
 */
class Chunk
{
private:
	friend class FixedAllocator;

	/** Initializes a just-constructed Chunk.
	 @param blockSize Number of bytes per block.
	 @param blocks Number of blocks per Chunk.
	 @return True for success, false for failure.
	 */
	bool Init( std::size_t blockSize, unsigned char blocks );

	/** Allocate a block within the Chunk.  Complexity is always O(1), and
	 this will never throw.  Does not actually "allocate" by calling
	 malloc, new, or any other function, but merely adjusts some internal
	 indexes to indicate an already allocated block is no longer available.
	 @return Pointer to block within Chunk.
	 */
	void *Allocate( std::size_t blockSize );

	/** Deallocate a block within the Chunk. Complexity is always O(1), and
	 this will never throw.  For efficiency, this assumes the address is
	 within the block and aligned along the correct byte boundary.  An
	 assertion checks the alignment, and a call to HasBlock is done from
	 within VicinityFind.  Does not actually "deallocate" by calling free,
	 delete, or other function, but merely adjusts some internal indexes to
	 indicate a block is now available.
	 */
	void Deallocate( void *p, std::size_t blockSize );

	/** Resets the Chunk back to pristine values. The available count is
	 set back to zero, and the first available index is set to the zeroth
	 block.  The stealth indexes inside each block are set to point to the
	 next block. This assumes the Chunk's data was already using Init.
	 */
	void Reset( std::size_t blockSize, unsigned char blocks );

	/// Releases the allocated block of memory.
	void Release();

	/** Determines if the Chunk has been corrupted.
	 @param numBlocks Total # of blocks in the Chunk.
	 @param blockSize # of bytes in each block.
	 @param checkIndexes True if caller wants to check indexes of available
	  blocks for corruption.  If false, then caller wants to skip some
	  tests tests just to run faster.  (Debug version does more checks, but
	  release version runs faster.)
	 @return True if Chunk is corrupt.
	 */
	bool IsCorrupt( unsigned char numBlocks, std::size_t blockSize,
	                bool checkIndexes ) const;

	/** Determines if block is available.
	 @param p Address of block managed by Chunk.
	 @param numBlocks Total # of blocks in the Chunk.
	 @param blockSize # of bytes in each block.
	 @return True if block is available, else false if allocated.
	 */
	bool IsBlockAvailable( void *p, unsigned char numBlocks,
	                       std::size_t blockSize ) const;

	/// Returns true if block at address P is inside this Chunk.
	inline bool HasBlock( void *p, std::size_t chunkLength ) const
	{
		unsigned char *pc = static_cast< unsigned char * >( p );
		return ( pData_ <= pc ) && ( pc < pData_ + chunkLength );
	}

	inline bool HasAvailable( unsigned char numBlocks ) const
	{
		return ( blocksAvailable_ == numBlocks );
	}

	inline bool IsFilled( void ) const
	{
		return ( 0 == blocksAvailable_ );
	}

	/// Pointer to array of allocated blocks.
	unsigned char *pData_;
	/// Index of first empty block.
	unsigned char firstAvailableBlock_;
	/// Count of empty blocks.
	unsigned char blocksAvailable_;
};

/** @class FixedAllocator
    @ingroup SmallObjectGroupInternal
 Offers services for allocating fixed-sized objects.  It has a container
 of "containers" of fixed-size blocks.  The outer container has all the
 Chunks.  The inner container is a Chunk which owns some blocks.

 @par Class Level Invariants
 - There is always either zero or one Chunk which is empty.
 - If this has no empty Chunk, then emptyChunk_ is NULL.
 - If this has an empty Chunk, then emptyChunk_ points to it.
 - If the Chunk container is empty, then deallocChunk_ and allocChunk_
   are NULL.
 - If the Chunk container is not-empty, then deallocChunk_ and allocChunk_
   are either NULL or point to Chunks within the container.
 - allocChunk_ will often point to the last Chunk in the container since
   it was likely allocated most recently, and therefore likely to have an
   available block.
 */
class FixedAllocator
{
private:

	/** Deallocates the block at address p, and then handles the internal
	 bookkeeping needed to maintain class invariants.  This assumes that
	 deallocChunk_ points to the correct chunk.
	 */
	void DoDeallocate( void *p );

	/** Creates an empty Chunk and adds it to the end of the ChunkList.
	 All calls to the lower-level memory allocation functions occur inside
	 this function, and so the only try-catch block is inside here.
	 @return true for success, false for failure.
	 */
	bool MakeNewChunk( void );

	/** Finds the Chunk which owns the block at address p.  It starts at
	 deallocChunk_ and searches in both forwards and backwards directions
	 from there until it finds the Chunk which owns p.  This algorithm
	 should find the Chunk quickly if it is deallocChunk_ or is close to it
	 in the Chunks container.  This goes both forwards and backwards since
	 that works well for both same-order and opposite-order deallocations.
	 (Same-order = objects are deallocated in the same order in which they
	 were allocated.  Opposite order = objects are deallocated in a last to
	 first order.  Complexity is O(C) where C is count of all Chunks.  This
	 never throws.
	 @return Pointer to Chunk that owns p, or NULL if no owner found.
	 */
	Chunk *VicinityFind( void *p ) const;

	/// Not implemented.
	FixedAllocator(const FixedAllocator &);
	/// Not implemented.
	FixedAllocator &operator=(const FixedAllocator &);

	/// Type of container used to hold Chunks.
	typedef std::vector< Chunk > Chunks;
	/// Iterator through container of Chunks.
	typedef Chunks::iterator ChunkIter;
	/// Iterator through const container of Chunks.
	typedef Chunks::const_iterator ChunkCIter;

	/// Fewest # of objects managed by a Chunk.
	static unsigned char MinObjectsPerChunk_;

	/// Most # of objects managed by a Chunk - never exceeds UCHAR_MAX.
	static unsigned char MaxObjectsPerChunk_;

	/// Number of bytes in a single block within a Chunk.
	std::size_t blockSize_;
	/// Number of blocks managed by each Chunk.
	unsigned char numBlocks_;

	/// Container of Chunks.
	Chunks chunks_;
	/// Pointer to Chunk used for last or next allocation.
	Chunk *allocChunk_;
	/// Pointer to Chunk used for last or next deallocation.
	Chunk *deallocChunk_;
	/// Pointer to the only empty Chunk if there is one, else NULL.
	Chunk *emptyChunk_;

public:
	/// Create a FixedAllocator which manages blocks of 'blockSize' size.
	FixedAllocator();

	/// Destroy the FixedAllocator and release all its Chunks.
	~FixedAllocator();

	/// Initializes a FixedAllocator by calculating # of blocks per Chunk.
	void Initialize( std::size_t blockSize, std::size_t pageSize );

	/** Returns pointer to allocated memory block of fixed size - or NULL
	 if it failed to allocate.
	 */
	void *Allocate( void );

	/** Deallocate a memory block previously allocated with Allocate.  If
	 the block is not owned by this FixedAllocator, it returns false so
	 that SmallObjAllocator can call the default deallocator.  If the
	 block was found, this returns true.
	 */
	bool Deallocate( void *p, Chunk *hint );

	/// Returns block size with which the FixedAllocator was initialized.
	inline std::size_t BlockSize() const
	{
		return blockSize_;
	}

	/** Releases the memory used by the empty Chunk.  This will take
	 constant time under any situation.
	 @return True if empty chunk found and released, false if none empty.
	 */
	bool TrimEmptyChunk( void );

	/** Releases unused spots from ChunkList.  This takes constant time
	 with respect to # of Chunks, but actual time depends on underlying
	 memory allocator.
	 @return False if no unused spots, true if some found and released.
	 */
	bool TrimChunkList( void );

	/** Returns count of empty Chunks held by this allocator.  Complexity
	 is O(C) where C is the total number of Chunks - empty or used.
	 */
	std::size_t CountEmptyChunks( void ) const;

	/** Determines if FixedAllocator is corrupt.  Checks data members to
	 see if any have erroneous values, or violate class invariants.  It
	 also checks if any Chunk is corrupt.  Complexity is O(C) where C is
	 the number of Chunks.  If any data is corrupt, this will return true
	 in release mode, or assert in debug mode.
	 */
	bool IsCorrupt( void ) const;

	/** Returns true if the block at address p is within a Chunk owned by
	 this FixedAllocator.  Complexity is O(C) where C is the total number
	 of Chunks - empty or used.
	 */
	const Chunk *HasBlock( void *p ) const;
	inline Chunk *HasBlock( void *p )
	{
		return const_cast< Chunk * >(
		           const_cast< const FixedAllocator * >( this )->HasBlock( p ) );
	}

};

unsigned char FixedAllocator::MinObjectsPerChunk_ = 8;
unsigned char FixedAllocator::MaxObjectsPerChunk_ = UCHAR_MAX;

// Chunk::Init ----------------------------------------------------------------

bool Chunk::Init( std::size_t blockSize, unsigned char blocks )
{
	assert(blockSize > 0);
	assert(blocks > 0);
	// Overflow check
	const std::size_t allocSize = blockSize * blocks;
	assert( allocSize / blockSize == blocks);

#ifdef USE_NEW_TO_ALLOCATE
	// If this new operator fails, it will throw, and the exception will get
	// caught one layer up.
	pData_ = static_cast< unsigned char * >( ::operator new ( allocSize ) );
#else
	// malloc can't throw, so its only way to indicate an error is to return
	// a NULL pointer, so we have to check for that.
	pData_ = static_cast< unsigned char * >( ::std::malloc( allocSize ) );
	if ( NULL == pData_ ) return false;
#endif

	Reset( blockSize, blocks );
	return true;
}

// Chunk::Reset ---------------------------------------------------------------

void Chunk::Reset(std::size_t blockSize, unsigned char blocks)
{
	assert(blockSize > 0);
	assert(blocks > 0);
	// Overflow check
	assert((blockSize * blocks) / blockSize == blocks);

	firstAvailableBlock_ = 0;
	blocksAvailable_ = blocks;

	unsigned char i = 0;
	for ( unsigned char *p = pData_; i != blocks; p += blockSize )
	{
		*p = ++i;
	}
}

// Chunk::Release -------------------------------------------------------------

void Chunk::Release()
{
	assert( NULL != pData_ );
#ifdef USE_NEW_TO_ALLOCATE
	::operator delete ( pData_ );
#else
	::std::free( static_cast< void * >( pData_ ) );
#endif
}

// Chunk::Allocate ------------------------------------------------------------

void *Chunk::Allocate(std::size_t blockSize)
{
	if ( IsFilled() ) return NULL;

	assert((firstAvailableBlock_ * blockSize) / blockSize ==
	       firstAvailableBlock_);
	unsigned char *pResult = pData_ + (firstAvailableBlock_ * blockSize);
	firstAvailableBlock_ = *pResult;
	--blocksAvailable_;

	return pResult;
}

// Chunk::Deallocate ----------------------------------------------------------

void Chunk::Deallocate(void *p, std::size_t blockSize)
{
	assert(p >= pData_);

	unsigned char *toRelease = static_cast<unsigned char *>(p);
	// Alignment check
	assert((toRelease - pData_) % blockSize == 0);
	unsigned char index = static_cast< unsigned char >(
	                          ( toRelease - pData_ ) / blockSize);

#if defined(DEBUG) || defined(_DEBUG)
	// Check if block was already deleted.  Attempting to delete the same
	// block more than once causes Chunk's linked-list of stealth indexes to
	// become corrupt.  And causes count of blocksAvailable_ to be wrong.
	if ( 0 < blocksAvailable_ )
		assert( firstAvailableBlock_ != index );
#endif

	*toRelease = firstAvailableBlock_;
	firstAvailableBlock_ = index;
	// Truncation check
	assert(firstAvailableBlock_ == (toRelease - pData_) / blockSize);

	++blocksAvailable_;
}

// Chunk::IsCorrupt -----------------------------------------------------------

bool Chunk::IsCorrupt( unsigned char numBlocks, std::size_t blockSize,
                       bool checkIndexes ) const
{

	if ( numBlocks < blocksAvailable_ )
	{
		// Contents at this Chunk corrupted.  This might mean something has
		// overwritten memory owned by the Chunks container.
		assert( false );
		return true;
	}
	if ( IsFilled() )
		// Useless to do further corruption checks if all blocks allocated.
		return false;
	unsigned char index = firstAvailableBlock_;
	if ( numBlocks <= index )
	{
		// Contents at this Chunk corrupted.  This might mean something has
		// overwritten memory owned by the Chunks container.
		assert( false );
		return true;
	}
	if ( !checkIndexes )
		// Caller chose to skip more complex corruption tests.
		return false;

	/* If the bit at index was set in foundBlocks, then the stealth index was
	 found on the linked-list.
	 */
	std::bitset< UCHAR_MAX > foundBlocks;
	unsigned char *nextBlock = NULL;

	/* The loop goes along singly linked-list of stealth indexes and makes sure
	 that each index is within bounds (0 <= index < numBlocks) and that the
	 index was not already found while traversing the linked-list.  The linked-
	 list should have exactly blocksAvailable_ nodes, so the for loop will not
	 check more than blocksAvailable_.  This loop can't check inside allocated
	 blocks for corruption since such blocks are not within the linked-list.
	 Contents of allocated blocks are not changed by Chunk.

	 Here are the types of corrupted link-lists which can be verified.  The
	 corrupt index is shown with asterisks in each example.

	 Type 1: Index is too big.
	  numBlocks == 64
	  blocksAvailable_ == 7
	  firstAvailableBlock_ -> 17 -> 29 -> *101*
	  There should be no indexes which are equal to or larger than the total
	  number of blocks.  Such an index would refer to a block beyond the
	  Chunk's allocated domain.

	 Type 2: Index is repeated.
	  numBlocks == 64
	  blocksAvailable_ == 5
	  firstAvailableBlock_ -> 17 -> 29 -> 53 -> *17* -> 29 -> 53 ...
	  No index should be repeated within the linked-list since that would
	  indicate the presence of a loop in the linked-list.
	 */
	for ( unsigned char cc = 0; ; )
	{
		nextBlock = pData_ + ( index * blockSize );
		foundBlocks.set( index, true );
		++cc;
		if ( cc >= blocksAvailable_ )
			// Successfully counted off number of nodes in linked-list.
			break;
		index = *nextBlock;
		if ( numBlocks <= index )
		{
			/* This catches Type 1 corruptions as shown in above comments.
			 This implies that a block was corrupted due to a stray pointer
			 or an operation on a nearby block overran the size of the block.
			 */
			assert( false );
			return true;
		}
		if ( foundBlocks.test( index ) )
		{
			/* This catches Type 2 corruptions as shown in above comments.
			 This implies that a block was corrupted due to a stray pointer
			 or an operation on a nearby block overran the size of the block.
			 Or perhaps the program tried to delete a block more than once.
			 */
			assert( false );
			return true;
		}
	}
	if ( foundBlocks.count() != blocksAvailable_ )
	{
		/* This implies that the singly-linked-list of stealth indexes was
		 corrupted.  Ideally, this should have been detected within the loop.
		 */
		assert( false );
		return true;
	}

	return false;
}

// Chunk::IsBlockAvailable ----------------------------------------------------

bool Chunk::IsBlockAvailable( void *p, unsigned char numBlocks,
                              std::size_t blockSize ) const
{
	(void) numBlocks;

	if ( IsFilled() )
		return false;

	unsigned char *place = static_cast< unsigned char * >( p );
	// Alignment check
	assert( ( place - pData_ ) % blockSize == 0 );
	unsigned char blockIndex = static_cast< unsigned char >(
	                               ( place - pData_ ) / blockSize );

	unsigned char index = firstAvailableBlock_;
	assert( numBlocks > index );
	if ( index == blockIndex )
		return true;

	/* If the bit at index was set in foundBlocks, then the stealth index was
	 found on the linked-list.
	 */
	std::bitset< UCHAR_MAX > foundBlocks;
	unsigned char *nextBlock = NULL;
	for ( unsigned char cc = 0; ; )
	{
		nextBlock = pData_ + ( index * blockSize );
		foundBlocks.set( index, true );
		++cc;
		if ( cc >= blocksAvailable_ )
			// Successfully counted off number of nodes in linked-list.
			break;
		index = *nextBlock;
		if ( index == blockIndex )
			return true;
		assert( numBlocks > index );
		assert( !foundBlocks.test( index ) );
	}

	return false;
}

// FixedAllocator::FixedAllocator ---------------------------------------------

FixedAllocator::FixedAllocator()
	: blockSize_( 0 )
	, numBlocks_( 0 )
	, chunks_( 0 )
	, allocChunk_( NULL )
	, deallocChunk_( NULL )
	, emptyChunk_( NULL )
{
}

// FixedAllocator::~FixedAllocator --------------------------------------------

FixedAllocator::~FixedAllocator()
{
#ifdef DO_EXTRA_LOKI_TESTS
	TrimEmptyChunk();
	assert( chunks_.empty() && "Memory leak detected!" );
#endif
	for ( ChunkIter i( chunks_.begin() ); i != chunks_.end(); ++i )
		i->Release();
}

// FixedAllocator::Initialize -------------------------------------------------

void FixedAllocator::Initialize( std::size_t blockSize, std::size_t pageSize )
{
	assert( blockSize > 0 );
	assert( pageSize >= blockSize );
	blockSize_ = blockSize;

	std::size_t numBlocks = pageSize / blockSize;
	if ( numBlocks > MaxObjectsPerChunk_ ) numBlocks = MaxObjectsPerChunk_;
	else if ( numBlocks < MinObjectsPerChunk_ ) numBlocks = MinObjectsPerChunk_;

	numBlocks_ = static_cast<unsigned char>(numBlocks);
	assert(numBlocks_ == numBlocks);
}

// FixedAllocator::CountEmptyChunks -------------------------------------------

std::size_t FixedAllocator::CountEmptyChunks( void ) const
{
#ifdef DO_EXTRA_LOKI_TESTS
	// This code is only used for specialized tests of the allocator.
	// It is #ifdef-ed so that its O(C) complexity does not overwhelm the
	// functions which call it.
	std::size_t count = 0;
	for ( ChunkCIter it( chunks_.begin() ); it != chunks_.end(); ++it )
	{
		const Chunk &chunk = *it;
		if ( chunk.HasAvailable( numBlocks_ ) )
			++count;
	}
	return count;
#else
	return ( NULL == emptyChunk_ ) ? 0 : 1;
#endif
}

// FixedAllocator::IsCorrupt --------------------------------------------------

bool FixedAllocator::IsCorrupt( void ) const
{
	const bool isEmpty = chunks_.empty();
	ChunkCIter start( chunks_.begin() );
	ChunkCIter last( chunks_.end() );
	const size_t emptyChunkCount = CountEmptyChunks();

	if ( isEmpty )
	{
		if ( start != last )
		{
			assert( false );
			return true;
		}
		if ( 0 < emptyChunkCount )
		{
			assert( false );
			return true;
		}
		if ( NULL != deallocChunk_ )
		{
			assert( false );
			return true;
		}
		if ( NULL != allocChunk_ )
		{
			assert( false );
			return true;
		}
		if ( NULL != emptyChunk_ )
		{
			assert( false );
			return true;
		}
	}

	else
	{
		const Chunk *front = &chunks_.front();
		const Chunk *back  = &chunks_.back();
		if ( start >= last )
		{
			assert( false );
			return true;
		}
		if ( back < deallocChunk_ )
		{
			assert( false );
			return true;
		}
		if ( back < allocChunk_ )
		{
			assert( false );
			return true;
		}
		if ( front > deallocChunk_ )
		{
			assert( false );
			return true;
		}
		if ( front > allocChunk_ )
		{
			assert( false );
			return true;
		}

		switch ( emptyChunkCount )
		{
		case 0:
			if ( emptyChunk_ != NULL )
			{
				assert( false );
				return true;
			}
			break;
		case 1:
			if ( emptyChunk_ == NULL )
			{
				assert( false );
				return true;
			}
			if ( back < emptyChunk_ )
			{
				assert( false );
				return true;
			}
			if ( front > emptyChunk_ )
			{
				assert( false );
				return true;
			}
			if ( !emptyChunk_->HasAvailable( numBlocks_ ) )
			{
				// This may imply somebody tried to delete a block twice.
				assert( false );
				return true;
			}
			break;
		default:
			assert( false );
			return true;
		}
		for ( ChunkCIter it( start ); it != last; ++it )
		{
			const Chunk &chunk = *it;
			if ( chunk.IsCorrupt( numBlocks_, blockSize_, true ) )
				return true;
		}
	}

	return false;
}

// FixedAllocator::HasBlock ---------------------------------------------------

const Chunk *FixedAllocator::HasBlock( void *p ) const
{
	const std::size_t chunkLength = numBlocks_ * blockSize_;
	for ( ChunkCIter it( chunks_.begin() ); it != chunks_.end(); ++it )
	{
		const Chunk &chunk = *it;
		if ( chunk.HasBlock( p, chunkLength ) )
			return &chunk;
	}
	return NULL;
}

// FixedAllocator::TrimEmptyChunk ---------------------------------------------

bool FixedAllocator::TrimEmptyChunk( void )
{
	// prove either emptyChunk_ points nowhere, or points to a truly empty Chunk.
	assert( ( NULL == emptyChunk_ ) || ( emptyChunk_->HasAvailable( numBlocks_ ) ) );
	if ( NULL == emptyChunk_ ) return false;

	// If emptyChunk_ points to valid Chunk, then chunk list is not empty.
	assert( !chunks_.empty() );
	// And there should be exactly 1 empty Chunk.
	assert( 1 == CountEmptyChunks() );

	Chunk *lastChunk = &chunks_.back();
	if ( lastChunk != emptyChunk_ )
		std::swap( *emptyChunk_, *lastChunk );
	assert( lastChunk->HasAvailable( numBlocks_ ) );
	lastChunk->Release();
	chunks_.pop_back();

	if ( chunks_.empty() )
	{
		allocChunk_ = NULL;
		deallocChunk_ = NULL;
	}
	else
	{
		if ( deallocChunk_ == emptyChunk_ )
		{
			deallocChunk_ = &chunks_.front();
			assert( deallocChunk_->blocksAvailable_ < numBlocks_ );
		}
		if ( allocChunk_ == emptyChunk_ )
		{
			allocChunk_ = &chunks_.back();
			assert( allocChunk_->blocksAvailable_ < numBlocks_ );
		}
	}

	emptyChunk_ = NULL;
	assert( 0 == CountEmptyChunks() );

	return true;
}

// FixedAllocator::TrimChunkList ----------------------------------------------

bool FixedAllocator::TrimChunkList( void )
{
	if ( chunks_.empty() )
	{
		assert( NULL == allocChunk_ );
		assert( NULL == deallocChunk_ );
	}

	if ( chunks_.size() == chunks_.capacity() )
		return false;
	// Use the "make-a-temp-and-swap" trick to remove excess capacity.
	Chunks( chunks_ ).swap( chunks_ );

	return true;
}

// FixedAllocator::MakeNewChunk -----------------------------------------------

bool FixedAllocator::MakeNewChunk( void )
{
	bool allocated = false;
	try
	{
		std::size_t size = chunks_.size();
		// Calling chunks_.reserve *before* creating and initializing the new
		// Chunk means that nothing is leaked by this function in case an
		// exception is thrown from reserve.
		if ( chunks_.capacity() == size )
		{
			if ( 0 == size ) size = 4;
			chunks_.reserve( size * 2 );
		}
		Chunk newChunk;
		allocated = newChunk.Init( blockSize_, numBlocks_ );
		if ( allocated )
			chunks_.push_back( newChunk );
	}
	catch ( ... )
	{
		allocated = false;
	}
	if ( !allocated ) return false;

	allocChunk_ = &chunks_.back();
	deallocChunk_ = &chunks_.front();
	return true;
}

// FixedAllocator::Allocate ---------------------------------------------------

void *FixedAllocator::Allocate( void )
{
	// prove either emptyChunk_ points nowhere, or points to a truly empty Chunk.
	assert( ( NULL == emptyChunk_ ) || ( emptyChunk_->HasAvailable( numBlocks_ ) ) );
	assert( CountEmptyChunks() < 2 );

	if ( ( NULL == allocChunk_ ) || allocChunk_->IsFilled() )
	{
		if ( NULL != emptyChunk_ )
		{
			allocChunk_ = emptyChunk_;
			emptyChunk_ = NULL;
		}
		else
		{
			for ( ChunkIter i( chunks_.begin() ); ; ++i )
			{
				if ( chunks_.end() == i )
				{
					if ( !MakeNewChunk() )
						return NULL;
					break;
				}
				if ( !i->IsFilled() )
				{
					allocChunk_ = &*i;
					break;
				}
			}
		}
	}
	else if ( allocChunk_ == emptyChunk_)
		// detach emptyChunk_ from allocChunk_, because after
		// calling allocChunk_->Allocate(blockSize_); the chunk
		// is no longer empty.
		emptyChunk_ = NULL;

	assert( allocChunk_ != NULL );
	assert( !allocChunk_->IsFilled() );
	void *place = allocChunk_->Allocate( blockSize_ );

	// prove either emptyChunk_ points nowhere, or points to a truly empty Chunk.
	assert( ( NULL == emptyChunk_ ) || ( emptyChunk_->HasAvailable( numBlocks_ ) ) );
	assert( CountEmptyChunks() < 2 );
#ifdef LOKI_CHECK_FOR_CORRUPTION
	if ( allocChunk_->IsCorrupt( numBlocks_, blockSize_, true ) )
	{
		assert( false );
		return NULL;
	}
#endif

	return place;
}

// FixedAllocator::Deallocate -------------------------------------------------

bool FixedAllocator::Deallocate( void *p, Chunk *hint )
{
	assert(!chunks_.empty());
	assert(&chunks_.front() <= deallocChunk_);
	assert(&chunks_.back() >= deallocChunk_);
	assert( &chunks_.front() <= allocChunk_ );
	assert( &chunks_.back() >= allocChunk_ );
	assert( CountEmptyChunks() < 2 );

	Chunk *foundChunk = ( NULL == hint ) ? VicinityFind( p ) : hint;
	if ( NULL == foundChunk )
		return false;

	assert( foundChunk->HasBlock( p, numBlocks_ * blockSize_ ) );
#ifdef LOKI_CHECK_FOR_CORRUPTION
	if ( foundChunk->IsCorrupt( numBlocks_, blockSize_, true ) )
	{
		assert( false );
		return false;
	}
	if ( foundChunk->IsBlockAvailable( p, numBlocks_, blockSize_ ) )
	{
		assert( false );
		return false;
	}
#endif
	deallocChunk_ = foundChunk;
	DoDeallocate(p);
	assert( CountEmptyChunks() < 2 );

	return true;
}

// FixedAllocator::VicinityFind -----------------------------------------------

Chunk *FixedAllocator::VicinityFind( void *p ) const
{
	if ( chunks_.empty() ) return NULL;
	assert(deallocChunk_);

	const std::size_t chunkLength = numBlocks_ * blockSize_;
	Chunk *lo = deallocChunk_;
	Chunk *hi = deallocChunk_ + 1;
	const Chunk *loBound = &chunks_.front();
	const Chunk *hiBound = &chunks_.back() + 1;

	// Special case: deallocChunk_ is the last in the array
	if (hi == hiBound) hi = NULL;

	for (;;)
	{
		if (lo)
		{
			if ( lo->HasBlock( p, chunkLength ) ) return lo;
			if ( lo == loBound )
			{
				lo = NULL;
				if ( NULL == hi ) break;
			}
			else --lo;
		}

		if (hi)
		{
			if ( hi->HasBlock( p, chunkLength ) ) return hi;
			if ( ++hi == hiBound )
			{
				hi = NULL;
				if ( NULL == lo ) break;
			}
		}
	}

	return NULL;
}

// FixedAllocator::DoDeallocate -----------------------------------------------

void FixedAllocator::DoDeallocate(void *p)
{
	// Show that deallocChunk_ really owns the block at address p.
	assert( deallocChunk_->HasBlock( p, numBlocks_ * blockSize_ ) );
	// Either of the next two assertions may fail if somebody tries to
	// delete the same block twice.
	assert( emptyChunk_ != deallocChunk_ );
	assert( !deallocChunk_->HasAvailable( numBlocks_ ) );
	// prove either emptyChunk_ points nowhere, or points to a truly empty Chunk.
	assert( ( NULL == emptyChunk_ ) || ( emptyChunk_->HasAvailable( numBlocks_ ) ) );

	// call into the chunk, will adjust the inner list but won't release memory
	deallocChunk_->Deallocate(p, blockSize_);

	if ( deallocChunk_->HasAvailable( numBlocks_ ) )
	{
		assert( emptyChunk_ != deallocChunk_ );
		// deallocChunk_ is empty, but a Chunk is only released if there are 2
		// empty chunks.  Since emptyChunk_ may only point to a previously
		// cleared Chunk, if it points to something else besides deallocChunk_,
		// then FixedAllocator currently has 2 empty Chunks.
		if ( NULL != emptyChunk_ )
		{
			// If last Chunk is empty, just change what deallocChunk_
			// points to, and release the last.  Otherwise, swap an empty
			// Chunk with the last, and then release it.
			Chunk *lastChunk = &chunks_.back();
			if ( lastChunk == deallocChunk_ )
				deallocChunk_ = emptyChunk_;
			else if ( lastChunk != emptyChunk_ )
				std::swap( *emptyChunk_, *lastChunk );
			assert( lastChunk->HasAvailable( numBlocks_ ) );
			lastChunk->Release();
			chunks_.pop_back();
			if ( ( allocChunk_ == lastChunk ) || allocChunk_->IsFilled() )
				allocChunk_ = deallocChunk_;
		}
		emptyChunk_ = deallocChunk_;
	}

	// prove either emptyChunk_ points nowhere, or points to a truly empty Chunk.
	assert( ( NULL == emptyChunk_ ) || ( emptyChunk_->HasAvailable( numBlocks_ ) ) );
}

// GetOffset ------------------------------------------------------------------
/// @ingroup SmallObjectGroupInternal
/// Calculates index into array where a FixedAllocator of numBytes is located.
inline std::size_t GetOffset( std::size_t numBytes, std::size_t alignment )
{
	const std::size_t alignExtra = alignment-1;
	return ( numBytes + alignExtra ) / alignment;
}

// DefaultAllocator -----------------------------------------------------------
/** @ingroup SmallObjectGroupInternal
 Calls the default allocator when SmallObjAllocator decides not to handle a
 request.  SmallObjAllocator calls this if the number of bytes is bigger than
 the size which can be handled by any FixedAllocator.
 @param numBytes number of bytes
 @param doThrow True if this function should throw an exception, or false if it
  should indicate failure by returning a NULL pointer.
*/
void *DefaultAllocator( std::size_t numBytes, bool doThrow )
{
#ifdef USE_NEW_TO_ALLOCATE
	return doThrow ? ::operator new( numBytes ) :
	       ::operator new( numBytes, std::nothrow_t() );
#else
	void *p = ::std::malloc( numBytes );
	if ( doThrow && ( NULL == p ) )
		throw std::bad_alloc();
	return p;
#endif
}

// DefaultDeallocator ---------------------------------------------------------
/** @ingroup SmallObjectGroupInternal
 Calls default deallocator when SmallObjAllocator decides not to handle a
 request.  The default deallocator could be the global delete operator or the
 free function.  The free function is the preferred default deallocator since
 it matches malloc which is the preferred default allocator.  SmallObjAllocator
 will call this if an address was not found among any of its own blocks.
 */
void DefaultDeallocator( void *p )
{
#ifdef USE_NEW_TO_ALLOCATE
	::operator delete( p );
#else
	::std::free( p );
#endif
}

// SmallObjAllocator::SmallObjAllocator ---------------------------------------

SmallObjAllocator::SmallObjAllocator( std::size_t pageSize,
                                      std::size_t maxObjectSize, std::size_t objectAlignSize ) :
	pool_( NULL ),
	maxSmallObjectSize_( maxObjectSize ),
	objectAlignSize_( objectAlignSize )
{
#ifdef DO_EXTRA_LOKI_TESTS
	std::cout << "SmallObjAllocator " << this << std::endl;
#endif
	assert( 0 != objectAlignSize );
	const std::size_t allocCount = GetOffset( maxObjectSize, objectAlignSize );
	pool_ = new FixedAllocator[ allocCount ];
	for ( std::size_t i = 0; i < allocCount; ++i )
		pool_[ i ].Initialize( ( i+1 ) * objectAlignSize, pageSize );
}

// SmallObjAllocator::~SmallObjAllocator --------------------------------------

SmallObjAllocator::~SmallObjAllocator( void )
{
#ifdef DO_EXTRA_LOKI_TESTS
	std::cout << "~SmallObjAllocator " << this << std::endl;
#endif
	delete [] pool_;
}

// SmallObjAllocator::TrimExcessMemory ----------------------------------------

bool SmallObjAllocator::TrimExcessMemory( void )
{
	bool found = false;
	const std::size_t allocCount = GetOffset( GetMaxObjectSize(), GetAlignment() );
	std::size_t i = 0;
	for ( ; i < allocCount; ++i )
	{
		if ( pool_[ i ].TrimEmptyChunk() )
			found = true;
	}
	for ( i = 0; i < allocCount; ++i )
	{
		if ( pool_[ i ].TrimChunkList() )
			found = true;
	}

	return found;
}

// SmallObjAllocator::Allocate ------------------------------------------------

void *SmallObjAllocator::Allocate( std::size_t numBytes, bool doThrow )
{
	if ( numBytes > GetMaxObjectSize() )
		return DefaultAllocator( numBytes, doThrow );

	assert( NULL != pool_ );
	if ( 0 == numBytes ) numBytes = 1;
	const std::size_t index = GetOffset( numBytes, GetAlignment() ) - 1;
	const std::size_t allocCount = GetOffset( GetMaxObjectSize(), GetAlignment() );
	(void) allocCount;
	assert( index < allocCount );

	FixedAllocator &allocator = pool_[ index ];
	assert( allocator.BlockSize() >= numBytes );
	assert( allocator.BlockSize() < numBytes + GetAlignment() );
	void *place = allocator.Allocate();

	if ( ( NULL == place ) && TrimExcessMemory() )
		place = allocator.Allocate();

	if ( ( NULL == place ) && doThrow )
	{
#ifdef _MSC_VER
		throw std::bad_alloc( "could not allocate small object" );
#else
		// GCC did not like a literal string passed to std::bad_alloc.
		// so just throw the default-constructed exception.
		throw std::bad_alloc();
#endif
	}
	return place;
}

// SmallObjAllocator::Deallocate ----------------------------------------------

void SmallObjAllocator::Deallocate( void *p, std::size_t numBytes )
{
	if ( NULL == p ) return;
	if ( numBytes > GetMaxObjectSize() )
	{
		DefaultDeallocator( p );
		return;
	}
	assert( NULL != pool_ );
	if ( 0 == numBytes ) numBytes = 1;
	const std::size_t index = GetOffset( numBytes, GetAlignment() ) - 1;
	const std::size_t allocCount = GetOffset( GetMaxObjectSize(), GetAlignment() );
	(void) allocCount;
	assert( index < allocCount );
	FixedAllocator &allocator = pool_[ index ];
	assert( allocator.BlockSize() >= numBytes );
	assert( allocator.BlockSize() < numBytes + GetAlignment() );
	const bool found = allocator.Deallocate( p, NULL );
	(void) found;
	assert( found );
}

// SmallObjAllocator::Deallocate ----------------------------------------------

void SmallObjAllocator::Deallocate( void *p )
{
	if ( NULL == p ) return;
	assert( NULL != pool_ );
	FixedAllocator *pAllocator = NULL;
	const std::size_t allocCount = GetOffset( GetMaxObjectSize(), GetAlignment() );
	Chunk *chunk = NULL;

	for ( std::size_t ii = 0; ii < allocCount; ++ii )
	{
		chunk = pool_[ ii ].HasBlock( p );
		if ( NULL != chunk )
		{
			pAllocator = &pool_[ ii ];
			break;
		}
	}
	if ( NULL == pAllocator )
	{
		DefaultDeallocator( p );
		return;
	}

	assert( NULL != chunk );
	const bool found = pAllocator->Deallocate( p, chunk );
	(void) found;
	assert( found );
}

// SmallObjAllocator::IsCorrupt -----------------------------------------------

bool SmallObjAllocator::IsCorrupt( void ) const
{
	if ( NULL == pool_ )
	{
		assert( false );
		return true;
	}
	if ( 0 == GetAlignment() )
	{
		assert( false );
		return true;
	}
	if ( 0 == GetMaxObjectSize() )
	{
		assert( false );
		return true;
	}
	const std::size_t allocCount = GetOffset( GetMaxObjectSize(), GetAlignment() );
	for ( std::size_t ii = 0; ii < allocCount; ++ii )
	{
		if ( pool_[ ii ].IsCorrupt() )
			return true;
	}
	return false;
}

} // end namespace Loki

