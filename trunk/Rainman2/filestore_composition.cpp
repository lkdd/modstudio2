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
#include "filestore_composition.h"
#include "exception.h"
#include "hash.h"
#include <algorithm>
#include <unordered_map>
#include <stack>

FileStoreComposition::FileStoreComposition() throw()
{
}

FileStoreComposition::~FileStoreComposition() throw()
{
  for(std::vector<file_store_info_t*>::iterator itr = m_vFileStores.begin(); itr != m_vFileStores.end(); ++itr)
  {
    if((*itr)->m_bOwnsPointer)
      delete (*itr)->m_pStore;
    delete *itr;
  }
}

bool FileStoreComposition::_file_store_priority_sort(file_store_info_t* a, file_store_info_t* b)
{
  return a->m_iPriority > b->m_iPriority;
}

void FileStoreComposition::addFileStore(IFileStore *pStore, const RainString &sPrefix, int iPriority, bool bTakeOwnership) throw(...)
{
  for(std::vector<file_store_info_t*>::iterator itr = m_vFileStores.begin(); itr != m_vFileStores.end(); ++itr)
  {
    if((**itr).m_pStore == pStore && (**itr).m_sPrefix == sPrefix)
      THROW_SIMPLE(L"Filestore is already in the composition");
  }

  file_store_info_t *pInfo = CHECK_ALLOCATION(new (std::nothrow) file_store_info_t);
  pInfo->m_pStore = pStore;
  if(sPrefix.isEmpty() || sPrefix.suffix(1) == "\\")
    pInfo->m_sPrefix = sPrefix;
  else
    pInfo->m_sPrefix = sPrefix + L"\\";
  pInfo->m_iPriority = iPriority;
  pStore->getCaps(pInfo->m_oCaps);
  pInfo->m_bOwnsPointer = bTakeOwnership;
  pInfo->m_bEnabled = true;
  m_vFileStores.push_back(pInfo);
  std::sort(m_vFileStores.begin(), m_vFileStores.end(), _file_store_priority_sort);

  try
  {
    reenumerateEntryPoints();
  }
  CATCH_THROW_SIMPLE(L"Cannot enumerate entry points with new store", {
    delete pInfo;
    m_vFileStores.erase(std::find(m_vFileStores.begin(), m_vFileStores.end(), pInfo));
  });
}

size_t FileStoreComposition::enableFileStore(IFileStore *pStore, bool bEnable) throw(...)
{
  size_t iCount = 0;
  for(std::vector<file_store_info_t*>::iterator itr = m_vFileStores.begin(); itr != m_vFileStores.end(); ++itr)
  {
    if((**itr).m_pStore == pStore)
    {
      if((**itr).m_bEnabled != bEnable)
        (**itr).m_bEnabled = bEnable, ++iCount;
    }
  }
  return iCount;
}

size_t FileStoreComposition::disableFileStore(IFileStore *pStore, bool bDisable) throw(...)
{
  return enableFileStore(pStore, !bDisable);
}

void FileStoreComposition::reenumerateEntryPoints() throw(...)
{
  std::tr1::unordered_map<unsigned long, bool> mapEntryPoints;
  m_vEntryPoints.clear();
  for(std::vector<file_store_info_t*>::iterator itr = m_vFileStores.begin(); itr != m_vFileStores.end(); ++itr)
  {
    if(!(*itr)->m_bEnabled)
      continue;
    if((*itr)->m_sPrefix.isEmpty())
    {
      size_t iEntryPointCount = (**itr).m_pStore->getEntryPointCount();
      for(size_t i = 0; i < iEntryPointCount; ++i)
      {
        const RainString& sName = (**itr).m_pStore->getEntryPointName(i);
        unsigned long iHash = CRCCaselessHashSimple(sName.getCharacters(), sName.length() * sizeof(RainChar));
        if(mapEntryPoints[iHash] == false)
        {
          m_vEntryPoints.push_back(sName);
          mapEntryPoints[iHash] = true;
        }
      }
    }
    else
    {
      std::auto_ptr<IDirectory> pDirectory((**itr).m_pStore->openDirectory((**itr).m_sPrefix));
      for(IDirectory::iterator itrDir = pDirectory->begin(); itrDir != pDirectory->end(); ++itrDir)
      {
        if(itrDir->isDirectory())
        {
          const RainString& sName = itrDir->name();
          unsigned long iHash = CRCCaselessHashSimple(sName.getCharacters(), sName.length() * sizeof(RainChar));
          if(mapEntryPoints[iHash] == false)
          {
            m_vEntryPoints.push_back(sName);
            mapEntryPoints[iHash] = true;
          }
        }
      }
    }
  }
}

