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
#ifndef __WFILE__
#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define __WFILE__ WIDEN(__FILE__)
#endif
#include "api.h"
#include <stdarg.h>
#include <stdio.h>
#include <algorithm>
#include <iterator>
#include <ostream>

#pragma warning(push)
#pragma warning(disable: 4996)

template <class T> struct RAINMAN2_API RainStrFunctions
{
  enum {VALID = -1};
};
template <> struct RAINMAN2_API RainStrFunctions<char>
{
  enum {VALID = 1};
  static size_t len(const char* sZeroTerminated);
  static bool isWhitespace(const char cCharacter);
};
template <> struct RAINMAN2_API RainStrFunctions<wchar_t>
{
  enum {VALID = 1};
  static size_t len(const wchar_t* sZeroTerminated);
  static bool isWhitespace(const wchar_t cCharacter);
};

//! The character type used for Rainman strings
typedef wchar_t RainChar;

//! A RainString is a fancy wrapping around one of these character buffers
/*!
  The details of this buffer are an implementation detail, hence why details
  of the structure are only present in string.cpp
*/
struct rain_string_buffer_t;

#ifdef RAINMAN2_USE_LUA
//! Forward reference of a lua_State (saves having to include the entire lua header)
struct lua_State;
#endif

#ifdef RAINMAN2_USE_WX
//! Forward reference of a wxString (saves having to include all the wx headers)
class wxString;
#endif

//! The class used by Rainman for string/text data
/*!
  All operations that can throw can exception (marked by throw(...)), will
  throw a RainException pointer. Most will throw on a memory allocation
  issue, although others may throw for additional reasons where noted.
*/
class RAINMAN2_API RainString
{
public:
  //! Default constructor; yields empty string
  RainString() throw(...);

#ifdef RAINMAN2_USE_WX
  //! Construct from a wxString
  RainString(wxString s) throw(...);
#endif

  //! Construct from a zero-terminated array of characters
  /*!
    The first \0 character in the string is interpreted as the end of string marker.
    Each source character is converted into one RainChar; no fancy UTF-8 translation
    is done for 8bit characters.
  */
  template <class T>
  RainString(const T* sZeroTermString) throw(...)
  {
    struct template_contraints { int char_type_expected[RainStrFunctions<T>::VALID]; };
    _initFromChars(sZeroTermString, sZeroTermString ? RainStrFunctions<T>::len(sZeroTermString) : 0);
  }

#ifdef RAINMAN2_USE_LUA
  //! Construct from a Lua string
  /*!
    \param L The Lua state to copy the string from
    \param idx The index in the Lua stack (or a psuedo-index) of the string
      to copy. Defaults to stack top (-1).
  */
  RainString(lua_State *L, int idx = -1) throw(...);
#endif

  //! Construct from an existing string
  RainString(const RainString&) throw();

  //! Construct from an array of characters with known length
  /*!
    Each source character is converted into one RainChar; no fancy UTF-8 translation
    is done for 8bit characters.
  */
  template <class T>
  RainString(const T* sString, size_t iLength) throw(...)
  {
    _initFromChars(sString, iLength);
  }

  //! Construct from an array of characters with length known at compile-time
  /*!
    Each source character is converted into one RainChar; no fancy UTF-8 translation
    is done for 8bit characters.
  */
  template <class T, size_t iLength>
  RainString(const T sString[iLength]) throw(...)
  {
    _initFromChars(sString, iLength);
  }

  //! Destructor
  ~RainString() throw();

  //! Replace current string with a zero-terminated unicode character array
  RainString& operator= (const RainChar* sZeroTermString) throw(...);

  //! Replace current string with a zero-terminated ASCII character array
  RainString& operator= (const char* sZeroTermString) throw(...);

  //! Replace current string with a different one
  RainString& operator= (const RainString&) throw();

#ifdef RAINMAN2_USE_WX
  //! Operator to seamlessly convert a RainString to a wxString
  operator wxString() const;
#endif

  //! Default value returned by the indexOf() functions to indicate that the search string was not found
  static const size_t NOT_FOUND = static_cast<size_t>(-1);

