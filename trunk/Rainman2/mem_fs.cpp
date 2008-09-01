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
#include "mem_fs.h"
#include "memfile.h"
#include "exception.h"
#include <time.h>
#include <algorithm>

class MemFileReadAdaptor : public MemoryReadFile
{
public:
  MemFileReadAdaptor(MemoryFileStore::file_t *pFile) throw()
    : MemoryReadFile(pFile->m_pData, pFile->m_iLength), m_pFile(pFile)
  {
    pFile->m_iOpenCount++;
  }

  virtual ~MemFileReadAdaptor() throw()
  {
    m_pFile->m_iOpenCount--;
  }

protected:
  MemoryFileStore::file_t *m_pFile;
};

class MemFileWriteAdaptor : public MemoryWriteFile
{
public:
  MemFileWriteAdaptor(MemoryFileStore::file_t *pFile) throw()
    : MemoryWriteFile(1024), m_pFile(pFile)
  {
    pFile->m_iOpenCount--;
    delete[] m_pFile->m_pData;
    m_pFile->m_pData = 0;
    m_pFile->m_iLength = 0;
    time(&m_pFile->m_iTimestamp);
  }

  virtual ~MemFileWriteAdaptor() throw()
  {
    seekNoThrow(0, SR_End);
    m_pFile->m_pData = m_pBuffer;
    m_pFile->m_iLength = tell();
    m_pFile->m_iOpenCount++;
    m_pBuffer = 0;
  }

protected:
  MemoryFileStore::file_t *m_pFile;
};

class MemDirectoryAdaptor : public IDirectory
{
public:
  MemDirectoryAdaptor(MemoryFileStore::directory_t *pDirectory, MemoryFileStore *pStore, RainString sPath) throw(...)
    : m_pDirectory(pDirectory), m_pStore(pStore)
  {
    if(sPath.suffix(1) == L"\\")
      m_sPath = sPath;
    else
      m_sPath = sPath + L"\\";
  }

  virtual ~MemDirectoryAdaptor() throw()
  {
  }
  
  virtual size_t getItemCount() throw()
  {
    return m_pDirectory->m_vSubdirectories.size() + m_pDirectory->m_vFiles.size();
  }

  virtual void getItemDetails(size_t iIndex, directory_item_t& oDetails) throw(...)
  {
    CHECK_RANGE_LTMAX(static_cast<size_t>(0), iIndex, getItemCount());
    if(iIndex < m_pDirectory->m_vSubdirectories.size())
    {
      MemoryFileStore::directory_t *pDirectory = m_pDirectory->m_vSubdirectories[iIndex];
      if(oDetails.oFields.name)
        oDetails.sName = pDirectory->m_sName;
      if(oDetails.oFields.dir)
        oDetails.bIsDirectory = true;
    }
    else
    {
      iIndex -= m_pDirectory->m_vSubdirectories.size();
      MemoryFileStore::file_t *pFile = m_pDirectory->m_vFiles[iIndex];
      if(oDetails.oFields.name)
        oDetails.sName = pFile->m_sName;
      if(oDetails.oFields.dir)
        oDetails.bIsDirectory = false;
      if(oDetails.oFields.size)
        oDetails.iSize.iUpper = 0, oDetails.iSize.iLower = pFile->m_iLength;
      if(oDetails.oFields.time)
        oDetails.iTimestamp = pFile->m_iTimestamp;
    }
  }

  virtual const RainString& getPath() throw()
  {
    return m_sPath;
  }

  virtual IFileStore* getStore() throw()
  {
    return m_pStore;
  }

public:
  MemoryFileStore::directory_t *m_pDirectory;
  MemoryFileStore *m_pStore;
  RainString m_sPath;
};

