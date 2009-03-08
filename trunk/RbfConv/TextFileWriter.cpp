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

#include "TextFileWriter.h"

RbfTxtWriter::RbfTxtWriter(IFile *p)
  : m_oOutput(p)
  , m_sIndentArray(0)
  , m_pUcsFile(0)
  , m_iUcsReferenceThreshold(100)
  , m_iIndentLevel(0)
  , m_iSpacesPerLevel(2)
  , m_iIndentArrayLength(0)
  , m_cIndentChar(' ')
  , m_cIndentLevelChar('|')
{
  m_oOutput.write("-- Made by Corsix\'s Crude RBF Convertor\r\n");
}

RbfTxtWriter::~RbfTxtWriter()
{
  delete[] m_sIndentArray;
}

void RbfTxtWriter::setIndentProperties(int iIndentSize, char cIndentChar, char cLevelMarker)
{
  m_iSpacesPerLevel = iIndentSize;
  m_cIndentChar = cIndentChar;
  m_cIndentLevelChar = cLevelMarker;
}

void RbfTxtWriter::setUCS(UcsFile *pUcsFile, long iUcsReferenceThreshold)
{
  m_pUcsFile = pUcsFile;
  m_iUcsReferenceThreshold = iUcsReferenceThreshold;
}

void RbfTxtWriter::writeTable(IAttributeTable *pTable)
{
  unsigned long iCount = pTable->getChildCount();
  m_oOutput.write("{\r\n", 3);
  indent();
  for(unsigned long i = 0; i < iCount; ++i)
  {
    std::auto_ptr<IAttributeValue> pValue(pTable->getChild(i));
    writeValue(&*pValue);
  }
  unindent();
  writeLinePrefix();
  m_oOutput.write("};\r\n", 4);
}

char static UcsCommentConvertor(wchar_t x)
{
  if(x > 0xFF)
    return '?';
  if(x == '\r' || x == '\n')
    return ' ';
  return (char)x;
}

void RbfTxtWriter::writeValue(IAttributeValue *pValue)
{
  size_t iNameLength;
  const char* sName = RgdDictionary::getSingleton()->hashToAscii(pValue->getName(), &iNameLength);
  writeLinePrefix();
  m_oOutput.write(sName, iNameLength);
  m_oOutput.write(": ", 2);
  switch(pValue->getType())
  {
  case VT_Boolean:
    if(pValue->getValueBoolean())
      m_oOutput.write("true;\r\n", 7);
    else
      m_oOutput.write("false;\r\n", 8);
    break;
  case VT_Table:
    {
      std::auto_ptr<IAttributeTable> pTable(pValue->getValueTable());
      writeTable(&*pTable);
      break;
    }
  case VT_Integer:
    if(m_pUcsFile)
    {
      long iValue = pValue->getValueInteger();
      const RainString *pString = (*m_pUcsFile)[iValue];
      m_oOutput.write(m_sBuffer, sprintf_s(m_sBuffer, 32, "%li;", iValue));
      if(pString && iValue >= m_iUcsReferenceThreshold)
      {
        m_oOutput.write(" -- ", 4);
        m_oOutput.writeConverting(pString->getCharacters(), pString->length(), UcsCommentConvertor);
      }
      m_oOutput.write("\r\n", 2);
    }
    else
      m_oOutput.write(m_sBuffer, sprintf_s(m_sBuffer, 32, "%li;\r\n", pValue->getValueInteger()));
    break;
  case VT_Float:
    m_oOutput.write(m_sBuffer, sprintf_s(m_sBuffer, 32, "%.7gf;\r\n", pValue->getValueFloat()));
    break;
  case VT_String:
    m_oOutput.write("\"", 1);
    {
      size_t iLength;
      const char *sBase = pValue->getValueStringRaw(&iLength);
      for(size_t i = 0; i < iLength; ++i)
      {
        if(sBase[i] == '\"')
        {
          m_oOutput.write(sBase, i);
          m_oOutput.write("\"", 1);
          sBase += i;
          iLength -= i;
          i = 0;
        }
      }
      m_oOutput.write(sBase, iLength);
    }
    m_oOutput.write("\";\r\n", 4);
    break;
  default:
    m_oOutput.write("?;\r\n", 4);
  };
}

void RbfTxtWriter::writeLinePrefix()
{
  if(m_iIndentArrayLength < m_iIndentLevel)
  {
    delete[] m_sIndentArray;
    m_iIndentArrayLength = (m_iIndentArrayLength * 2) + ((m_iIndentLevel - m_iIndentArrayLength) * 4);
    m_sIndentArray = new char[m_iIndentArrayLength];
    memset(m_sIndentArray, m_cIndentChar, m_iIndentArrayLength);
    for(int i = 0; i < m_iIndentArrayLength; i += 2)
      m_sIndentArray[i] = m_cIndentLevelChar;
  }
  m_oOutput.write(m_sIndentArray, m_iIndentLevel);
}

void RbfTxtWriter::indent()
{
  m_iIndentLevel += m_iSpacesPerLevel;
  if(m_iIndentLevel < 0) // integer wraparound
    m_iIndentLevel -= m_iSpacesPerLevel;
}

void RbfTxtWriter::unindent()
{
  m_iIndentLevel -= m_iSpacesPerLevel;
  if(m_iIndentLevel < 0)
    m_iIndentLevel = 0;
}