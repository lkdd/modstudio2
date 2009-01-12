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
#include "string.h"
#include "exception.h"
#include "va_copy.h"
#include <memory.h>
#include <new>
#include <locale>
#include <functional>
#include <limits>
#include <wchar.h>
extern "C" {
#include <lua.h>
}
#include <wx/string.h>
#include "new_trace.h"

#pragma warning(disable: 4996)
#define const_begin const_cast<const RainString*>(this)->begin

//! The buffer used by RainString
/*!
  A buffer holds all the data required for a RainString:
   * The length of the string
   * The characters of the string
   * The length of the buffer
   * The number of strings using the buffer
  The length of the buffer will always be greater than the length of the
  string, for two reasons:
   * The length of the string does not include the \0 on the end
   * There is often room in the buffer to allow the string to grow without
     having to reallocate memory
  The buffer actually operates in two different modes:
   * mini-buffer mode: the characters of the string are stored within the
     buffer structure itself. This is done when the string length is <= 4
     (5 characters in the buffer including the terminating \0)
   * full-buffer mode: the characters of the string are stored in the heap
     (operator new[] / delete[]) and just a pointer is stored
*/
struct rain_string_buffer_t
{
  //! Construct a buffer with the given length of characters, and initialise it to an empty string
  rain_string_buffer_t(size_t iLength) throw(...)
    : pBuffer(0), iLengthUsed(0), cLengthUsed(0), iBufferLength(iLength), iReferenceCount(1)
  {
    // 1 character is required to store the empty string (due to the \0 terminator), so
    // if length was 0, pretend it was 1
    if(iLength == 0)
       iBufferLength = iLength = 1;

    // Allocate the memory on the heap if the mini-buffer isn't being used
    if(isUsingMiniBuffer())
      iBufferLength = MINI_BUFFER_SIZE;
    else
      CHECK_ALLOCATION(pBuffer = new NOTHROW RainChar[iLength]);

    // Initialise to the empty string
    std::fill(getBuffer(), getBuffer() + iBufferLength, 0);
  }

  //! Free the buffer
  /*!
    Decrements the reference count, and then deletes the buffer and any associated memory if
    the reference count is now 0 (i.e. nobody using the buffer)
  */
  void free() throw()
  {
    if(--iReferenceCount == 0)
    {
      if(!isUsingMiniBuffer())
        delete[] pBuffer;
      delete this;
    }
  }

  //! Ensure that the buffer length is >= the specified length
  void upsize(size_t iNewLength) throw(...)
  {
    if(iNewLength <= iBufferLength || (isUsingMiniBuffer() && iNewLength <= MINI_BUFFER_SIZE))
      return;
    RainChar* pNewBuffer;
    CHECK_ALLOCATION(pNewBuffer = new NOTHROW RainChar[iNewLength]);
    std::copy(getBuffer(), getBuffer() + iBufferLength, pNewBuffer);
    std::fill(pNewBuffer + iBufferLength, pNewBuffer + iNewLength, 0);
    if(isUsingMiniBuffer())
      iLengthUsed = cLengthUsed;
    else
      delete[] pBuffer;
    pBuffer = pNewBuffer;
    iBufferLength = iNewLength;
  }

  inline bool isUsingMiniBuffer() const
  {
    return iBufferLength <= MINI_BUFFER_SIZE;
  }

  inline size_t getLengthUsed() const
  {
    return isUsingMiniBuffer() ? static_cast<size_t>(cLengthUsed) : iLengthUsed;
  }

  inline RainChar* getBuffer()
  {
    return isUsingMiniBuffer() ? &aMiniBuffer[0] : pBuffer;
  }

  static const int MINI_BUFFER_SIZE = 5;

  union
  {
    struct
    {
      size_t iLengthUsed;
      RainChar* pBuffer;
    };
    struct
    {
      RainChar cLengthUsed;
      RainChar aMiniBuffer[MINI_BUFFER_SIZE];
    };
  };
  size_t iBufferLength;
  size_t iReferenceCount;
};

RainString::RainString() throw(...)
{
  CHECK_ALLOCATION(m_pBuffer = new NOTHROW rain_string_buffer_t(1));
}

