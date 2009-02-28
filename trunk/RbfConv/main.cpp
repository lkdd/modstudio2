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

// Yes this file is quite horrible at the moment, it'll be cleaned up with time

#define RAINMAN2_NO_WX
#define RAINMAN2_NO_LUA
#include <Rainman2.h>
#include <stdio.h>
#include <math.h>
#include <stack>

struct command_line_options_t
{
  command_line_options_t()
    : ePrintLevel(PRINT_NORMAL)
    , cPathSeperator('|')
    , pUCS(0)
    , bSort(true)
    , bCache(true)
    , bInputIsList(false)
    , bRelicStyle(false)
  {
  }

  RainString sInput;
  RainString sOutput;
  RainString sUCS;

  UcsFile *pUCS;
  long iUcsLimit;

  enum
  {
    PRINT_VERBOSE,
    PRINT_NORMAL,
    PRINT_QUIET,
  } ePrintLevel;

  char cPathSeperator;
  bool bSort;
  bool bCache;
  bool bRelicStyle;
  bool bInputIsList;
} g_oCommandLine;

int nullprintf(const wchar_t*, ...) {return 0;}
#define VERBOSEwprintf(...) ((g_oCommandLine.ePrintLevel <= command_line_options_t::PRINT_VERBOSE ? wprintf : nullprintf)(__VA_ARGS__))
#define NOTQUIETwprintf(...) ((g_oCommandLine.ePrintLevel < command_line_options_t::PRINT_QUIET ? wprintf : nullprintf)(__VA_ARGS__))

IFile* OpenInputFile(const RainString& sFile)
{
  return RainOpenFileNoThrow(sFile, FM_Read);
}

IFile* OpenOutputFile(const RainString& sFile, const RainString& sFileIn)
{
  if(sFile.isEmpty())
  {
    RainString sNewFileName = sFileIn.beforeLast('.');
    if(sFileIn.afterLast('.').compareCaseless("rbf") == 0)
      sNewFileName += L".txt";
    else
      sNewFileName += L".rbf";
    return RainOpenFileNoThrow(sNewFileName, FM_Write);
  }
  else
    return RainOpenFileNoThrow(sFile, FM_Write);
}

char static UcsCommentConvertor(wchar_t x)
{
  if(x > 0xFF)
    return '?';
  if(x == '\r' || x == '\n')
    return ' ';
  return (char)x;
}

class RbfTxtWriter
{
public:
  RbfTxtWriter(IFile *p)
    : m_iIndentLevel(0)
    , m_iSpacesPerLevel(2)
    , m_iIndentArrayLength(0)
    , m_sIndentArray(0)
    , m_oOutput(p)
  {
    m_oOutput.write("-- Made by Corsix\'s Crude RBF Convertor\r\n");
  }

  ~RbfTxtWriter()
  {
    delete[] m_sIndentArray;
  }

  void writeTable(IAttributeTable *pTable)
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

  void writeValue(IAttributeValue *pValue)
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
      if(g_oCommandLine.pUCS)
      {
        long iValue = pValue->getValueInteger();
        const RainString *pString = (*g_oCommandLine.pUCS)[iValue];
        m_oOutput.write(m_sBuffer, sprintf(m_sBuffer, "%li;", iValue));
        if(pString && iValue >= 100)
        {
          m_oOutput.write(" -- ", 4);
          m_oOutput.writeConverting(pString->getCharacters(), pString->length(), UcsCommentConvertor);
        }
        m_oOutput.write("\r\n", 2);
      }
      else
        m_oOutput.write(m_sBuffer, sprintf(m_sBuffer, "%li;\r\n", pValue->getValueInteger()));
      break;
    case VT_Float:
      m_oOutput.write(m_sBuffer, sprintf(m_sBuffer, "%.7gf;\r\n", pValue->getValueFloat()));
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

