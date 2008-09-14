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
#include "win32pe.h"
#include "exception.h"

#ifndef DWORD
#define DWORD unsigned long
#endif
#ifndef WORD
#define WORD unsigned short
#endif

Win32PEFile::Win32PEFile()
{
}

Win32PEFile::~Win32PEFile()
{
  clear();
}

void Win32PEFile::clear()
{
  m_vSpecialTables.clear();
  m_vSections.clear();
  m_vImportedLibraries.clear();
}

unsigned long Win32PEFile::_mapToPhysicalAddress(unsigned long iVirtualAddress) throw(...)
{
  for(std::vector<Section>::iterator itr = m_vSections.begin(); itr != m_vSections.end(); ++itr)
  {
    if(itr->iVirtualAddress <= iVirtualAddress && iVirtualAddress < (itr->iVirtualAddress + itr->iVirtualSize))
    {
      if(iVirtualAddress < (itr->iVirtualAddress + itr->iPhysicalSize))
        return iVirtualAddress - itr->iVirtualAddress + itr->iPhysicalAddress;
      else
        THROW_SIMPLE_2(L"Cannot map to physical; %X is uninitialised virtual memory in section \'%s\'", iVirtualAddress, itr->sName.getCharacters());
    }
  }
  THROW_SIMPLE_1(L"Cannot map to physical; %X lies outside of any section", iVirtualAddress);
}

void Win32PEFile::load(IFile *pFile) throw(...)
{
  clear();

  // Locate the COFF header, and assert that the file is PE
  try
  {
    // "At location 0x3c, the stub has the file offset to the PE signature"
    pFile->seek(0x3c, SR_Start);

    char cSignature[4];
    pFile->readArray(cSignature, 4);

    if(memcmp(cSignature, "PE\0\0", 4) != 0)
      THROW_SIMPLE(L"PE signature does not match");
  }
  CATCH_THROW_SIMPLE(L"Cannot locate valid PE header; is this a PE file?", {});

  // Read the information we need from the COFF file header
  WORD wSectionCount;
  try
  {
    WORD wMachineType;

    pFile->readOne(wMachineType);
    pFile->readOne(wSectionCount);
    pFile->seek(16, SR_Current); // COFF file header is 20 bytes, the two previous fields are 4, 20-4=16
  }
  CATCH_THROW_SIMPLE(L"Cannot read COFF file header", {});

  // Read the information we need from the COFF 'optional' (not for PE files) header
  bool bIsPE32Plus = false;
  DWORD dwSpecialTableCount;
  try
  {
    WORD wMagicValue;

    pFile->readOne(wMagicValue);

    switch(wMagicValue)
    {
    case 0x10B: // PE32 format
      bIsPE32Plus = false;
      break;

    case 0x20B: // PE32+ format
      bIsPE32Plus = true;
      break;

    default:
      THROW_SIMPLE_1(L"Unrecognised COFF optional header magic value (%i)", static_cast<int>(wMagicValue));
      break;
    }

    pFile->seek(bIsPE32Plus ? 106 : 90, SR_Current); // skip everything upto the NumberOfRvaAndSizes field

    pFile->readOne(dwSpecialTableCount);
  }
  CATCH_THROW_SIMPLE(L"Cannot read COFF optional header", {});

  // Read the special table locations and sizes
  m_vSpecialTables.reserve(dwSpecialTableCount);
  for(DWORD dwTable = 0; dwTable < dwSpecialTableCount; ++dwTable)
  {
    DWORD dwVirtualAddress;
    DWORD dwSize;

    try
    {
      pFile->readOne(dwVirtualAddress);
      pFile->readOne(dwSize);
    }
    CATCH_THROW_SIMPLE_1(L"Cannot read optional header data directory #%i", static_cast<int>(dwTable), {});

    m_vSpecialTables.push_back(std::make_pair(dwVirtualAddress, dwSize));
  }

  // Read the sections
  m_vSections.reserve(wSectionCount);
  for(WORD wSection = 0; wSection < wSectionCount; ++wSection)
  {
    m_vSections.push_back(Section());
    Section &oSection = m_vSections[wSection];
    char aName[9] = {0};

    try
    {
      pFile->readArray(aName, 8);
      pFile->readOne(oSection.iVirtualSize);
      pFile->readOne(oSection.iVirtualAddress);
      pFile->readOne(oSection.iPhysicalSize);
      pFile->readOne(oSection.iPhysicalAddress);
      pFile->seek(12, SR_Current); // skip a few fields
      pFile->readOne(oSection.iFlags);
    }
    CATCH_THROW_SIMPLE_1(L"Cannot read section #%i information", static_cast<int>(wSection), {});

    oSection.sName = aName;
  }

  // Map the special table addresses from virtual to physical
  for(DWORD dwTable = 0; dwTable < dwSpecialTableCount; ++dwTable)
  {
    try
    {
      if(m_vSpecialTables[dwTable].first != 0)
        m_vSpecialTables[dwTable].first = _mapToPhysicalAddress(m_vSpecialTables[dwTable].first);
    }
    CATCH_THROW_SIMPLE_1(L"Cannot map special table #%i from virtual to physical", static_cast<int>(dwTable), {});
  }

  // Load all the special tables which we are interested in
  switch(dwSpecialTableCount)
  {
  default:
    // Other tables
    // (all statements fall-through)

  case 3:
    // Resource table
    // (this isn't loaded yet, but might be in the future)

  case 2:
    // Import table
    try
    {
      _loadImports(pFile);
    }
    CATCH_THROW_SIMPLE(L"Cannot load imports", {});

  case 1:
    // Export table
    // (this isn't loaded yet, but might be in the future)

  case 0:
    // 0 is provided so that default is hit for everything greater than the highest index we care about
    break;
  }
}

void Win32PEFile::load(RainString sFile) throw(...)
{
  IFile* pFile = 0;
  try
  {
    pFile = RainOpenFile(sFile, FM_Read);
    load(pFile);
  }
  CATCH_THROW_SIMPLE_1(L"Cannot load PE file \'%s\'", sFile.getCharacters(), {delete pFile;});
  delete pFile;
}

void Win32PEFile::_loadImports(IFile *pFile)
{
  try
  {
    pFile->seek(m_vSpecialTables[1].first, SR_Start);
  }
  CATCH_THROW_SIMPLE(L"Cannot seek to imports table", {});

  std::vector<DWORD> vLibraryNameAddresses;

  for(int iLib = 0;; ++iLib)
  {
    DWORD dwNameOffset;

    try
    {
      pFile->seek(12, SR_Current);
      pFile->readOne(dwNameOffset);
      pFile->seek(4, SR_Current);
    }
    CATCH_THROW_SIMPLE_1(L"Cannot load import details for library #%i", iLib, {});

    if(dwNameOffset == 0)
      break;
    else
      vLibraryNameAddresses.push_back(dwNameOffset);
  }

  for(std::vector<DWORD>::iterator itr = vLibraryNameAddresses.begin(); itr != vLibraryNameAddresses.end(); ++itr)
  {
    try
    {
      pFile->seek(_mapToPhysicalAddress(*itr), SR_Start);
      m_vImportedLibraries.push_back(pFile->readString<char>(0));
    }
    CATCH_THROW_SIMPLE(L"Cannot read library name", {});
  }
}
