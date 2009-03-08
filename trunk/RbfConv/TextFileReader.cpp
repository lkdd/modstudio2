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

#include "TextFileReader.h"
#include <math.h>

TxtReader::TxtReader(IFile *pInputFile, RbfWriter *pWriter)
  : m_pFile(pInputFile)
  , m_pWriter(pWriter)
  , m_iBufferLength(1024)
  , m_iBytesOfDataInBuffer(0)
{
  m_sBuffer = m_sBufferBase = (char*)malloc(m_iBufferLength);
  m_iBufferRemaining = m_iBufferLength;
}

TxtReader::~TxtReader()
{
  free(m_sBufferBase);
}

bool TxtReader::isKeyChar(char c)
{
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '$' || c == '_';
}

static int intexp(int base, int power)
{
  switch(power)
  {
  case 0:
    return 1;
  case 1:
    return base;
  }
  int halfpower = power /2;
  int value = intexp(base, halfpower);
  value *= value;
  if(power - halfpower != halfpower)
    value *= base;
  return value;
}

void TxtReader::readNumber()
{
  bool bHasFractionalPart = false;
  bool bHasExponent = false;
  bool bIsFloat = false;
  bool bIsNegative = false;
  int iLength = 0;
  float fFraction;
  long iExponent;

  if(isChar(0) && getChar(0) == '-')
  {
    bIsNegative = true;
    consume(1);
  }

  while(isChar(iLength) && '0' <= getChar(iLength) && getChar(iLength) <= '9')
    ++iLength;

  if(iLength == 0)
  {
    inputError(L"Expected number after minus sign");
    return;
  }

  if(isChar(iLength) && getChar(iLength) == '.')
  {
    ++iLength;
    bHasFractionalPart = true;
    fFraction = 0.0f;
    float fMultiplier = 0.1f;
    while(isChar(iLength) && '0' <= getChar(iLength) && getChar(iLength) <= '9')
    {
      fFraction += (getChar(iLength) - '0') * fMultiplier;
      fMultiplier *= 0.1f;
      ++iLength;
    }
  }

  if(isChar(iLength) && (getChar(iLength) == 'E' || getChar(iLength) == 'e'))
  {
    bool bNegativeExponent = false;
    ++iLength;
    bHasExponent = true;
    iExponent = 0;

    if(isChar(iLength) && getChar(iLength) == '+')
      ++iLength;
    else if(isChar(iLength) && getChar(iLength) == '-')
    {
      bNegativeExponent = true;
      ++iLength;
    }

    while(isChar(iLength) && '0' <= getChar(iLength) && getChar(iLength) <= '9')
    {
      iExponent *= 10;
      iExponent += getChar(iLength) - '0';
      ++iLength;
    }

    if(bNegativeExponent)
      iExponent = -iExponent;
  }

  if(isChar(iLength) && (getChar(iLength) == 'f' || getChar(iLength) == 'F'))
  {
    bIsFloat = true;
    ++iLength;
  }

  if(bHasFractionalPart && !bIsFloat)
  {
    inputError(L"Expected number with decimal part to have the \'f\' prefix to mark it as a float");
    return;
  }

  if(bIsFloat)
  {
    float fValue = 0.0f;
    for(int i = 0; i < iLength; ++i)
    {
      char c = getChar(i);
      if('0' <= c && c <= '9')
      {
        fValue *= 10.0f;
        fValue += (float)(c - '0');
      }
      else
        break;
    }
    if(bHasFractionalPart)
      fValue += fFraction;
    if(bIsNegative)
      fValue = -fValue;
    if(bHasExponent)
      fValue *= exp(log(10.0f) * (float)iExponent);
    m_pWriter->setValueFloat(fValue);
  }
  else
  {
    long iValue = 0;
    for(int i = 0; i < iLength; ++i)
    {
      char c = getChar(i);
      if('0' <= c && c <= '9')
      {
        iValue *= 10;
        iValue += c - '0';
      }
      else
        break;
    }
    if(bIsNegative)
      iValue = -iValue;
    if(bHasExponent)
    {
      if(iExponent < 0)
      {
        iValue = iValue / intexp(10, -iExponent);
      }
      else
      {
        iValue = iValue * intexp(10, iExponent);
      }
    }
    m_pWriter->setValueInteger(iValue);
  }
  consume(iLength);
  m_eState = STATE_WANT_END_STATEMENT;
}

