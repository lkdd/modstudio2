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
#pragma warning(push)
#pragma warning(disable: 4996)
#include "file.h"

template <class TChar, size_t iBufferLength = 1024>
class BufferingInputTextStream
{
public:
  BufferingInputTextStream(IFile* pFile) throw()
    : m_pFile(pFile), m_iBufferPosition(0), m_bEndOfFile(false)
  {
  }

  RainString readLine(RainChar cDelimiter = '\n') throw(...)
  {
    if(m_bEndOfFile)
      THROW_SIMPLE(L"End of file reached");

    size_t iDelimiterIndex;
    while((iDelimiterIndex = m_sBuffer.indexOf(cDelimiter, m_iBufferPosition)) == static_cast<size_t>(-1))
    {
      TChar cBuffer[iBufferLength];
      size_t iNumBytes = m_pFile->readArrayNoThrow(cBuffer, iBufferLength);
      if(iNumBytes == 0)
      {
        m_bEndOfFile = true;
        return m_sBuffer.mid(m_iBufferPosition, m_sBuffer.length() - m_iBufferPosition);
      }
      else
      {
        m_sBuffer.append(cBuffer, iNumBytes);
      }
    }

    ++iDelimiterIndex;
    RainString sReturnValue = m_sBuffer.mid(m_iBufferPosition, iDelimiterIndex - m_iBufferPosition);
    m_iBufferPosition = iDelimiterIndex;
    if(m_iBufferPosition > iBufferLength)
    {
      m_sBuffer = m_sBuffer.mid(m_iBufferPosition, m_sBuffer.length() - m_iBufferPosition);
      m_iBufferPosition = 0;
    }
    return sReturnValue;
  }

  bool isEOF() const
  {
    return m_bEndOfFile;
  }

protected:
  RainString m_sBuffer;
  IFile *m_pFile;
  size_t m_iBufferPosition;
  bool m_bEndOfFile;
};

template <class TChar, size_t iBufferLength = 1024>
class BufferingOutputStream
{
public:
  BufferingOutputStream(IFile* pFile) throw()
    : m_pFile(pFile), m_iBufferSize(0)
  {
  }

  ~BufferingOutputStream() throw()
  {
    try
    {
      flush();
    }
    catch(RainException *pE)
    {
      delete pE;
    }
  }

  void write(const TChar* pZeroTerminatedBuffer) throw(...)
  {
    const TChar* pZero;
    for(pZero = pZeroTerminatedBuffer; *pZero != 0; ++pZero)
    {
    }
    write(pZeroTerminatedBuffer, pZero - pZeroTerminatedBuffer);
  }

  template <class TChar2, typename TConvertor>
  void writeConverting(const TChar2* pBuffer, size_t iLength, TConvertor fnConvertor)
  {
    // Append as much as possible to the buffer
    size_t iToAppend = std::min(iBufferLength - m_iBufferSize, iLength);
    std::transform(pBuffer, pBuffer + iToAppend, m_aBuffer + m_iBufferSize, fnConvertor);
    m_iBufferSize += iToAppend;
    pBuffer += iToAppend;
    iLength -= iToAppend;
    if(m_iBufferSize == iBufferLength)
      flush();

    // Write entire chunks to the file
    while(iLength >= iBufferLength)
    {
      m_iBufferSize = iBufferLength;
      std::transform(pBuffer, pBuffer + iBufferLength, m_aBuffer, fnConvertor);
      pBuffer += iBufferLength;
      iLength -= iBufferLength;
      flush();
    }

    // Put the remainder in the buffer
    if(iLength)
    {
      m_iBufferSize = iLength;
      std::transform(pBuffer, pBuffer + iLength, m_aBuffer, fnConvertor);
    }
  }

  template <typename TConvertor>
  void writeConverting(const RainString& str, TConvertor fnConvertor)
  {
    writeConverting(str.getCharacters(), str.length(), fnConvertor);
  }

  void write(TChar cCharacter) throw(...)
  {
    write(&cCharacter, 1);
  }

  void write(const TChar* pBuffer, size_t iLength) throw(...)
  {
    // Append as much as possible to the buffer
    size_t iToAppend = std::min(iBufferLength - m_iBufferSize, iLength);
    std::copy(pBuffer, pBuffer + iToAppend, m_aBuffer + m_iBufferSize);
    m_iBufferSize += iToAppend;
    pBuffer += iToAppend;
    iLength -= iToAppend;
    if(m_iBufferSize == iBufferLength)
      flush();

    // Write entire chunks to the file
    if(iLength >= iBufferLength)
    {
      size_t iToWrite = (iLength / iBufferLength) * iBufferLength;
      m_pFile->writeArray(pBuffer, iToWrite);
      pBuffer += iToWrite;
      iLength -= iToWrite;
    }

    // Put the remainder in the buffer
    if(iLength)
    {
      m_iBufferSize = iLength;
      std::copy(pBuffer, pBuffer + iLength, m_aBuffer);
    }
  }

  void flush() throw(...)
  {
    if(m_iBufferSize)
    {
      m_pFile->writeArray(m_aBuffer, m_iBufferSize);
      m_iBufferSize = 0;
    }
  }

protected:
  IFile *m_pFile;
  size_t m_iBufferSize;
  TChar m_aBuffer[iBufferLength];
};

#pragma warning(pop)