RainString::RainString(wxString s) throw(...)
{
  _initFromChars(s.c_str(), s.size());
}

RainString::RainString(const RainString& oCopyFrom) throw()
{
  m_pBuffer = oCopyFrom.m_pBuffer;
  ++m_pBuffer->iReferenceCount;
}

RainString::RainString(lua_State *L, int idx) throw(...)
{
  size_t len;
  const char* str = lua_tolstring(L, idx, &len);
  _initFromChars(str, len);
}

RainString& RainString::operator= (const RainChar* sZeroTermString) throw(...)
{
  size_t iLength = sZeroTermString ? wcslen(sZeroTermString) : 0;
  if(m_pBuffer->iReferenceCount == 1 && iLength < m_pBuffer->iBufferLength)
  {
    std::copy(sZeroTermString, sZeroTermString + iLength + 1, m_pBuffer->getBuffer());
    if(m_pBuffer->isUsingMiniBuffer())
      m_pBuffer->cLengthUsed = static_cast<RainChar>(iLength);
    else
      m_pBuffer->iLengthUsed = iLength;
  }
  else
  {
    m_pBuffer->free();
    m_pBuffer = 0;
    _initFromChars(sZeroTermString, iLength);
  }
  return *this;
}

RainString& RainString::operator= (const char* sZeroTermString) throw(...)
{
  size_t iLength = sZeroTermString ? strlen(sZeroTermString) : 0;
  if(m_pBuffer->iReferenceCount == 1 && iLength < m_pBuffer->iBufferLength)
  {
    std::copy(sZeroTermString, sZeroTermString + iLength + 1, m_pBuffer->getBuffer());
    if(m_pBuffer->isUsingMiniBuffer())
      m_pBuffer->cLengthUsed = static_cast<RainChar>(iLength);
    else
      m_pBuffer->iLengthUsed = iLength;
  }
  else
  {
    m_pBuffer->free();
    m_pBuffer = 0;
    _initFromChars(sZeroTermString, iLength);
  }
  return *this;
}

RainString& RainString::operator= (const RainString& oCopyFrom) throw()
{
  // If doing S = S, with refCount == 1, we want to increment refcount before freeing
  ++oCopyFrom.m_pBuffer->iReferenceCount;
  m_pBuffer->free();
  m_pBuffer = oCopyFrom.m_pBuffer;
  return *this;
}

RAINMAN2_API RainString operator+(const RainString& sA, const wchar_t* sB)
{
  return sA + RainString(sB);
}

RAINMAN2_API RainString operator+(const wchar_t* sA, const RainString& sB)
{
  return RainString(sA) + sB;
}

RainString::operator wxString() const
{
  return wxString(begin(), length());
}

void RainString::clear() throw(...)
{
  *this = "";
}

void RainString::resize(size_type iNewLength) throw(...)
{
  resize(iNewLength, RainChar());
}

void RainString::resize(size_type iNewLength, RainChar cDefault) throw(...)
{
  if(iNewLength < size())
    erase(begin() + iNewLength, end());
  //else if(iNewLength > size())
    //insert(end(), iNewLength - size(), cDefault); TODO
}

RainString::iterator RainString::erase(RainString::iterator item) throw(...)
{
  return erase(item, item + 1);
}

RainString::iterator RainString::erase(RainString::iterator range_begin, RainString::iterator range_end) throw(...)
{
  difference_type iReturnPosition = range_begin - const_begin();
  difference_type iRangeSize = range_end - range_begin;
  if(range_end != const_cast<const RainString*>(this)->end())
  {
    _ensureExclusiveBufferAccess(range_begin, range_end);

    // Reverse range_end...end() then range_begin...range_end...end()
    // This places the range (back to front) at the end of the container
    std::reverse(range_end, end());
    std::reverse(range_begin, end());
    range_begin = end() - iRangeSize;
    range_end = end();
  }
  else if(range_begin == const_begin())
  {
    // optimisation case for erasing the entire container
    clear();
    return begin();
  }
  
  // clear the end of the container to zeros then decrease the length
  _ensureExclusiveBufferAccess(range_begin, range_end);
  std::fill(range_begin, range_end, 0);
  if(m_pBuffer->isUsingMiniBuffer())
    m_pBuffer->cLengthUsed -= static_cast<RainChar>(iRangeSize);
  else
    m_pBuffer->iLengthUsed -= static_cast<size_t>(iRangeSize);

  return begin() + iReturnPosition;
}

