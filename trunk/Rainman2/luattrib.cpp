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
#include "luattrib.h"
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lobject.h>
#include <lopcodes.h>
#include <lstate.h>
}
#include <math.h>
#include <new>
#include <algorithm>
#include <locale>
#include <limits>
#include "hash.h"
#include "exception.h"
#include "rgd_dict.h"
#include "binaryattrib.h"
#pragma warning(disable: 4996)

class LuaAttribTableAdapter : public IAttributeTable
{
public:
  LuaAttribTableAdapter(LuaAttrib::_table_t *pTable, LuaAttrib *pFile);
  ~LuaAttribTableAdapter();

  unsigned long getChildCount() throw();
  IAttributeValue* getChild(unsigned long iIndex) throw(...);
  void addChild(unsigned long iName) throw(...);
  void deleteChild(unsigned long iIndex, bool bRevertIsSufficient) throw(...);
  unsigned long findChildIndex(unsigned long iName) throw();

protected:
  void _sortValues();

  std::vector<unsigned long> m_vValues;
  LuaAttrib::_table_t *m_pTable;
  LuaAttrib *m_pFile;
};

class LuaAttribValueAdapter : public IAttributeValue
{
public:
  LuaAttribValueAdapter(LuaAttrib::_value_t *pValue, LuaAttrib::_table_t *pParent, unsigned long iName, LuaAttrib *pFile)
    : m_pValue(pValue), m_pParent(pParent), m_iName(iName), m_pFile(pFile)
  {
    m_bConst = !pParent || (pParent->mapContents.count(iName) == 0);
  }
  ~LuaAttribValueAdapter()
  {
  }

  unsigned long        getName()      const throw()
  {
    return m_iName;
  }

  eAttributeValueTypes getType()      const throw()
  {
    switch(m_pValue->eType)
    {
    case LuaAttrib::_value_t::T_String:
      return VT_String;
    case LuaAttrib::_value_t::T_Boolean:
      return VT_Boolean;
    case LuaAttrib::_value_t::T_Float:
      return VT_Float;
    case LuaAttrib::_value_t::T_Table:
      return VT_Table;
    default:
      return VT_Unknown;
    }
  }

  RainString           getFileSetIn() const throw()
  {
    if(m_pFile)
      return m_pFile->getName();
    return L"";
  }

  eAttributeValueIcons getIcon()      const throw()
  {
    if(isTable())
    {
      if(m_pValue->pValue->bTotallyUnchaged)
        return VI_SameAsParent;
      else
        return VI_Table;
    }
    if(m_pParent->mapContents.count(m_iName) == 0)
      return VI_SameAsParent;
    const LuaAttrib::_table_t *pGrandparent = _findGrandparentWithKey();
    if(!pGrandparent)
      return VI_NewSinceParent;
    const LuaAttrib::_value_t &oValue = ((LuaAttrib::_table_t *)pGrandparent)->mapContents[m_iName];
    if(m_pValue->eType != oValue.eType)
      return VI_DifferentToParent;
    switch(getType())
    {
    case VT_String:
      return (m_pValue->sValue == oValue.sValue || (m_pValue->iLength == oValue.iLength && strcmp(oValue.sValue, m_pValue->sValue) == 0)) ? VI_SameAsParent : VI_DifferentToParent;
    case VT_Float:
      return (m_pValue->fValue == oValue.fValue) ? VI_SameAsParent : VI_DifferentToParent;
    case VT_Boolean:
      return (m_pValue->bValue == oValue.bValue) ? VI_SameAsParent : VI_DifferentToParent;
    default:
      return VI_DifferentToParent;
    };
  }

  const IAttributeValue* getParent() const throw(...)
  {
    LuaAttrib::_table_t *pGrandparent = (LuaAttrib::_table_t*)_findGrandparentWithKey();
    if(!pGrandparent)
      THROW_SIMPLE(L"Value is not present in any parent");
    return CHECK_ALLOCATION(new (std::nothrow) LuaAttribValueAdapter(&pGrandparent->mapContents[m_iName], pGrandparent, m_iName, pGrandparent->pSourceFile));
  }

  const IAttributeValue* getParentNoThrow() const throw()
  {
    LuaAttrib::_table_t *pGrandparent = (LuaAttrib::_table_t*)_findGrandparentWithKey();
    if(!pGrandparent)
      return 0;
    return (new (std::nothrow) LuaAttribValueAdapter(&pGrandparent->mapContents[m_iName], pGrandparent, m_iName, pGrandparent->pSourceFile));
  }

  RainString       getValueString () const throw(...)
  {
    CHECK_ASSERT(m_pValue->eType == LuaAttrib::_value_t::T_String);
    return RainString(m_pValue->sValue, m_pValue->iLength);
  }

  bool             getValueBoolean() const throw(...)
  {
    CHECK_ASSERT(m_pValue->eType == LuaAttrib::_value_t::T_Boolean);
    return m_pValue->bValue;
  }

  IAttributeTable* getValueTable  () const throw(...)
  {
    CHECK_ASSERT(m_pValue->eType == LuaAttrib::_value_t::T_Table);
    return new LuaAttribTableAdapter(m_pValue->pValue, m_pFile);
  }

  float            getValueFloat  () const throw(...)
  {
    CHECK_ASSERT(m_pValue->eType == LuaAttrib::_value_t::T_Float);
    return m_pValue->fValue;
  }

  virtual void setName(unsigned long iName          ) throw(...)
  {
    if(!m_pParent)
      THROW_SIMPLE(L"Cannot change the name of this item");
    m_pParent->mapContents[iName] = *m_pValue;
    m_pParent->mapContents.erase(m_iName);
    m_iName = iName;
  }

  virtual void setType(eAttributeValueTypes eNewType) throw(...)
  {
    if(getType() != eNewType)
    {
      _checkConst();
      m_pValue->free();
      switch(eNewType)
      {
      case VT_String:
        m_pValue->sValue = "";
        break;
      case VT_Boolean:
        m_pValue->bValue = false;
        break;
      case VT_Table:
        CHECK_ALLOCATION(m_pValue->pValue = new (std::nothrow) LuaAttrib::_table_t(m_pFile));
        break;
      case VT_Float:
        m_pValue->fValue = 0.0f;
        break;
      };
    }
  }

