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
#include "filestore_adaptors.h"
#include "exception.h"

class ReadOnlyFileStoreDirectoryAdaptor : public IDirectory
{
public:
  ReadOnlyFileStoreDirectoryAdaptor(IDirectory *pDirectory, ReadOnlyFileStoreAdaptor *pAdaptor)
    : m_pDirectory(pDirectory), m_pAdaptor(pAdaptor)
  {
  }

  ~ReadOnlyFileStoreDirectoryAdaptor()
  {
    delete m_pDirectory;
  }

  virtual size_t getItemCount() throw()
  {
    return m_pDirectory->getItemCount();
  }

  virtual void getItemDetails(size_t iIndex, directory_item_t& oDetails) throw(...)
  {
    return m_pDirectory->getItemDetails(iIndex, oDetails);
  }

  virtual const RainString& getPath() throw()
  {
    return m_pDirectory->getPath();
  }

  virtual IFileStore* getStore() throw()
  {
    return m_pAdaptor;
  }

  virtual IFile* openFile(size_t iIndex, eFileOpenMode eMode) throw(...)
  {
    if(eMode == FM_Write)
      THROW_SIMPLE_1(L"Cannot open index %lu for writing - it is read-only", static_cast<unsigned long>(iIndex));
    return m_pDirectory->openFile(iIndex, eMode);
  }

  virtual IFile* openFileNoThrow(size_t iIndex, eFileOpenMode eMode) throw()
  {
    if(eMode == FM_Write)
      return 0;
    return m_pDirectory->openFileNoThrow(iIndex, eMode);
  }

  virtual IDirectory* openDirectory(size_t iIndex) throw(...)
  {
    IDirectory* pDirectory = m_pDirectory->openDirectory(iIndex);
    IDirectory* pWrapped = new (std::nothrow) ReadOnlyFileStoreDirectoryAdaptor(pDirectory, m_pAdaptor);
    if(pWrapped == 0)
    {
      delete pDirectory;
      CHECK_ALLOCATION(pWrapped);
    }
    return pWrapped;
  }

  virtual IDirectory* openDirectoryNoThrow(size_t iIndex) throw()
  {
    IDirectory* pDirectory = m_pDirectory->openDirectoryNoThrow(iIndex);
    IDirectory* pWrapped = new (std::nothrow) ReadOnlyFileStoreDirectoryAdaptor(pDirectory, m_pAdaptor);
    if(pWrapped == 0)
    {
      delete pDirectory;
      return 0;
    }
    return pWrapped;
  }

protected:
  IDirectory *m_pDirectory;
  ReadOnlyFileStoreAdaptor *m_pAdaptor;
};

ReadOnlyFileStoreAdaptor::ReadOnlyFileStoreAdaptor(IFileStore *pFileStore, bool bTakeOwnership) throw()
  : m_pFileStore(pFileStore), m_bOwnsFileStore(bTakeOwnership)
{
}

ReadOnlyFileStoreAdaptor::~ReadOnlyFileStoreAdaptor() throw()
{
  if(m_bOwnsFileStore)
    delete m_pFileStore;
}

void ReadOnlyFileStoreAdaptor::getCaps(file_store_caps_t& oCaps) const throw()
{
  m_pFileStore->getCaps(oCaps);
  oCaps.bCanWriteFiles = false;
  oCaps.bCanDeleteFiles = false;
}

IFile* ReadOnlyFileStoreAdaptor::openFile(const RainString& sPath, eFileOpenMode eMode) throw(...)
{
  if(eMode == FM_Write)
    THROW_SIMPLE_1(L"Cannot write to file \'%s\' - it is read-only", sPath.getCharacters());
  else
    return m_pFileStore->openFile(sPath, eMode);
}

IFile* ReadOnlyFileStoreAdaptor::openFileNoThrow(const RainString& sPath, eFileOpenMode eMode) throw()
{
  if(eMode == FM_Write)
    return 0;
  else
    return m_pFileStore->openFileNoThrow(sPath, eMode);
}

void ReadOnlyFileStoreAdaptor::deleteFile(const RainString& sPath) throw(...)
{
  THROW_SIMPLE_1(L"Cannot delete file \'%s\' - it is read-only", sPath.getCharacters());
}

bool ReadOnlyFileStoreAdaptor::deleteFileNoThrow(const RainString& sPath) throw()
{
  return false;
}

size_t ReadOnlyFileStoreAdaptor::getEntryPointCount() throw()
{
  return m_pFileStore->getEntryPointCount();
}

const RainString& ReadOnlyFileStoreAdaptor::getEntryPointName(size_t iIndex) throw(...)
{
  return m_pFileStore->getEntryPointName(iIndex);
}

IDirectory* ReadOnlyFileStoreAdaptor::openDirectory(const RainString& sPath) throw(...)
{
  IDirectory* pDirectory = m_pFileStore->openDirectory(sPath);
  IDirectory* pWrapped = new (std::nothrow) ReadOnlyFileStoreDirectoryAdaptor(pDirectory, this);
  if(pWrapped == 0)
  {
    delete pDirectory;
    CHECK_ALLOCATION(pWrapped);
  }
  return pWrapped;
}

IDirectory* ReadOnlyFileStoreAdaptor::openDirectoryNoThrow(const RainString& sPath) throw()
{
  IDirectory* pDirectory = m_pFileStore->openDirectoryNoThrow(sPath);
  IDirectory* pWrapped = new (std::nothrow) ReadOnlyFileStoreDirectoryAdaptor(pDirectory, this);
  if(pWrapped == 0)
  {
    delete pDirectory;
    return 0;
  }
  return pWrapped;
}
