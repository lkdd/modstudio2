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
#include "inifile.h"
#include "exception.h"
#include "buffering_streams.h"

template <class T, T* T::* pPrev, T* T::* pNext>
struct DLinkList
{
  typedef DLinkList<T, pNext, pPrev> reversed;

  static T* erase(T* p)
  {
    if(p->*pPrev)
      p->*pPrev->*pNext = p->*pNext;
    if(p->*pNext)
      p->*pNext->*pPrev = p->*pPrev;
    p->*pPrev = 0;
    p->*pNext = 0;
    return p;
  }

  static T* append(T* existing, T* p)
  {
    p->*pPrev = existing;
    p->*pNext = 0;
    if(existing)
    {
      if(existing->*pNext)
      {
        p->*pNext = existing->*pNext;
        p->*pNext->*pPrev = p;
      }
      existing->*pNext = p;
    }
    return p;
  }

  static T* append_end(T* list, T* p)
  {
    return append(last(list), p);
  }

  static T* prepend(T* existing, T* p)
  {
    return reversed::append(existing, p);
  }

  static T* prepend_start(T* existing, T* p)
  {
    return reversed::append_end(existing, p);
  }

  static T* last(T* list)
  {
    if(list)
    {
      for(T* next; (next = list->*pNext); list = next)
      {
      }
    }
    return list;
  }

  static T* first(T* list)
  {
    return reversed::first(list);
  }

  template <class A>
  static T* find_forward(T* list, const A& what)
  {
    for(; list; = list = list->*pNext)
    {
      if(*list == what)
        return list;
    }
    return 0;
  }

  template <class A>
  static T* find_backward(T* list, const A& what)
  {
    return reversed::find_forward<A>(list, what);
  }
};

#define DLinkList_Section     DLinkList<IniFile::Section       , &IniFile::Section       ::m_pPrev    , &IniFile::Section       ::m_pNext    >
#define DLinkList_SectionTwin DLinkList<IniFile::Section       , &IniFile::Section       ::m_pPrevTwin, &IniFile::Section       ::m_pNextTwin>
#define DLinkList_Value       DLinkList<IniFile::Section::Value, &IniFile::Section::Value::m_pPrev    , &IniFile::Section::Value::m_pNext    >
#define DLinkList_ValueTwin   DLinkList<IniFile::Section::Value, &IniFile::Section::Value::m_pPrevTwin, &IniFile::Section::Value::m_pNextTwin>

IniFile::IniFile()
{
  m_pFirstSection = 0;
  m_pLastSection = 0;
  m_cCommentCharacter = ';';
  m_cAssignmentCharacter = '=';
  m_cSectionStartCharacter = '[';
  m_cSectionEndCharacter = ']';
  m_oEmptySection.m_pFile = this;
}

IniFile::~IniFile()
{
  clear();
}

IniFile::iterator::iterator() throw()
{
  m_pSection = 0;
  m_pPrev = 0;
  m_pNext = 0;
}

IniFile::Section::iterator::iterator() throw()
{
  m_pValue = 0;
  m_pPrev = 0;
  m_pNext = 0;
}

IniFile::iterator::iterator(const IniFile::iterator& other) throw()
{
  m_pSection = other.m_pSection;
  m_pPrev = other.m_pPrev;
  m_pNext = other.m_pNext;
}

IniFile::Section::iterator::iterator(const IniFile::Section::iterator& other) throw()
{
  m_pValue = other.m_pValue;
  m_pPrev = other.m_pPrev;
  m_pNext = other.m_pNext;
}

IniFile::iterator::iterator(IniFile::iterator::value_type* pSection, IniFile::iterator::value_type* IniFile::iterator::value_type::* pPrev, IniFile::iterator::value_type* IniFile::iterator::value_type::* pNext) throw()
{
  m_pSection = pSection;
  m_pPrev = pPrev;
  m_pNext = pNext;
}

IniFile::Section::iterator::iterator(IniFile::Section::iterator::value_type* pValue, IniFile::Section::iterator::value_type* IniFile::Section::iterator::value_type::* pPrev, IniFile::Section::iterator::value_type* IniFile::Section::iterator::value_type::* pNext) throw()
{
  m_pValue = pValue;
  m_pPrev = pPrev;
  m_pNext = pNext;
}

IniFile::iterator& IniFile::iterator::operator =(const IniFile::iterator& other) throw()
{
  m_pSection = other.m_pSection;
  m_pPrev = other.m_pPrev;
  m_pNext = other.m_pNext;
  return *this;
}

IniFile::Section::iterator& IniFile::Section::iterator::operator =(const IniFile::Section::iterator& other) throw()
{
  m_pValue = other.m_pValue;
  m_pPrev = other.m_pPrev;
  m_pNext = other.m_pNext;
  return *this;
}