void FileStoreComposition::getCaps(file_store_caps_t& oCaps) const throw()
{
  oCaps = false;
  for(std::vector<file_store_info_t*>::const_iterator itr = m_vFileStores.begin(); itr != m_vFileStores.end(); ++itr)
  {
    if(!(*itr)->m_bEnabled)
      continue;

    file_store_caps_t oThisCaps;
    (**itr).m_pStore->getCaps(oThisCaps);
    oCaps.bCanReadFiles = oCaps.bCanReadFiles || oThisCaps.bCanReadFiles;
    oCaps.bCanWriteFiles = oCaps.bCanWriteFiles || oThisCaps.bCanWriteFiles;
    oCaps.bCanDeleteFiles = oCaps.bCanDeleteFiles || oThisCaps.bCanDeleteFiles;
    oCaps.bCanOpenDirectories = oCaps.bCanOpenDirectories || oThisCaps.bCanOpenDirectories;
    oCaps.bCanCreateDirectories = oCaps.bCanCreateDirectories || oThisCaps.bCanCreateDirectories;
    oCaps.bCanDeleteDirectories = oCaps.bCanDeleteDirectories || oThisCaps.bCanDeleteDirectories;
  }
}

IFile* FileStoreComposition::openFile(const RainString& sPath, eFileOpenMode eMode) throw(...)
{
  if(eMode == FM_Read)
  {
    try
    {
      for(std::vector<file_store_info_t*>::iterator itr = m_vFileStores.begin(); itr != m_vFileStores.end(); ++itr)
      {
        if(!(*itr)->m_bEnabled)
          continue;

        if((**itr).m_oCaps.bCanReadFiles)
        {
          RainString sFullPath = (**itr).m_sPrefix + sPath;
          if((**itr).m_pStore->doesFileExist(sFullPath))
            return (**itr).m_pStore->openFile(sPath, eMode);
        }
      }
      THROW_SIMPLE_1(L"\'%s\' does not exist in any file stores", sPath.getCharacters());
    }
    CATCH_THROW_SIMPLE_1(L"Error opening file \'%s\' for reading", sPath.getCharacters(), {});
  }
  else
  {
    try
    {
      for(std::vector<file_store_info_t*>::iterator itr = m_vFileStores.begin(); itr != m_vFileStores.end(); ++itr)
      {
        if(!(*itr)->m_bEnabled)
          continue;

        RainString sFullPath = (**itr).m_sPrefix + sPath;
        if((**itr).m_pStore->doesFileExist(sFullPath))
        {
          return (**itr).m_pStore->openFile(sPath, eMode);
        }
        else if((**itr).m_oCaps.bCanWriteFiles)
        {
          RainString sParent = sPath.beforeLast('\\');
          RainString sFullParent = (**itr).m_sPrefix + sParent;
          (**itr).ensureDirectory(sFullParent, true);
          return (**itr).m_pStore->openFile(sPath, eMode);
        }
      }
      THROW_SIMPLE(L"No file stores with write capability to create the file in");
    }
    CATCH_THROW_SIMPLE_1(L"Error opening file \'%s\' for writing", sPath.getCharacters(), {});
  }
}