  size_t indexOf(RainChar cCharacter, size_t iStartAt = 0, size_t iNotFoundValue = NOT_FOUND) const;

  size_t indexOf(const RainString& sString, size_t iStartAt = 0, size_t iNotFoundValue = NOT_FOUND) const;

  //! Obtain a copy of the string with all whitespace characters removed from both ends
  RainString trimWhitespace() const throw(...);

  //! Obtain a substring of the string
  /*!
    An exception will be thrown if the range [iStart, iStart + iLength)
    specifies one or more characters outsize of the string (length of
    0 even when starting outside the string will *not* throw)
  */
  RainString mid(size_t iStart, size_t iLength) const throw(...);

  //! Gets a prefix of the string (a copy of the first N characters)
  /*!
    An exception will be thrown if iLength is longer than the length of
    the string.
  */
  RainString prefix(size_t iLength) const throw(...);

  //! Gets a suffix of the string (a copy of the last N characters)
  /*!
    An exception will be thrown if iLength is longer than the length of
    the string.
  */
  RainString suffix(size_t iLength) const throw(...);

  //! Gets the substring upto the first occurance of cChar
  /*!
    The returned substring will not include the first occurance.
    If cChar is not present, then the entire string is returned.
  */
  RainString beforeFirst(RainChar cChar) const throw(...);

  //! Gets the substring of every after the first occurance of cChar
  /*!
    The returned substring will not include the first occurance.
    If cChar is not present, then a blank string is returned.
  */
  RainString afterFirst(RainChar cChar) const throw(...);

  //! Gets the substring upto the last occurance of cChar
  /*!
    The returned substring will not include the last occurance.
    If cChar is not present, then a blank string is returned.
  */
  RainString beforeLast(RainChar cChar) const throw(...);

  //! Gets the substring upto the last occurance of cChar
  /*!
    The returned substring will not include the last occurance.
    If cChar is not present, then the entire string is returned.
  */
  RainString afterLast(RainChar cChar) const throw(...);

  //! Replace the string with a formatted expression
  /*!
    Equivalent to a swprintf call using this string as the
    (destination except that this way ensures that a large-enough
    buffer is allocated).
    \param sFormat The format string (note that embedded \0
      characters will be interpreted as the end of the format
      string, however any \0 characters in the result will
      not mark the end of the result)
    \param ... The parameters for the format
  */
  RainString& printf(const RainString sFormat, ...) throw(...);

  //! Replace the string with a formatted expression
  /*!
    Equivalent to a vswprintf call using this string as the
    (destination except that this way ensures that a large-enough
    buffer is allocated).
    \param sFormat The format string (note that embedded \0
      characters will be interpreted as the end of the format
      string, however any \0 characters in the result will
      not mark the end of the result)
    \param args The parameters for the format
  */
  RainString& printfV(const RainString& sFormat, va_list args) throw(...);

  //! Replace all occurances of a character with a different character
  RainString& replaceAll(RainChar cFind, RainChar cReplace) throw(...);

  RainString& replaceAll(const RainString& sNeedle, const RainString& sReplacement) throw(...);

  //! Convert the string to lower-case
  RainString& toLower() throw(...);

  //! Convert the string to upper-case
  RainString& toUpper() throw(...);

  //! Creates a string which is this one repeated a number of times
  RainString repeat(size_t iCount) const throw(...);

  //! Queries if the string is empty
  bool isEmpty() const throw();

  //! Swaps the contents of this string with the contents of another
  /*!
    This is more efficient than a simple swap, and is used by std::swap
    to swap RainString instances.
  */
  void swap(RainString& other) throw();

  //! Gets the length of the string
  size_t length() const throw();

  //! Gets a pointer to the characters of the string
  const RainChar* getCharacters() const throw();

  //! Extract a single character from the string
  /*!
    This can also be used to modify the string on a character by
    character basis (although std::transform with begin() and end()
    is probably more efficient).
    Will throw an exception if index is outside the string.
  */
  RainChar& operator[] (size_t iIndex) throw(...);

