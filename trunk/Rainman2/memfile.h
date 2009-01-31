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
#include "file.h"
#include "exception.h"

//! Common code in both MemoryReadFile and MemoryReadFile
/*!
  TCharPtr should be either `char*` or `const char*` depending on whether or not
  the buffers used by the class are constant or not.
  Contains implementations of read, seek and tell functions. write functions must
  be provided in a derived class.
*/
template <class TCharPtr>
class RAINMAN2_API MemoryFileBase : public IFile
{
public:
  virtual void read(void* pDestination, size_t iItemSize, size_t iItemCount) throw(...)
  {
    size_t iBytes = iItemSize * iItemCount;
    if(m_pPointer + iBytes > m_pEnd)
    {
      THROW_SIMPLE_(L"Reading %lu items of size %lu would exceed the memory buffer (only %lu bytes remaining)",
        static_cast<unsigned long>(iItemCount), static_cast<unsigned long>(iItemSize), static_cast<unsigned long>(m_pEnd - m_pPointer));
    }
    memcpy(pDestination, m_pPointer, iBytes);
    m_pPointer += iBytes;
  }

  virtual size_t readNoThrow(void* pDestination, size_t iItemSize, size_t iItemCount) throw()
  {
    size_t iBytes = iItemSize * iItemCount;
    if(m_pPointer + iBytes > m_pEnd)
    {
      iItemCount = (m_pEnd - m_pPointer) / iItemSize;
      iBytes = iItemSize * iItemCount;
    }
    memcpy(pDestination, m_pPointer, iBytes);
    m_pPointer += iBytes;
    return iItemCount;
  }

  virtual void seek(seek_offset_t iOffset, seek_relative_t eRelativeTo) throw(...)
  {
    seekNoThrow(iOffset, eRelativeTo);
  }

  virtual bool seekNoThrow(seek_offset_t iOffset, seek_relative_t eRelativeTo) throw()
  {
    switch(eRelativeTo)
    {
    case SR_Start:
      m_pPointer = m_pBuffer + iOffset;
      break;

    case SR_Current:
      m_pPointer = m_pPointer + iOffset;
      break;

    case SR_End:
      m_pPointer = m_pEnd - iOffset;
      break;
    };
    return true;
  }

  virtual seek_offset_t tell() throw()
  {
    return static_cast<seek_offset_t>(m_pPointer - m_pBuffer);
  }

protected:
  TCharPtr m_pBuffer, m_pPointer, m_pEnd;
  size_t m_iSize;
};

class RAINMAN2_API MemoryReadFile : public MemoryFileBase<const char*>
{
public:
  MemoryReadFile(const char *pBuffer, size_t iSize, bool bTakeOwnership = false) throw();
  virtual ~MemoryReadFile() throw();

  virtual void write(const void* pSource, size_t iItemSize, size_t iItemCount) throw(...);
  virtual size_t writeNoThrow(const void* pSource, size_t iItemSize, size_t iItemCount) throw();

protected:
  bool m_bOwnsBuffer;
};

class RAINMAN2_API MemoryWriteFile : public MemoryFileBase<char*>
{
public:
  MemoryWriteFile(size_t iInitialSize = 1024) throw(...);
  virtual ~MemoryWriteFile() throw();

  inline char* getBuffer() throw() {return m_pBuffer;}
  inline size_t getLengthUsed() throw() {return m_pEnd - m_pBuffer;}

  virtual void write(const void* pSource, size_t iItemSize, size_t iItemCount) throw(...);
  virtual size_t writeNoThrow(const void* pSource, size_t iItemSize, size_t iItemCount) throw();

protected:
  char *m_pBufferEnd;
};
