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
#include "attributes.h"
#include "file.h"
#ifdef RAINMAN2_USE_RBF

class RAINMAN2_API RbfAttributeFile
{
public:
  RbfAttributeFile() throw();
  ~RbfAttributeFile() throw();

  const RainString& getFilename() throw() {return m_sFilename;}
  void setFilename(const RainString& sFilename) {m_sFilename = sFilename;}

  //! Load a single .RBF file
  void load(IFile *pFile) throw(...);

  //! Get the root-level GameData table
  IAttributeTable* getRootTable() throw(...);

protected:
  friend class RbfAttrTableAdapter;
  friend class RbfAttrValueAdapter;
  friend class LuaRainmanRbfMethods;

#pragma pack(push)
#pragma pack(1)
  struct _header_raw_t
  {
    char sTypeAndVer[8]; // Should be "RBF V0.1"
    // Offset of, and number of entries in, the table array
    unsigned long iTablesOffset,
                  iTablesCount,
    // Offset of, and number of entries in, the keys array
                  iKeysOffset,
                  iKeysCount,
    // Offset of, and number of entries in, the data index array
                  iDataIndexOffset,
                  iDataIndexCount,
    // Offset of, and number of entries in, the data array
                  iDataOffset,
                  iDataCount,
    // Offset of, and number of bytes in, the string block
                  iStringsOffset,
                  iStringsLength;
  };

  struct _table_raw_t
  {
    // If count == 0, then no children
    // If count == 1, then child is iChildIndex in the data array
    // Otherwise, the range [iChildIndex, iChildIndex + iChildCount) in the data index array gives the indicies in the data array
    unsigned long iChildCount,
                  iChildIndex;
  };

  struct _data_raw_t
  {
    enum eTypes
    {
      // boolean; uValue is 1 or 0
      T_Bool = 0,
      // single-precision floating point; fValue
      T_Float,
      // 32-bit signed int; iValue
      T_Int,
      // ASCII string; iValue gives the position in the string block of the length of the string (as a 32-bit int)
      //               iValue+4 is the position in the string block of the actual string data (NOT null terminated)
      T_String,
      // table; uValue is the index in the table array
      T_Table,
      // force eTypes to take 32 bits of storage
      T_force_dword = 0xffffffff
    } eType;
    // Index in the key array of the key
    unsigned long iKeyIndex;
    union
    {
     long iValue;
     unsigned long uValue;
     float fValue;
    };
  };
#pragma pack(pop)

  typedef char _key_raw_t[64];

  void _zeroSelf() throw();
  void _cleanSelf() throw();

  _header_raw_t  m_oHeader;
  _table_raw_t  *m_pTables;
  _data_raw_t   *m_pData;
  _key_raw_t    *m_pKeys;
  unsigned long *m_pDataIndex;
  char          *m_sStringBlock;
  RainString     m_sFilename;
};

#endif