IFile* FileStoreComposition::openFileNoThrow(const RainString& sPath, eFileOpenMode eMode) throw()
{
  if(eMode == FM_Read)
  {
    for(std::vector<file_store_info_t*>::iterator itr = m_vFileStores.begin(); itr != m_vFileStores.end(); ++itr)
    {
      if(!(*itr)->m_bEnabled)
        continue;

      if((**itr).m_oCaps.bCanReadFiles)
      {
        RainString sFullPath = (**itr).m_sPrefix + sPath;
        if((**itr).m_pStore->doesFileExist(sFullPath))
        {
          IFile* pFile = (**itr).m_pStore->openFileNoThrow(sFullPath, eMode);
          if(pFile)
            return pFile;
        }
      }
    }
  }
  else
  {
    for(std::vector<file_store_info_t*>::iterator itr = m_vFileStores.begin(); itr != m_vFileStores.end(); ++itr)
    {
      if(!(*itr)->m_bEnabled)
        continue;

      RainString sFullPath = (**itr).m_sPrefix + sPath;
      if((**itr).m_pStore->doesFileExist(sFullPath))
      {
        return (**itr).m_pStore->openFileNoThrow(sPath, eMode);
      }
      else if((**itr).m_oCaps.bCanWriteFiles)
      {
        RainString sParent = sPath.beforeLast('\\');
        RainString sFullParent = (**itr).m_sPrefix + sParent;
        if((**itr).ensureDirectory(sFullParent, false))
          return (**itr).m_pStore->openFile(sPath, eMode);
        return 0; // while we could continue through the list, to ensure consistent behaviour
                  // with openFile(), we abort after the first writable file store
      }
    }
  }
  return 0;
}

bool FileStoreComposition::file_store_info_t::ensureDirectory(RainString sFullPath, bool bThrow)
{
  if(m_pStore->doesDirectoryExist(sFullPath))
    return true;
  RainString sParent = sFullPath.beforeLast('\\');
  try
  {
    if(!sParent.isEmpty())
      ensureDirectory(sParent, bThrow);
    if(bThrow)
      return m_pStore->createDirectory(sFullPath), true;
    else
      return m_pStore->createDirectoryNoThrow(sFullPath);
  }
  CATCH_THROW_SIMPLE_1(L"Cannot create directory \'%s\'", sFullPath.getCharacters(), {});
}

bool FileStoreComposition::doesFileExist(const RainString& sPath) throw()
{
  for(std::vector<file_store_info_t*>::iterator itr = m_vFileStores.begin(); itr != m_vFileStores.end(); ++itr)
  {
    if((*itr)->m_bEnabled && (**itr).m_pStore->doesFileExist((**itr).m_sPrefix + sPath))
      return true;
  }
  return false;
}

void FileStoreComposition::deleteFile(const RainString& sPath) throw(...)
{
  try
  {
    // Optimisations for simple cases
    switch(m_vFileStores.size())
    {
    case 1:
      if(m_vFileStores[0]->m_bEnabled)
      {
        m_vFileStores[0]->m_pStore->deleteFile(m_vFileStores[0]->m_sPrefix + sPath);
        return;
      }
      // fall-through

    case 0:
      THROW_SIMPLE_1(L"Cannot delete non-existant file \'%s\'", sPath.getCharacters());
      break;

    default:
      break;
    }

    // 2 or more filestores in the composition; test each for file existance and delete capability
    std::vector<file_store_info_t*> vToDeleteFrom;
    for(std::vector<file_store_info_t*>::iterator itr = m_vFileStores.begin(); itr != m_vFileStores.end(); ++itr)
    {
      if((**itr).m_bEnabled && (**itr).m_pStore->doesFileExist((**itr).m_sPrefix + sPath))
      {
        if((*itr)->m_oCaps.bCanDeleteFiles == false)
          THROW_SIMPLE_1(L"\'%s\' exists in one or more file stores without delete capability", sPath.getCharacters());
        vToDeleteFrom.push_back(*itr);
      }
    }
    if(vToDeleteFrom.empty())
      THROW_SIMPLE_1(L"\'%s\' does not exist in any file stores", sPath.getCharacters());
    for(std::vector<file_store_info_t*>::iterator itr = vToDeleteFrom.begin(); itr != vToDeleteFrom.end(); ++itr)
    {
      (**itr).m_pStore->deleteFile((**itr).m_sPrefix + sPath);
    }
  }
  CATCH_THROW_SIMPLE_1(L"Cannot delete file \'%s\'", sPath.getCharacters(), {});
}

bool FileStoreComposition::deleteFileNoThrow(const RainString& sPath) throw()
{
  try
  {
    deleteFile(sPath);
    return true;
  }
  catch(RainException *pE)
  {
    delete pE;
    return false;
  }
}

