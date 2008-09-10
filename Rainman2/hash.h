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

class RAINMAN2_API IHash
{
public:
  virtual ~IHash() throw() = 0;

  virtual size_t getSizeInBytes() const throw() = 0;
  virtual bool canUpdatesBeSplit() const throw() = 0;
  virtual const char* getFamily() const throw() = 0;
  virtual bool isCaseIndependant() const throw() = 0;

  virtual void update(const char* pBuffer, size_t iBufferLength) throw() = 0;
  virtual void updateFromFile(IFile* pFile, size_t iByteCount) throw(...);
  virtual void updateFromString(const char* sString) throw();

  virtual void finalise(unsigned char* pOutput) throw() = 0;
  virtual unsigned long finalise() throw(...);
};

class RAINMAN2_API MD5Hash : public IHash
{
public:
  MD5Hash() throw();
  ~MD5Hash() throw();

  virtual size_t getSizeInBytes() const throw(); //!< returns 16
  virtual bool canUpdatesBeSplit() const throw(); //!< returns true
  virtual const char* getFamily() const throw(); //!< returns "MD"
  virtual bool isCaseIndependant() const throw(); //!< returns false

  virtual void update(const char* pBuffer, size_t iBufferLength) throw();
  virtual void finalise(unsigned char* pOutput) throw();

protected:
  void _transform();

  unsigned long buf[4];
  unsigned long bits[2];
  unsigned char in[64];
};

class RAINMAN2_API RGDHash : public IHash
{
public:
  RGDHash() throw();
  ~RGDHash() throw();

  virtual size_t getSizeInBytes() const throw(); //!< returns 4
  virtual bool canUpdatesBeSplit() const throw(); //!< returns false
  virtual const char* getFamily() const throw(); //!< returns "RGD"
  virtual bool isCaseIndependant() const throw(); //!< returns false

  virtual void update(const char* pBuffer, size_t iBufferLength) throw();
  virtual void finalise(unsigned char* pOutput) throw();

protected:
  unsigned long m_iValue;
};

RAINMAN2_API unsigned long RGDHashSimple(const void* pData, size_t iDataLength, unsigned long iHashValue = 0);

class RAINMAN2_API CRCHash : public IHash
{
public:
  CRCHash() throw();
  ~CRCHash() throw();

  virtual size_t getSizeInBytes() const throw(); //!< returns 4
  virtual bool canUpdatesBeSplit() const throw(); //!< returns true
  virtual const char* getFamily() const throw(); //!< returns "CRC"
  virtual bool isCaseIndependant() const throw(); //!< returns false

  virtual void update(const char* pBuffer, size_t iBufferLength) throw();
  virtual void finalise(unsigned char* pOutput) throw();

protected:
  unsigned long m_iValue;
};

RAINMAN2_API unsigned long CRCHashSimple(const void* pData, size_t iDataLength, unsigned long iHashValue = 0);

class RAINMAN2_API CRCCaselessHash : public IHash
{
public:
  CRCCaselessHash() throw();
  ~CRCCaselessHash() throw();

  virtual size_t getSizeInBytes() const throw(); //!< returns 4
  virtual bool canUpdatesBeSplit() const throw(); //!< returns true
  virtual const char* getFamily() const throw(); //!< returns "CRC"
  virtual bool isCaseIndependant() const throw(); //!< returns true

  virtual void update(const char* pBuffer, size_t iBufferLength) throw();
  virtual void finalise(unsigned char* pOutput) throw();

protected:
  unsigned long m_iValue;
};

RAINMAN2_API unsigned long CRCCaselessHashSimple(const void* pData, size_t iDataLength, unsigned long iHashValue = 0);
RAINMAN2_API unsigned long CRCCaselessHashSimpleAsciiFromUnicode(const wchar_t* pData, size_t iDataLength, unsigned long iHashValue = 0);