  virtual void setValueString (const RainString& sValue) throw(...)
  {
    setType(VT_String);
    _checkConst();
    lua_State *L = m_pFile->getCache()->getLuaState();
    lua_checkstack(L, 8);
    lua_pushlightuserdata(L, reinterpret_cast<void*>(m_pFile));
    lua_gettable(L, LUA_REGISTRYINDEX);
    if(lua_type(L, -1) == LUA_TNIL)
    {
      lua_pop(L, 1);
      lua_newtable(L);
      lua_pushlightuserdata(L, reinterpret_cast<void*>(m_pFile));
      lua_pushvalue(L, -2);
      lua_settable(L, LUA_REGISTRYINDEX);
    }
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    for(size_t i = 0; i < sValue.length(); ++i)
      luaL_addchar(&b, sValue.getCharacters()[i]);
    luaL_pushresult(&b);
    m_pValue->sValue = lua_tostring(L, -1);
    m_pValue->iLength = static_cast<long>(sValue.length());
    lua_rawseti(L, -2, static_cast<int>(lua_objlen(L, -2) + 1));
    lua_pop(L, 1);
  }

  virtual void setValueBoolean(bool bValue             ) throw(...)
  {
    setType(VT_Boolean);
    _checkConst();
    m_pValue->bValue = bValue;
  }

  virtual void setValueFloat  (float fValue            ) throw(...)
  {
    setType(VT_Float);
    _checkConst();
    m_pValue->fValue = fValue;
  }

  void _checkConst()
  {
    if(m_bConst)
    {
      if(!m_pParent)
        THROW_SIMPLE(L"Value is immutable");
      m_pParent->mapContents[m_iName] = *m_pValue;
      m_pValue = &m_pParent->mapContents[m_iName];
      m_pFile = m_pParent->pSourceFile;
      m_bConst = false;
    }
  }

  const LuaAttrib::_table_t* _findGrandparentWithKey() const
  {
    const LuaAttrib::_table_t *pTable = m_pParent;
    for(; pTable; pTable = pTable->pInheritFrom)
    {
      if(pTable->mapContents.count(m_iName))
      {
        for(pTable = pTable->pInheritFrom; pTable; pTable = pTable->pInheritFrom)
        {
          if(pTable->mapContents.count(m_iName))
            return pTable;
        }
        return 0;
      }
    }
    return 0;
  }

protected:
  LuaAttrib::_value_t *m_pValue;
  LuaAttrib::_table_t *m_pParent;
  unsigned long m_iName;
  bool m_bConst;
  LuaAttrib *m_pFile;
};

LuaAttribTableAdapter::LuaAttribTableAdapter(LuaAttrib::_table_t *_pTable, LuaAttrib *pFile)
  : m_pTable(_pTable), m_pFile(pFile)
{
  std::map<unsigned long, bool> mapKeys;
  for(const LuaAttrib::_table_t* pTable = _pTable; pTable; pTable = pTable->pInheritFrom)
  {
    for(std::map<unsigned long, LuaAttrib::_value_t>::const_iterator itr = pTable->mapContents.begin(); itr != pTable->mapContents.end(); ++itr)
    {
      if(mapKeys.count(itr->first) == 0)
      {
        mapKeys[itr->first] = true;
        m_vValues.push_back(itr->first);
      }
    }
  }
  _sortValues();
}

LuaAttribTableAdapter::~LuaAttribTableAdapter()
{
}

unsigned long LuaAttribTableAdapter::getChildCount() throw()
{
  return static_cast<unsigned long>(m_vValues.size());
}

IAttributeValue* LuaAttribTableAdapter::getChild(unsigned long iIndex) throw(...)
{
  CHECK_RANGE_LTMAX((unsigned long)0, iIndex, getChildCount());
  unsigned long iKey = m_vValues[iIndex];
  for(LuaAttrib::_table_t* pTable = m_pTable; pTable; pTable = const_cast<LuaAttrib::_table_t*>(pTable->pInheritFrom))
  {
    if(pTable->mapContents.count(iKey))
    {
      LuaAttrib::_value_t *pValue = &pTable->mapContents[iKey];
      if(pValue->eType == LuaAttrib::_value_t::T_Table && pTable != m_pTable)
      {
        LuaAttrib::_value_t oValue = *pValue;
        m_pFile->_wraptable(&oValue);
        oValue.pValue->bTotallyUnchaged = true;
        m_pTable->mapContents[iKey] = oValue;
        pTable = m_pTable;
        pValue = &pTable->mapContents[iKey];
      }
      return CHECK_ALLOCATION(new (std::nothrow) LuaAttribValueAdapter(pValue, m_pTable, iKey, pTable->pSourceFile));
    }
  }
  THROW_SIMPLE(L"Child not found - should never happen");
}

void LuaAttribTableAdapter::addChild(unsigned long iName) throw(...)
{
  if(m_pTable->mapContents.count(iName))
    THROW_SIMPLE(L"Cannot add a child which already exists");
  m_pTable->mapContents[iName] = LuaAttrib::_value_t();
  m_vValues.push_back(iName);
  _sortValues();
}

void LuaAttribTableAdapter::deleteChild(unsigned long iIndex, bool bRevertIsSufficient) throw(...)
{
  CHECK_RANGE_LTMAX((unsigned long)0, iIndex, getChildCount());
  unsigned long iKey = m_vValues[iIndex];
  bool bInTopLevel, bInFurtherLevels = false;
  bInTopLevel = m_pTable->mapContents.count(iKey) == 1;
  for(const LuaAttrib::_table_t* pTable = m_pTable->pInheritFrom; pTable; pTable = pTable->pInheritFrom)
  {
    bInFurtherLevels = bInFurtherLevels || (pTable->mapContents.count(iKey) == 1);
  }
  if(bInFurtherLevels)
  {
    if(!bInTopLevel || !bRevertIsSufficient)
      THROW_SIMPLE(L"Cannot delete child as it is set in inherited files");
    m_pTable->mapContents.erase(iKey);
  }
  else
  {
    m_pTable->mapContents.erase(iKey);
    m_vValues.erase(m_vValues.begin() + iIndex);
  }
}