size_t FileStoreComposition::getEntryPointCount() throw()
{
  return m_vEntryPoints.size();
}

const RainString& FileStoreComposition::getEntryPointName(size_t iIndex) throw(...)
{
  CHECK_RANGE_LTMAX(static_cast<size_t>(0), iIndex, getEntryPointCount());
  return m_vEntryPoints[iIndex];
}

static bool directories_first_alphabetical_predicate(const auto_directory_item& a, const auto_directory_item& b)
{
  if(a.isDirectory() != b.isDirectory())
    return a.isDirectory();
  return a.name().compareCaseless(b.name()) < 0;
}

class FileStoreCompositionDirectory : public IDirectory
{
public:
  FileStoreCompositionDirectory(const RainString &sPath, FileStoreComposition* pStore) throw()
    : m_sPath(sPath), m_pStore(pStore)
  {
  }

  virtual ~FileStoreCompositionDirectory()
  {
    for(std::vector<IDirectory*>::iterator itr = m_vDirectories.begin(); itr != m_vDirectories.end(); ++itr)
      delete *itr;
  }

  void init() throw(...)
  {
    if(m_sPath.suffix(1) != L"\\")
      m_sPath += '\\';

    std::tr1::unordered_map<unsigned long, bool> mapContents;
    for(std::vector<FileStoreComposition::file_store_info_t*>::iterator itr = m_pStore->m_vFileStores.begin(); itr != m_pStore->m_vFileStores.end(); ++itr)
    {
      if(!(*itr)->m_bEnabled)
        continue;

      RainString sFullPath = (*itr)->m_sPrefix + m_sPath;
      if(!(*itr)->m_pStore->doesDirectoryExist(sFullPath))
        continue;
      IDirectory *pDirectory = (*itr)->m_pStore->openDirectory(sFullPath);
      m_vDirectories.push_back(pDirectory);
      for(IDirectory::iterator itrDir = pDirectory->begin(); itrDir != pDirectory->end(); ++itrDir)
      {
        const RainString& sName = itrDir->name();
        unsigned long iHash = CRCCaselessHashSimple(sName.getCharacters(), sName.length() * sizeof(RainChar));
        if(mapContents[iHash] == false)
        {
          mapContents[iHash] = true;
          m_vItems.push_back(*itrDir);
        }
      }
    }
    std::sort(m_vItems.begin(), m_vItems.end(), directories_first_alphabetical_predicate);
  }

  size_t getItemCount() throw()
  {
    return m_vItems.size();
  }

  void getItemDetails(size_t iIndex, directory_item_t& oDetails) throw(...)
  {
    CHECK_RANGE_LTMAX(static_cast<size_t>(0), iIndex, getItemCount());

    auto_directory_item& oItem = m_vItems[iIndex];
    if(oDetails.oFields.name)
      oDetails.sName = oItem.name();
    if(oDetails.oFields.dir)
      oDetails.bIsDirectory = oItem.isDirectory();
    if(oDetails.oFields.size)
      oDetails.iSize = oItem.size();
    if(oDetails.oFields.time)
      oDetails.iTimestamp = oItem.timestamp();
  }

  const RainString& getPath() throw()
  {
    return m_sPath;
  }

  IFileStore* getStore() throw()
  {
    return m_pStore;
  }

protected:
  std::vector<auto_directory_item> m_vItems;
  std::vector<IDirectory*> m_vDirectories;
  RainString m_sPath;
  FileStoreComposition* m_pStore;
};

IDirectory* FileStoreComposition::openDirectory(const RainString& sPath) throw(...)
{
  FileStoreCompositionDirectory *pDirectory = CHECK_ALLOCATION(new (std::nothrow) FileStoreCompositionDirectory(sPath, this));
  pDirectory->init();
  return pDirectory;
}

IDirectory* FileStoreComposition::openDirectoryNoThrow(const RainString& sPath) throw()
{
  FileStoreCompositionDirectory *pDirectory = new (std::nothrow) FileStoreCompositionDirectory(sPath, this);
  if(pDirectory != 0)
  {
    try
    {
      pDirectory->init();
    }
    catch(RainException *pE)
    {
      delete pE;
      return 0;
    }
  }
  return pDirectory;
}

