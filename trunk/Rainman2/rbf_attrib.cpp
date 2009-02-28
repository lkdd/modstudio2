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
#include "rbf_attrib.h"
#include "rgd_dict.h"
#include "exception.h"
#include "hash.h"
#ifdef RAINMAN2_USE_RBF

RbfAttributeFile::RbfAttributeFile() throw()
  : m_bAutoSortTableChildren(true)
{
  _zeroSelf();
}

RbfAttributeFile::~RbfAttributeFile() throw()
{
  _cleanSelf();
}

void RbfAttributeFile::load(IFile *pFile) throw(...)
{
  _cleanSelf();
  try
  {
    pFile->readOne(m_oHeader);
    if(memcmp(m_oHeader.sTypeAndVer, "RBF V0.1", 8) != 0)
      THROW_SIMPLE_(L"Invalid RBF signature (%.8s)", m_oHeader.sTypeAndVer);
  }
  CATCH_THROW_SIMPLE(_cleanSelf(), L"Error loading RBF header");

  try
  {
    pFile->seek(m_oHeader.iTablesOffset, SR_Start);
    CHECK_ALLOCATION(m_pTables = new (std::nothrow) _table_raw_t[m_oHeader.iTablesCount]);
    pFile->readArray(m_pTables, m_oHeader.iTablesCount);

    pFile->seek(m_oHeader.iKeysOffset, SR_Start);
    CHECK_ALLOCATION(m_pKeys = new (std::nothrow) _key_raw_t[m_oHeader.iKeysCount]);
    pFile->readArray(m_pKeys, m_oHeader.iKeysCount);

    pFile->seek(m_oHeader.iDataIndexOffset, SR_Start);
    CHECK_ALLOCATION(m_pDataIndex = new (std::nothrow) unsigned long[m_oHeader.iDataIndexCount]);
    pFile->readArray(m_pDataIndex, m_oHeader.iDataIndexCount);

    pFile->seek(m_oHeader.iDataOffset, SR_Start);
    CHECK_ALLOCATION(m_pData = new (std::nothrow) _data_raw_t[m_oHeader.iDataCount]);
    pFile->readArray(m_pData, m_oHeader.iDataCount);

    pFile->seek(m_oHeader.iStringsOffset, SR_Start);
    CHECK_ALLOCATION(m_sStringBlock = new char[m_oHeader.iStringsLength]);
    pFile->readArray(m_sStringBlock, m_oHeader.iStringsLength);

    if(m_oHeader.iTablesCount < 1)
      THROW_SIMPLE(L"RBF files must contain at least the top-level container table");
    for(unsigned long i = 0; i < m_oHeader.iTablesCount; ++i)
    {
      if(m_pTables[i].iChildCount == 1)
      {
        if(m_pTables[i].iChildIndex >= m_oHeader.iDataCount)
          THROW_SIMPLE_(L"Invalid data index on table %lu - points to %lu, but limit is %lu", i, m_pTables[i].iChildIndex, m_oHeader.iDataCount);
      }
      else if(m_pTables[i].iChildCount != 0)
      {
        if(m_pTables[i].iChildIndex >= m_oHeader.iDataIndexCount)
          THROW_SIMPLE_(L"Invalid index index on table %lu - points to %lu, but limit is %lu", i, m_pTables[i].iChildIndex, m_oHeader.iDataIndexCount);
        if((m_pTables[i].iChildIndex + m_pTables[i].iChildCount - 1) >= m_oHeader.iDataIndexCount)
          THROW_SIMPLE_(L"Invalid index count on table %lu - points to %lu, but limit is %lu", i, m_pTables[i].iChildIndex + m_pTables[i].iChildCount, m_oHeader.iDataIndexCount);
      }
    }
    for(unsigned long i = 0; i < m_oHeader.iDataIndexCount; ++i)
    {
      if(m_pDataIndex[i] >= m_oHeader.iDataCount)
        THROW_SIMPLE_(L"Invalid data index entry %lu - points to %lu, but limit is %lu", i, m_pDataIndex[i], m_oHeader.iDataCount);
    }
  }
  CATCH_THROW_SIMPLE(_cleanSelf(), L"Error reading RBF data");
}

