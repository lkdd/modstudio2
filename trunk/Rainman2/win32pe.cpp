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
  m_pRootResourceNode = 0;
}

Win32PEFile::~Win32PEFile()
{
  clear();
}

void Win32PEFile::clear()
{
  delete m_pRootResourceNode;
  m_pRootResourceNode = 0;
  for(std::vector<IconGroup*>::iterator itr = m_vIconGroups.begin(); itr != m_vIconGroups.end(); ++itr)
    delete *itr;
  m_vIconGroups.clear();
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
        THROW_SIMPLE_(L"Cannot map to physical; %X is uninitialised virtual memory in section \'%s\'", iVirtualAddress, itr->sName.getCharacters());
    }
  }
  THROW_SIMPLE_(L"Cannot map to physical; %X lies outside of any section", iVirtualAddress);
}

unsigned long Win32PEFile::_mapToPhysicalAddressNoThrow(unsigned long iVirtualAddress) throw(...)
{
  for(std::vector<Section>::iterator itr = m_vSections.begin(); itr != m_vSections.end(); ++itr)
  {
    if(itr->iVirtualAddress <= iVirtualAddress && iVirtualAddress < (itr->iVirtualAddress + itr->iVirtualSize))
    {
      if(iVirtualAddress < (itr->iVirtualAddress + itr->iPhysicalSize))
        return iVirtualAddress - itr->iVirtualAddress + itr->iPhysicalAddress;
      else
        return NOT_MAPPED;
    }
  }
  return NOT_MAPPED;
}

void Win32PEFile::load(IFile *pFile) throw(...)
{
  clear();

  // Locate the COFF header, and assert that the file is PE
  try
  {
    // "At location 0x3c, the stub has the file offset to the PE signature"
    pFile->seek(0x3c, SR_Start);

    DWORD iHeadPosition;
    pFile->readOne(iHeadPosition);
    pFile->seek(iHeadPosition, SR_Start);

    char cSignature[4];
    pFile->readArray(cSignature, 4);

    if(memcmp(cSignature, "PE\0\0", 4) != 0)
      THROW_SIMPLE(L"PE signature does not match");
  }
  CATCH_THROW_SIMPLE({}, L"Cannot locate valid PE header; is this a PE file?");

  // Read the information we need from the COFF file header
  WORD wSectionCount;
  try
  {
    WORD wMachineType;

    pFile->readOne(wMachineType);
    pFile->readOne(wSectionCount);
    pFile->seek(16, SR_Current); // COFF file header is 20 bytes, the two previous fields are 4, 20-4=16
  }
  CATCH_THROW_SIMPLE({}, L"Cannot read COFF file header");

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
      THROW_SIMPLE_(L"Unrecognised COFF optional header magic value (%i)", static_cast<int>(wMagicValue));
      break;
    }

    pFile->seek(bIsPE32Plus ? 106 : 90, SR_Current); // skip everything upto the NumberOfRvaAndSizes field

    pFile->readOne(dwSpecialTableCount);
  }
  CATCH_THROW_SIMPLE({}, L"Cannot read COFF optional header");

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
    CATCH_THROW_SIMPLE_({}, L"Cannot read optional header data directory #%i", static_cast<int>(dwTable));

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
    CATCH_THROW_SIMPLE_({}, L"Cannot read section #%i information", static_cast<int>(wSection));

    oSection.sName = aName;
  }

  // Map the special table addresses from virtual to physical
  for(DWORD dwTable = 0; dwTable < dwSpecialTableCount; ++dwTable)
  {
    try
    {
      if(m_vSpecialTables[dwTable].first != 0 && m_vSpecialTables[dwTable].second != 0)
        m_vSpecialTables[dwTable].first = _mapToPhysicalAddressNoThrow(m_vSpecialTables[dwTable].first);
    }
    CATCH_THROW_SIMPLE_({}, L"Cannot map special table #%i from virtual to physical", static_cast<int>(dwTable));
  }

  // Load all the special tables which we are interested in
  switch(dwSpecialTableCount)
  {
  default:
    // Other tables
    // (all statements fall-through)

  case 3:
    // Resource table
    try
    {
      _loadResources(pFile);
    }
    CATCH_THROW_SIMPLE({}, L"Cannot load resources");

  case 2:
    // Import table
    try
    {
      _loadImports(pFile);
    }
    CATCH_THROW_SIMPLE({}, L"Cannot load imports");

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
  CATCH_THROW_SIMPLE_(delete pFile, L"Cannot load PE file \'%s\'", sFile.getCharacters());
  delete pFile;
}