unsigned long LuaAttribTableAdapter::findChildIndex(unsigned long iName) throw()
{
  unsigned long iIndex = 0;
  for(std::vector<unsigned long>::iterator itr = m_vValues.begin(); itr != m_vValues.end(); ++itr, ++iIndex)
  {
    if(*itr == iName)
      return iIndex;
  }
  return NO_INDEX;
}

//! Alphabetical comparison function for RGD hashes
/*!
  Sorting with this function as the predicate causes hashes with
  no known text equivalent to be placed first, in ascending order,
  followed by hashes with known text equivalents, in alphabetical
  order.
*/
static bool HashSortLessThen(unsigned long i1, unsigned long i2)
{
  RgdDictionary *pDictionary = RgdDictionary::getSingleton();
  const char* s1 = pDictionary->hashToAsciiNoThrow(i1);
  const char* s2 = pDictionary->hashToAsciiNoThrow(i2);

  // Two strings => lexicographical less-than
  if(s1 && s2)
    return stricmp(s1, s2) < 0;
  else
  {
    // One hash and one string => hash comes first
    if(s1)
      return false;
    else if(s2)
      return true;

    // Two hashes => smaller one first
    return i1 < i2;
  }
}

//! HashSortLessThen as a function object
struct HashSortLessThen_t
{
  bool operator()(unsigned long a, unsigned long b) const
  {
    return HashSortLessThen(a, b);
  }
};

void LuaAttribTableAdapter::_sortValues()
{
  std::sort(m_vValues.begin(), m_vValues.end(), HashSortLessThen);
}

LuaAttribCache::LuaAttribCache()
{
  m_L = 0;
  m_pDirectory = 0;
  m_oEmptyTable.eType = LuaAttrib::_value_t::T_Table;
  m_oEmptyTable.pValue = new LuaAttrib::_table_t(0);
  m_pEmptyFile = new LuaAttrib;
  m_pEmptyFile->setName(L"");
  m_oEmptyTable.pValue->pSourceFile = m_pEmptyFile;
}

LuaAttribCache::~LuaAttribCache()
{
  delete m_pEmptyFile;
  for(std::map<unsigned long, LuaAttrib*>::iterator itr = m_mapFiles.begin(); itr != m_mapFiles.end(); ++itr)
    delete itr->second;
  if(m_L)
    lua_close(m_L);
}

lua_State* LuaAttribCache::getLuaState()
{
  if(!m_L)
    m_L = luaL_newstate();
  return m_L;
}

void LuaAttribCache::setBaseFolder(IDirectory *pFolder)
{
  m_pDirectory = pFolder;
}

LuaAttrib* LuaAttribCache::_performGet(const char* sFilename)
{
  if(sFilename[0] == 0)
    return 0;
  unsigned long iHash = CRCCaselessHashSimple((const void*)sFilename, strlen(sFilename));
  LuaAttrib* pAttribFile = m_mapFiles[iHash];
  if(!pAttribFile)
  {
    IFile* pFile = 0;
    try
    {
      pFile = m_pDirectory->getStore()->openFile(m_pDirectory->getPath() + RainString(sFilename), FM_Read);
    }
    CATCH_THROW_SIMPLE_({}, L"Cannot open attrib file \'%S\'", sFilename);
    lua_State *L = getLuaState();
    int iLuaStackTop = lua_gettop(L);
    try
    {
      CHECK_ALLOCATION(pAttribFile = new LuaAttrib);
      pAttribFile->setName(sFilename);
      pAttribFile->setBaseFolder(m_pDirectory);
      pAttribFile->setCache(this, false);
      pAttribFile->loadFromFile(pFile);
      delete pFile;
      pFile = 0;
    }
    CATCH_THROW_SIMPLE_({delete pAttribFile; lua_settop(L, iLuaStackTop); delete pFile;}, L"Cannot load attrib file \'%S\'", sFilename);
    int iNewStackTop = lua_gettop(L);
    if(iNewStackTop > iLuaStackTop)
    {
      lua_checkstack(L, 5);
      lua_createtable(L, iNewStackTop - iLuaStackTop, 0);
      lua_pushlightuserdata(L, (void*)pAttribFile);
      lua_pushvalue(L, -2);
      lua_settable(L, LUA_REGISTRYINDEX);
      for(int iIndex = (iLuaStackTop + 1); iIndex <= iNewStackTop; ++iIndex)
      {
        lua_pushvalue(L, iIndex);
        lua_rawseti(L, -2, iIndex - iLuaStackTop);
      }
      lua_settop(L, iLuaStackTop);
    }
    else if(iNewStackTop < iLuaStackTop)
    {
      delete pAttribFile;
      THROW_SIMPLE_(L"LuaAttrib loading \'%S\' popped items off the stack", sFilename);
    }
    m_mapFiles[iHash] = pAttribFile;
  }
  return pAttribFile;
}

LuaAttrib::_value_t LuaAttribCache::getMetaData(const char* sFilename)
{
  LuaAttrib *pFile = _performGet(sFilename);
  return pFile ? pFile->m_oMetaData : m_oEmptyTable;
}

LuaAttrib::_value_t LuaAttribCache::getGameData(const char* sFilename)
{
  LuaAttrib *pFile = _performGet(sFilename);
  return pFile ? pFile->m_oGameData : m_oEmptyTable;
}

size_t LuaAttribCache::writeTableToBinaryFile(const LuaAttrib::_table_t* pTable, IFile* pFile, bool bCacheResult)
{
  while(pTable->bTotallyUnchaged && pTable->pInheritFrom)
  {
    pTable = pTable->pInheritFrom;
    bCacheResult = true;
  }
  try
  {
    if(m_mapTables.count(pTable) != 0)
    {
      MemoryWriteFile *pCached = m_mapTables[pTable];
      pFile->write(pCached->getBuffer(), 1, pCached->tell());
      return pCached->tell();
    }
    else
    {
      if(bCacheResult)
      {
        MemoryWriteFile *pCached = new MemoryWriteFile;
        m_mapTables[pTable] = pCached;
        pTable->writeToBinary(pCached);
        pFile->write(pCached->getBuffer(), 1, pCached->tell());
        return pCached->tell();
      }
      else
      {
        return pTable->writeToBinary(pFile);
      }
    }
  }
  catch(RainException *pE)
  {
    RETHROW_SIMPLE(pE, L"Error writing Lua table to binary file");
  }
}

