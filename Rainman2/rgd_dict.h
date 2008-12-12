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

//! A two-way dictionary for converting between strings and their RGD hashes
/*!
  After calling one of the asciiToHash() methods, the string being hashed is
  remembered so that the hash can be converted back to a string using one of
  the hashTo methods.
  In order to get a complete dictionary as possible, it is recommended to always
  use getSingleton() to get a reusable dictionary, rather than create a new
  dictionary for something.
*/
class RAINMAN2_API RgdDictionary
{
public:
  ~RgdDictionary() throw(); //!< default  destructor

  //! Get an existing RgdDictionary which can can be re-used multiple times
  /*!
    The singleton RgdDictionary is silently created with the first call to
    getSingleton(), and then returned by all future calls to getSingleton().
    If the singleton is accidently deleted, then it will be remade by the
    next call to getSingleton(). The singleton will be automatically deleted
    when the Rainman DLL is unloaded.
  */
  static RgdDictionary* getSingleton() throw();

  //! Calculate the hash code for a zero-terminated ASCII string
  /*!
    Equivalent to calling asciiToHash(sString, strlen(sString))
  */
  unsigned long asciiToHash(const char* sString) throw();

  //! Calculate the hash code for a known-length ASCII string
  /*!
    RGDHashSimple() is used to calculate the hash, and then if that hash is not
    known to the dictionary, then an entry is made in the dictionary pairing up
    the hash and the string which hashed to it.
    In all cases, no exceptions are ever thrown, and the hash code is returned.
  */
  unsigned long asciiToHash(const char* sString, size_t iLength) throw();
  
  //! Determine whether a string which hashes to a given value is known
  /*!
    Any value ever returned by asciiToHash() will be known by the dictionary, so
    that hash value can be converted back to a string. 
  */
  bool isHashKnown(unsigned long iHash) const throw();

  //! Try and get a string which hashes to a given value
  /*!
    \param iHash The hash code to find a string for
    \param pLength If non-NULL, then the length of the string will be stored here
    \return A valid zero-terminated ASCII string (although it may also contain embedded zeros)

    If the hash is not known, then an exception is thrown. This function operates in constant
    time - it does not try to brute force the hash if it is not known.
  */
  const char* hashToAscii(unsigned long iHash, size_t* pLength = NULL) throw(...);

  //! Try and get a string which hashes to a given value
  /*!
    Same as hashToAscii(), except NULL is returned if the hash is not known, rather than an
    exception being thrown.
  */
  const char* hashToAsciiNoThrow(unsigned long iHash, size_t* pLength = NULL) throw();

  //! Try to get the unicode version of the string which hashes to a given value
  /*!
    \param iHash The hash code to find a string for
    \return A valid RainString pointer

    If the hash is not known, then an exception is thrown. This function operates in constant
    time - it does not try to brute force the hash if it is not known. Returns the same value
    as hashToAscii(), except extended to unicode and in the form of a RainString.
  */
  const RainString* hashToString(unsigned long iHash) throw(...);

  //! Try to get the unicode version of the string which hashes to a given value
  /*!
    Same as hashToString(), except NULL is returned if the hash is not known, rather than an
    exception being thrown.
  */
  const RainString* hashToStringNoThrow(unsigned long iHash) throw();

  //! Hash of "$REF", an important entry in RGD tables, often treated specially
  /*!
    checkStaticHashes() should be called in application init to ensure that this value
    matches the runtime-calculated hash of "$REF"
  */
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