  void writeLinePrefix()
  {
    if(m_iIndentArrayLength < m_iIndentLevel)
    {
      delete[] m_sIndentArray;
      m_iIndentArrayLength = (m_iIndentArrayLength * 2) + ((m_iIndentLevel - m_iIndentArrayLength) * 4);
      m_sIndentArray = new char[m_iIndentArrayLength];
      memset(m_sIndentArray, ' ', m_iIndentArrayLength);
      for(int i = 0; i < m_iIndentArrayLength; i += 2)
        m_sIndentArray[i] = g_oCommandLine.cPathSeperator;
    }
    m_oOutput.write(m_sIndentArray, m_iIndentLevel);
  }

  void indent()
  {
    m_iIndentLevel += m_iSpacesPerLevel;
  }

  void unindent()
  {
    m_iIndentLevel -= m_iSpacesPerLevel;
    if(m_iIndentLevel < 0)
      m_iIndentLevel = 0;
  }

protected:
  int m_iIndentLevel,
      m_iSpacesPerLevel,
      m_iIndentArrayLength;
  char  *m_sIndentArray;
  BufferingOutputStream<char> m_oOutput;
  char   m_sBuffer[32];
};

void ConvertRbfToTxt(IFile *pIn, IFile *pOut)
{
  RbfAttributeFile oAttribFile;
  if(!g_oCommandLine.bSort)
    oAttribFile.enableTableChildrenSort(false);
  try
  {
    oAttribFile.load(pIn);
  }
  CATCH_THROW_SIMPLE({}, L"Cannot load input RBF");
  RbfTxtWriter oWriter(pOut);
  std::auto_ptr<IAttributeTable> pTable(oAttribFile.getRootTable());
  oWriter.writeTable(&*pTable);
}

int intexp(int base, int power)
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

class TxtReader
{
public:
  TxtReader(IFile *pInputFile, RbfWriter *pWriter)
    : m_pFile(pInputFile)
    , m_pWriter(pWriter)
    , m_iBufferLength(1024)
    , m_iBytesOfDataInBuffer(0)
  {
    m_sBuffer = m_sBufferBase = (char*)malloc(m_iBufferLength);
    m_iBufferRemaining = m_iBufferLength;
  }

  ~TxtReader()
  {
    free(m_sBufferBase);
  }

  static bool isKeyChar(char c)
  {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '$' || c == '_';
  }

  void readNumber()
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

  void readString()
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

  void read()
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

  void readKey(int iLength)
  {
    m_pWriter->setKey(m_sBuffer, iLength);
  }

  void pushValueBoolean(bool bValue)
  {
    m_pWriter->setValueBoolean(bValue);
  }

  void pushTable()
  {
    m_pWriter->pushTable();
  }

  void endTable()
  {
    m_pWriter->setValueTable(m_pWriter->popTable());
  }

  void endStatement()
  {
    if(!m_stkTableLineNumbers.empty())
      m_pWriter->setTable();
  }

  void inputError(const wchar_t *sReason)
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

  bool isChar(int n)
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

  bool areChars(const char *s)
  {
    for(int n = 0; *s; ++s, ++n)
    {
      if(!isChar(n) || getChar(n) != *s)
        return false;
    }
    return true;
  }

  char getChar(int n)
  {
    return m_sBuffer[n];
  }

  void consume(int n)
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

protected:
  IFile *m_pFile;
  char *m_sBuffer, *m_sBufferBase;
  RbfWriter *m_pWriter;
  int m_iBufferLength, m_iBufferRemaining, m_iBytesOfDataInBuffer;
  int m_iLineNumber;
  std::stack<int> m_stkTableLineNumbers;
  enum
  {
    STATE_WANT_SEPERATOR,
    STATE_WANT_VALUE,
    STATE_WANT_KEY,
    STATE_WANT_END_STATEMENT,
    STATE_WANT_END_FILE,
  } m_eState;
};