LuaAttrib::LuaAttrib() throw()
{
  m_pDirectory = 0;
  m_pCache = 0;
  m_bOwnCache = false;
}

LuaAttrib::~LuaAttrib() throw()
{
  if(m_bOwnCache)
    delete m_pCache;
}

IAttributeValue* LuaAttrib::getGameData() throw(...)
{
  return new LuaAttribValueAdapter(&m_oGameData, 0, RgdDictionary::getSingleton()->asciiToHash("GameData"), this);
}

IAttributeValue* LuaAttrib::getMetaData() throw(...)
{
  return new LuaAttribValueAdapter(&m_oMetaData, 0, RgdDictionary::getSingleton()->asciiToHash("MetaData"), this);
}

void LuaAttrib::setName(const RainString& sName) throw()
{
  m_sName = sName;
}

RainString& LuaAttrib::getName() throw()
{
  return m_sName;
}

LuaAttribCache* LuaAttrib::getCache() throw(...)
{
  if(!m_pCache)
  {
    CHECK_ALLOCATION(m_pCache = new (std::nothrow) LuaAttribCache);
    m_pCache->setBaseFolder(m_pDirectory);
    m_bOwnCache = true;
  }
  return m_pCache;
}

void LuaAttrib::setCache(LuaAttribCache* pCache, bool bOwnCache) throw(...)
{
  CHECK_ASSERT(m_pCache == 0 && pCache != 0);
  m_pCache = pCache;
  m_bOwnCache = bOwnCache;
}

void LuaAttrib::setBaseFolder(IDirectory *pFolder) throw()
{
  m_pDirectory = pFolder;
}

template <class TIn, class TOut>
static TOut static_cast_tolower(TIn v)
{
  return std::tolower(static_cast<TOut>(v), std::locale::classic());
}

//! Calculates the length of a string literal at compile-time, rather than leaving it to runtime
/*!
  \param s A string literal
  Returns two values; the literal itself, and the length of the literal, *not* including the
  NULL terminator (hence the -1).
*/
#define LITERAL(s) (s), ((sizeof(s)/sizeof(*(s)))-1)

void LuaAttrib::saveToTextFile(IFile *pFile) throw(...)
{
  CHECK_ASSERT(m_oGameData.eType == _value_t::T_Table && m_oMetaData.eType == _value_t::T_Table);

  try
  {
    BufferingOutputStream<char> oOutput(pFile);
    oOutput.write(LITERAL("----------------------------------------\r\n"));
    oOutput.write(LITERAL("-- File: \'"));
    oOutput.writeConverting(m_sName.getCharacters(), m_sName.length(), static_cast_tolower<RainChar, char>);
    oOutput.write(LITERAL("\'\r\n"));
    oOutput.write(LITERAL("-- Created by: Corsix\'s LuaEdit\r\n"));
    oOutput.write(LITERAL("-- Note: Feel free to edit by hand!\r\n"));
    oOutput.write(LITERAL("-- Partially or fully (c) Relic Entertainment Inc.\r\n\r\n"));
    _writeInheritLine(oOutput, m_oGameData, false);
    _writeInheritLine(oOutput, m_oMetaData, true);
    m_oGameData.pValue->writeToText(oOutput, LITERAL("GameData"));
    oOutput.write(LITERAL("\r\n"));
    _writeMetaData(oOutput);
  }
  CATCH_THROW_SIMPLE({}, L"Error encountered while writing Lua data");
}

void LuaAttrib::_writeInheritLine(BufferingOutputStream<char>& oOutput, LuaAttrib::_value_t& oTable, bool bIsMetaData)
{
  if(bIsMetaData)
    oOutput.write(LITERAL("MetaData = InheritMeta([["));
  else
    oOutput.write(LITERAL("GameData = Inherit([["));
  if(oTable.pValue->pInheritFrom)
  {
    RainString &sInheritFrom = oTable.pValue->pInheritFrom->pSourceFile->m_sName;
    oOutput.writeConverting(sInheritFrom.getCharacters(), sInheritFrom.length(), static_cast_tolower<RainChar, char>);
  }
  oOutput.write("]])\r\n\r\n", bIsMetaData ? 7 : 5);
}

void LuaAttrib::_writeMetaData(BufferingOutputStream<char>& oOutput)
{
  // Re-order the keys alphabetically, stripping out $REF
  std::map<unsigned long, _value_t*, HashSortLessThen_t> mapKeys;
  for(std::map<unsigned long, _value_t>::iterator itr = m_oMetaData.pValue->mapContents.begin(); itr != m_oMetaData.pValue->mapContents.end(); ++itr)
  {
    if(itr->first != RgdDictionary::_REF)
      mapKeys[itr->first] = &itr->second;
  }

  // Print keys
  for(std::map<unsigned long, _value_t*, HashSortLessThen_t>::iterator itr = mapKeys.begin(); itr != mapKeys.end(); ++itr)
  {
    if(itr->first == RgdDictionary::_REF)
      continue;

    oOutput.write(LITERAL("MetaData[\""));
    oOutput.write(RgdDictionary::getSingleton()->hashToAscii(itr->first));
    oOutput.write(LITERAL("\"] = "));

    CHECK_ASSERT(itr->second->eType == _value_t::T_Table && "MetaData children should all be tables");

    itr->second->pValue->writeToTextAsMetaDataTable(oOutput);
  }
}

static bool StringHasSpecialChars(const char* s)
{
  for(; *s; ++s)
  {
    if(*s == '\\' || *s == '\r' || *s == '\n' || *s == '\"')
      return true;
  }
  return false;
}