void TxtReader::readString()
{
  int iLength = 0;
  while(isChar(iLength) && getChar(iLength) != '\"')
    ++iLength;
  while(!isChar(iLength) || getChar(iLength) != '\"')
  {
    inputError(L"String without closing \'\"\'");
    return;
  }
  m_pWriter->setValueString(m_sBuffer, iLength);
  consume(iLength + 1);
  m_eState = STATE_WANT_END_STATEMENT;
}

void TxtReader::read()
{
  m_eState = STATE_WANT_VALUE;
  m_iLineNumber = 1;
  while(isChar(0))
  {
    switch(getChar(0))
    {
    case '|': case '.':
      if(m_eState != STATE_WANT_KEY)
      {
        inputError(L"Unexpected character");
        return;
      }
      consume(1);
      break;
    case '\n':
      ++m_iLineNumber;
      // no break
    case ' ': case '\r': case '\t':
      // whitespace
      consume(1);
      break;
    case '{':
      // table constructor
      if(m_eState != STATE_WANT_VALUE)
      {
        inputError(L"Unexpected table");
        return;
      }
      consume(1);
      m_stkTableLineNumbers.push(m_iLineNumber);
      pushTable();
      m_eState = STATE_WANT_KEY;
      break;
    case '}':
      // table end
      if(m_eState != STATE_WANT_KEY)
      {
        inputError(L"Unexpected end of table");
        return;
      }
      if(m_stkTableLineNumbers.size() == 0)
      {
        inputError(L"Unbalanced table (\'}\' without a matching \'{\')");
        return;
      }
      m_stkTableLineNumbers.pop();
      consume(1);
      endTable();
      m_eState = STATE_WANT_END_STATEMENT;
      break;
    case '\"':
      if(m_eState != STATE_WANT_VALUE)
      {
        inputError(L"Unexpected string");
        return;
      }
      consume(1);
      readString();
      m_eState = STATE_WANT_END_STATEMENT;
      break;
    case '-':
      if(isChar(1) && getChar(1) == '-')
      {
        consume(2);
        while(isChar(0) && getChar(0) != '\r' && getChar(0) != '\n')
          consume(1);
        break;
      }
      // no break
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      if(m_eState != STATE_WANT_VALUE)
      {
        inputError(L"Unexpected number");
        return;
      }
      readNumber();
      m_eState = STATE_WANT_END_STATEMENT;
      break;
    case 't': case 'f':
      if(m_eState == STATE_WANT_VALUE)
      {
        if(areChars("true"))
        {
          pushValueBoolean(true);
          consume(4);
          m_eState = STATE_WANT_END_STATEMENT;
          break;
        }
        else if(areChars("false"))
        {
          pushValueBoolean(false);
          consume(5);
          m_eState = STATE_WANT_END_STATEMENT;
          break;
        }
      }
      // no break
    case '$': case 'a': case 'b': case 'c': case 'd': case 'e':           case 'g': case 'h':
    case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q':
    case 'r': case 's':           case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
    case '_': case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H':
    case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q':
    case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
      if(m_eState != STATE_WANT_KEY)
      {
        inputError(L"Unexpected key");
        return;
      }
      {
        int n = 1;
        while(isChar(n) && isKeyChar(getChar(n)))
          ++n;
        readKey(n);
        consume(n);
        m_eState = STATE_WANT_SEPERATOR;
      }
      break;
    case ':':
      if(m_eState != STATE_WANT_SEPERATOR)
      {
        inputError(L"Unexpected key/value seperator");
        return;
      }
      consume(1);
      m_eState = STATE_WANT_VALUE;
      break;
    case ';':
      // end of statement
      if(m_eState != STATE_WANT_END_STATEMENT)
      {
        inputError(L"Unexpected end of statement");
        return;
      }
      consume(1);
      endStatement();
      if(m_stkTableLineNumbers.size() > 0)
        m_eState = STATE_WANT_KEY;
      else
        m_eState = STATE_WANT_END_FILE;
      break;
    default:
      inputError(L"Invalid character");
      return;
    };
  }
  if(m_stkTableLineNumbers.size() != 0)
  {
    THROW_SIMPLE_(L"Error while reading text file; on line %li:\nUnbalanced table (\'{\' without a matching \'}\')", m_stkTableLineNumbers.top());
  }
}

