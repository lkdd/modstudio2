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
#ifdef RAINMAN2_USE_RBF

RbfAttributeFile::RbfAttributeFile() throw()
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
  }
  CATCH_THROW_SIMPLE(_cleanSelf(), L"Error reading RBF data");
}

void RbfAttributeFile::_zeroSelf() throw()
{
  memset(&m_oHeader, 0, sizeof(m_oHeader));
  m_pTables = 0;
  m_pData = 0;
  m_pDataIndex = 0;
  m_sStringBlock = 0;
  m_pKeys = 0;
}

void RbfAttributeFile::_cleanSelf() throw()
{
  delete[] m_pTables;
  delete[] m_pData;
  delete[] m_pDataIndex;
  delete[] m_sStringBlock;
  delete[] m_pKeys;
  _zeroSelf();
}

class RbfAttrTableAdapter : public IAttributeTable
{
public:
  RbfAttrTableAdapter(RbfAttributeFile *pFile, unsigned long iIndex);

  virtual unsigned long getChildCount() throw();
  virtual IAttributeValue* getChild(unsigned long iIndex) throw(...);
  virtual void addChild(unsigned long iName) throw(...);
  virtual unsigned long findChildIndex(unsigned long iName) throw();
  virtual void deleteChild(unsigned long iIndex, bool bRevertIsSufficient) throw(...);

protected:
  RbfAttributeFile *m_pFile;
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
  : m_pFile(pFile), m_oTable(pFile->m_pTables[iIndex])
{
}

unsigned long RbfAttrTableAdapter::getChildCount() throw()
{
  return m_oTable.iChildCount;
}

IAttributeValue* RbfAttrTableAdapter::getChild(unsigned long iIndex) throw(...)
{
  CHECK_RANGE_LTMAX(0, iIndex, m_oTable.iChildCount);
  iIndex += m_oTable.iChildIndex;
  if(m_oTable.iChildCount != 1)
    iIndex = m_pFile->m_pDataIndex[iIndex];
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
    for(unsigned long iIndex = 0; iIndex < m_oTable.iChildCount; ++iIndex)
    {
      if(iName == pDict->asciiToHash(m_pFile->m_pKeys[m_pFile->m_pData[m_pFile->m_pDataIndex[iIndex + m_oTable.iChildIndex]].iKeyIndex]))
        return iIndex;
    }
  }
  return NO_INDEX;
}

void RbfAttrTableAdapter::deleteChild(unsigned long iIndex, bool bRevertIsSufficient) throw(...)
{
  THROW_SIMPLE(L"Cannot delete children from an RBF table");
}

#endif