const std::vector<Win32PEFile::IconGroup*>& Win32PEFile::getIconGroupArray() const throw()
{
  return m_vIconGroups;
}

void Win32PEFile::_loadImports(IFile *pFile)
{
  if(m_vSpecialTables.size() <= 1 || m_vSpecialTables[1].first == 0 || m_vSpecialTables[1].first == NOT_MAPPED)
    return;

  try
  {
    pFile->seek(m_vSpecialTables[1].first, SR_Start);
  }
  CATCH_THROW_SIMPLE({}, L"Cannot seek to imports table");

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
    CATCH_THROW_SIMPLE_({}, L"Cannot load import details for library #%i", iLib);

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
    CATCH_THROW_SIMPLE({}, L"Cannot read library name");
  }
}

void Win32PEFile::_loadResources(IFile *pFile)
{
  if(m_vSpecialTables.size() <= 2 || m_vSpecialTables[2].first == 0 || m_vSpecialTables[2].first == NOT_MAPPED)
    return;

  try
  {
    pFile->seek(m_vSpecialTables[2].first, SR_Start);
  }
  CATCH_THROW_SIMPLE({}, L"Cannot seek to resources table");

  m_pRootResourceNode = new ResourceNode;
  _loadResourceTable(pFile, m_pRootResourceNode->m_vChildren);

  const ResourceNode *pIconGroups = findResource(14);
  if(pIconGroups)
  {
    for(ResourceNode::Children::const_iterator itrGrp = pIconGroups->getChildren().begin(), endGrp = pIconGroups->getChildren().end(); itrGrp != endGrp; ++itrGrp)
    {
      IconGroup *pGroup = new IconGroup;
      m_vIconGroups.push_back(pGroup);

      const ResourceNode *pData = (*itrGrp)->findFirstChildWithData();
      if(pData)
      {
        pFile->seek(pData->getDataAddress(), SR_Start);
        _loadIconGroup(pFile, pGroup);
      }
    }
  }
}