bool FileStoreComposition::doesDirectoryExist(const RainString& sPath) throw()
{
  for(std::vector<file_store_info_t*>::iterator itr = m_vFileStores.begin(); itr != m_vFileStores.end(); ++itr)
  {
    if((**itr).m_bEnabled && (**itr).m_pStore->doesDirectoryExist((**itr).m_sPrefix + sPath))
      return true;
  }
  return false;
}

void FileStoreComposition::createDirectory(const RainString& sPath) throw(...)
{
  if(doesDirectoryExist(sPath))
    THROW_SIMPLE_1(L"Cannot create directory \'%s\' - it already exists", sPath.getCharacters());
  try
  {
    for(std::vector<file_store_info_t*>::iterator itr = m_vFileStores.begin(); itr != m_vFileStores.end(); ++itr)
    {
      if((**itr).m_bEnabled && (**itr).m_oCaps.bCanCreateDirectories)
      {
        const RainString& sPrefix = (**itr).m_sPrefix;
        IFileStore* pStore = (**itr).m_pStore;
        std::stack<RainString> stkPaths;
        stkPaths.push(sPath);
        while(!stkPaths.empty())
        {
          RainString sParent = stkPaths.top().beforeLast('\\');
          if(pStore->doesDirectoryExist(sPrefix + sParent))
          {
            pStore->createDirectory(sPrefix + stkPaths.top());
            stkPaths.pop();
          }
          else
          {
            if(sParent == stkPaths.top())
              THROW_SIMPLE(L"Cannot create ancestors");
            stkPaths.push(sParent);
          }
        }
        return;
      }
    }
    THROW_SIMPLE(L"No file stores can create directories");
  }
  CATCH_THROW_SIMPLE_1(L"Cannot create directory \'%s\'", sPath.getCharacters(), {});
}

bool FileStoreComposition::createDirectoryNoThrow(const RainString& sPath) throw()
{
  try
  {
    createDirectory(sPath);
    return true;
  }
  catch(RainException *pE)
  {
    delete pE;
    return false;
  }
}

void FileStoreComposition::deleteDirectory(const RainString& sPath) throw(...)
{
  try
  {
    // Optimisations for simple cases
    switch(m_vFileStores.size())
    {
    case 1:
      if(m_vFileStores[0]->m_bEnabled)
      {
        m_vFileStores[0]->m_pStore->deleteDirectory(m_vFileStores[0]->m_sPrefix + sPath);
        return;
      }
      // fall-through

    case 0:
      THROW_SIMPLE_1(L"Cannot delete non-existant directory \'%s\'", sPath.getCharacters());
      break;

    default:
      break;
    }

    // 2 or more filestores in the composition; test each for file existance and delete capability
    std::vector<file_store_info_t*> vToDeleteFrom;
    for(std::vector<file_store_info_t*>::iterator itr = m_vFileStores.begin(); itr != m_vFileStores.end(); ++itr)
    {
      if((**itr).m_bEnabled && (**itr).m_pStore->doesDirectoryExist((**itr).m_sPrefix + sPath))
      {
        if((*itr)->m_oCaps.bCanDeleteDirectories == false)
          THROW_SIMPLE_1(L"\'%s\' exists in one or more file stores without delete capability", sPath.getCharacters());
        vToDeleteFrom.push_back(*itr);
      }
    }
    if(vToDeleteFrom.empty())
      THROW_SIMPLE_1(L"\'%s\' does not exist in any file stores", sPath.getCharacters());
    for(std::vector<file_store_info_t*>::iterator itr = vToDeleteFrom.begin(); itr != vToDeleteFrom.end(); ++itr)
    {
      (**itr).m_pStore->deleteDirectory((**itr).m_sPrefix + sPath);
    }
  }
  CATCH_THROW_SIMPLE_1(L"Cannot delete directory \'%s\'", sPath.getCharacters(), {});
}

bool FileStoreComposition::deleteDirectoryNoThrow(const RainString& sPath) throw()
{
  try
  {
    deleteDirectory(sPath);
    return true;
  }
  catch(RainException *pE)
  {
    delete pE;
    return false;
  }
}
