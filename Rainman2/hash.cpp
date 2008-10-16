/*
Parts copyright (c) 2008 Peter "Corsix" Cawley
RGD Hash and MD5 code from public domain

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
#include "hash.h"
#include "exception.h"
#include <string.h>

IHash::~IHash() {}

void IHash::updateFromFile(IFile* pFile, size_t iByteCount) throw(...)
{
  const size_t iBufferSize = 1024;
  unsigned char sBuffer[iBufferSize];
  while(iByteCount > iBufferSize)
  {
    try
    {
      pFile->readArray(sBuffer, iBufferSize);
    }
    CATCH_THROW_SIMPLE({}, L"Cannot read required data for hash");
    iByteCount -= iBufferSize;
    update((const char*)sBuffer, iBufferSize);
  }
  try
  {
    pFile->readArray(sBuffer, iByteCount);
  }
  CATCH_THROW_SIMPLE({}, L"Cannot read required data for hash");
  update((const char*)sBuffer, iByteCount);
}

void IHash::updateFromString(const char* sString) throw()
{
  update(sString, strlen(sString));
}

unsigned long IHash::finalise() throw(...)
{
  if(getSizeInBytes() != sizeof(unsigned long))
    THROW_SIMPLE(L"Hash cannot be finalised to 32 bits");
  unsigned long iReturnValue;
  finalise((unsigned char*)&iReturnValue);
  return iReturnValue;
}

MD5Hash::MD5Hash()
{
  buf[0] = 0x67452301;
  buf[1] = 0xefcdab89;
  buf[2] = 0x98badcfe;
  buf[3] = 0x10325476;
  bits[0] = 0;
  bits[1] = 0;
}

MD5Hash::~MD5Hash()
{
}

size_t MD5Hash::getSizeInBytes() const throw() {return 16;}
bool MD5Hash::canUpdatesBeSplit() const throw() {return true;}
const char* MD5Hash::getFamily() const throw() {return "MD";}
bool MD5Hash::isCaseIndependant() const throw() {return false;}

void MD5Hash::update(const char* pBuffer, size_t iBufferLength) throw()
{
  // Update bitcount
  unsigned long t;
  t = bits[0];
  if((bits[0] = t + ((unsigned long) iBufferLength << 3)) < t)
	  bits[1]++; // Carry from low to high
  bits[1] += (unsigned long)iBufferLength >> 29;
  t = (t >> 3) & 0x3f;	// Bytes already in data

  // Handle any leading odd-sized chunks
  if(t)
  {
	  unsigned char *p = (unsigned char *)in + t;
	  t = 64 - t;
	  if(iBufferLength < t)
    {
	    memcpy(p, pBuffer, iBufferLength);
	    return;
	  }
	  memcpy(p, pBuffer, t);
	  _transform();
	  pBuffer += t;
	  iBufferLength -= t;
  }

  // Process data in 64-byte chunks
  while(iBufferLength >= 64)
  {
	  memcpy(in, pBuffer, 64);
    _transform();
	  pBuffer += 64;
	  iBufferLength -= 64;
  }

  // Handle any remaining bytes of data
  memcpy(in, pBuffer, iBufferLength);
}

void MD5Hash::finalise(unsigned char* digest) throw()
{
  unsigned count;
  unsigned char *p;

  // Compute number of bytes mod 64
  count = (bits[0] >> 3) & 0x3F;

  // Set the first char of padding to 0x80.  This is safe since there is always at least one byte free
  p = in + count;
  *p++ = 0x80;

  // Bytes of padding needed to make 64 bytes
  count = 64 - 1 - count;

  /* Pad out to 56 mod 64 */
  if(count < 8)
  {
	  // Two lots of padding:  Pad the first block to 64 bytes
	  memset(p, 0, count);
	  _transform();
	  // Now fill the next block with 56 bytes
	  memset(in, 0, 56);
  }
  else
	  // Pad block to 56 bytes
	  memset(p, 0, count - 8);

  // Append length in bits and transform
  ((unsigned long*)in)[14] = bits[0];
  ((unsigned long*)in)[15] = bits[1];
  _transform();

  memcpy(digest, buf, 16);
  buf[0] = 0x67452301;
  buf[1] = 0xefcdab89;
  buf[2] = 0x98badcfe;
  buf[3] = 0x10325476;
  bits[0] = 0;
  bits[1] = 0;
}

// Four MD5 core functions
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

// Central part of MD5
#define MD5STEP(f, w, x, y, z, data, s) ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