void LuaAttrib::_table_t::writeToTextAsMetaDataTable(BufferingOutputStream<char>& oOutput)
{
  oOutput.write(LITERAL("{"));
  for(std::map<unsigned long, _value_t>::iterator itr = mapContents.begin(); itr != mapContents.end(); ++itr)
  {
    oOutput.write(RgdDictionary::getSingleton()->hashToAscii(itr->first));
    oOutput.write(LITERAL(" = "));
    switch(itr->second.eType)
    {
    case LuaAttrib::_value_t::T_Float: {
      float fValue = itr->second.fValue;
      char sBuffer[64];
      if(fValue == floor(fValue) && fValue <= static_cast<float>(std::numeric_limits<long>::max()) && fValue >= static_cast<float>(std::numeric_limits<long>::min()))
      {
        sprintf(sBuffer, "%li", static_cast<long>(fValue));
      }
      else
      {
        sprintf(sBuffer, "%.3f", fValue);
      }
      oOutput.write(sBuffer);
      break; }

    case LuaAttrib::_value_t::T_String:
      oOutput.write(LITERAL("[["));
      oOutput.write(itr->second.sValue, itr->second.iLength);
      oOutput.write(LITERAL("]]"));
      break;

    case LuaAttrib::_value_t::T_Boolean:
      if(itr->second.bValue)
        oOutput.write(LITERAL("true"));
      else
        oOutput.write(LITERAL("false"));
      break;

    case LuaAttrib::_value_t::T_Integer: {
      char sBuffer[64];
      sprintf(sBuffer, "%li", itr->second.iValue);
      break; }

    case LuaAttrib::_value_t::T_Table:
      itr->second.pValue->writeToTextAsMetaDataTable(oOutput);
      break;

    default:
      THROW_SIMPLE_(L"Cannot write meta data of type %i to text file", static_cast<int>(itr->second.eType));
    }
    oOutput.write(LITERAL(", "));
  }
  oOutput.write(LITERAL("}\r\n"));
}

void LuaAttrib::_table_t::writeToText(BufferingOutputStream<char>& oOutput, const char* sPrefix, size_t iPrefixLength)
{
  // Re-order the keys alphabetically, stripping out $REF
  std::map<unsigned long, _value_t*, HashSortLessThen_t> mapKeys;
  for(std::map<unsigned long, _value_t>::iterator itr = mapContents.begin(); itr != mapContents.end(); ++itr)
  {
    if(itr->first != RgdDictionary::_REF)
      mapKeys[itr->first] = &itr->second;
  }

  // Print keys
  for(std::map<unsigned long, _value_t*, HashSortLessThen_t>::iterator itr = mapKeys.begin(); itr != mapKeys.end(); ++itr)
  {
    if(itr->second->eType != _value_t::T_Table || itr->second->pValue->mapContents.count(RgdDictionary::_REF) == 1)
    {
      oOutput.write(sPrefix, iPrefixLength);
      oOutput.write(LITERAL("[\""));
      oOutput.write(RgdDictionary::getSingleton()->hashToAscii(itr->first));
      oOutput.write(LITERAL("\"] = "));
      switch(itr->second->eType)
      {
      case _value_t::T_Float: {
        char sBuffer[64];
        sprintf(sBuffer, "%.5f", itr->second->fValue);
        oOutput.write(sBuffer);
        break; }

      case _value_t::T_String:
        if(StringHasSpecialChars(itr->second->sValue))
        {
          oOutput.write(LITERAL("[["));
          oOutput.write(itr->second->sValue, itr->second->iLength);
          oOutput.write(LITERAL("]]"));
        }
        else
        {
          oOutput.write(LITERAL("\""));
          oOutput.write(itr->second->sValue, itr->second->iLength);
          oOutput.write(LITERAL("\""));
        }
        break;

      case _value_t::T_Boolean:
        if(itr->second->bValue)
          oOutput.write(LITERAL("true"));
        else
          oOutput.write(LITERAL("false"));
        break;

      case _value_t::T_Integer: {
        char sBuffer[64];
        sprintf(sBuffer, "%li", itr->second->iValue);
        break; }

      case _value_t::T_Table: {
        oOutput.write(LITERAL("Reference([["));
        RainString sReferenceFrom = itr->second->pValue->pInheritFrom->pSourceFile->m_sName;
        oOutput.writeConverting(sReferenceFrom.getCharacters(), sReferenceFrom.length(), static_cast_tolower<RainChar, char>);
        oOutput.write(LITERAL("]])"));
        break; }

      default:
        THROW_SIMPLE_(L"Cannot write data of type %i to text file", static_cast<int>(itr->second->eType));
      }
      oOutput.write(LITERAL("\r\n"));
    }
    if(itr->second->eType == _value_t::T_Table)
    {
      _table_t *pChildTable = itr->second->pValue;
      if(!pChildTable->bTotallyUnchaged)
      {
        const char *sName = RgdDictionary::getSingleton()->hashToAscii(itr->first);
        size_t iNameLength = strlen(sName);
        char *sNewPrefix = CHECK_ALLOCATION(new (std::nothrow) char[iPrefixLength + 5 + iNameLength]);
        sprintf(sNewPrefix, "%s[\"%s\"]", sPrefix, sName);
        try
        {
          pChildTable->writeToText(oOutput, sNewPrefix, iPrefixLength + 4 + iNameLength);
        }
        CATCH_THROW_SIMPLE_(delete[] sNewPrefix, L"Cannot write child table \'%S\'", sName);
        delete[] sNewPrefix;
      }
    }
  }
}

#undef LITERAL

void LuaAttrib::loadFromFile(IFile *pFile) throw(...)
{
  m_oGameData.free();
  lua_State *L = getCache()->getLuaState();
  lua_checkstack(L, 1);
  if(pFile->lua_load(L, "attrib file") != 0)
  {
    // Try and parse the Lua error into a nice exception
    // Lua gives an error as "file:line:message", which can be reparsed into an exception
    size_t iLength;
    const char* sErr = lua_tolstring(L, -1, &iLength);
    if(sErr)
    {
      const char* sLineBegin = strchr(sErr, ':');
      if(sLineBegin)
      {
        ++sLineBegin;
        const char* sMessageBegin = strchr(sLineBegin, ':');
        if(sMessageBegin)
        {
          ++sMessageBegin;
          throw new RainException(__FILE__, __LINE__, RainString(L"Cannot parse attrib file"), new
            RainException(RainString(sErr, sLineBegin - sErr - 1), atoi(sLineBegin), RainString(sMessageBegin, iLength - (sMessageBegin - sErr))));
        }
      }
    }

    // ... or just throw a simple exception
    throw new RainException(__FILE__, __LINE__, RainString(L"Cannot parse attrib file: ") + RainString(L, -1));
  }
  Proto *pPrototype = reinterpret_cast<Closure*>(const_cast<void*>(lua_topointer(L, -1)))->l.p;
  _execute(pPrototype);
}