  //! Extract a single character from the string
  RainChar operator[] (size_t iIndex) const throw(...);

  RainString& append(const RainString& sOther) throw(...);

  RainString& append(const RainChar cCharacter) throw(...);

  template <class T>
  RainString& append(const T* pZeroTerminated) throw(...)
  {
    return append(pZeroTerminated, RainStrFunctions<T>::len(pZeroTerminated));
  }

  template <class T>
  RainString& append(const T* pString, size_t iLength) throw(...)
  {
    std::copy(pString, pString + iLength, _commonAppend(iLength));
    return *this;
  }

  //! Append an entire string onto the end of this one
  RainString& operator+= (const RainString& sOther) throw(...);

  //! Append a single character onto the end of the string
  /*!
    It is generally quite inefficient to append lots of characters to
    the end of a string compared to appending an entire string. Note
    that adding one character at a time will not cause a new buffer to
    be allocated each time.
  */
  RainString& operator+= (RainChar cCharacter) throw(...);

  template <class T>
  RainString& operator+= (const T* pZeroTerminatedString) throw(...)
  {
    return append(pZeroTerminatedString);
  }

  //! Compare two strings for equality
  /*!
    More efficient than using compare(sOther) == 0
  */
  bool operator== (const RainString& sOther) const throw();
  bool operator== (const wchar_t* sString) const throw();

  //! Compare two strings for inequality
  /*!
    More efficient than using compare(sOther) != 0
  */
  bool operator!= (const RainString& sOther) const throw();
  bool operator!= (const wchar_t* sString) const throw();

  //! Test if this string is lexicographically less than another
  /*!
    More efficient than using compare(sOther) < 0
  */
  bool operator<  (const RainString& sOther) const throw();

  //! Lexicographically compares this string with another
  /*!
    Returns 0 if the strings are equal, less than 0 if this
    string is less than sCompareTo, and greater than 0 if
    this string is greater than sCompareTo.
    Embedded \0 characters are handled correctly, unlike wcscmp
  */
  int compare(const RainString& sCompareTo) const throw();

  //! Lexicographically compares this string with another, without regard to case
  /*!
    Case is ignored, so for example "abc" and "ABC" are consided equal.
    Returns 0 if the strings are equal, less than 0 if this
    string is less than sCompareTo, and greater than 0 if
    this string is greater than sCompareTo.
    Embedded \0 characters are handled correctly, unlike wcsicmp
  */
  int compareCaseless(const RainString& sCompareTo) const throw();

  int compareCaseless(const char* sCompareTo) const throw();
  int compareCaseless(const char* sCompareTo, size_t iLength) const throw();

  // Typedefs required to act as an STL container
  typedef RainChar value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef value_type* iterator;
  typedef const value_type* const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef value_type& reference;
  typedef const value_type& const_reference;

  //! Gets an iterator to the first character in the string
  iterator begin() throw(...);

  //! Gets an iterator to the character after the last character in the string
  /*!
    This iterator is to the \0 terminator which is present at the end of a
    RainString for convenience when using the C standard library. As such,
    this iterator will always dereference to 0 even though the iterator is
    to a character not part of the string.
  */
  iterator end() throw(...);

  //! Gets a constant iterator to the first character in the string
  const_iterator begin() const throw();

  //! Gets a constant iterator to the character after the last character in the string
  const_iterator end() const throw();

  //! Get a reverse iterator to the last character in the string
  reverse_iterator rbegin() throw(...);

  //! Get a reverse iterator to the character before the first character in the string
  /*!
    Unlike end(), this iterator cannot be dereferenced.
  */
  reverse_iterator rend() throw(...);

  //! Get a constant reverse iterator to the last character in the string
  const_reverse_iterator rbegin() const throw();

  //! Get a constant reverse iterator to the character before the first character in the string
  const_reverse_iterator rend() const throw();

  //! Gets the length of the string
  /*!
    While length() is usually used to get the length of the string, in
    order to satisfy the requirements for an STL container, size() is
    provided as an alias of length().
  */
  size_type size() const throw();