void TxtReader::readKey(int iLength)
{
  m_pWriter->setKey(m_sBuffer, iLength);
}

void TxtReader::pushValueBoolean(bool bValue)
{
  m_pWriter->setValueBoolean(bValue);
}

void TxtReader::pushTable()
{
  m_pWriter->pushTable();
}

void TxtReader::endTable()
{
  m_pWriter->setValueTable(m_pWriter->popTable());
}

void TxtReader::endStatement()
{
  if(!m_stkTableLineNumbers.empty())
    m_pWriter->setTable();
}

void TxtReader::inputError(const wchar_t *sReason)
{
  const wchar_t *sExpected = L"";
  switch(m_eState)
  {
  case STATE_WANT_SEPERATOR:
    sExpected = L"(was expecting a \':\')"; break;
  case STATE_WANT_VALUE:
    sExpected = L"(was expecting a value)"; break;
  case STATE_WANT_KEY:
    sExpected = L"(was expecting a key or end of table)"; break;
  case STATE_WANT_END_STATEMENT:
    sExpected = L"(was expecting a semicolon)"; break;
  case STATE_WANT_END_FILE:
    sExpected = L"(was expecting end of file)"; break;
  }
  THROW_SIMPLE_(L"Error while reading text file; on line %li, at \"%.20S\":\n%s %s", m_iLineNumber, m_sBuffer, sReason, sExpected);
}

bool TxtReader::isChar(int n)
{
  while(true)
  {
    if(n < m_iBytesOfDataInBuffer)
      return true;
    if(m_pFile == 0)
      return false;
    if(m_iBufferRemaining == 0)
    {
      if(m_sBuffer != m_sBufferBase)
      {
        memmove(m_sBufferBase, m_sBuffer, m_iBytesOfDataInBuffer);
        m_iBufferRemaining = m_sBuffer - m_sBufferBase;
        m_sBuffer = m_sBufferBase;
      }
      else
      {
        m_sBuffer = (char*)realloc(m_sBuffer, m_iBufferLength * 2);
        m_iBufferRemaining = m_iBufferLength;
        m_iBufferLength *= 2;
      }
    }
    int iAmt = m_pFile->readArrayNoThrow(m_sBuffer + m_iBytesOfDataInBuffer, m_iBufferRemaining - m_iBytesOfDataInBuffer);
    if(iAmt == 0)
      m_pFile = 0;
    m_iBufferRemaining -= iAmt;
    m_iBytesOfDataInBuffer += iAmt;
  }
}

bool TxtReader::areChars(const char *s)
{
  for(int n = 0; *s; ++s, ++n)
  {
    if(!isChar(n) || getChar(n) != *s)
      return false;
  }
  return true;
}

char TxtReader::getChar(int n)
{
  return m_sBuffer[n];
}

void TxtReader::consume(int n)
{
  m_sBuffer += n;
  m_iBufferRemaining -= n;
  m_iBytesOfDataInBuffer -= n;
  if((m_iBufferRemaining * 2) < m_iBufferLength)
  {
    memmove(m_sBufferBase, m_sBuffer, m_iBytesOfDataInBuffer);
    m_iBufferRemaining = m_iBufferLength;
    m_sBuffer = m_sBufferBase;
  }
}
