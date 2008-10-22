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
#include <limits.h>

class IAttributeTable;

//! All the possible types of values found in attribute files
enum eAttributeValueTypes
{
  VT_String,  //!< String data (represented with a RainString, but may be stored as ASCII)
  VT_Boolean, //!< Boolean data (true/false)
  VT_Table,   //!< A table containing further values
  VT_Float,   //!< Single-precision floating point data

  //! Should never be found in attribute files, but provided as a convenience
  VT_Unknown,
};

//! All the possible icons which represent inheritance in an attribute file
/*!
  Only some types of attribute files support inheritance. These icons are designed
  to make editing those easier by making it clear what values are inherited and
  which aren't.
*/
enum eAttributeValueIcons
{
  //! The value/table is *exactly* the same as in the parent file
  VI_SameAsParent,

  //! The value is different to the parent file (not applicable to tables)
  VI_DifferentToParent,

  //! The table inherits from a parent, but has some values which are not the same as parent
  /*!
    If *all* the values within a table are the same as the parent, then the table itself is
    the same as parent. Otherwise, this icon is used if there is a parent, and VI_NewSinceParent
    is used if not.
  */
  VI_Table,

  //! The value/table is not in the parent file
  VI_NewSinceParent,
};

//! Interface for a key/value pair from an attribute file
class RAINMAN2_API IAttributeValue
{
public:
  virtual ~IAttributeValue(); //!< Virtual destructor

  //! Get the name (key) of the value
  /*!
    The returned value is a hash. Use one of the RgdDictionary::hashTo methods
    to convert it to text.
  */
  virtual unsigned long        getName()      const throw() = 0;

  virtual eAttributeValueTypes getType()      const throw() = 0;
  virtual eAttributeValueIcons getIcon()      const throw() = 0;
  virtual RainString           getFileSetIn() const throw() = 0;

  virtual const IAttributeValue* getParent() const throw(...) = 0;
  virtual const IAttributeValue* getParentNoThrow() const throw() = 0;

  inline bool isString () const throw() {return getType() == VT_String ;}
  inline bool isBoolean() const throw() {return getType() == VT_Boolean;}
  inline bool isTable  () const throw() {return getType() == VT_Table  ;}
  inline bool isFloat  () const throw() {return getType() == VT_Float  ;}

  virtual RainString       getValueString () const throw(...) = 0;
  virtual bool             getValueBoolean() const throw(...) = 0;
  virtual IAttributeTable* getValueTable  () const throw(...) = 0;
  virtual float            getValueFloat  () const throw(...) = 0;

  virtual void setName(unsigned long iName          ) throw(...) = 0;
  virtual void setType(eAttributeValueTypes eNewType) throw(...) = 0;

  virtual void setValueString (const RainString& sValue) throw(...) = 0;
  virtual void setValueBoolean(bool bValue             ) throw(...) = 0;
  virtual void setValueFloat  (float fValue            ) throw(...) = 0;
};

class RAINMAN2_API IAttributeTable
{
public:
  virtual ~IAttributeTable();

  static const unsigned long NO_INDEX = static_cast<unsigned long>(-1);

  virtual unsigned long getChildCount() throw() = 0;
  virtual IAttributeValue* getChild(unsigned long iIndex) throw(...) = 0;
  virtual void addChild(unsigned long iName) throw(...) = 0;
  virtual unsigned long findChildIndex(unsigned long iName) throw() = 0;
  virtual void deleteChild(unsigned long iIndex, bool bRevertIsSufficient) throw(...) = 0;
};