IniFile::iterator& IniFile::iterator::operator ++() throw()
{
  m_pSection = m_pSection->*m_pNext;
  return *this;
}

IniFile::Section::iterator& IniFile::Section::iterator::operator ++() throw()
{
  m_pValue = m_pValue->*m_pNext;
  return *this;
}

IniFile::iterator& IniFile::iterator::operator --() throw()
{
  m_pSection = m_pSection->*m_pPrev;
  return *this;
}

IniFile::Section::iterator& IniFile::Section::iterator::operator --() throw()
{
  m_pValue = m_pValue->*m_pPrev;
  return *this;
}

bool IniFile::iterator::operator ==(const IniFile::iterator& other) const throw()
{
  return m_pSection == other.m_pSection;
}

bool IniFile::Section::iterator::operator ==(const IniFile::Section::iterator& other) const throw()
{
  return m_pValue == other.m_pValue;
}

bool IniFile::iterator::operator !=(const IniFile::iterator& other) const throw()
{
  return m_pSection != other.m_pSection;
}

bool IniFile::Section::iterator::operator !=(const IniFile::Section::iterator& other) const throw()
{
  return m_pValue != other.m_pValue;
}

IniFile::iterator::reference IniFile::iterator::operator *() throw()
{
  return *m_pSection;
}

IniFile::Section::iterator::reference IniFile::Section::iterator::operator *() throw()
{
  return *m_pValue;
}

IniFile::iterator::pointer IniFile::iterator::operator ->() throw()
{
  return m_pSection;
}

IniFile::Section::iterator::pointer IniFile::Section::iterator::operator ->() throw()
{
  return m_pValue;
}

IniFile::iterator IniFile::begin() throw()
{
  return iterator(m_pFirstSection, &Section::m_pPrev, &Section::m_pNext);
}

IniFile::Section::iterator IniFile::Section::begin() throw()
{
  return iterator(m_pFirstValue, &Value::m_pPrev, &Value::m_pNext);
}

IniFile::iterator IniFile::begin(RainString sSectionName) throw()
{
  map_type::iterator itr = m_mapSections.find(sSectionName);
  if(itr == m_mapSections.end())
    return end();
  else
    return iterator(itr->second, &Section::m_pPrevTwin, &Section::m_pNextTwin);
}

IniFile::Section::iterator IniFile::Section::begin(RainString sValueName) throw()
{
  map_type::iterator itr = m_mapValues.find(sValueName);
  if(itr == m_mapValues.end())
    return end();
  else
    return iterator(itr->second, &Value::m_pPrevTwin, &Value::m_pNextTwin);
}

IniFile::iterator IniFile::end() throw()
{
  return iterator(0, 0, 0);
}

IniFile::Section::iterator IniFile::Section::end() throw()
{
  return iterator(0, 0, 0);
}

void IniFile::setSpecialCharacters(RainChar cCommentMarker, RainChar cNameValueDelimiter, RainChar cSectionStarter, RainChar cSectionFinisher) throw()
{
  m_cCommentCharacter = cCommentMarker;
  m_cAssignmentCharacter = cNameValueDelimiter;
  m_cSectionStartCharacter = cSectionStarter;
  m_cSectionEndCharacter = cSectionFinisher;
}

void IniFile::clear() throw()
{
  m_sComment = "";
  m_mapSections.clear();
  while(m_pFirstSection)
    delete m_pFirstSection;
}

IniFile::Section& IniFile::operator[](const RainString& sSection)
{
  map_type::iterator itr = m_mapSections.find(sSection);
  if(itr == m_mapSections.end())
  {
    return appendSection(sSection);
  }
  else
  {
    return *itr->second;
  }
}

const IniFile::Section& IniFile::operator[](const RainString& sSection) const
{
  map_type::const_iterator itr = m_mapSections.find(sSection);
  if(itr == m_mapSections.end())
  {
    return m_oEmptySection;
  }
  else
  {
    return *itr->second;
  }
}

IniFile::Section::Section()
{
  m_pFirstValue = 0;
  m_pLastValue = 0;
  m_pPrev = 0;
  m_pNext = 0;
  m_pPrevTwin = 0;
  m_pNextTwin = 0;
  m_pFile = 0;
  m_oEmptyValue.m_pSection = this;
}

bool IniFile::Section::empty() const throw()
{
  return m_pFirstValue == 0;
}

void IniFile::Section::clear() throw()
{
  while(m_pFirstValue)
    delete m_pFirstValue;
}

IniFile* IniFile::Section::getFile() const throw()
{
  return m_pFile;
}