void RainString::swap(RainString& other) throw()
{
  std::swap(m_pBuffer, other.m_pBuffer);
}

RainString::~RainString() throw()
{
  m_pBuffer->free();
}

RainString& RainString::replaceAll(RainChar cFind, RainChar cReplace) throw(...)
{
  std::replace(begin(), end(), cFind, cReplace);
  return *this;
}

RainString& RainString::replaceAll(const RainString& sNeedle, const RainString& sReplacement) throw(...)
{
  size_t iLookIndex = 0;
  size_t iCommonLength = std::min(sNeedle.length(), sReplacement.length());
  while((iLookIndex = indexOf(sNeedle, iLookIndex)) != NOT_FOUND)
  {
    _ensureExclusiveBufferAccess();
    std::copy(sReplacement.begin(), sReplacement.begin() + iCommonLength, begin() + iLookIndex);
    if(sNeedle.length() < sReplacement.length())
    {
      insert(begin() + iLookIndex + iCommonLength, sReplacement.begin() + iCommonLength, sReplacement.end());
    }
    else
    {
      erase(begin() + iLookIndex + iCommonLength, begin() + iLookIndex + (sNeedle.length() - iCommonLength));
    }
    iLookIndex += sReplacement.length();
  }
  return *this;
}

void RainString::insert(iterator position, const_iterator to_insert, const_iterator to_insert_end) throw(...)
{
  if(to_insert_end <= to_insert)
    return;
  size_t iInsertionLength = to_insert_end - to_insert;

  // Make space at the end of the buffer
  {
    size_t iPos = position - begin();
    _ensureExclusiveBufferAccess();
    m_pBuffer->upsize(length() + iInsertionLength + 1);
    position = begin() + iPos;
  }

  // Shift things to make space in the middle of the buffer
  for(RainChar *sPointer = end(); sPointer >= position; --sPointer)
  {
    sPointer[iInsertionLength] = *sPointer;
  }

  // Copy the new text in
  std::copy(to_insert, to_insert_end, position);

  // Update length
  if(m_pBuffer->isUsingMiniBuffer())
    m_pBuffer->cLengthUsed += static_cast<RainChar>(iInsertionLength);
  else
    m_pBuffer->iLengthUsed += iInsertionLength;
}

RainString& RainString::toLower() throw(...)
{
  std::transform(begin(), end(), begin(), towlower);
  return *this;
}

RainString& RainString::toUpper() throw(...)
{
  std::transform(begin(), end(), begin(), towupper);
  return *this;
}

RainString RainString::repeat(size_t iCount) const throw(...)
{
  size_t iLength = iCount * length();

  RainString sNew(*this);
  sNew.m_pBuffer->upsize(iLength + 1);
  while(sNew.length() < iLength)
  {
    sNew.append(sNew.getCharacters(), std::min(sNew.length(), iLength - sNew.length()));
  }
  return sNew;
}

size_t RainString::indexOf(RainChar cCharacter, size_t iStartAt, size_t iNotFoundValue) const
{
  const_iterator begin = this->begin() + iStartAt, end = this->end();
  if(begin >= end)
    return iNotFoundValue;
  const_iterator position = std::find(begin, end, cCharacter);
  if(position == end)
    return iNotFoundValue;
  else
    return position - begin + iStartAt;
}

size_t RainString::indexOf(const RainString& sString, size_t iStartAt, size_t iNotFoundValue) const
{
  if(sString.length() == 0)
    return iStartAt;

  size_t iStringLength = sString.length();
  RainChar cStringInitial = sString[0];

  while((iStartAt = indexOf(cStringInitial, iStartAt, iNotFoundValue)) != iNotFoundValue)
  {
    if((iStartAt + iStringLength) <= length())
    {
      if(std::equal(sString.begin() + 1, sString.end(), begin() + iStartAt + 1))
        return iStartAt;
    }
  }

  return iStartAt;
}

