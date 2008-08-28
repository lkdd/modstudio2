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
#include "rgd_dict.h"
#include "hash.h"
#include "exception.h"
#include "new_trace.h"

RgdDictionary* RgdDictionary::m_pSingleton = 0;

RgdDictionary::RgdDictionary()
{
  if(!m_pSingleton)
    m_pSingleton = this;
}

RgdDictionary::~RgdDictionary()
{
  for(std::map<unsigned long, _hash_entry>::iterator itr = m_mapHashes.begin(); itr != m_mapHashes.end(); ++itr)
  {
    delete[] itr->second.sString;
    delete itr->second.pString;
  }
  if(this == m_pSingleton)
    m_pSingleton = 0;
}

void RgdDictionary::checkStaticHashes() throw(...)
{
  CHECK_ASSERT(getSingleton()->asciiToHash("$REF") == _REF);
}

unsigned long RgdDictionary::asciiToHash(const char* sString) throw()
{
  return asciiToHash(sString, strlen(sString));
}

RgdDictionary::_hash_entry::_hash_entry()
{
  sString = 0;
  iLength = 0;
  pString = 0;
}

unsigned long RgdDictionary::asciiToHash(const char* sString, size_t iLength) throw()
{
  unsigned long iHash = RGDHashSimple((const void*)sString, iLength);
  if(!isHashKnown(iHash))
  {
    _hash_entry oEntry;
    oEntry.iLength = iLength;
    oEntry.sString = new NOTHROW char[iLength + 1];
    if(oEntry.sString)
    {
      memcpy(oEntry.sString, sString, iLength + 1);
      m_mapHashes[iHash] = oEntry;
    }
  }
  return iHash;
}

bool RgdDictionary::isHashKnown(unsigned long iHash) throw()
{
  return m_mapHashes.count(iHash) == 1;
}

#define CHECK_HASH(hash) if(!isHashKnown(hash)) THROW_SIMPLE_1(L"Hash not in the dictionary: %lu", (unsigned long)(hash));

const char* RgdDictionary::hashToAscii(unsigned long iHash) throw(...)
{
  CHECK_HASH(iHash);
  return m_mapHashes[iHash].sString;
}

const char* RgdDictionary::hashToAsciiNoThrow(unsigned long iHash) throw()
{
  if(!isHashKnown(iHash))
    return 0;
  return m_mapHashes[iHash].sString;
}

const RainString* RgdDictionary::hashToString(unsigned long iHash) throw(...)
{
  CHECK_HASH(iHash);
  _hash_entry &oEntry = m_mapHashes[iHash];
  if(!oEntry.pString)
    CHECK_ALLOCATION(oEntry.pString = new NOTHROW RainString(oEntry.sString, oEntry.iLength));
  return oEntry.pString;
}

const RainString* RgdDictionary::hashToStringNoThrow(unsigned long iHash) throw()
{
  if(!isHashKnown(iHash))
    return 0;
  _hash_entry &oEntry = m_mapHashes[iHash];
  if(!oEntry.pString)
    oEntry.pString = new NOTHROW RainString(oEntry.sString, oEntry.iLength);
  return oEntry.pString;
}

static void __cdecl RgdDictionarySingletonCleanup()
{
  delete RgdDictionary::getSingleton();
}

RgdDictionary* RgdDictionary::getSingleton()
{
  if(!m_pSingleton)
  {
    m_pSingleton = new NOTHROW RgdDictionary;
    atexit(RgdDictionarySingletonCleanup);
  }
  return m_pSingleton;
}