void ConvertTxtToRbf(IFile *pIn, IFile *pOut)
{
  RbfWriter oWriter;
  TxtReader oReader(pIn, &oWriter);

  if(!g_oCommandLine.bCache)
    oWriter.enableCaching(false);

  oWriter.initialise();
  oReader.read();
  if(g_oCommandLine.bRelicStyle)
  {
    RbfWriter oRelicStyled;
    oRelicStyled.initialise();
    oWriter.rewriteInRelicStyle(&oRelicStyled);
    oRelicStyled.writeToFile(pOut, true);
  }
  else
    oWriter.writeToFile(pOut);

  VERBOSEwprintf(L"RBF stats:\n");
  VERBOSEwprintf(L"  %lu tables\n", oWriter.getTableCount());
  VERBOSEwprintf(L"  %lu keys\n", oWriter.getKeyCount());
  VERBOSEwprintf(L"  %lu data items via %lu indicies\n", oWriter.getDataCount(), oWriter.getDataIndexCount());
  VERBOSEwprintf(L"  %lu strings over %lu bytes\n", oWriter.getStringCount(), oWriter.getStringsLength());
  VERBOSEwprintf(L"  %lu bytes saved by caching\n", oWriter.getAmountSavedByCache());
}

bool DoWork(RainString& sInput, RainString& sOutput)
{
  std::auto_ptr<IFile> pInFile;
  std::auto_ptr<IFile> pOutFile;

  pInFile.reset(OpenInputFile(sInput));
  pOutFile.reset(OpenOutputFile(sOutput, sInput));

  if(pInFile.get() && pOutFile.get())
  {
    if(sInput.afterLast('.').compareCaseless("rbf") == 0)
    {
      NOTQUIETwprintf(L"Converting RBF to text...\n");
      ConvertRbfToTxt(&*pInFile, &*pOutFile);
    }
    else
    {
      NOTQUIETwprintf(L"Converting text to RBF...\n");
      ConvertTxtToRbf(&*pInFile, &*pOutFile);
    }
    return true;
  }
  else
  {
    if(!pInFile.get())
      fwprintf(stderr, L"Cannot open input file \'%s\'\n", sInput.getCharacters());
    if(!pOutFile.get())
      fwprintf(stderr, L"Cannot open output file \'%s\'\n", sOutput.getCharacters());
  }
  return false;
}