void Win32PEFile::_loadResourceTable(IFile *pFile, Win32PEFile::ResourceNode::Children& vDestination)
{
  WORD wNamedChildrenCount;
  WORD wIdentifiedChildrenCount;

  try
  {
    pFile->seek(12, SR_Current); // skip characteristics, timestamp and version
    pFile->readOne(wNamedChildrenCount);
    pFile->readOne(wIdentifiedChildrenCount);
  }
  CATCH_THROW_SIMPLE({}, L"Cannot read resource table header");

  DWORD dwChildrenCount, dwNamedCutoff;
  dwChildrenCount = static_cast<DWORD>(wNamedChildrenCount) + static_cast<DWORD>(wIdentifiedChildrenCount);
  dwNamedCutoff = wNamedChildrenCount;

  for(DWORD dwChildIndex = 0; dwChildIndex < dwChildrenCount; ++dwChildIndex)
  {
    union
    {
      unsigned long iNameOffset;
      unsigned long iIdentifier;
    };
    union
    {
      unsigned long iDataInfoAddress;
      unsigned long iChildrenAddress;
    };

    try
    {
      pFile->readOne(iNameOffset);
      pFile->readOne(iDataInfoAddress);
    }
    CATCH_THROW_SIMPLE_({}, L"Cannot read table entry #%lu", dwChildIndex);

    ResourceNode *pNewNode = CHECK_ALLOCATION(new (std::nothrow) ResourceNode);
    vDestination.push_back(pNewNode);

    if(dwChildIndex < dwNamedCutoff)
    {
      pNewNode->m_bHasName = true;
      pNewNode->m_iIdentifier = m_vSpecialTables[2].first + (iNameOffset & ~(1 << 31));
    }
    else
    {
      pNewNode->m_bHasIdentifer = true;
      pNewNode->m_iIdentifier = iIdentifier;
    }

    if(iChildrenAddress & (1 << 31))
    {
      pNewNode->m_bHasChildren = true;
      pNewNode->m_iDataOffset = iChildrenAddress & ~(1 << 31);
    }
    else
    {
      pNewNode->m_bHasData = true;
      pNewNode->m_iDataOffset = iDataInfoAddress;
    }
    pNewNode->m_iDataOffset += m_vSpecialTables[2].first;
  }

  for(DWORD dwChildIndex = 0; dwChildIndex < dwChildrenCount; ++dwChildIndex)
  {
    ResourceNode *pNode = vDestination[dwChildIndex];

    if(pNode->m_bHasName)
    {
      try
      {
        pFile->seek(pNode->m_iIdentifier, SR_Start);
        pNode->m_iIdentifier = 0;

        WORD wLength;
        pFile->readOne(wLength);

        pNode->m_sName = RainString(L"\0", 1).repeat(wLength);
        pFile->readArray(pNode->m_sName.begin(), wLength);
      }
      CATCH_THROW_SIMPLE_({}, L"Cannot read name of table entry #%lu", dwChildIndex);
    }

    if(pNode->m_bHasChildren)
    {
      try
      {
        pFile->seek(pNode->m_iDataOffset, SR_Start);
        pNode->m_iDataOffset = 0;
        _loadResourceTable(pFile, pNode->m_vChildren);
      }
      CATCH_THROW_SIMPLE_({}, L"Cannot read children of table entry #%lu", dwChildIndex);

      for(ResourceNode::Children::iterator itr = pNode->m_vChildren.begin(), end = pNode->m_vChildren.end(); itr != end; ++itr)
      {
        (**itr).m_pParent = pNode;
      }
    }

    if(pNode->m_bHasData)
    {
      try
      {
        pFile->seek(pNode->m_iDataOffset, SR_Start);
        pFile->readOne(pNode->m_iDataOffset);
        pFile->readOne(pNode->m_iDataLength);
        pFile->readOne(pNode->m_iDataCodepage);
      }
      CATCH_THROW_SIMPLE_({}, L"Cannot read data information of table entry #%lu", dwChildIndex);

      pNode->m_iDataOffset = _mapToPhysicalAddressNoThrow(pNode->m_iDataOffset);
    }
  }
}

void Win32PEFile::_loadIconGroup(IFile *pFile, Win32PEFile::IconGroup *pDestination)
{
  WORD wIconCount;

  pFile->seek(4, SR_Current);
  pFile->readOne(wIconCount);

  for(WORD wIcon = 0; wIcon < wIconCount; ++wIcon)
  {
    Icon oIcon;

    pFile->readOne(oIcon.iWidth);
    pFile->readOne(oIcon.iHeight);
    pFile->readOne(oIcon.iColourCount);
    pFile->readOne(oIcon.iReserved);
    pFile->readOne(oIcon.iNumPlanes);
    pFile->readOne(oIcon.iBitsPerPixel);
    pFile->readOne(oIcon.iSize);
    pFile->readOne(oIcon.iIndex);

    const ResourceNode *pNode = findResource(3, oIcon.iIndex)->findFirstChildWithData();
    if(pNode)
      oIcon.iOffset = pNode->getDataAddress();

    pDestination->push_back(oIcon);
  }
}

const Win32PEFile::ResourceNode* Win32PEFile::getResources() const throw()
{
  return m_pRootResourceNode;
}

Win32PEFile::ResourceNode::ResourceNode()
{
  m_iIdentifier = 0;
  m_iDataOffset = 0;
  m_iDataLength = 0;
  m_iDataCodepage = 0;
  m_pParent = 0;
  m_bHasName = false;
  m_bHasIdentifer = false;
  m_bHasData = false;
  m_bHasChildren = false;
}

Win32PEFile::ResourceNode::~ResourceNode()
{
  for(Children::iterator itr = m_vChildren.begin(), end = m_vChildren.end(); itr != end; ++itr)
  {
    delete *itr;
  }
}