void LuaAttrib::_loadk(_value_t *pDestination, Proto* pFunction, int iK)
{
  pDestination->free();
  TValue *pValue = pFunction->k + iK;
  switch(pValue->tt)
  {
  case LUA_TNIL:
    break;
  case LUA_TBOOLEAN:
    pDestination->eType = _value_t::T_Boolean;
    pDestination->bValue = pValue->value.b != 0;
    break;
  case LUA_TNUMBER:
    pDestination->eType = _value_t::T_Float;
    pDestination->fValue = static_cast<float>(pValue->value.n);
    break;
  case LUA_TSTRING:
    pDestination->eType = _value_t::T_String;
    pDestination->sValue = svalue(pValue);
    pDestination->iLength = static_cast<long>(tsvalue(pValue)->len);
    break;
  default:
    THROW_SIMPLE_(L"Unsupported constant type: %i", static_cast<int>(pValue->tt));
  };
}

unsigned long LuaAttrib::_hashvalue(_value_t* pValue)
{
  switch(pValue->eType)
  {
  case _value_t::T_String:
    return RgdDictionary::getSingleton()->asciiToHash(pValue->sValue, pValue->iLength);
  case _value_t::T_Boolean:
    return RgdDictionary::getSingleton()->asciiToHash(pValue->bValue ? "true" : "false");
  case _value_t::T_Float: {
    char sBuffer[32];
    if(floor(pValue->fValue) == pValue->fValue)
      sprintf(sBuffer, "%.0f", pValue->fValue);
    else
      sprintf(sBuffer, "%.7f", pValue->fValue);
    return RgdDictionary::getSingleton()->asciiToHash(sBuffer, strlen(sBuffer)); }
  default:
    THROW_SIMPLE_("Cannot hash value of type %i", static_cast<int>(pValue->eType));
  }
}

void LuaAttrib::_wraptable(_value_t* pValue) throw(...)
{
  _table_t* pTable = pValue->pValue;
  pValue->pValue = CHECK_ALLOCATION(new (std::nothrow) _table_t(this));
  pValue->pValue->pInheritFrom = pTable;
}

template <class T>
struct AutoDeleteArray
{
  AutoDeleteArray(T* p) : m_p(p) {}
  ~AutoDeleteArray() {delete[] m_p;}
  T* m_p;
};