IniFile::Section::~Section()
{
  clear();
  if(m_pFile)
  {
    if(m_pFile->m_pFirstSection == this)
      m_pFile->m_pFirstSection = m_pNext;
    if(m_pFile->m_pLastSection == this)
      m_pFile->m_pLastSection = m_pPrev;
    if(m_pFile->m_mapSections[m_sName] == this)
    {
      if(m_pNextTwin)
        m_pFile->m_mapSections[m_sName] = m_pNextTwin;
      else
        m_pFile->m_mapSections.erase(m_sName);
    }
  }
  DLinkList_Section::erase(this);
  DLinkList_SectionTwin::erase(this);
}

IniFile::Section::Value& IniFile::Section::operator[](const RainString& sKey)
{
  map_type::iterator itr = m_mapValues.find(sKey);
  if(itr == m_mapValues.end())
  {
    return appendValue(sKey, RainString());
  }
  else
  {
    return *itr->second;
  }
}

const IniFile::Section::Value& IniFile::Section::operator[](const RainString& sKey) const
{
  map_type::const_iterator itr = m_mapValues.find(sKey);
  if(itr == m_mapValues.end())
  {
    return m_oEmptyValue;
  }
  else
  {
    return *itr->second;
  }
}

bool IniFile::Section::hasValue(const RainString& sKey, bool bReturnValueOnBlank) const
{
  map_type::const_iterator itr = m_mapValues.find(sKey);
  if(itr == m_mapValues.end())
  {
    return false;
  }
  else if(bReturnValueOnBlank)
  {
    return true;
  }
  else
  {
    return !itr->second->value().isEmpty();
  }
}

IniFile::Section::Value::Value()
{
  m_pPrev = 0;
  m_pNext = 0;
  m_pPrevTwin = 0;
  m_pNextTwin = 0;
  m_pSection = 0;
}

IniFile::Section::Value::~Value()
{
  if(m_pSection)
  {
    if(m_pSection->m_pFirstValue == this)
      m_pSection->m_pFirstValue = m_pNext;
    if(m_pSection->m_pLastValue == this)
      m_pSection->m_pLastValue = m_pPrev;
    if(m_pSection->m_mapValues[m_sKey] == this)
    {
      if(m_pNextTwin)
        m_pSection->m_mapValues[m_sKey] = m_pNextTwin;
      else
        m_pSection->m_mapValues.erase(m_sKey);
    }
  }
  DLinkList_Value::erase(this);
  DLinkList_ValueTwin::erase(this);
}