void RbfAttributeFile::_buildKeyOrdering() throw(...)
{
  // quickSort defined statically within the function to get access to RbfAttributeFile's privates
  struct statics
  {
    static void quickSort(unsigned long* pArray, unsigned long iLength, RbfAttributeFile* pFile)
    {
quickSort_tailcall:
      if(iLength <= 1)
        return;

      unsigned long iPivot = pArray[0];
      unsigned long iIndex = 1;
      unsigned long iSwap;

      for(unsigned long i = 1; i < iLength; ++i)
      {
        if(strcmp(pFile->m_pKeys[pArray[i]], pFile->m_pKeys[iPivot]) < 0)
        {
          iSwap = pArray[i];
          pArray[i] = pArray[iIndex];
          pArray[iIndex] = iSwap;
          ++iIndex;
        }
      }
      --iIndex;
      pArray[0] = pArray[iIndex];
      pArray[iIndex] = iPivot;

      quickSort(pArray, iIndex, pFile);
      pArray = pArray + iIndex + 1;
      iLength = iLength - iIndex - 1;
      goto quickSort_tailcall;
    }
  };

  if(m_pKeyOrdering)
  {
    delete[] m_pKeyOrdering;
    m_pKeyOrdering = 0;
  }
  unsigned long *pKeyOrdering = CHECK_ALLOCATION(new (std::nothrow) unsigned long[m_oHeader.iKeysCount]);
  for(unsigned long i = 0; i < m_oHeader.iKeysCount; ++i)
  {
    pKeyOrdering[i] = i;
  }
  statics::quickSort(pKeyOrdering, m_oHeader.iKeysCount, this);
  m_pKeyOrdering = new (std::nothrow) unsigned long[m_oHeader.iKeysCount];
  if(!m_pKeyOrdering)
  {
    delete[] pKeyOrdering;
    CHECK_ALLOCATION(m_pKeyOrdering);
  }
  for(unsigned long i = 0; i < m_oHeader.iKeysCount; ++i)
  {
    m_pKeyOrdering[pKeyOrdering[i]] = i;
  }
  delete[] pKeyOrdering;
}

void RbfAttributeFile::_zeroSelf() throw()
{
  memset(&m_oHeader, 0, sizeof(m_oHeader));
  m_pTables = 0;
  m_pData = 0;
  m_pDataIndex = 0;
  m_sStringBlock = 0;
  m_pKeys = 0;
  m_pKeyOrdering = 0;
}

void RbfAttributeFile::_cleanSelf() throw()
{
  delete[] m_pTables;
  delete[] m_pData;
  delete[] m_pDataIndex;
  delete[] m_sStringBlock;
  delete[] m_pKeys;
  delete[] m_pKeyOrdering;
  _zeroSelf();
}

class RbfAttrTableAdapter : public IAttributeTable
{
public:
  RbfAttrTableAdapter(RbfAttributeFile *pFile, unsigned long iIndex);
  ~RbfAttrTableAdapter();

  virtual unsigned long getChildCount() throw();
  virtual IAttributeValue* getChild(unsigned long iIndex) throw(...);
  virtual void addChild(unsigned long iName) throw(...);
  virtual unsigned long findChildIndex(unsigned long iName) throw();
  virtual void deleteChild(unsigned long iIndex, bool bRevertIsSufficient) throw(...);

protected:
  RbfAttributeFile *m_pFile;
  unsigned long *m_pSortedIndex;
  RbfAttributeFile::_table_raw_t m_oTable;
};

class RbfAttrValueAdapter : public IAttributeValue
{
public:
  RbfAttrValueAdapter(RbfAttributeFile *pFile, unsigned long iIndex)
    : m_pFile(pFile), m_oData(pFile->m_pData[iIndex])
  {
  }

  unsigned long getName() const throw()
  {
    return RgdDictionary::getSingleton()->asciiToHash(m_pFile->m_pKeys[m_oData.iKeyIndex]);
  }

  eAttributeValueTypes getType() const throw()
  {
    switch(m_oData.eType)
    {
    case RbfAttributeFile::_data_raw_t::T_Bool:
      return VT_Boolean;
    case RbfAttributeFile::_data_raw_t::T_Float:
      return VT_Float;
    case RbfAttributeFile::_data_raw_t::T_Int:
      return VT_Integer;
    case RbfAttributeFile::_data_raw_t::T_String:
      return VT_String;
    case RbfAttributeFile::_data_raw_t::T_Table:
      return VT_Table;
    default:
      return VT_Unknown;
    }
  }

  eAttributeValueIcons getIcon() const throw()
  {
    if(m_oData.eType == RbfAttributeFile::_data_raw_t::T_Table)
      return VI_Table;
    else
      return VI_NewSinceParent;
  }

  RainString getFileSetIn() const throw()
  {
    return m_pFile->getFilename();
  }