RainString RainString::trimWhitespace() const throw(...)
{
  const RainChar* pChars = getCharacters();
  size_t iBegin = 0;
  while(RainStrFunctions<wchar_t>::isWhitespace(pChars[iBegin]))
    ++iBegin;
  size_t iEnd = length();
  while(iEnd > iBegin && RainStrFunctions<wchar_t>::isWhitespace(pChars[iEnd - 1]))
    --iEnd;
  return mid(iBegin, iEnd - iBegin);
}

RainString& RainString::printfV(const RainString& sFormat, va_list v) throw(...)
{
  _ensureExclusiveBufferAccess();
  int iLength;
  va_list vArgs;
  RainVaCopy(vArgs, v);
  while((iLength = _vsnwprintf(m_pBuffer->getBuffer(), m_pBuffer->iBufferLength - 1, sFormat.getCharacters(), vArgs)) == -1)
  {
    va_end(vArgs);
    size_t iNewLength = m_pBuffer->iBufferLength * 2 + 8;
    if(iNewLength >= (1024 * 1024 * 16))
    {
      // printf into a 8mb buffer failed, and we're now trying to printf into a 16mb buffer.
      // in 99.9% of times, this means something has gone horribly wrong
      // See https://connect.microsoft.com/VisualStudio/feedback/ViewFeedback.aspx?FeedbackID=376809
      *this = L"[[--Unrenderable formatted message (in " __WFILE__ L") --]]";
      return *this;
    }
    RainChar* pNewChars;
    if(!m_pBuffer->isUsingMiniBuffer())
      delete[] m_pBuffer->pBuffer;
    CHECK_ALLOCATION(pNewChars = new NOTHROW RainChar[iNewLength]);
    m_pBuffer->pBuffer = pNewChars;
    m_pBuffer->iBufferLength = iNewLength;
    m_pBuffer->iLengthUsed = 0;
    pNewChars[0] = 0;
    RainVaCopy(vArgs, v);
  }
  va_end(vArgs);
  if(m_pBuffer->isUsingMiniBuffer())
    m_pBuffer->cLengthUsed = static_cast<RainChar>(iLength);
  else
    m_pBuffer->iLengthUsed = iLength;
  return *this;
}

RainString& RainString::printf(const RainString sFormat, ...) throw(...)
{
  va_list vArgs;
  va_start(vArgs, sFormat);
  try
  {
    printfV(sFormat, vArgs);
  }
  CATCH_THROW_SIMPLE(va_end(vArgs), L"Error creating formatted string");
  va_end(vArgs);
  return *this;
}

RainChar* RainString::_commonInit(size_t iLength)
{
  CHECK_ALLOCATION(m_pBuffer = new NOTHROW rain_string_buffer_t(iLength + 1));
  m_pBuffer->iLengthUsed = iLength;
  RainChar* pBuffer = m_pBuffer->getBuffer();
  pBuffer[iLength] = 0;
  return pBuffer;
}

void RainString::_ensureExclusiveBufferAccess() throw(...)
{
  if(m_pBuffer->iReferenceCount > 1)
  {
    rain_string_buffer_t* pNewBuffer;
    CHECK_ALLOCATION(pNewBuffer = new NOTHROW rain_string_buffer_t(m_pBuffer->iBufferLength));
    iterator begin = const_begin();
    std::copy(begin, begin + m_pBuffer->iBufferLength, pNewBuffer->getBuffer());
    if(pNewBuffer->isUsingMiniBuffer())
      pNewBuffer->cLengthUsed = m_pBuffer->cLengthUsed;
    else
      pNewBuffer->iLengthUsed = m_pBuffer->iLengthUsed;
    m_pBuffer->free();
    m_pBuffer = pNewBuffer;
  }
}

void RainString::_ensureExclusiveBufferAccess(RainString::iterator& a, RainString::iterator& b) throw(...)
{
  if(m_pBuffer->iReferenceCount > 1)
  {
    difference_type da = a - const_begin();
    difference_type db = b - const_begin();
    _ensureExclusiveBufferAccess();
    a = begin() + da;
    b = begin() + db;
  }
}

size_t RainString::length() const throw()
{
  return m_pBuffer->getLengthUsed();
}