void MemoryFileStore::addEntryPoint(const RainString& sName) throw(...)
{
  if(std::find(m_vEntryPoints.begin(), m_vEntryPoints.end(), sName) != m_vEntryPoints.end())
    THROW_SIMPLE_1(L"Entry point \'%s\' already exists", sName.getCharacters());

  m_vEntryPoints.push_back(CHECK_ALLOCATION(new (std::nothrow) directory_t(sName)));
}

template <class T>
static void delete_fn(T* p)
{
  delete p;
}

MemoryFileStore::~MemoryFileStore()
{
  std::for_each(m_vEntryPoints.begin(), m_vEntryPoints.end(), delete_fn<directory_t>);
}

MemoryFileStore::directory_t::directory_t(RainString sName)
  : m_sName(sName), m_pParent(0)
{
}

MemoryFileStore::directory_t::~directory_t()
{
  std::for_each(m_vSubdirectories.begin(), m_vSubdirectories.end(), delete_fn<directory_t>);
  std::for_each(m_vFiles.begin(), m_vFiles.end(), delete_fn<MemoryFileStore::file_t>);
}

MemoryFileStore::file_t::file_t()
{
  m_pData = 0;
  m_iLength = 0;
  m_iOpenCount = 1;
  time(&m_iTimestamp);
}

MemoryFileStore::file_t::~file_t()
{
  delete[] m_pData;
}

bool MemoryFileStore::_find(const RainString& sWhat, MemoryFileStore::directory_t** ppResultDirectory, MemoryFileStore::file_t** ppResultFile, bool bCreateFile, bool bCreateDir, bool bThrow)
{
  if(ppResultDirectory)
    *ppResultDirectory = 0;
  if(ppResultFile)
    *ppResultFile = 0;

  RainString sEntryPoint = sWhat.beforeFirst('\\');
  RainString sRemainder = sWhat.afterFirst('\\');

  directory_t* pCurrentDirectory = 0;
  std::vector<directory_t*>::iterator itrDirectory = std::find(m_vEntryPoints.begin(), m_vEntryPoints.end(), sEntryPoint);
  if(itrDirectory == m_vEntryPoints.end())
  {
    if(!bCreateDir)
    {
      if(bThrow)
        THROW_SIMPLE_2(L"Cannot find entry point \'%s\' for \'%s\'", sEntryPoint.getCharacters(), sWhat.getCharacters());
      else
        return false;
    }
    pCurrentDirectory = new (std::nothrow) directory_t(sEntryPoint);
    if(bThrow)
      CHECK_ALLOCATION(pCurrentDirectory);
    else if(pCurrentDirectory == 0)
      return false;
    m_vEntryPoints.push_back(pCurrentDirectory);
  }
  else
  {
    pCurrentDirectory = *itrDirectory;
  }

  while(!sRemainder.isEmpty())
  {
    RainString sPart = sRemainder.beforeFirst('\\');
    sRemainder = sRemainder.afterFirst('\\');

    if(sRemainder.isEmpty())
    {
      file_t* pFile = 0;
      std::vector<file_t*>::iterator itrFile = std::find(pCurrentDirectory->m_vFiles.begin(), pCurrentDirectory->m_vFiles.end(), sPart);
      if(itrFile == pCurrentDirectory->m_vFiles.end())
      {
        if(bCreateFile)
        {
          pFile = new (std::nothrow) file_t;
          if(bThrow)
            CHECK_ALLOCATION(pFile);
          else if(pFile == 0)
            return false;
          pFile->m_sName = sPart;
          pCurrentDirectory->m_vFiles.push_back(pFile);
        }
      }
      else
      {
        pFile = *itrFile;
      }
      if(pFile)
      {
        if(ppResultDirectory)
          *ppResultDirectory = pCurrentDirectory;
        if(ppResultFile)
          *ppResultFile = pFile;
        return true;
      }
    }

    directory_t* pNextDirectory = 0;
    itrDirectory = std::find(pCurrentDirectory->m_vSubdirectories.begin(), pCurrentDirectory->m_vSubdirectories.end(), sPart);
    if(itrDirectory == pCurrentDirectory->m_vSubdirectories.end())
    {
      if(!bCreateDir)
      {
        if(bThrow)
          THROW_SIMPLE_3(L"Cannot find %s \'%s\' for \'%s\'", sRemainder.isEmpty() ? L"item" : L"directory", sPart.getCharacters(), sWhat.getCharacters());
        else
          return false;
      }
      pNextDirectory = new (std::nothrow) directory_t(sPart);
      if(bThrow)
        CHECK_ALLOCATION(pNextDirectory);
      else if(pNextDirectory == 0)
        return false;
      pNextDirectory->m_pParent = pCurrentDirectory;
      pCurrentDirectory->m_vSubdirectories.push_back(pNextDirectory);
    }
    else
    {
      pNextDirectory = *itrDirectory;
    }
    pCurrentDirectory = pNextDirectory;
  }

  if(ppResultDirectory)
    *ppResultDirectory = pCurrentDirectory;
  return true;
}