  const IAttributeValue* getParent() const throw(...)
  {
    THROW_SIMPLE(L"RBF datum do not have parents");
  }

  const IAttributeValue* getParentNoThrow() const throw()
  {
    return NULL;
  }

  RainString getValueString() const throw(...)
  {
    CHECK_ASSERT(m_oData.eType == RbfAttributeFile::_data_raw_t::T_String);
    return RainString(m_pFile->m_sStringBlock + m_oData.uValue + sizeof(unsigned long), *(unsigned long*)(m_pFile->m_sStringBlock + m_oData.uValue));
  }

  const char* getValueStringRaw(size_t* iLength) const throw()
  {
    if(iLength)
      *iLength = *(unsigned long*)(m_pFile->m_sStringBlock + m_oData.uValue);
    return m_pFile->m_sStringBlock + m_oData.uValue + sizeof(unsigned long);
  }

  bool getValueBoolean() const throw(...)
  {
    CHECK_ASSERT(m_oData.eType == RbfAttributeFile::_data_raw_t::T_Bool);
    return m_oData.iValue != 0;
  }

  IAttributeTable* getValueTable() const throw(...)
  {
    CHECK_ASSERT(m_oData.eType == RbfAttributeFile::_data_raw_t::T_Table);
    return new RbfAttrTableAdapter(m_pFile, m_oData.uValue);
  }

  float getValueFloat() const throw(...)
  {
    CHECK_ASSERT(m_oData.eType == RbfAttributeFile::_data_raw_t::T_Float);
    return m_oData.fValue;
  }

  long getValueInteger() const throw(...)
  {
    CHECK_ASSERT(m_oData.eType == RbfAttributeFile::_data_raw_t::T_Int);
    return m_oData.iValue;
  }

  void setName(unsigned long iName) throw(...) { THROW_SIMPLE(L"Cannot modify RDF data"); }
  void setType(eAttributeValueTypes eNewType) throw(...) { THROW_SIMPLE(L"Cannot modify RDF data"); }
  void setValueString (const RainString& sValue) throw(...) { THROW_SIMPLE(L"Cannot modify RDF data"); }
  void setValueBoolean(bool bValue) throw(...) { THROW_SIMPLE(L"Cannot modify RDF data"); }
  void setValueFloat  (float fValue) throw(...) { THROW_SIMPLE(L"Cannot modify RDF data"); }

protected:
  RbfAttributeFile *m_pFile;
  RbfAttributeFile::_data_raw_t m_oData;
};

IAttributeTable* RbfAttributeFile::getRootTable() throw(...)
{
  return new RbfAttrTableAdapter(this, 0);
}

RbfAttrTableAdapter::RbfAttrTableAdapter(RbfAttributeFile *pFile, unsigned long iIndex)
  : m_pFile(pFile), m_oTable(pFile->m_pTables[iIndex]), m_pSortedIndex(0)
{
  struct statics
  {
    static void quickSort(unsigned long* pArray, unsigned long iLength, RbfAttributeFile* pFile)
    {
quickSort_tailcall:
      if(iLength <= 1)
        return;

      unsigned long iPivot = pArray[0];
      unsigned long iPivotValue = pFile->m_pKeyOrdering[pFile->m_pData[iPivot].iKeyIndex];
      unsigned long iIndex = 1;
      unsigned long iSwap;

      for(unsigned long i = 1; i < iLength; ++i)
      {
        if(pFile->m_pKeyOrdering[pFile->m_pData[pArray[i]].iKeyIndex] < iPivotValue)
        {
          iSwap = pArray[i];
          pArray[i] = pArray[iIndex];
          pArray[iIndex] = iSwap;
          ++iIndex;
        }
      }
      --iIndex;
      pArray[0] = pArray[iIndex];
      pArray[iIndex] = iPivot;

      quickSort(pArray, iIndex, pFile);
      pArray = pArray + iIndex + 1;
      iLength = iLength - iIndex - 1;
      goto quickSort_tailcall;
    }
  };

  if(m_oTable.iChildCount > 1 && pFile->m_bAutoSortTableChildren)
  {
    if(pFile->m_pKeyOrdering == 0)
    {
      pFile->_buildKeyOrdering();
    }
    CHECK_ALLOCATION(m_pSortedIndex = new (std::nothrow) unsigned long[m_oTable.iChildCount]);
    std::copy(pFile->m_pDataIndex + m_oTable.iChildIndex, pFile->m_pDataIndex + m_oTable.iChildIndex + m_oTable.iChildCount, m_pSortedIndex);
    statics::quickSort(m_pSortedIndex, m_oTable.iChildCount, pFile);
  }
}