const Win32PEFile::ResourceNode* Win32PEFile::ResourceNode::findFirstChildWithData() const
{
  if(this == 0 || m_bHasData)
    return this;

  const ResourceNode *pReturnValue = 0;
  for(Children::const_iterator itr = m_vChildren.begin(), end = m_vChildren.end(); pReturnValue == 0 && itr != end; ++itr)
  {
    pReturnValue = (*itr)->findFirstChildWithData();
  }
  return pReturnValue;
}

const Win32PEFile::ResourceNode* Win32PEFile::ResourceNode::findChild(ResourceNodeName oName) const
{
  if(this == 0)
    return 0;
  Children::const_iterator itr = std::find(m_vChildren.begin(), m_vChildren.end(), oName);
  if(itr == m_vChildren.end())
    return 0;
  else
    return *itr;
}

const Win32PEFile::ResourceNode* Win32PEFile::findResource(ResourceNodeName oType) const throw()
{
  return getResources()->findChild(oType);
}

const Win32PEFile::ResourceNode* Win32PEFile::findResource(ResourceNodeName oType, ResourceNodeName oName) const throw()
{
  return findResource(oType)->findChild(oName);
}

const Win32PEFile::ResourceNode* Win32PEFile::findResource(ResourceNodeName oType, ResourceNodeName oName, ResourceNodeName oLanguage) const throw()
{
  return findResource(oType, oName)->findChild(oLanguage);
}

Win32PEFile::ResourceNodeName::ResourceNodeName(const Win32PEFile::ResourceNodeName& o)
  : m_sName(o.m_sName), m_iIdentifier(o.m_iIdentifier), m_bUsesName(o.m_bUsesName)
{
}

Win32PEFile::ResourceNodeName::ResourceNodeName(RainString sName)
  : m_sName(sName), m_bUsesName(true)
{
}

Win32PEFile::ResourceNodeName::ResourceNodeName(unsigned long iIdentifier)
  : m_iIdentifier(iIdentifier), m_bUsesName(false)
{
}

Win32PEFile::Icon::Icon()
{
  iWidth = 0;
  iHeight = 0;
  iColourCount = 0;
  iReserved = 0;
  iNumPlanes = 0;
  iBitsPerPixel = 0;
  iIndex = 0;
  iSize = 0;
  iOffset = Win32PEFile::NOT_MAPPED;
}

bool operator == (const Win32PEFile::ResourceNodeName& a, const Win32PEFile::ResourceNode& b)
{
  if(a.hasName())
    return b.hasName() && a.getName() == b.getName();
  else
    return b.hasIdentifier() && a.getIdentifier() == b.getIdentifier();
}

bool operator == (const Win32PEFile::ResourceNodeName& a, const Win32PEFile::ResourceNode* b)
{
  return a == *b;
}

bool operator == (const Win32PEFile::ResourceNode& a, const Win32PEFile::ResourceNodeName& b)
{
  return b == a;
}

bool operator == (const Win32PEFile::ResourceNode* a, const Win32PEFile::ResourceNodeName& b)
{
  return b == a;
}

size_t Win32PEFile::getSectionCount() const throw()
{
  return getSectionArray().size();
}

const Win32PEFile::Section& Win32PEFile::getSection(size_t iIndex) const throw(...)
{
  CHECK_RANGE_LTMAX(0, iIndex, getSectionCount());
  return getSectionArray()[iIndex];
}

const std::vector<Win32PEFile::Section>& Win32PEFile::getSectionArray() const throw()
{
  return m_vSections;
}

size_t Win32PEFile::getImportedLibraryCount() const throw()
{
  return getImportedLibraryArray().size();
}

const RainString& Win32PEFile::getImportedLibrary(size_t iIndex) const throw(...)
{
  CHECK_RANGE_LTMAX(0, iIndex, getImportedLibraryCount());
  return getImportedLibraryArray()[iIndex];
}

const std::vector<RainString>& Win32PEFile::getImportedLibraryArray() const throw()
{
  return m_vImportedLibraries;
}
