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
#include "string.h"
#include <map>

class RAINMAN2_API RgdDictionary
{
public:
  RgdDictionary();
  ~RgdDictionary();

  static RgdDictionary* getSingleton();

  unsigned long asciiToHash(const char* sString) throw();
  unsigned long asciiToHash(const char* sString, size_t iLength) throw();
  bool isHashKnown(unsigned long iHash) throw();
  const char* hashToAscii(unsigned long iHash) throw(...);
  const char* hashToAsciiNoThrow(unsigned long iHash) throw();
  const RainString* hashToString(unsigned long iHash) throw(...);
  const RainString* hashToStringNoThrow(unsigned long iHash) throw();

  //! Hash of "$REF", an important entry in RGD tables, often treated specially
  static const unsigned long _REF = 0x49D60FAE;

  static void checkStaticHashes() throw(...);

protected:
  static RgdDictionary* m_pSingleton;

  struct _hash_entry
  {
    _hash_entry();

    char* sString;
    size_t iLength;
    RainString* pString;
  };

  std::map<unsigned long, _hash_entry> m_mapHashes;
};
