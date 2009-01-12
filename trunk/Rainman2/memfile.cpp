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
#include "memfile.h"

MemoryReadFile::MemoryReadFile(const char *pBuffer, size_t iSize, bool bTakeOwnership) throw()
{
  m_pBuffer = m_pPointer = pBuffer;
  m_iSize = iSize;
  m_pEnd = m_pBuffer + iSize;
  m_bOwnsBuffer = bTakeOwnership;
}

MemoryReadFile::~MemoryReadFile()
{
  if(m_bOwnsBuffer)
    delete[] const_cast<char*>(m_pBuffer);
}

void MemoryReadFile::write(const void* pSource, size_t iItemSize, size_t iItemCount) throw(...)
{
  THROW_SIMPLE(L"Cannot write to a memory read file");
}

size_t MemoryReadFile::writeNoThrow(const void* pSource, size_t iItemSize, size_t iItemCount) throw()
{
  return 0;
}

MemoryWriteFile::MemoryWriteFile(size_t iInitialSize) throw(...)
{
  CHECK_ALLOCATION(m_pBuffer = m_pPointer = m_pEnd = new (std::nothrow) char[iInitialSize]);
  m_pBufferEnd = m_pBuffer + iInitialSize;
  m_iSize = iInitialSize;
}

MemoryWriteFile::~MemoryWriteFile() throw()
{
  delete[] m_pBuffer;
}

void MemoryWriteFile::write(const void* pSource, size_t iItemSize, size_t iItemCount) throw(...)
{
  size_t iBytes = iItemSize * iItemCount;
  if(m_pPointer + iBytes >= m_pBufferEnd)
  {
    m_iSize = m_iSize * 2 + iBytes;
    char *pNewBuffer = CHECK_ALLOCATION(new (std::nothrow) char[m_iSize]);
    m_pBufferEnd = pNewBuffer + m_iSize;
    m_pEnd = m_pEnd - m_pBuffer + pNewBuffer;
    std::copy(m_pBuffer, m_pPointer, pNewBuffer);
    m_pPointer = m_pPointer - m_pBuffer + pNewBuffer; 
    delete[] m_pBuffer;
    m_pBuffer = pNewBuffer;
  }
  memcpy(m_pPointer, pSource, iBytes);
  m_pPointer += iBytes;
  if(m_pPointer > m_pEnd)
    m_pEnd = m_pPointer;
}

size_t MemoryWriteFile::writeNoThrow(const void* pSource, size_t iItemSize, size_t iItemCount) throw()
{
  size_t iBytes = iItemSize * iItemCount;
  if(m_pPointer + iBytes >= m_pBufferEnd)
  {
    char *pNewBuffer = new (std::nothrow) char[m_iSize << 1];
    if(pNewBuffer)
    {
      m_iSize <<= 1;
      m_pBufferEnd = pNewBuffer + m_iSize;
      m_pEnd = m_pEnd - m_pBuffer + pNewBuffer;
      std::copy(m_pBuffer, m_pPointer, pNewBuffer);
      m_pPointer = m_pPointer - m_pBuffer + pNewBuffer;
      delete[] m_pBuffer;
      m_pBuffer = pNewBuffer;
    }
    else
    {
      iItemCount = (m_pBufferEnd - m_pPointer) / iItemSize;
      iBytes = iItemSize * iItemCount;
    }
  }
  memcpy(m_pPointer, pSource, iBytes);
  m_pPointer += iBytes;
  if(m_pPointer > m_pEnd)
    m_pEnd = m_pPointer;
  return iItemCount;
}