  //! Gets the maximum length string which can be contained in a RainString
  /*!
    Note that this is the maximum theoretical length; the host's maximum memory
    will likely prevent this length from ever being reached.
  */
  static size_type max_size() throw();

  //! Erases the contents of the string
  void clear() throw(...);

  //! Resizes the string to be the requested length
  /*!
    Either \0 characters are appended, or characters at the end are removed.
  */
  void resize(size_type iNewLength) throw(...);

  //! Resizes the string to be the requested length, appending the given character if needed
  /*!
    Either cDefault characters are appended, or characters at the end are removed.
  */
  void resize(size_type iNewLength, RainChar cDefault) throw(...);

  //! Erases a single character from the string
  /*!
    Equivalent to erase(item, item + 1)
  */
  iterator erase(iterator item) throw(...);

  //! Erases a range of characters from the string
  /*!
    Erases everything in range [range_begin, range_end).
    Returns an iterator to the next item after the range
  */
  iterator erase(iterator range_begin, iterator range_end) throw(...);

  void insert(iterator position, const_iterator to_insert, const_iterator to_insert_end) throw(...);

protected:
  //! Generic compare function
  /*!
    \param sCompareTo The string to compare this one with
    \param fnCompare A function which takes two zero-terminated character arrays and returns
      0 on equality, and any other value on inequality (i.e. wcscmp, wcsicmp)
    If the compare function returns 0 due to embedded \0 characters, then it will be called
    again with pointers to after the embedded \0.
  */
  int _compare(const RainString& sCompareTo, int(*fnCompare)(const RainChar*, const RainChar*)) const throw();

  //! First part of _initFromChars() which is not dependant upon the template type
  /*!
    This function is mainly provided so that _initFromChars() does not require
    knowledge of rain_string_buffer_t, as that would require that rain_string_buffer_t
    be fully defined in the header file.
    \return Pointer to a buffer ready to receive the specified number of characters
  */
  RainChar* _commonInit(size_t iLength);

  RainChar* _commonAppend(size_t iLength) throw(...);

  //! Initialise the string from an array of characters with known length
  template <class T>
  void _initFromChars(const T* sString, size_t iLength) throw(...)
  {
    std::copy(sString, sString + iLength, _commonInit(iLength));
  }

  //! Makes the buffer mutable, if it isn't already
  /*!
    As multiple strings can share a single buffer for efficiency, then a
    buffer is not mutable if more than one string is using it, as changing
    it would cause other strings to be changed in addition to this one.
    Hence, if the buffer is being used by other strings, then a new buffer
    will be allocated as a copy of the existing one. This copy will only
    be in use by this string, and thus will be mutable.
  */
  void _ensureExclusiveBufferAccess() throw(...);

  void _ensureExclusiveBufferAccess(iterator& a, iterator& b) throw(...);

  //! The buffer being used by the string
  /*!
    A RainString is a very fancy wrapping around one of these buffer objects,
    hence this pointer is the only actual member variable of a RainString. Thus
    sizeof(RainString) should equal sizeof(void*), which is good for efficiency
    as the entire RainString should fit into a native register. Copying a string
    is also a lightweight operation; just copy the buffer pointer and increment
    the reference count.
  */
  rain_string_buffer_t* m_pBuffer;
};

extern RAINMAN2_API const RainString RainEmptyString;

//! Global operator to add (concatenate) two RainStrings
RAINMAN2_API RainString operator+(const RainString& sA, const RainString& sB);

//! Global operator to add (concatenate) a RainString with a unicode character array
RAINMAN2_API RainString operator+(const RainString& sA, const wchar_t* sB);
RAINMAN2_API RainString operator+(const wchar_t* sA, const RainString& sB);

//! Efficient specialisation of std::swap for RainStrings
template <> RAINMAN2_API void std::swap<RainString>(RainString& a, RainString& b);

//! Output a RainString to a unicode stream
RAINMAN2_API std::wostream& operator<< (std::wostream& stream, const RainString& string) throw();

#pragma warning(pop)