void LuaAttrib::_execute(Proto* pFunction)
{
  _value_t oTempK1, oTempK2;
  _value_t* pStack = CHECK_ALLOCATION(new (std::nothrow) _value_t[pFunction->maxstacksize]);
  Instruction *I;
  AutoDeleteArray<_value_t> DeleteStack(pStack);

  bool bContinue = true;
  try
  {
    for(I = pFunction->code; bContinue; ++I)
    {
      switch(GET_OPCODE(*I))
      {
      case OP_MOVE:      // A B   R(A) := R(B)
        pStack[GETARG_A(*I)] = pStack[GETARG_B(*I)];
        break;

      case OP_LOADK:     // A Bx  R(A) := Kst(Bx)
        _loadk(pStack + GETARG_A(*I), pFunction, GETARG_Bx(*I));
        break;

      case OP_LOADBOOL:  // A B C R(A) := (Bool)B; if (C) pc++
        pStack[GETARG_A(*I)].free();
        pStack[GETARG_A(*I)].eType = _value_t::T_Boolean;
        pStack[GETARG_A(*I)].bValue = GETARG_B(*I) != 0;
        if(GETARG_C(*I))
          ++I;
        break;

      case OP_LOADNIL:   // A B   R(A) := ... := R(B) := nil
        for(int i = GETARG_A(*I); i <= GETARG_B(*I); ++i)
          pStack[i].free();
        break;

      case OP_GETGLOBAL:{// A Bx  R(A) := Gbl[Kst(Bx)]
        unsigned int iHash = tsvalue(pFunction->k + GETARG_Bx(*I))->hash;
        const char* s = svalue(pFunction->k + GETARG_Bx(*I));
        switch(iHash)
        {
        case LuaCompileTimeHashes::GameData:
          pStack[GETARG_A(*I)] = m_oGameData;
          break;
        case LuaCompileTimeHashes::MetaData:
          pStack[GETARG_A(*I)] = m_oMetaData;
          break;
        case LuaCompileTimeHashes::Inherit:
        case LuaCompileTimeHashes::Reference:
          pStack[GETARG_A(*I)].free();
          pStack[GETARG_A(*I)].eType = _value_t::T_ReferenceFn;
          break;
        case LuaCompileTimeHashes::InheritMeta:
          pStack[GETARG_A(*I)].free();
          pStack[GETARG_A(*I)].eType = _value_t::T_InheritMetaFn;
          break;
        default:
          THROW_SIMPLE_(L"Unsupported global: %S", s);
        }
        break; }

      case OP_GETTABLE: {// A B C R(A) := R(B)[RK(C)]
        _value_t* pTable = pStack + GETARG_B(*I);
        CHECK_ASSERT(pTable->eType == _value_t::T_Table && pTable->pValue && "Using a non-table value as a table");
        _value_t* pKey = ISK(GETARG_C(*I)) ? (_loadk(&oTempK1, pFunction, INDEXK(GETARG_C(*I))), &oTempK1) : pStack + GETARG_C(*I);
        unsigned long iHash = _hashvalue(pKey);
        _value_t oValue;
        _table_t* pLookin = pTable->pValue;
        while(pLookin && pLookin->mapContents.count(iHash) == 0)
          pLookin = const_cast<_table_t*>(pLookin->pInheritFrom);
        if(pLookin)
        {
          oValue = pLookin->mapContents[iHash];
          if(oValue.eType == _value_t::T_Table && pLookin != pTable->pValue)
          {
            _wraptable(&oValue);
            pTable->pValue->mapContents[iHash] = oValue;
          }
        }
        pStack[GETARG_A(*I)] = oValue;
        break; }

      case OP_SETGLOBAL:{// A Bx  Gbl[Kst(Bx)] := R(A)
        unsigned int iHash = tsvalue(pFunction->k + GETARG_Bx(*I))->hash;
        if(iHash == LuaCompileTimeHashes::GameData)
          m_oGameData = pStack[GETARG_A(*I)];
        else if(iHash == LuaCompileTimeHashes::MetaData)
          m_oMetaData = pStack[GETARG_A(*I)];
        else
          THROW_SIMPLE_(L"Unsupported global for write access: %S", svalue(pFunction->k + GETARG_Bx(*I)));
        break; }

      case OP_SETTABLE: {// A B C R(A)[RK(B)] := RK(C)
        _value_t* pTable = pStack + GETARG_A(*I);
        _value_t* pKey = ISK(GETARG_B(*I)) ? (_loadk(&oTempK2, pFunction, INDEXK(GETARG_B(*I))), &oTempK2) : pStack + GETARG_B(*I);
        _value_t* pValue = ISK(GETARG_C(*I)) ? (_loadk(&oTempK1, pFunction, INDEXK(GETARG_C(*I))), &oTempK1) : pStack + GETARG_C(*I);
        if(pTable->eType != _value_t::T_Table || !pTable->pValue)
        {
          THROW_SIMPLE_(L"Using a non-table value as a table: table['%S'] = '%S' on line %i",
            pKey->eType == _value_t::T_String ? pKey->sValue : "(non string)",
            pValue->eType == _value_t::T_String ? pValue->sValue : "(non string)",
            static_cast<int>(pFunction->sizelineinfo == pFunction->sizecode ? pFunction->lineinfo[I - pFunction->code] : -1));
        }
        unsigned long iHash = _hashvalue(pKey);
        pTable->pValue->mapContents[iHash] = *pValue;
        break; }

      case OP_NEWTABLE:  // A B C R(A) := {} (size = B,C)
        pStack[GETARG_A(*I)].free();
        pStack[GETARG_A(*I)].eType = _value_t::T_Table;
        CHECK_ALLOCATION(pStack[GETARG_A(*I)].pValue = new (std::nothrow) _table_t(this));
        break;

      case OP_NOT: {     // A B   R(A) := not R(B)
        bool bValue = false;
        _value_t* pValue = pStack + GETARG_B(*I);
        if(pValue->eType == _value_t::T_Nil)
          bValue = true;
        if(pValue->eType == _value_t::T_Boolean)
          bValue = !pValue->bValue;
        pValue = pStack + GETARG_A(*I);
        pValue->free();
        pValue->eType = _value_t::T_Boolean;
        pValue->bValue = bValue;
        break; }

      case OP_JMP:       // sBx   pc+=sBx
        I += GETARG_sBx(*I);
        break;

      case OP_CALL: {    // A B C R(A), ... ,R(A+C-2) := R(A)(R(A+1), ... ,R(A+B-1))
        _value_t* pFunction = pStack + GETARG_A(*I);
        CHECK_ASSERT((pFunction->eType == _value_t::T_InheritMetaFn || pFunction->eType == _value_t::T_ReferenceFn) && "Attempt to call a non-function");
        CHECK_ASSERT(GETARG_B(*I) != 1 && "Function call requires at least one argument");
        CHECK_ASSERT(GETARG_C(*I) == 2 && "Function call must have exactly one result");
        _value_t* pFilename = pStack + GETARG_A(*I) + 1;
        CHECK_ASSERT(pFilename->eType == _value_t::T_String && "Can only inherit/reference a string filename");
        try
        {
          if(pFunction->eType == _value_t::T_InheritMetaFn)
            pStack[GETARG_A(*I)] = getCache()->getMetaData(pFilename->sValue);
          else
            pStack[GETARG_A(*I)] = getCache()->getGameData(pFilename->sValue);
          if(pStack[GETARG_A(*I)].eType != _value_t::T_Table)
            THROW_SIMPLE(L"Inheritance function did not return a table");
        }
        CATCH_THROW_SIMPLE_({}, L"Cannot load \'%S\'", pFilename->sValue);
        if(pStack[GETARG_A(*I)].eType == _value_t::T_Table)
        {
          _wraptable(pStack + GETARG_A(*I));
          pStack[GETARG_A(*I)].pValue->mapContents[RgdDictionary::_REF] = *pFilename;
          pStack[GETARG_A(*I)].pValue->pSourceFile = this;
        }
        break; }

      case OP_RETURN:    // A B   return R(A), ... ,R(A+B-2)	(see note)
        bContinue = false;
        break;

      default:
        THROW_SIMPLE_(L"Unsupported Lua Opcode: %i", static_cast<int>(GET_OPCODE(*I)));
      }
    }
  }
  catch(RainException *pE)
  {
    RainString sSource(L"Lua function");
    if(pFunction->source && getstr(pFunction->source))
      sSource = RainString(getstr(pFunction->source), pFunction->source->tsv.len);
    unsigned long iLine = 0;
    if(pFunction->lineinfo)
      iLine = pFunction->lineinfo[I - pFunction->code];

    throw new RainException(sSource, iLine, L"Error while executing Lua in custom VM", pE);
  }
}

LuaAttrib::_value_t::_value_t()
{
  eType = T_Nil;
}

LuaAttrib::_value_t::_value_t(const _value_t& oOther)
{
  eType = T_Nil;
  *this = oOther;
}

LuaAttrib::_value_t::~_value_t()
{
  free();
}

LuaAttrib::_value_t& LuaAttrib::_value_t::operator =(const _value_t& oOther)
{
  if(oOther.eType == T_Table)
    ++oOther.pValue->iReferenceCount;
  free();
  memcpy(this, &oOther, sizeof(_value_t));
  return *this;
}