RbfAttrTableAdapter::~RbfAttrTableAdapter()
{
  delete[] m_pSortedIndex;
}

unsigned long RbfAttrTableAdapter::getChildCount() throw()
{
  return m_oTable.iChildCount;
}

IAttributeValue* RbfAttrTableAdapter::getChild(unsigned long iIndex) throw(...)
{
  CHECK_RANGE_LTMAX(0, iIndex, m_oTable.iChildCount);
  if(m_oTable.iChildCount == 1)
    iIndex += m_oTable.iChildIndex;
  else if(m_pSortedIndex)
    iIndex = m_pSortedIndex[iIndex];
  else
    iIndex = m_pFile->m_pDataIndex[m_oTable.iChildIndex + iIndex];
  return new RbfAttrValueAdapter(m_pFile, iIndex);
}

void RbfAttrTableAdapter::addChild(unsigned long iName) throw(...)
{
  THROW_SIMPLE(L"Cannot add children to an RBF table");
}

unsigned long RbfAttrTableAdapter::findChildIndex(unsigned long iName) throw()
{
  RgdDictionary *pDict = RgdDictionary::getSingleton();
  if(m_oTable.iChildCount == 1)
  {
    if(iName == pDict->asciiToHash(m_pFile->m_pKeys[m_pFile->m_pData[m_oTable.iChildIndex].iKeyIndex]))
        return 0;
  }
  else
  {
    // Linearlly searching an alphabetically sorted array is no faster than searching a pseudo-randomly
    // sorted array, so go for the pseudo-random array as the alphabetically sorted array may not exist
    unsigned long *pIndex = m_pFile->m_pDataIndex + m_oTable.iChildIndex;
    for(unsigned long iIndex = 0; iIndex < m_oTable.iChildCount; ++iIndex, ++pIndex)
    {
      if(iName == pDict->asciiToHash(m_pFile->m_pKeys[m_pFile->m_pData[*pIndex].iKeyIndex]))
        return iIndex;
    }
  }
  return NO_INDEX;
}

void RbfAttrTableAdapter::deleteChild(unsigned long iIndex, bool bRevertIsSufficient) throw(...)
{
  THROW_SIMPLE(L"Cannot delete children from an RBF table");
}

RbfWriter::RbfWriter()
  : m_pStringBlock(0)
  , m_bEnableCaching(true)
  , m_iAmountSavedByCaching(0)
{
  m_oDataValue.eType = RbfAttributeFile::_data_raw_t::T_Bool;
  m_oDataValue.iKeyIndex = 0;
  m_oDataValue.uValue = 0;
}

RbfWriter::~RbfWriter()
{
  delete m_pStringBlock;
  for(_tables_in_progress_stack_t::iterator itr = m_stkTables.begin(), itrEnd = m_stkTables.end(); itr != itrEnd; ++itr)
  {
    delete itr->second;
  }
}

void RbfWriter::initialise()
{
  // reserve some memory
  m_stkTables.reserve(2);
  m_vKeys.reserve(2);
  m_vData.reserve(2);
  m_vTables.reserve(2);
  m_vDataIndex.reserve(2);

  // heap allocations
  m_pStringBlock = new MemoryWriteFile;
}

#define CACHED_SET(hash, map, field, code, always, saved) \
  _cache_map_t::iterator itr = (always || m_bEnableCaching) ? map.find(hash) : map.end(); \
  if(itr == map.end()) \
  { \
    code; \
    map[hash] = field; \
  } \
  else \
  { \
    field = itr->second; \
    m_iAmountSavedByCaching += saved; \
  }

void RbfWriter::setKey(const char *sKey, size_t iKeyLength) throw(...)
{
  if(iKeyLength > 64)
    THROW_SIMPLE_(L"Key \'%S\' is too long (%lu); maximum key length is 64 characters", sKey, static_cast<unsigned long>(iKeyLength));
  unsigned long iCRC = CRCHashSimple(sKey, iKeyLength);
  CACHED_SET(iCRC, m_mapKeys, m_oDataValue.iKeyIndex, {
    m_oDataValue.iKeyIndex = m_vKeys.size();
    _keys_buffer_t::reference rKey = m_vKeys.push_back();
    memcpy(rKey, sKey, iKeyLength);
    memset(rKey + iKeyLength, 0, 64 - iKeyLength);
  }, true, 0);
}

void RbfWriter::setValueBoolean(bool bValue) throw()
{
  m_oDataValue.eType = RbfAttributeFile::_data_raw_t::T_Bool;
  m_oDataValue.uValue = bValue ? 1 : 0;
}