void MD5Hash::_transform()
{
  unsigned long a = buf[0];
  unsigned long b = buf[1];
  unsigned long c = buf[2];
  unsigned long d = buf[3];
  unsigned long *p = (unsigned long*)in;

  MD5STEP(F1, a, b, c, d, p[0] + 0xd76aa478, 7);
  MD5STEP(F1, d, a, b, c, p[1] + 0xe8c7b756, 12);
  MD5STEP(F1, c, d, a, b, p[2] + 0x242070db, 17);
  MD5STEP(F1, b, c, d, a, p[3] + 0xc1bdceee, 22);
  MD5STEP(F1, a, b, c, d, p[4] + 0xf57c0faf, 7);
  MD5STEP(F1, d, a, b, c, p[5] + 0x4787c62a, 12);
  MD5STEP(F1, c, d, a, b, p[6] + 0xa8304613, 17);
  MD5STEP(F1, b, c, d, a, p[7] + 0xfd469501, 22);
  MD5STEP(F1, a, b, c, d, p[8] + 0x698098d8, 7);
  MD5STEP(F1, d, a, b, c, p[9] + 0x8b44f7af, 12);
  MD5STEP(F1, c, d, a, b, p[10] + 0xffff5bb1, 17);
  MD5STEP(F1, b, c, d, a, p[11] + 0x895cd7be, 22);
  MD5STEP(F1, a, b, c, d, p[12] + 0x6b901122, 7);
  MD5STEP(F1, d, a, b, c, p[13] + 0xfd987193, 12);
  MD5STEP(F1, c, d, a, b, p[14] + 0xa679438e, 17);
  MD5STEP(F1, b, c, d, a, p[15] + 0x49b40821, 22);

  MD5STEP(F2, a, b, c, d, p[1] + 0xf61e2562, 5);
  MD5STEP(F2, d, a, b, c, p[6] + 0xc040b340, 9);
  MD5STEP(F2, c, d, a, b, p[11] + 0x265e5a51, 14);
  MD5STEP(F2, b, c, d, a, p[0] + 0xe9b6c7aa, 20);
  MD5STEP(F2, a, b, c, d, p[5] + 0xd62f105d, 5);
  MD5STEP(F2, d, a, b, c, p[10] + 0x02441453, 9);
  MD5STEP(F2, c, d, a, b, p[15] + 0xd8a1e681, 14);
  MD5STEP(F2, b, c, d, a, p[4] + 0xe7d3fbc8, 20);
  MD5STEP(F2, a, b, c, d, p[9] + 0x21e1cde6, 5);
  MD5STEP(F2, d, a, b, c, p[14] + 0xc33707d6, 9);
  MD5STEP(F2, c, d, a, b, p[3] + 0xf4d50d87, 14);
  MD5STEP(F2, b, c, d, a, p[8] + 0x455a14ed, 20);
  MD5STEP(F2, a, b, c, d, p[13] + 0xa9e3e905, 5);
  MD5STEP(F2, d, a, b, c, p[2] + 0xfcefa3f8, 9);
  MD5STEP(F2, c, d, a, b, p[7] + 0x676f02d9, 14);
  MD5STEP(F2, b, c, d, a, p[12] + 0x8d2a4c8a, 20);

  MD5STEP(F3, a, b, c, d, p[5] + 0xfffa3942, 4);
  MD5STEP(F3, d, a, b, c, p[8] + 0x8771f681, 11);
  MD5STEP(F3, c, d, a, b, p[11] + 0x6d9d6122, 16);
  MD5STEP(F3, b, c, d, a, p[14] + 0xfde5380c, 23);
  MD5STEP(F3, a, b, c, d, p[1] + 0xa4beea44, 4);
  MD5STEP(F3, d, a, b, c, p[4] + 0x4bdecfa9, 11);
  MD5STEP(F3, c, d, a, b, p[7] + 0xf6bb4b60, 16);
  MD5STEP(F3, b, c, d, a, p[10] + 0xbebfbc70, 23);
  MD5STEP(F3, a, b, c, d, p[13] + 0x289b7ec6, 4);
  MD5STEP(F3, d, a, b, c, p[0] + 0xeaa127fa, 11);
  MD5STEP(F3, c, d, a, b, p[3] + 0xd4ef3085, 16);
  MD5STEP(F3, b, c, d, a, p[6] + 0x04881d05, 23);
  MD5STEP(F3, a, b, c, d, p[9] + 0xd9d4d039, 4);
  MD5STEP(F3, d, a, b, c, p[12] + 0xe6db99e5, 11);
  MD5STEP(F3, c, d, a, b, p[15] + 0x1fa27cf8, 16);
  MD5STEP(F3, b, c, d, a, p[2] + 0xc4ac5665, 23);

  MD5STEP(F4, a, b, c, d, p[0] + 0xf4292244, 6);
  MD5STEP(F4, d, a, b, c, p[7] + 0x432aff97, 10);
  MD5STEP(F4, c, d, a, b, p[14] + 0xab9423a7, 15);
  MD5STEP(F4, b, c, d, a, p[5] + 0xfc93a039, 21);
  MD5STEP(F4, a, b, c, d, p[12] + 0x655b59c3, 6);
  MD5STEP(F4, d, a, b, c, p[3] + 0x8f0ccc92, 10);
  MD5STEP(F4, c, d, a, b, p[10] + 0xffeff47d, 15);
  MD5STEP(F4, b, c, d, a, p[1] + 0x85845dd1, 21);
  MD5STEP(F4, a, b, c, d, p[8] + 0x6fa87e4f, 6);
  MD5STEP(F4, d, a, b, c, p[15] + 0xfe2ce6e0, 10);
  MD5STEP(F4, c, d, a, b, p[6] + 0xa3014314, 15);
  MD5STEP(F4, b, c, d, a, p[13] + 0x4e0811a1, 21);
  MD5STEP(F4, a, b, c, d, p[4] + 0xf7537e82, 6);
  MD5STEP(F4, d, a, b, c, p[11] + 0xbd3af235, 10);
  MD5STEP(F4, c, d, a, b, p[2] + 0x2ad7d2bb, 15);
  MD5STEP(F4, b, c, d, a, p[9] + 0xeb86d391, 21);

  buf[0] += a;
  buf[1] += b;
  buf[2] += c;
  buf[3] += d;
}

