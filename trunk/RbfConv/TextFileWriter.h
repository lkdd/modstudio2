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
#define RAINMAN2_NO_WX
#define RAINMAN2_NO_LUA
#include <Rainman2.h>

class RbfTxtWriter
{
public:
  RbfTxtWriter(IFile *pOutputFile);
  ~RbfTxtWriter();

  void writeTable(IAttributeTable *pTable);
  void writeValue(IAttributeValue *pValue);

  //! Write the current indentation string
  void writeLinePrefix();

  void setUCS(UcsFile *pUcsFile, long iUcsReferenceThreshold = 100);

  //! Configure indentation behaviour
  /*!
    \param iIndentSize The number of characters to use per indentation level
    \param cIndentChar The character to use when indenting
    \param cLevelMarker The character to use when indenting to mark the indentation level

    This should be called before any writing occurs, as otherwise the changes will not
    take effect fully or consistently.

    If given the parameters (3, 'a', 'B'), indentation would be:
    // indentation level 0
    Baa // indentation level 1
    BaaBaa // indentation level 2
    BaaBaaBaa // indentation level 3
    BaaBaaBaaBaa // indentation level 4
    As can be seen, indentation always uses iIndentSize characters, the first of which is
    cLevelMarker, the remainder being cIndentChar.
  */
  void setIndentProperties(int iIndentSize = 2, char cIndentChar = ' ', char cLevelMarker = '|');

  //! Increase the indent level
  void indent();

  //! Decrease the indent level
  /*!
    Will not decrease the indent level to below 0.
  */
  void unindent();

protected:
  BufferingOutputStream<char> m_oOutput; //!< Buffer for writing output
  char    *m_sIndentArray;
  UcsFile *m_pUcsFile;
  long     m_iUcsReferenceThreshold;
  int      m_iIndentLevel, //!< The number of characters of indentation currently being used
           m_iSpacesPerLevel, //!< The number of characters to use per indentation level
           m_iIndentArrayLength;
  char     m_sBuffer[32]; //!< Buffer to use for sprintf and similar calls
  char     m_cIndentChar; //!< The major indentation character
  char     m_cIndentLevelChar; //!< The indentation level character
};
