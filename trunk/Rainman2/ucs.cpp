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
#include "ucs.h"
#include "exception.h"
#include "buffering_streams.h"

UcsFile::UcsFile()
{
}

UcsFile::~UcsFile()
{
}

void UcsFile::loadFromFile(const RainString& sFilename) throw(...)
{
  IFile *pFile = 0;
  try
  {
    pFile = RainOpenFile(sFilename, FM_Read);
    load(pFile);
  }
  CATCH_THROW_SIMPLE_(delete pFile, L"Unable to load UCS file\"%s\"", sFilename.getCharacters());
  delete pFile;
}

void UcsFile::load(IFile *pFile) throw(...)
{
  m_mapStrings.clear();

  unsigned short iByteOrder;
  pFile->readOne(iByteOrder);
  if(iByteOrder != 0xFEFF)
    THROW_SIMPLE(L"File is not a UCS file, or is in big-endian format");

  BufferingInputTextStream<RainChar, 4096> oReader(pFile);
  while(!oReader.isEOF())
  {
    const RainString sLine(oReader.readLine());
    const RainChar *pChars = sLine.getCharacters();
    unsigned long iKey = 0;
    while('0' <= *pChars && *pChars <= '9')
    {
      iKey = iKey * 10 + (*pChars - '0');
      ++pChars;
    }
    if(*pChars == '\t')
      ++pChars;
    const RainChar *pEnd = pChars;
    while(*pEnd != '\n' && *pEnd != '\r' && *pEnd != '\0')
      ++pEnd;

    m_mapStrings.insert(std::make_pair(iKey, RainString(pChars, pEnd - pChars)));
  }
  m_mapStrings.rehash(m_mapStrings.size() / 4);
}

const RainString* UcsFile::operator[](unsigned long iKey) const
{
  map_type::const_iterator itr = m_mapStrings.find(iKey);
  if(itr == m_mapStrings.end())
    return NULL;
  else
    return &itr->second;
}
