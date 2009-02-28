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
#include "api.h"
#include <memory.h>
#include <malloc.h>
#include <stdlib.h>

template <class T, class TSize = size_t>
class RAINMAN2_API RainSimpleContainer
{
public:
  typedef T  value_type;
  typedef T* pointer;
  typedef T& reference;
  typedef T* iterator;

  RainSimpleContainer(TSize iInitialAllocation = 0)
    : m_pMemory(0)
    , m_iSize(0)
    , m_iNumAllocated(iInitialAllocation)
  {
    if(iInitialAllocation != 0)
      m_pMemory = reinterpret_cast<T*>(malloc(sizeof(T) * m_iNumAllocated));
  }

  ~RainSimpleContainer()
  {
    free(m_pMemory);
  }

  TSize size() const
  {
    return m_iSize;
  }

  bool empty() const
  {
    return m_iSize == 0;
  }

  TSize max_size() const
  {
    return static_cast<TSize>(~0) / static_cast<TSize>(sizeof(T));
  }

  iterator begin()
  {
    return m_pMemory;
  }

  iterator end()
  {
    return m_pMemory + m_iSize;
  }

  T& operator* ()
  {
    return *m_pMemory;
  }

  const T& operator* () const
  {
    return *m_pMemory;
  }

  const T& operator[] (TSize iIndex) const
  {
    return m_pMemory[iIndex];
  }

  T& operator[] (TSize iIndex)
  {
    if(iIndex >= m_iSize)
    {
      m_iSize = iIndex + 1;
      if(m_iSize > m_iNumAllocated)
      {
        m_iNumAllocated = m_iSize * 2;
        m_pMemory = reinterpret_cast<T*>(realloc(m_pMemory, sizeof(T) * m_iNumAllocated));
      }
    }
    return m_pMemory[iIndex];
  }

  void reserve (TSize iCount)
  {
    iCount += m_iSize;
    if(iCount > m_iNumAllocated)
    {
      m_iNumAllocated = iCount;
      m_pMemory = reinterpret_cast<T*>(realloc(m_pMemory, sizeof(T) * m_iNumAllocated));
    }
  }

  T& last()
  {
    return (*this)[size() - 1];
  }

  bool pop_back()
  {
    if(size() >= 1)
    {
      --m_iSize;
      return true;
    }
    return false;
  }

  T& push_back()
  {
    return (*this)[size()];
  }

  void push_back(const T& value)
  {
    (*this)[size()] = value;
  }

  template <class TItr>
  void push_back(TItr begin, TItr end)
  {
    for(; begin != end; ++begin)
      push_back(*begin);
  }

  void push_back(const RainSimpleContainer<T, TSize>& cOther)
  {
    // Ensure memory allocated and set new size
    if(size() != 0 || cOther.size() != 0)
      operator[](size() + cOther.size() - 1);

    // Copy over data
    memcpy(end() - cOther.size(), cOther.m_pMemory, sizeof(T) * cOther.size());
  }

protected:
  T* m_pMemory;
  TSize m_iSize;
  TSize m_iNumAllocated;
};
