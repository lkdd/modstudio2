/*
Copyright (c) 2008 Peter "Corsix" Cawley

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/
#pragma once
#include <memory>

//! Memory allocator for Lua, intended to be used during the inheritance building process
/*!
  This allocator begins by allocating a large (i.e. many MB) block of memory all at once, then
  splits this into lots of small (<= 4kb) blocks, and dishes out small blocks whenever it can,
  only reverting to another heap allocation if the requested block is too large. The aim is to
  never have to allocate anything more on the heap after the large initial allocation, or very
  rarely allocate anything more on the heap.

  The philosophy of the allocator is to expend alot of memory (often wasting ALOT of memory) and
  in exchange for all this memory, squeeze out some extra speed. For any allocation request that
  is <= 4kb, an entire 4kb block is returned. For re-allocation requests up to 4kb, the block is
  reused completely unchanged, and is replaced with a new heap block if larger than 4kb. For
  downsizing re-allocation requests, the block is reused unchanged, unless it was a heap block,
  and is now small enough to fit into 4kb, in which case the heap block is freed and replaced by
  a 4kb block. This means that any and all blocks <= 4kb in size are 4kb blocks from the large
  memory pool.

  The allocator fails if all of its small blocks are taken and another one is requested, causing
  Lua to yield a memory error.
*/
class LuaShortSharpAlloc
{
public:
  LuaShortSharpAlloc();
  virtual ~LuaShortSharpAlloc();

  typedef unsigned char byte;

  byte* alloc(byte* ptr, size_t osize, size_t nsize);

  inline bool isPoolRunningLow() {return m_iAllocatedSmallCount * 2 > small_block_count;}

  static void* allocf(void *ud, void *ptr, size_t osize, size_t nsize);
protected:
  byte* _allocNextSmall();
  void _freeSmall(byte* ptr);

  static const int small_block_size  = 4096; //!< Size, in bytes, of a 'small' memory block
  static const int small_block_count = 8192; //!< The number of 'small' memory blocks to have in the pool

  std::allocator<byte> m_oFallbackAllocator;
  byte* m_pSmallBlocks;
  bool* m_bInUse;
  int m_iNextSmallBlock;
  int m_iAllocatedSmallCount;
};