bool RainString::operator== (const wchar_t* sString) const throw()
{
  return std::equal(begin(), end(), sString);
}

bool RainString::operator== (const RainString& sOther) const throw()
{
  if(sOther.getCharacters() == getCharacters())
    return true;
  if(length() != sOther.length())
    return false;
  return compare(sOther) == 0;
}

bool RainString::operator!= (const RainString& sOther) const throw()
{
  if(sOther.getCharacters() == getCharacters())
    return false;
  if(length() != sOther.length())
    return true;
  return compare(sOther) != 0;
}

bool RainString::operator!= (const wchar_t* sString) const throw()
{
  return !(*this == sString);
}

bool RainString::operator<  (const RainString& sOther) const throw()
{
  return std::lexicographical_compare(begin(), end(), sOther.begin(), sOther.end());
}

RainString::size_type RainString::size() const throw()
{
  return length();
}

bool RainString::isEmpty() const throw()
{
  return length() == 0;
}

RainString::iterator RainString::begin() throw(...)
{
  _ensureExclusiveBufferAccess();
  return m_pBuffer->getBuffer();
}

RainString::iterator RainString::end() throw(...)
{
  return begin() + length();
}

RainString::const_iterator RainString::begin() const throw()
{
  return m_pBuffer->getBuffer();
}

RainString::const_iterator RainString::end() const throw()
{
  return begin() + length();
}

RainString::reverse_iterator RainString::rbegin() throw(...)
{
  return reverse_iterator(end());
}

RainString::reverse_iterator RainString::rend() throw(...)
{
  return reverse_iterator(begin());
}

RainString::const_reverse_iterator RainString::rbegin() const throw()
{
  return const_reverse_iterator(end());
}

RainString::const_reverse_iterator RainString::rend() const throw()
{
  return const_reverse_iterator(begin());
}

const RainChar* RainString::getCharacters() const throw()
{
  return begin();
}

RainChar* RainString::_commonAppend(size_t iLength) throw(...)
{
  _ensureExclusiveBufferAccess();
  if(m_pBuffer->getLengthUsed() +iLength + 1 > m_pBuffer->iBufferLength)
    m_pBuffer->upsize(m_pBuffer->iBufferLength * 2 + iLength + 8);
  RainChar* pReturn = end();
  pReturn[iLength] = 0;
  if(m_pBuffer->isUsingMiniBuffer())
    m_pBuffer->cLengthUsed += static_cast<RainChar>(iLength);
  else
    m_pBuffer->iLengthUsed += iLength;
  return pReturn;
}

RainString& RainString::append(const RainString& sOther) throw(...)
{
  return append(sOther.getCharacters(), sOther.length());
}

RainString& RainString::append(const RainChar cCharacter) throw(...)
{
  *_commonAppend(1) = cCharacter;
  return *this;
}

RainString& RainString::operator+= (const RainString& sOther) throw(...)
{
  return append(sOther);
}

RainString& RainString::operator+= (RainChar cCharacter) throw(...)
{
  return append(cCharacter);
}

RainChar& RainString::operator[] (size_t iCharacterIndex) throw(...)
{
  CHECK_RANGE(static_cast<size_t>(0), iCharacterIndex, length());
  _ensureExclusiveBufferAccess();
  return m_pBuffer->getBuffer()[iCharacterIndex];
}

RainChar RainString::operator[] (size_t iCharacterIndex) const throw(...)
{
  CHECK_RANGE(static_cast<size_t>(0), iCharacterIndex, length());
  return m_pBuffer->getBuffer()[iCharacterIndex];
}

RAINMAN2_API RainString operator+(const RainString& sA, const RainString& sB)
{
  if(sA.isEmpty())
    return sB;
  if(sB.isEmpty())
    return sA;
  RainString sNew(sA);
  sNew += sB;
  return sNew;
}

RainString::size_type RainString::max_size() throw()
{
  return std::numeric_limits<size_type>::max() - 1;
}

