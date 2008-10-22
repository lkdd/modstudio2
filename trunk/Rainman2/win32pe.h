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
#include "api.h"
#include "file.h"
#include <vector>

class RAINMAN2_API Win32PEFile
{
public:
  struct Section
  {
    RainString sName;
    unsigned long iVirtualSize;
    unsigned long iVirtualAddress;
    unsigned long iPhysicalSize;
    unsigned long iPhysicalAddress;
    unsigned long iFlags;
  };

  class ResourceNodeName
  {
  public:
    ResourceNodeName(const ResourceNodeName&);
    ResourceNodeName(RainString sName);
    ResourceNodeName(unsigned long iIdentifier);

    inline       bool        hasName() const throw() {return m_bUsesName;}
    inline const RainString& getName() const throw() {return m_sName;}

    inline bool          hasIdentifier() const throw() {return !m_bUsesName;}
    inline unsigned long getIdentifier() const throw() {return m_iIdentifier;}

  protected:
    RainString m_sName;
    unsigned long m_iIdentifier;
    bool m_bUsesName;
  };

  class ResourceNode
  {
  public:
    typedef std::vector<ResourceNode*> Children;

    inline bool              hasName() const throw() {return m_bHasName;}
    inline const RainString& getName() const throw() {return m_sName;}

    inline bool          hasIdentifier() const throw() {return m_bHasIdentifer;}
    inline unsigned long getIdentifier() const throw() {return m_iIdentifier;}

    inline bool          hasData        () const throw() {return m_bHasData;}
    inline unsigned long getDataAddress () const throw() {return m_iDataOffset;}
    inline unsigned long getDataSize    () const throw() {return m_iDataLength;}
    inline unsigned long getDataCodepage() const throw() {return m_iDataCodepage;}

    inline       bool          hasParent() const throw() {return m_pParent != 0;}
    inline       ResourceNode* getParent()       throw() {return m_pParent;}
    inline const ResourceNode* getParent() const throw() {return m_pParent;}

    inline       bool      hasChildren() const throw() {return m_bHasChildren;}
    inline       Children& getChildren()       throw() {return m_vChildren;}
    inline const Children& getChildren() const throw() {return m_vChildren;}

    const ResourceNode* findFirstChildWithData() const;
    const ResourceNode* findChild(ResourceNodeName oName) const;

  protected:
    friend class Win32PEFile;

    ResourceNode();
    ~ResourceNode();

    RainString     m_sName;
    unsigned long  m_iIdentifier,
                   m_iDataOffset,
                   m_iDataLength,
                   m_iDataCodepage;
    ResourceNode*  m_pParent;
    Children       m_vChildren;
    bool           m_bHasName      : 1,
                   m_bHasIdentifer : 1,
                   m_bHasData      : 1,
                   m_bHasChildren  : 1;
  };

  struct Icon
  {
    Icon();

    unsigned char  iWidth,
                   iHeight,
                   iColourCount,
                   iReserved;
    unsigned short iNumPlanes,
                   iBitsPerPixel,
                   iIndex;
    unsigned long  iSize,
                   iOffset;
  };

  typedef std::vector<Icon> IconGroup;

  Win32PEFile();
  ~Win32PEFile();

  void clear();

  void load(IFile *pFile) throw(...);
  void load(RainString sFile) throw(...);

  size_t getSectionCount() const throw();
  const Section& getSection(size_t iIndex) const throw(...);
  const std::vector<Section>& getSectionArray() const throw();

  size_t getImportedLibraryCount() const throw();
  const RainString& getImportedLibrary(size_t iIndex) const throw(...);
  const std::vector<RainString>& getImportedLibraryArray() const throw();

  const ResourceNode* getResources() const throw();
  const ResourceNode* findResource(ResourceNodeName oType) const throw();
  const ResourceNode* findResource(ResourceNodeName oType, ResourceNodeName oName) const throw();
  const ResourceNode* findResource(ResourceNodeName oType, ResourceNodeName oName, ResourceNodeName oLanguage) const throw();

  const std::vector<IconGroup*>& getIconGroupArray() const throw();

protected:
  std::vector< std::pair<unsigned long, unsigned long> >m_vSpecialTables;
  std::vector<Section> m_vSections;
  std::vector<RainString> m_vImportedLibraries;
  std::vector<IconGroup*> m_vIconGroups;
  ResourceNode* m_pRootResourceNode;

  static const unsigned long NOT_MAPPED = static_cast<unsigned long>(-1);

  unsigned long _mapToPhysicalAddress(unsigned long iVirtualAddress) throw(...);
  unsigned long _mapToPhysicalAddressNoThrow(unsigned long iVirtualAddress) throw(...);

  void _loadImports(IFile *pFile);
  void _loadResources(IFile *pFile);
  void _loadResourceTable(IFile *pFile, ResourceNode::Children& vDestination);
  void _loadIconGroup(IFile *pFile, IconGroup *pDestination);
};

bool operator == (const Win32PEFile::ResourceNodeName&, const Win32PEFile::ResourceNode&);
bool operator == (const Win32PEFile::ResourceNodeName&, const Win32PEFile::ResourceNode*);
bool operator == (const Win32PEFile::ResourceNode&, const Win32PEFile::ResourceNodeName&);
bool operator == (const Win32PEFile::ResourceNode*, const Win32PEFile::ResourceNodeName&);
