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

//! Common exception class for all exceptions produced by Rainman 2
/*!
  Any Rainman 2 function that can throw exceptions will throw a RainException pointer.
  The exception records the position (file & line) of the error, along with a message and
  optionally a pointer to the exception which led to the current one.
*/
class RAINMAN2_API RainException
{
public:
  //! Simple constructor
  /*!
    \param sFile The file in which the error occured (normally __WFILE__)
    \param iLine The line on which the exception object was created (normally __LINE__)
    \param sMessage A message describing the error
    \param pPrevious A pointer to the exception which led to this one (or NULL). The
      pointer is owned by the newly created exception.
  */
  RainException(const RainString sFile, unsigned long iLine, const RainString sMessage, RainException* pPrevious = 0);

  //! Variable argument format string constructor
  /*!
    \param sFile The file in which the error occured (normally __WFILE__)
    \param iLine The line on which the exception object was created (normally __LINE__)
    \param pPrevious A pointer to the exception which led to this one (or NULL). The
      pointer is owned by the newly created exception.
    \param sFormat A message describing the error, containing (sw)printf style format
      specifiers
  */
  RainException(const RainString sFile, unsigned long iLine, RainException* pPrevious, const RainString sFormat, ...);

  virtual ~RainException();

  inline RainException* getPrevious() {return m_pPrevious;}
  inline const RainString& getFile() const {return m_sFile;}
  inline const RainString& getMessage() const {return m_sMessage;}
  inline const unsigned long getLine() const {return m_iLine;}

  //! Throw a memory allocation exception if a given pointer is NULL
  /*!
    Use the CHECK_ALLOCATION() macro instead of this function where possible, as it
    inserts most of the parameters automatically.
  */
  template <class T>
  static T* checkAllocation(const RainChar* sFile, unsigned long iLine, T* pPointer, const RainChar* sAlloc) throw(...)
  {
    if(pPointer)
      return pPointer;
    RainString sFile_(sFile);
    RainString sMessage(L"Memory allocation failed: ");
    sMessage += sAlloc;
    throw new RainException(sFile_, iLine, sMessage);
  }

  /*
    checkRange<T> and checkRangeLtMax<T> check that minimum <= value <= maximum or minimum <= value < maximum.
    Hence they all take three arguments of type T; minimum, value and maximum. For automatic template deduction
    to work, these three values need to have the same type, which seems simple until minimum or maximum are
    integer literals (i.e. checkRange(..., 0, someValue, 10, ...)). In this case, any such literals would have
    to be explicity type-casted to the required type, which is ugly. Automatic template deduction can however
    be tweaked into getting the type T from just one parameter, which is done by making the min and max parameters
    dependant types upon T, rather than type T itself (even though the resolved dependant type is T itself, the
    compiler doesn't know this until the type of T is already finalised).

    someClass<T>::someMember is called a dependant type of T, as the meaning of someMember is dependant upon the
    type T. Furthermore, the compiler doesn't know that someMember will be a typedef, hence the typename keyword.
  */
#define T_NO_AUTO  typename __DummyNoParamAutoDetect<T>::T
  template <class T> struct __DummyNoParamAutoDetect { typedef T T; };

  template <class T>
  static void checkRange(const RainChar* sFile, unsigned long iLine, T value, T_NO_AUTO min, T_NO_AUTO max, const RainChar* sCheck) throw(...)
  {
    if(value < min || value > max)
      throw new RainException(sFile, iLine, RainString(L"Value not within range: ") + RainString(sCheck));
  }

  template <class T>
  static void checkRangeLtMax(const RainChar* sFile, unsigned long iLine, T value, T_NO_AUTO min, T_NO_AUTO max, const RainChar* sCheck) throw(...)
  {
    if(value < min || value >= max)
      throw new RainException(sFile, iLine, RainString(L"Value not within range: ") + RainString(sCheck));
  }

#undef T_NO_AUTO

  static void checkAssert(const RainChar* sFile, unsigned long iLine, bool bAssertion, const RainChar* sCheck) throw(...);

protected:
  RainString m_sFile, m_sMessage;
  unsigned long m_iLine;
  RainException* m_pPrevious;
};

#define CHECK_ASSERT(assert) RainException::checkAssert(__WFILE__, __LINE__, assert, WIDEN(#assert))
#define CHECK_ALLOCATION(pointer) RainException::checkAllocation(__WFILE__, __LINE__, pointer, WIDEN(#pointer))
#define CHECK_RANGE(min, value, max) RainException::checkRange(__WFILE__, __LINE__, value, min, max, WIDEN(#min) L" <= " WIDEN(#value) L" <= " WIDEN(#max))
#define CHECK_RANGE_LTMAX(min, value, max) RainException::checkRangeLtMax(__WFILE__, __LINE__, value, min, max, WIDEN(#min) L" <= " WIDEN(#value) L" < " WIDEN(#max))

#define RETHROW_SIMPLE(previous, message) throw new RainException(__WFILE__, __LINE__, message, previous)
#define RETHROW_SIMPLE_(previous, message, ...) throw new RainException(__WFILE__, __LINE__, previous, message, __VA_ARGS__)
#define THROW_SIMPLE(message) RETHROW_SIMPLE(0, message)
#define THROW_SIMPLE_(message, ...) RETHROW_SIMPLE_(0, message, __VA_ARGS__)
#define CATCH_THROW_SIMPLE(cleanup, message) catch(RainException *e) { cleanup; RETHROW_SIMPLE(e, message); }
#define CATCH_THROW_SIMPLE_(cleanup, message, ...) catch(RainException *e) { cleanup; RETHROW_SIMPLE_(e, message, __VA_ARGS__); }
