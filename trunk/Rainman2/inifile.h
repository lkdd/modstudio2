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
#include "file.h"
#include <map>

//! Standard parser and manipulator for INI-like files
/*!
  .module and pipeline.ini files are all INI files, so it makes
  sense to have a good common INI class for reading and writing
  these files.
  Specific INI features (see http://en.wikipedia.org/wiki/INI_file#Varying_features):
    * Blank lines (lines with only whitespace on them) are permitted
    * Whitespace is removed from the outside of parameter names and values
    * Quoted values are not supported; quotes are interpreted as part of the value
    * Comments begin with ';' and can be anywhere on a line (although this character
      can be changed with setSpecialCharacters())
    * Backslashes and escapes are not supported
    * Duplicate names for sections and parameters are permitted, with no merging or
      discarding of previous things. For example, begin(L"name") can be used to
      iterate over all sections/parameters with that name.
    * '=' is the name/value delimiter, although this can be changed to any other
      character with setSpecialCharacters()
    * There is no hierarchy of sections
    * '[' and ']' mark sections, although these can be changed to other characters
      with setSpecialCharacters()
*/
class RAINMAN2_API IniFile
{
public:
  class RAINMAN2_API Section
  {
  public:
    class RAINMAN2_API Value
    {
    public:
      Value();
      ~Value();

      bool operator ==(const RainString& sValue) const;
      Value& operator =(const RainString& sValue);

      operator RainString&();
      operator const RainString&() const;
      RainString& value();
      const RainString& value() const;
      Value& setValue(const RainString& sValue);
      const RainString& key() const;
      Value& setKey(const RainString& sKey);

    protected:
      friend class IniFile;
      friend class Section;

      RainString m_sKey,
                 m_sValue,
                 m_sComment;
      Value     *m_pPrev,
                *m_pNext,
                *m_pPrevTwin,
                *m_pNextTwin;
      Section   *m_pSection;
    };

    Section();
    ~Section();

    bool operator ==(const RainString& sName) const;

    operator const RainString&() const;
    const RainString& name() const;
    Section& setName(const RainString& sName);

    bool empty() const;
    void clear() throw();

    IniFile* getFile() const throw();

    typedef RainString key_type;
    typedef Value value_type;
    typedef std::map<key_type, value_type*> map_type;

    Value& operator[](const RainString& sKey);
    const Value& operator[](const RainString& sKey) const;

    Value& appendValue(const RainString& sKey, const RainString& sValue);

  protected:
    friend class Value;
    friend class IniFile;

    map_type   m_mapValues;
    Value      m_oEmptyValue;
    RainString m_sName,
               m_sComment;
    Value     *m_pFirstValue,
              *m_pLastValue;
    Section   *m_pPrev,
              *m_pNext,
              *m_pPrevTwin,
              *m_pNextTwin;
    IniFile   *m_pFile;
  };

  class RAINMAN2_API iterator
  {
  public:
    //typedef std::input_iterator_tag iterator_category;
    typedef IniFile::Section value_type;
    typedef ptrdiff_t difference_type;
    typedef value_type* pointer;
    typedef value_type& reference;

    iterator() throw();
    iterator(const iterator&) throw();
    iterator(value_type* pSection, value_type* value_type::* pPrev, value_type* value_type::* pNext) throw();

    iterator& operator =  (const iterator&) throw();
    iterator& operator ++ ()                throw();
    iterator& operator -- ()                throw();
    bool      operator == (const iterator&) const throw();
    bool      operator != (const iterator&) const throw();
    reference operator *  ()                throw();
    pointer   operator -> ()                throw();

  protected:
    value_type* m_pSection;
    value_type* value_type::* m_pPrev;
    value_type* value_type::* m_pNext;
  };

  IniFile();
  ~IniFile();

  //! Change the characters used to mark comments, sections and names/values
  /*!
    \param cCommentMarker The character which turns the rest of the line into a comment (usually ';' or '#')
    \param cNameValueDelimiter The character placed between a parameter name and value  (usually '=' or ':')
    \param cSectionStarter The character placed before a new section name (usually '[')
    \param cSectionFinisher The character placed after a new section name (usually ']')
  */
  void setSpecialCharacters(RainChar cCommentMarker = ';', RainChar cNameValueDelimiter = '=', RainChar cSectionStarter = '[', RainChar cSectionFinisher = ']') throw();

  //! Load an INI file from a generic file
  /*!
    Any data currently in the file is erased, and replaced with the data from the given file.
    The file is interpreted as ASCII delimited with '\n' characters for newlines.
  */
  void load(IFile* pFile) throw(...);

  //! Load an INI file from a disk file
  /*!
    
  */
  void load(RainString sFile) throw(...);

  iterator begin() throw();
  iterator begin(RainString sSectionName) throw();
  iterator end() throw();

  void clear() throw();

  typedef RainString key_type;
  typedef Section value_type;
  typedef std::map<key_type, value_type*> map_type;

  Section& operator[](const RainString& sSection);
  const Section& operator[](const RainString& sSection) const;

  Section& appendSection(const RainString& sSection);

protected:
  friend class Section;
  friend class Section::Value;

  map_type   m_mapSections;
  Section    m_oEmptySection;
  RainString m_sComment;
  Section   *m_pFirstSection,
            *m_pLastSection;
  RainChar   m_cCommentCharacter,
             m_cAssignmentCharacter,
             m_cSectionStartCharacter,
             m_cSectionEndCharacter;
};