void LuaAttrib::_value_t::free()
{
  if(eType == T_Table)
  {
    if(--pValue->iReferenceCount == 0)
      delete pValue;
  }
  eType = T_Nil;
}

LuaAttrib::_table_t::_table_t(LuaAttrib* pFile)
{
  pSourceFile = pFile;
  iReferenceCount = 1;
  pInheritFrom = 0;
  bTotallyUnchaged = false;
}

LuaAttrib::_table_t::~_table_t()
{
  if(pInheritFrom && --((_table_t*)pInheritFrom)->iReferenceCount == 0)
    delete pInheritFrom;
}

size_t LuaAttrib::_table_t::writeToBinary(IFile* pFile) const
{
  // Generate a list of keys to write
  std::map<unsigned long, const _table_t*> mapKeys;
  for(const _table_t* pTable = this; pTable; pTable = pTable->pInheritFrom)
  {
    for(std::map<unsigned long, _value_t>::const_iterator itr = pTable->mapContents.begin(); itr != pTable->mapContents.end(); ++itr)
    {
      if(mapKeys.count(itr->first) == 0)
        mapKeys[itr->first] = pTable;
    }
  }

  // Prepare a memory buffer for the header and leave space in the output to write it when done
  size_t iHeaderLength = mapKeys.size() * sizeof(long) * 3;
  MemoryWriteFile fTableHeader(iHeaderLength);
  pFile->writeArray(fTableHeader.getBuffer(), iHeaderLength);

  // Write keys
  size_t iDataLength = 0;
  for(std::map<unsigned long, const _table_t*>::iterator itr = mapKeys.begin(); itr != mapKeys.end(); ++itr)
  {
    const _value_t& oValue = itr->second->mapContents.find(itr->first)->second;
    // Set the data type code and alignment length for the type being written
    bool bConvertStringToWide = false;
    long iDataTypeCode;
    size_t iAlignLength;
    switch(oValue.eType)
    {
    case LuaAttrib::_value_t::T_Float:
      iDataTypeCode = BinaryAttribDataTypeCode<float>::code;
      iAlignLength = sizeof(float);
      break;

    case LuaAttrib::_value_t::T_String:
      iDataTypeCode = BinaryAttribDataTypeCode<char*>::code;
      // The the opportunity to check if the string needs to be converted to a unicode (wide) string
      // This is done for all strings of form "$[0-9]+"
      if(oValue.sValue[0] == '$' && oValue.iLength >= 2)
      {
        bConvertStringToWide = true;
        for(long i = 1; i < oValue.iLength; ++i)
        {
          if(oValue.sValue[i] < '0' || oValue.sValue[i] > '9')
          {
            bConvertStringToWide = false;
            break;
          }
        }
        if(bConvertStringToWide)
        {
          iDataTypeCode = BinaryAttribDataTypeCode<wchar_t*>::code;
          iAlignLength = sizeof(wchar_t);
        }
      }
      break;

    case LuaAttrib::_value_t::T_Boolean:
      iDataTypeCode = BinaryAttribDataTypeCode<bool>::code;
      iAlignLength = sizeof(char);
      break;

    case LuaAttrib::_value_t::T_Integer:
      iDataTypeCode = BinaryAttribDataTypeCode<long>::code;
      iAlignLength = sizeof(long);
      break;

    case LuaAttrib::_value_t::T_Table:
      iDataTypeCode = BinaryAttribDataTypeCode<IAttributeTable>::code;
      iAlignLength = sizeof(long);
      break;

    default:
      THROW_SIMPLE_(L"Cannot write type %li to binary file", static_cast<long>(oValue.eType));
    }

    // Write any alignment bytes
    if(iDataLength % iAlignLength)
    {
#define max2(a, b) ((a) > (b) ? (a) : (b))
#define max4(a, b, c, d) max2( max2((a), (b)), max2((c), (d)) )
      unsigned char cNullBytes[max4(sizeof(long), sizeof(float), sizeof(char), sizeof(wchar_t))] = {0};
#undef max4
#undef max2
      iAlignLength = iAlignLength - (iDataLength % iAlignLength);
      iDataLength += iAlignLength;
      pFile->writeArray(cNullBytes, iAlignLength);
    }

    fTableHeader.writeOne(itr->first);
    fTableHeader.writeOne(iDataTypeCode);
    fTableHeader.writeOne(static_cast<unsigned long>(iDataLength));

    switch(oValue.eType)
    {
    case LuaAttrib::_value_t::T_Float:
      pFile->writeOne(oValue.fValue);
      iDataLength += sizeof(float);
      break;

    case LuaAttrib::_value_t::T_String:
      if(bConvertStringToWide)
      {
        wchar_t c;
        for(long i = 0; i <= oValue.iLength; ++i)
        {
          c = oValue.sValue[i];
          pFile->writeOne(c);
        }
        iDataLength += oValue.iLength * sizeof(wchar_t);
      }
      else
      {
        pFile->writeArray(oValue.sValue, oValue.iLength + 1);
        iDataLength += oValue.iLength + 1;
      }
      break;

    case LuaAttrib::_value_t::T_Boolean: {
      pFile->writeOne(oValue.bValue ? BinaryAttribBooleanEncoding<true>::value : BinaryAttribBooleanEncoding<false>::value);
      iDataLength += sizeof BinaryAttribBooleanEncoding<true>::value;
      break; }

    case LuaAttrib::_value_t::T_Integer:
      pFile->writeOne(oValue.iValue);
      iDataLength += sizeof(long);
      break;

    case LuaAttrib::_value_t::T_Table:
      iDataLength += pSourceFile->getCache()->writeTableToBinaryFile(oValue.pValue, pFile, itr->second != this);
      break;
    };
  }

  // Write table header
  pFile->seek(-static_cast<seek_offset_t>(iDataLength + iHeaderLength), SR_Current);
  pFile->writeArray(fTableHeader.getBuffer(), iHeaderLength);
  pFile->seek(-static_cast<seek_offset_t>(iDataLength), SR_Current);

  return iDataLength + iHeaderLength;
}