void MemoryFileStore::getCaps(file_store_caps_t& oCaps) const throw()
{
  oCaps = false;
  oCaps.bCanReadFiles = true;
  oCaps.bCanWriteFiles = true;
  oCaps.bCanDeleteFiles = true;
  oCaps.bCanOpenDirectories = true;
  oCaps.bCanCreateDirectories = true;
  oCaps.bCanDeleteDirectories = true;
}

IFile* MemoryFileStore::openFile(const RainString& sPath, eFileOpenMode eMode) throw(...)
{
  file_t* pFile = 0;
  if(!_find(sPath, 0, &pFile, eMode == FM_Write, eMode == FM_Write, true) || pFile == 0)
    THROW_SIMPLE_1(L"Cannot open file \'%s\' - it is a directory", sPath.getCharacters());
  if(eMode == FM_Write)
  {
    if(pFile->m_iOpenCount != 1)
      THROW_SIMPLE_1(L"Cannot open file \'%s\' for writing - it is already in use", sPath.getCharacters());
    return CHECK_ALLOCATION(new (std::nothrow) MemFileWriteAdaptor(pFile));
  }
  else
  {
    if(pFile->m_iOpenCount == 0)
      THROW_SIMPLE_1(L"Cannot open file \'%s\' for reading - it is being written to", sPath.getCharacters());
    return CHECK_ALLOCATION(new (std::nothrow) MemFileReadAdaptor(pFile));
  }
}

IFile* MemoryFileStore::openFileNoThrow(const RainString& sPath, eFileOpenMode eMode) throw()
{
  file_t* pFile = 0;
  if(!_find(sPath, 0, &pFile, eMode == FM_Write, eMode == FM_Write, false) || pFile == 0)
    return 0;
  if(eMode == FM_Write)
  {
    if(pFile->m_iOpenCount != 1)
      return 0;
    return new (std::nothrow) MemFileWriteAdaptor(pFile);
  }
  else
  {
    if(pFile->m_iOpenCount == 0)
      return 0;
    return new (std::nothrow) MemFileReadAdaptor(pFile);
  }
}

bool MemoryFileStore::doesFileExist(const RainString& sPath) throw()
{
  file_t* pFile = 0;
  return _find(sPath, 0, &pFile, false, false, false) && pFile;
}

void MemoryFileStore::deleteFile(const RainString& sPath) throw(...)
{
  directory_t* pDirectory = 0;
  file_t* pFile = 0;
  if(!_find(sPath, &pDirectory, &pFile, false, false, true) || pFile == 0)
    THROW_SIMPLE_1(L"Cannot delete file \'%s\' - it is a directory", sPath.getCharacters());

  std::vector<file_t*>::iterator itrFile = std::find(pDirectory->m_vFiles.begin(), pDirectory->m_vFiles.end(), pFile->m_sName);
  CHECK_ASSERT(itrFile != pDirectory->m_vFiles.end());
  pDirectory->m_vFiles.erase(itrFile);
  delete pFile;
}

