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
#include <vector>
#include "api.h"
#include "file.h"

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

  Win32PEFile();
  ~Win32PEFile();

  void clear();

  void load(IFile *pFile) throw(...);
  void load(RainString sFile) throw(...);

  size_t getSectionCount() const throw();
  const Section& getSection(size_t iIndex) const throw(...);

  size_t getImportedLibraryCount() const throw();
  const RainString& getImportedLibrary(size_t iIndex) const throw(...);

protected:
  std::vector< std::pair<unsigned long, unsigned long> >m_vSpecialTables;
  std::vector<Section> m_vSections;
  std::vector<RainString> m_vImportedLibraries;

  unsigned long _mapToPhysicalAddress(unsigned long iVirtualAddress) throw(...);

  void _loadImports(IFile *pFile);
};