static bool iswhite(RainChar c)
{
  return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

void IniFile::load(IFile* pFile) throw(...)
{
  clear();

  RainString *pLastComment = &m_sComment;
  Section *pLastSection = 0;
  Section::Value *pLastValue = 0;

  BufferingInputTextStream<char> oFile(pFile);
  do
  {
    RainString sLine = oFile.readLine();
    RainString sComment;
    size_t iCommentBegin = sLine.indexOf(m_cCommentCharacter, 0, sLine.length());
    for(; iCommentBegin > 0 && iswhite(sLine.getCharacters()[iCommentBegin - 1]); --iCommentBegin)
    {
    }
    if(iCommentBegin == 0)
    {
      pLastComment->append(sLine);
    }
    else
    {
      sComment = sLine.mid(iCommentBegin, sLine.length() - iCommentBegin);
      size_t iPrefixLength = 0;
      for(; iswhite(sLine.getCharacters()[iPrefixLength]); ++iPrefixLength)
      {
      }
      if(iPrefixLength != 0)
      {
        pLastComment->append(sLine.getCharacters(), iPrefixLength);
      }
      if(iPrefixLength != sLine.length())
      {
        sLine = sLine.mid(iPrefixLength, iCommentBegin - iPrefixLength);
        if(sLine.getCharacters()[0] == m_cSectionStartCharacter && sLine.getCharacters()[sLine.length() - 1] == m_cSectionEndCharacter)
        {
          pLastSection = &appendSection(sLine.mid(1, sLine.length() - 2));
          pLastComment = &pLastSection->m_sComment;
        }
        else
        {
          if(pLastSection == 0)
            pLastSection = &appendSection(RainString());
          if(sLine.indexOf(m_cAssignmentCharacter) == static_cast<size_t>(-1))
          {
            THROW_SIMPLE_(L"Line is not a section nor a value \'%s\'", sLine.getCharacters());
          }
          pLastValue = &pLastSection->appendValue(sLine.beforeFirst(m_cAssignmentCharacter).trimWhitespace(), sLine.afterFirst(m_cAssignmentCharacter).trimWhitespace());
          pLastComment = &pLastValue->m_sComment;
        }
      }
      pLastComment->append(sComment);
    }

  } while(!oFile.isEOF());
}

void IniFile::load(const RainString& sFile) throw(...)
{
  IFile* pFile = 0;
  try
  {
    pFile = RainOpenFile(sFile, FM_Read);
    load(pFile);
  }
  CATCH_THROW_SIMPLE_(delete pFile, L"Cannot load INI file \'%s\'", sFile.getCharacters());
  delete pFile;
}

static char RainCharToChar(RainChar c)
{
  return c & ~0xFF ? '?' : static_cast<char>(c);
}

void IniFile::save(IFile* pFile) throw(...)
{
  BufferingOutputStream<char> oOutput(pFile);
  oOutput.writeConverting(m_sComment, RainCharToChar);
  for(iterator itr_s = begin(), end_s = end(); itr_s != end_s; ++itr_s)
  {
    oOutput.write(RainCharToChar(m_cSectionStartCharacter));
    oOutput.writeConverting(itr_s->name(), RainCharToChar);
    oOutput.write(RainCharToChar(m_cSectionEndCharacter));
    if(itr_s->comment().isEmpty())
      oOutput.writeConverting(L"\r\n", 2, RainCharToChar);
    else
      oOutput.writeConverting(itr_s->comment(), RainCharToChar);

    for(Section::iterator itr_v = itr_s->begin(), end_v = itr_s->end(); itr_v != end_v; ++itr_v)
    {
      oOutput.writeConverting(itr_v->key(), RainCharToChar);
      oOutput.write(' ');
      oOutput.write(RainCharToChar(m_cAssignmentCharacter));
      oOutput.write(' ');
      oOutput.writeConverting(itr_v->value(), RainCharToChar);
      if(itr_v->comment().isEmpty())
        oOutput.writeConverting(L"\r\n", 2, RainCharToChar);
      else
        oOutput.writeConverting(itr_v->comment(), RainCharToChar);
    }
  }
}

void IniFile::save(const RainString &sFile) throw(...)
{
  IFile* pFile = 0;
  try
  {
    pFile = RainOpenFile(sFile, FM_Write);
    save(pFile);
  }
  CATCH_THROW_SIMPLE_(delete pFile, L"Cannot save INI file \'%s\'", sFile.getCharacters());
  delete pFile;
}

IniFile::Section& IniFile::appendSection(const RainString& sSection)
{
  Section *pNewSection = new Section;
  if(m_pFirstSection == 0)
    m_pFirstSection = pNewSection;
  DLinkList_Section::append(m_pLastSection, pNewSection);
  map_type::iterator itr = m_mapSections.find(sSection);
  if(itr == m_mapSections.end())
  {
    m_mapSections[sSection] = pNewSection;
  }
  else
  {
    DLinkList_SectionTwin::append_end(itr->second, pNewSection);
  }
  m_pLastSection = pNewSection;
  pNewSection->m_sName = sSection;
  pNewSection->m_pFile = this;
  return *pNewSection;
}

IniFile::Section::Value& IniFile::Section::appendValue(const RainString& sKey, const RainString& sValue)
{
  Value *pNewValue = new Value;
  if(m_pFirstValue == 0)
    m_pFirstValue = pNewValue;
  DLinkList_Value::append(m_pLastValue, pNewValue);
  map_type::iterator itr = m_mapValues.find(sKey);
  if(itr == m_mapValues.end())
  {
    m_mapValues[sKey] = pNewValue;
  }
  else
  {
    DLinkList_ValueTwin::append_end(itr->second, pNewValue);
  }
  m_pLastValue = pNewValue;
  pNewValue->m_sKey = sKey;
  pNewValue->m_sValue = sValue;
  pNewValue->m_pSection = this;
  return *pNewValue;
}

IniFile::Section::operator const RainString&() const
{
  return m_sName;
}

const RainString& IniFile::Section::name() const
{
  return m_sName;
}

const RainString& IniFile::comment() const
{
  return m_sComment;
}

const RainString& IniFile::Section::comment() const
{
  return m_sComment;
}

const RainString& IniFile::Section::Value::comment() const
{
  return m_sComment;
}

bool IniFile::Section::operator ==(const RainString& sName) const
{
  return m_sName == sName;
}

bool IniFile::Section::Value::operator ==(const RainString& sValue) const
{
  return m_sValue == sValue;
}

IniFile::Section::Value& IniFile::Section::Value::operator =(const RainString& sValue)
{
  m_sValue = sValue;
  return *this;
}

IniFile::Section::Value::operator RainString&()
{
  return m_sValue;
}

IniFile::Section::Value::operator const RainString&() const
{
  return m_sValue;
}

RainString& IniFile::Section::Value::value()
{
  return m_sValue;
}

const RainString& IniFile::Section::Value::value() const
{
  return m_sValue;
}

IniFile::Section::Value& IniFile::Section::Value::setValue(const RainString& sValue)
{
  value() = sValue;
  return *this;
}

const RainString& IniFile::Section::Value::key() const
{
  return m_sKey;
}