bool MemoryFileStore::deleteFileNoThrow(const RainString& sPath) throw()
{
  directory_t* pDirectory = 0;
  file_t* pFile = 0;
  if(!_find(sPath, &pDirectory, &pFile, false, false, false) || pFile == 0)
    return false;

  std::vector<file_t*>::iterator itrFile = std::find(pDirectory->m_vFiles.begin(), pDirectory->m_vFiles.end(), pFile->m_sName);
  if(itrFile == pDirectory->m_vFiles.end())
    return false;

  pDirectory->m_vFiles.erase(itrFile);
  delete pFile;
  return true;
}

size_t MemoryFileStore::getEntryPointCount() throw()
{
  return m_vEntryPoints.size();
}

const RainString& MemoryFileStore::getEntryPointName(size_t iIndex) throw(...)
{
  CHECK_RANGE_LTMAX(static_cast<size_t>(0), iIndex, m_vEntryPoints.size());
  return m_vEntryPoints[iIndex]->m_sName;
}

IDirectory* MemoryFileStore::openDirectory(const RainString& sPath) throw(...)
{
  directory_t* pDirectory = 0;
  if(!_find(sPath, &pDirectory, 0, false, false, true) || pDirectory == 0)
    THROW_SIMPLE_1(L"Cannot open directory \'%s\' - it is a file", sPath.getCharacters());
  return CHECK_ALLOCATION(new (std::nothrow) MemDirectoryAdaptor(pDirectory, this, sPath));
}

IDirectory* MemoryFileStore::openDirectoryNoThrow(const RainString& sPath) throw()
{
  directory_t* pDirectory = 0;
  if(!_find(sPath, &pDirectory, 0, false, false, false) || pDirectory == 0)
    return 0;
  return new (std::nothrow) MemDirectoryAdaptor(pDirectory, this, sPath);
}

bool MemoryFileStore::doesDirectoryExist(const RainString& sPath) throw()
{
  directory_t* pDirectory = 0;
  return _find(sPath, &pDirectory, 0, false, false, false) && pDirectory;
}

void MemoryFileStore::createDirectory(const RainString& sPath) throw(...)
{
  if(_find(sPath, 0, 0, false, false, false))
    THROW_SIMPLE_1(L"Cannot create directory \'%s\' - it already exists", sPath.getCharacters());
  _find(sPath, 0, 0, false, true, true);
}

bool MemoryFileStore::createDirectoryNoThrow(const RainString& sPath) throw()
{
  if(_find(sPath, 0, 0, false, false, false))
    return false;
  directory_t* pDirectory = 0;
  return _find(sPath, &pDirectory, 0, false, true, false) && pDirectory;
}

void MemoryFileStore::deleteDirectory(const RainString& sPath) throw(...)
{
  directory_t* pDirectory = 0;
  _find(sPath, &pDirectory, 0, false, false, true);
  if(pDirectory->m_pParent == 0)
    THROW_SIMPLE_1(L"Cannot delete root directory \'%s\'", sPath.getCharacters());
  pDirectory->m_pParent->m_vSubdirectories.erase(std::find(pDirectory->m_pParent->m_vSubdirectories.begin(), pDirectory->m_pParent->m_vSubdirectories.end(), pDirectory));
  delete pDirectory;
}

bool MemoryFileStore::deleteDirectoryNoThrow(const RainString& sPath) throw()
{
  directory_t* pDirectory = 0;
  if(!_find(sPath, &pDirectory, 0, false, false, false) || !pDirectory || pDirectory->m_pParent == 0)
    return false;
  pDirectory->m_pParent->m_vSubdirectories.erase(std::find(pDirectory->m_pParent->m_vSubdirectories.begin(), pDirectory->m_pParent->m_vSubdirectories.end(), pDirectory));
  delete pDirectory;
  return true;
}