#undef MD5STEP
#undef F4
#undef F3
#undef F2
#undef F1

RGDHash::RGDHash() throw()
  : m_iValue(0) {}
RGDHash::~RGDHash() throw()
{}

size_t RGDHash::getSizeInBytes() const throw() {return sizeof(unsigned long);}
bool RGDHash::canUpdatesBeSplit() const throw() {return false;}
const char* RGDHash::getFamily() const throw() {return "RGD";}
bool RGDHash::isCaseIndependant() const throw() {return false;}

void RGDHash::update(const char* pBuffer, size_t iBufferLength) throw()
{
  m_iValue = RGDHashSimple((void*)pBuffer, iBufferLength, m_iValue);
}

void RGDHash::finalise(unsigned char* pOutput) throw()
{
  memcpy(pOutput, &m_iValue, sizeof(unsigned long));
}

#define ub4 unsigned long
#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

RAINMAN2_API unsigned long RGDHashSimple(const void* pData, size_t iDataLength, unsigned long initval)
{
  const unsigned char* k = (const unsigned char*)pData;
  register unsigned long a,b,c,len;

   /* Set up the internal state */
   len = static_cast<unsigned long>(iDataLength);
   a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
   c = initval;           /* the previous hash value */

   /*---------------------------------------- handle most of the key */
   while (len >= 12)
   {
      a += (k[0] +((ub4)k[1]<<8) +((ub4)k[2]<<16) +((ub4)k[3]<<24));
      b += (k[4] +((ub4)k[5]<<8) +((ub4)k[6]<<16) +((ub4)k[7]<<24));
      c += (k[8] +((ub4)k[9]<<8) +((ub4)k[10]<<16)+((ub4)k[11]<<24));
      mix(a,b,c);
      k += 12; len -= 12;
   }

   /*------------------------------------- handle the last 11 bytes */
   c += static_cast<unsigned long>(iDataLength);
   switch(len)              /* all the case statements fall through */
   {
   case 11: c+=((ub4)k[10]<<24);
   case 10: c+=((ub4)k[9]<<16);
   case 9 : c+=((ub4)k[8]<<8);
      /* the first byte of c is reserved for the length */
   case 8 : b+=((ub4)k[7]<<24);
   case 7 : b+=((ub4)k[6]<<16);
   case 6 : b+=((ub4)k[5]<<8);
   case 5 : b+=k[4];
   case 4 : a+=((ub4)k[3]<<24);
   case 3 : a+=((ub4)k[2]<<16);
   case 2 : a+=((ub4)k[1]<<8);
   case 1 : a+=k[0];
     /* case 0: nothing left to add */
   }
   mix(a,b,c);
   /*-------------------------------------------- report the result */
   return c;
}

#undef mix
#undef ub4

CRCHash::CRCHash() throw()
  : m_iValue(0) {}
CRCHash::~CRCHash() throw()
{}

size_t CRCHash::getSizeInBytes() const throw() {return sizeof(unsigned long);}
bool CRCHash::canUpdatesBeSplit() const throw() {return true;}
const char* CRCHash::getFamily() const throw() {return "CRC";}
bool CRCHash::isCaseIndependant() const throw() {return false;}

void CRCHash::update(const char* pBuffer, size_t iBufferLength) throw()
{
  m_iValue = CRCHashSimple((void*)pBuffer, iBufferLength, m_iValue);
}

void CRCHash::finalise(unsigned char* pOutput) throw()
{
  memcpy(pOutput, &m_iValue, sizeof(unsigned long));
}

CRCCaselessHash::CRCCaselessHash() throw()
  : m_iValue(0) {}
CRCCaselessHash::~CRCCaselessHash() throw()
{}

size_t CRCCaselessHash::getSizeInBytes() const throw() {return sizeof(unsigned long);}
bool CRCCaselessHash::canUpdatesBeSplit() const throw() {return true;}
const char* CRCCaselessHash::getFamily() const throw() {return "CRC";}
bool CRCCaselessHash::isCaseIndependant() const throw() {return true;}

void CRCCaselessHash::update(const char* pBuffer, size_t iBufferLength) throw()
{
  m_iValue = CRCCaselessHashSimple((void*)pBuffer, iBufferLength, m_iValue);
}

void CRCCaselessHash::finalise(unsigned char* pOutput) throw()
{
  memcpy(pOutput, &m_iValue, sizeof(unsigned long));
}