void RbfWriter::setValueFloat(float fValue) throw()
{
  m_oDataValue.eType = RbfAttributeFile::_data_raw_t::T_Float;
  m_oDataValue.fValue = fValue;
}

void RbfWriter::setValueInteger(long iValue) throw()
{
  m_oDataValue.eType = RbfAttributeFile::_data_raw_t::T_Int;
  m_oDataValue.iValue = iValue;
}

void RbfWriter::setValueString(const char *sString, size_t iStringLength) throw(...)
{
  m_oDataValue.eType = RbfAttributeFile::_data_raw_t::T_String;
  unsigned long iCRC = CRCHashSimple(sString, iStringLength);
  CACHED_SET(iCRC, m_mapStrings, m_oDataValue.uValue, {
    m_oDataValue.uValue = m_pStringBlock->getLengthUsed();
    m_pStringBlock->writeOne(iStringLength);
    m_pStringBlock->writeArray(sString, iStringLength);
  }, false, iStringLength + 4);
}

void RbfWriter::setValueTable(unsigned long iIndex) throw()
{
  m_oDataValue.eType = RbfAttributeFile::_data_raw_t::T_Table;
  m_oDataValue.uValue = iIndex;
}

void RbfWriter::setTable() throw(...)
{
  if(m_stkTables.empty())
    THROW_SIMPLE(L"Cannot setTable into a non-existant table");
  unsigned long iCRC = CRCHashSimple(&m_oDataValue, sizeof(RbfAttributeFile::_data_raw_t));
  unsigned long iDataIndex;
  CACHED_SET(iCRC, m_mapData, iDataIndex, {
    iDataIndex = m_vData.size();
    m_vData.push_back(m_oDataValue);
  }, false, 12);
  m_stkTables.last().second->push_back(iDataIndex);
}

#undef CACHED_SET

void RbfWriter::pushTable() throw(...)
{
  _tables_in_progress_stack_t::reference rTable = m_stkTables.push_back();
  rTable.first = m_oDataValue.iKeyIndex;
  rTable.second = new _table_children_buffer_t;
}

unsigned long RbfWriter::popTable() throw(...)
{
  RbfAttributeFile::_table_raw_t oTable;
  {
    _tables_in_progress_stack_t::reference rTable = m_stkTables.last();
    m_oDataValue.iKeyIndex = rTable.first;
    oTable.iChildCount = rTable.second->size();
    if(oTable.iChildCount == 0)
      oTable.iChildIndex = 0;
    else if(oTable.iChildCount == 1)
      oTable.iChildIndex = rTable.second->last();
    else
    {
      oTable.iChildIndex = m_vDataIndex.size();
      m_vDataIndex.push_back(*rTable.second);
    }
    delete rTable.second;
    m_stkTables.pop_back();
  }
  {
    unsigned long iTableIndex;
    if(m_stkTables.empty())
      iTableIndex = 0;
    else
      iTableIndex = m_vTables.size() + 1;
    m_vTables[iTableIndex] = oTable;
    return iTableIndex;
  }
}

void RbfWriter::writeToFile(IFile *pFile) const throw(...)
{
  RbfAttributeFile::_header_raw_t oHead;
  oHead.iKeysOffset = 48;
  oHead.iKeysCount = m_vKeys.size();
  oHead.iDataOffset = oHead.iKeysOffset + 64 * oHead.iKeysCount;
  oHead.iDataCount = m_vData.size();
  oHead.iTablesOffset = oHead.iDataOffset + 12 * oHead.iDataCount;
  oHead.iTablesCount = m_vTables.size();
  oHead.iDataIndexOffset = oHead.iTablesOffset + 8 * oHead.iTablesCount;
  oHead.iDataIndexCount = m_vDataIndex.size();
  oHead.iStringsOffset = oHead.iDataIndexOffset + 4 * oHead.iDataIndexCount;
  oHead.iStringsLength = m_pStringBlock->getLengthUsed();

  pFile->writeArray("RBF V0.1", 8);
  pFile->writeOne(oHead);
  pFile->writeArray(&*m_vKeys, m_vKeys.size());
  pFile->writeArray(&*m_vData, m_vData.size());
  pFile->writeArray(&*m_vTables, m_vTables.size());
  pFile->writeArray(&*m_vDataIndex, m_vDataIndex.size());
  pFile->writeArray(m_pStringBlock->getBuffer(), m_pStringBlock->getLengthUsed());
}

#endif