int wmain(int argc, wchar_t** argv)
{
#define REQUIRE_NEXT_ARG(noun) if((i + 1) >= argc) { \
    fwprintf(stderr, L"Expected " L ## noun L" to follow \"%s\"\n", arg); \
    return -2; \
  }

  for(int i = 1; i < argc; ++i)
  {
    wchar_t *arg = argv[i];
    if(arg[0] == '-')
    {
      bool bValid = false;
      if(arg[1] != 0 && arg[2] == 0)
      {
        bValid = true;
        switch(arg[1])
        {
        case 'i':
          REQUIRE_NEXT_ARG("filename");
          g_oCommandLine.sInput = argv[++i];
          break;
        case 'o':
          REQUIRE_NEXT_ARG("filename");
          g_oCommandLine.sOutput = argv[++i];
          break;
        case 's':
          g_oCommandLine.bSort = false;
          break;
        case 'R':
          g_oCommandLine.bRelicStyle = true;
          // no break
        case 'c':
          g_oCommandLine.bCache = false;
          break;
        case 'v':
          g_oCommandLine.ePrintLevel = command_line_options_t::PRINT_VERBOSE;
          break;
        case 'q':
          g_oCommandLine.ePrintLevel = command_line_options_t::PRINT_QUIET;
          break;
        case 'p':
          REQUIRE_NEXT_ARG("path seperator");
          g_oCommandLine.cPathSeperator = argv[++i][0];
          break;
        case 'u':
          REQUIRE_NEXT_ARG("filename");
          g_oCommandLine.sUCS = argv[++i];
          break;
        case 'U':
          REQUIRE_NEXT_ARG("number");
          g_oCommandLine.iUcsLimit = _wtoi(argv[++i]);
          break;
        case 'L':
          g_oCommandLine.bInputIsList = true;
          break;
        default:
          bValid = false;
          break;
        }
      }
      if(!bValid)
      {
        fwprintf(stderr, L"Unrecognised command line switch \"%s\"\n", arg);
        return -1;
      }
    }
    else
    {
      fwprintf(stderr, L"Expected command line switch rather than \"%s\"\n", arg);
      return -3;
    }
  }

#undef REQUIRE_NEXT_ARG

  NOTQUIETwprintf(L"** Corsix\'s Crude RBF Convertor **\n");
  if(g_oCommandLine.sInput.isEmpty())
  {
    fwprintf(stderr, L"Expected an input filename. Command format is:\n");
    fwprintf(stderr, L"%s -i infile [-L | -o outfile] [-q | -v] [-p \".\" | -p \" \"] [-u ucsfile] [-U threshold] [-s] [-R | -c]\n", wcsrchr(*argv, '\\') ? (wcsrchr(*argv, '\\') + 1) : (*argv));
    fwprintf(stderr, L"  -i; file to read from, either a .rbf file or a .txt file\n");
    fwprintf(stderr, L"  -o; file to write to, either a .rbf file or a .txt file\n");
    fwprintf(stderr, L"      if not given, then uses input name with .txt swapped for .rbf (and vice versa)\n");
    fwprintf(stderr, L"  -L; infile is a file containing a list of input files (one per line)\n");
    fwprintf(stderr, L"  -q; quiet output to console\n");
    fwprintf(stderr, L"  -v; verbose output to console\n");
    fwprintf(stderr, L"  Options for writing text files:\n");
    fwprintf(stderr, L"  -p; path seperator to use (defaults to vertical bar)\n");
    fwprintf(stderr, L"  -u; UCS file to use to comment number values\n");
    fwprintf(stderr, L"  -U; Integers above which to interpret as UCS references (defaults to 100)\n");
    fwprintf(stderr, L"  -s; Disable sorting of values (use order from RBF file)\n");
    fwprintf(stderr, L"  Options for writing RBF files:\n");
    fwprintf(stderr, L"  -c; Disable caching of data (faster, but makes larger file)\n");
    fwprintf(stderr, L"  -R; Write in Relic style (takes longer, also forces -c)\n");
    
    return -4;
  }
  if(g_oCommandLine.sInput.compareCaseless(g_oCommandLine.sOutput) == 0)
  {
    fwprintf(stderr, L"Expected input file and output file to be different files");
    return -5;
  }

  bool bAllGood = false;

  try
  {
    if(!g_oCommandLine.sUCS.isEmpty())
    {
      g_oCommandLine.pUCS = new UcsFile;
      g_oCommandLine.pUCS->loadFromFile(g_oCommandLine.sUCS);
    }

    if(g_oCommandLine.bInputIsList)
    {
      bAllGood = true;
      g_oCommandLine.sOutput.clear();
      std::auto_ptr<IFile> pListFile(RainOpenFile(g_oCommandLine.sInput, FM_Read));
      BufferingInputTextStream<char> oListFile(&*pListFile);
      while(!oListFile.isEOF())
      {
        RainString sLine(oListFile.readLine().trimWhitespace());
        if(!sLine.isEmpty())
        {
          NOTQUIETwprintf(L"%s\n", sLine.getCharacters());
          bAllGood = DoWork(sLine, g_oCommandLine.sOutput) && bAllGood;
        }
      }
    }
    else
      bAllGood = DoWork(g_oCommandLine.sInput, g_oCommandLine.sOutput);
  }
  catch(RainException *pE)
  {
    bAllGood = false;
    fwprintf(stderr, L"Fatal exception:\n");
    for(RainException *p = pE; p; p = p->getPrevious())
    {
      fwprintf(stderr, L"%s:%li - %s\n", p->getFile().getCharacters(), p->getLine(), p->getMessage().getCharacters());
      if(g_oCommandLine.ePrintLevel >= command_line_options_t::PRINT_QUIET)
        break;
    }
    delete pE;
  }

  delete g_oCommandLine.pUCS;

  if(bAllGood)
  {
    NOTQUIETwprintf(L"Done\n");
    return 0;
  }
  return -10;
}