int RainString::_compare(const RainString& sCompareTo, int(*fnCompare)(const RainChar*, const RainChar*)) const throw()
{
  const RainChar *pOurChars = getCharacters();
  const RainChar *pTheirChars = sCompareTo.getCharacters();

  // Optimisation case
  if(pOurChars == pTheirChars)
    return 0;

  size_t iLength = length();
  for(;;)
  {
    int i = fnCompare(pOurChars, pTheirChars);
    if(i == 0)
    {
      size_t iZTLen = wcslen(pOurChars);
      if(iZTLen != iLength && iZTLen == wcslen(pTheirChars))
      {
        ++iZTLen;
        pOurChars += iZTLen;
        pTheirChars += iZTLen;
        iLength -= iZTLen;
        continue;
      }
    }
    return i;
  }
}

int RainString::compare(const RainString& sCompareTo) const throw()
{
  return _compare(sCompareTo, wcscmp);
}

int RainString::compareCaseless(const RainString& sCompareTo) const throw()
{
  return _compare(sCompareTo, _wcsicmp);
}

int RainString::compareCaseless(const char* sCompareTo) const throw()
{
  size_t iLength = length();
  const wchar_t* pChars = getCharacters();
  size_t i;
  for(i = 0; i < iLength && sCompareTo[i]; ++i)
  {
    int iStatus = tolower(pChars[i]) - tolower(sCompareTo[i]);
    if(iStatus != 0)
      return iStatus;
  }
  if(i == iLength && sCompareTo[i] == 0)
    return 0;
  else if(i == iLength)
    return -1;
  else
    return 1;
}

RainString RainString::mid(size_t iStart, size_t iLength) const throw(...)
{
  if(iStart == 0 && iLength == length())
    return *this;
  if(iLength > 0)
  {
    CHECK_RANGE(static_cast<size_t>(0), iStart + iLength, length());
    return RainString(getCharacters() + iStart, iLength);
  }
  else
  {
    return RainString();
  }
}

RainString RainString::prefix(size_t iLength) const throw(...)
{
  return mid(0, iLength);
}

RainString RainString::suffix(size_t iLength) const throw(...)
{
  return mid(length() - iLength, iLength);
}

RainString RainString::beforeFirst(RainChar cChar) const throw(...)
{
  const RainChar* pChar = wcschr(getCharacters(), cChar);
  if(pChar == 0)
    return *this;
  size_t iStart = pChar - getCharacters();
  return mid(0, iStart);
}

RainString RainString::afterFirst(RainChar cChar) const throw(...)
{
  const RainChar* pChar = wcschr(getCharacters(), cChar);
  if(pChar == 0)
    return RainString();
  size_t iStart = pChar - getCharacters() + 1;
  return mid(iStart, length() - iStart);
}

RainString RainString::beforeLast(RainChar cChar) const throw(...)
{
  const RainChar* pChar = wcsrchr(getCharacters(), cChar);
  if(pChar == 0)
    return RainString();
  size_t iStart = pChar - getCharacters();
  return mid(0, iStart);
}

RainString RainString::afterLast(RainChar cChar) const throw(...)
{
  const RainChar* pChar = wcsrchr(getCharacters(), cChar);
  if(pChar == 0)
    return *this;
  size_t iStart = pChar - getCharacters() + 1;
  return mid(iStart, length() - iStart);
}

RAINMAN2_API std::wostream& operator<< (std::wostream& stream, const RainString& string) throw()
{
  return stream.write(string.getCharacters(), static_cast<std::streamsize>(string.length()));
}

template <> RAINMAN2_API void std::swap<RainString>(RainString& a, RainString& b) { a.swap(b); }

size_t RainStrFunctions<char>::len(const char* sZeroTerminated)
{
  return strlen(sZeroTerminated);
}

size_t RainStrFunctions<wchar_t>::len(const wchar_t* sZeroTerminated)
{
  return wcslen(sZeroTerminated);
}

bool RainStrFunctions<char>::isWhitespace(const char cCharacter)
{
  return cCharacter == ' ' || cCharacter == '\n' || cCharacter == '\t' || cCharacter == '\r';
}

bool RainStrFunctions<wchar_t>::isWhitespace(const wchar_t cCharacter)
{
  return cCharacter == ' ' || cCharacter == '\n' || cCharacter == '\t' || cCharacter == '\r';
}
