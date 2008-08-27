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
#include "CustomLuaAlloc.h"

LuaShortSharpAlloc::LuaShortSharpAlloc()
{
  size_t iByteCount = small_block_size * small_block_count;
  if(iByteCount == 0)
    iByteCount = 1;
  m_pSmallBlocks = new byte[iByteCount];
  m_bInUse = new bool[small_block_count];
  std::fill(m_pSmallBlocks, m_pSmallBlocks + iByteCount, 0xCE);
  std::fill(m_bInUse, m_bInUse + small_block_count, false);
  m_iNextSmallBlock = 0;
}

LuaShortSharpAlloc::~LuaShortSharpAlloc()
{
  delete[] m_pSmallBlocks;
  delete[] m_bInUse;
}

LuaShortSharpAlloc::byte* LuaShortSharpAlloc::alloc(LuaShortSharpAlloc::byte* ptr, size_t osize, size_t nsize)
{
#define alloc_heap m_oFallbackAllocator.allocate(nsize)
#define dealloc_heap m_oFallbackAllocator.deallocate(ptr, osize)
#define alloc_small _allocNextSmall()
#define dealloc_small _freeSmall(ptr)
  if(ptr == NULL)
  {
    // Allocating size of 0 => NULL
    if(nsize == 0)
      return 0;
    // Allocating size of small block => next small block
    else if(nsize <= small_block_size)
      return alloc_small;
    // Allocating something larger than a small block => heap block
    else
      return alloc_heap;
  }
  else
  {
    if(nsize <= osize)
    {
      // Downsizing
      if(nsize == 0)
      {
        // Freeing
        if(osize > small_block_size)
          dealloc_heap;
        else
          dealloc_small;
        return 0;
      }
      if(osize > small_block_size && nsize <= small_block_size)
      {
        // Moving from heap block to small block
        dealloc_heap;
        return alloc_small;
      }
      // Downsizing, return the old pointer
      return ptr;
    }
    // Increasing in size
    if(nsize <= small_block_size)
    {
      // Must have been a small, and still is
      return ptr;
    }
    // Increasing from small or heap to (larger) heap
    byte* new_ptr = alloc_heap;
    memcpy(new_ptr, ptr, osize);
    if(osize > small_block_size)
      dealloc_heap;
    else
      dealloc_small;
    return new_ptr;
  }
#undef dealloc_heap
#undef alloc_heap
#undef dealloc_small
#undef alloc_small
}

LuaShortSharpAlloc::byte* LuaShortSharpAlloc::_allocNextSmall()
{
  while(m_bInUse[m_iNextSmallBlock])
    m_iNextSmallBlock = ++m_iNextSmallBlock % small_block_count;
  byte* ret = m_pSmallBlocks + (small_block_size * m_iNextSmallBlock);
  m_bInUse[m_iNextSmallBlock] = true;
  if(++m_iAllocatedSmallCount == small_block_count)
    return --m_iAllocatedSmallCount, 0; // eek
  return ret;
}

void LuaShortSharpAlloc::_freeSmall(byte* ptr)
{
  --m_iAllocatedSmallCount;
  m_bInUse[(ptr - m_pSmallBlocks) / small_block_size] = false;
}

void* LuaShortSharpAlloc::allocf(void *ud, void *ptr, size_t osize, size_t nsize)
{
  return reinterpret_cast<void*>(reinterpret_cast<LuaShortSharpAlloc*>(ud)->alloc(reinterpret_cast<byte*>(ptr), osize, nsize));
}
