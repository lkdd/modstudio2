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
#include "TextFileReader.h"
#include <stdio.h>

struct command_line_options_t
{
  command_line_options_t()
    : ePrintLevel(PRINT_NORMAL)
    , cPathSeperator('|')
    , pUCS(0)
    , iUcsLimit(100)
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
  if(sFile.length() == 1 && sFile[0] == '-')
    return RainOpenFilePtr(stdin, false);
  else
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
  oWriter.setUCS(g_oCommandLine.pUCS, g_oCommandLine.iUcsLimit);
  oWriter.setIndentProperties(2, ' ', g_oCommandLine.cPathSeperator);
  std::auto_ptr<IAttributeTable> pTable(oAttribFile.getRootTable());
  oWriter.writeTable(&*pTable);
}

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
    fwprintf(stderr, L"      if \"-\", then uses stdin as input text file or input list file\n");
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
      std::auto_ptr<IFile> pListFile(OpenInputFile(g_oCommandLine.sInput));
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
