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
#include "file.h"
#include "exception.h"
#include "new_trace.h"
#include <direct.h>
#include <stdio.h>
#include <windows.h>
#include <time.h>
#include <errno.h>
#ifdef RAINMAN2_USE_LUA
extern "C" {
#include <lua.h>
}
#endif
#pragma warning(disable: 4996)

FileSystemStore g_oRainFSO;

IFile::~IFile() {}

#ifdef RAINMAN2_USE_LUA
struct IFile_load_load_t
{
  IFile_load_load_t(IFile* pFile) : pFile(pFile) {}

  static const int buffer_length = 4096;
  IFile* pFile;
  char aBuffer[buffer_length];

  static const char* reader(lua_State *L, void *data, size_t *size)
  {
    IFile_load_load_t *pThis = reinterpret_cast<IFile_load_load_t*>(data);
    if(pThis->pFile)
    {
      *size = pThis->pFile->readArrayNoThrow(pThis->aBuffer, buffer_length);
      return pThis->aBuffer;
    }
    *size = 0;
    return NULL;
  }
};

int IFile::lua_load(lua_State *L, const char* sChunkName) throw()
{
  IFile_load_load_t oLoadStruct(this);
  return ::lua_load(L, IFile_load_load_t::reader, reinterpret_cast<void*>(&oLoadStruct), sChunkName);
}
#endif

IDirectory::~IDirectory() {}
IFileStore::~IFileStore() {}

filesize_t operator+ (const filesize_t& a, const filesize_t& b)
{
  filesize_t r;
  r.iLower = a.iLower + b.iLower;
  r.iUpper = a.iUpper + b.iUpper;
  if(r.iLower < a.iLower)
    ++r.iUpper;
  return r;
}

directory_item_t::field_list_t& directory_item_t::field_list_t::operator= (bool value) throw()
{
  dir = name = size = time = value;
  return *this;
}

auto_directory_item::auto_directory_item() throw()
  : m_pDirectory(0), m_iIndex(0)
{
  m_oFieldsPresent = false;
}

auto_directory_item::auto_directory_item(const auto_directory_item& other) throw()
  : m_pDirectory(other.m_pDirectory), m_iIndex(other.m_iIndex)
  , m_oItem(other.m_oItem), m_oFieldsPresent(other.m_oFieldsPresent)
{
}

auto_directory_item::auto_directory_item(IDirectory *pDirectory, size_t iIndex) throw()
  : m_pDirectory(pDirectory), m_iIndex(iIndex)
{
  m_oFieldsPresent = false;
}

auto_directory_item& auto_directory_item::operator = (const auto_directory_item& other) throw()
{
  m_pDirectory = other.m_pDirectory;
  m_iIndex = other.m_iIndex;
  m_oItem = other.m_oItem;
  m_oFieldsPresent = other.m_oFieldsPresent;
  return *this;
}

bool auto_directory_item::operator == (const auto_directory_item& other) const throw()
{
  return m_pDirectory == other.m_pDirectory && m_iIndex == other.m_iIndex;
}

bool auto_directory_item::operator != (const auto_directory_item& other) const throw()
{
  return m_pDirectory != other.m_pDirectory || m_iIndex != other.m_iIndex;
}

void auto_directory_item::setItem(IDirectory *pDirectory, size_t iIndex) throw()
{
  m_pDirectory = pDirectory;
  m_iIndex = iIndex;
  m_oFieldsPresent = false;
}

bool auto_directory_item::isValid() const throw()
{
  return m_pDirectory && m_iIndex < m_pDirectory->getItemCount();
}

IDirectory* auto_directory_item::getDirectory() const throw()
{
  return m_pDirectory;
}

size_t auto_directory_item::getIndex() const throw()
{
  return m_iIndex;
}

RainString auto_directory_item::name() const throw(...)
{
  if(!m_oFieldsPresent.name)
  {
    m_oItem.oFields = false;
    m_oItem.oFields.name = m_oFieldsPresent.name = true;
    m_pDirectory->getItemDetails(m_iIndex, m_oItem);
  }
  return m_oItem.sName;
}

bool auto_directory_item::isDirectory() const throw(...)
{
  if(!m_oFieldsPresent.dir)
  {
    m_oItem.oFields = false;
    m_oItem.oFields.dir = m_oFieldsPresent.dir = true;
    m_pDirectory->getItemDetails(m_iIndex, m_oItem);
  }
  return m_oItem.bIsDirectory;
}

filesize_t auto_directory_item::size() const throw(...)
{
  if(!m_oFieldsPresent.size)
  {
    m_oItem.oFields = false;
    m_oItem.oFields.size = m_oFieldsPresent.size = true;
    m_pDirectory->getItemDetails(m_iIndex, m_oItem);
  }
  return m_oItem.iSize;
}

filetime_t auto_directory_item::timestamp() const throw(...)
{
  if(!m_oFieldsPresent.time)
  {
    m_oItem.oFields = false;
    m_oItem.oFields.time = m_oFieldsPresent.time = true;
    m_pDirectory->getItemDetails(m_iIndex, m_oItem);
  }
  return m_oItem.iTimestamp;
}

IDirectory* auto_directory_item::open() const throw(...)
{
  return m_pDirectory->openDirectory(m_iIndex);
}

IFile* auto_directory_item::open(eFileOpenMode eMode) const throw(...)
{
  return m_pDirectory->openFile(m_iIndex, eMode);
}

void auto_directory_item::pump(IFile *pSink) const throw(...)
{
  return m_pDirectory->pumpFile(m_iIndex, pSink);
}

IDirectory* auto_directory_item::openNoThrow() const throw()
{
  return m_pDirectory->openDirectoryNoThrow(m_iIndex);
}

IFile* auto_directory_item::openNoThrow(eFileOpenMode eMode) const throw()
{
  return m_pDirectory->openFileNoThrow(m_iIndex, eMode);
}

IDirectory::iterator::iterator() throw()
  : m_item()
{}

IDirectory::iterator::iterator(const iterator& other) throw()
  : m_item(other.m_item)
{}

IDirectory::iterator::iterator(IDirectory *directory, size_t index) throw()
  : m_item(directory, index)
{}

IDirectory::iterator& IDirectory::iterator::operator = (const IDirectory::iterator& other) throw()
{
  m_item = other.m_item;
  return *this;
}

IDirectory::iterator& IDirectory::iterator::operator ++ () throw()
{
  m_item.setItem(m_item.getDirectory(), m_item.getIndex() + 1);
  return *this;
}

IDirectory::iterator& IDirectory::iterator::operator -- () throw()
{
  m_item.setItem(m_item.getDirectory(), m_item.getIndex() - 1);
  return *this;
}

bool IDirectory::iterator::operator == (const IDirectory::iterator& other) const throw()
{
  return m_item == other.m_item;
}

bool IDirectory::iterator::operator != (const IDirectory::iterator& other) const throw()
{
  return m_item != other.m_item;
}

IDirectory::iterator::reference IDirectory::iterator::operator * () const throw()
{
  return m_item;
}

IDirectory::iterator::pointer IDirectory::iterator::operator -> () const throw()
{
  return &m_item;
}

IDirectory::iterator IDirectory::begin()
{
  return iterator(this, 0);
}

IDirectory::iterator IDirectory::end()
{
  return iterator(this, getItemCount());
}

IFile* IDirectory::openFile(size_t iIndex, eFileOpenMode eMode) throw(...)
{
  directory_item_t oDetails;
  oDetails.oFields = false;
  oDetails.oFields.name = true;
  getItemDetails(iIndex, oDetails);
  return getStore()->openFile(getPath() + oDetails.sName, eMode);
}

void IDirectory::pumpFile(size_t iIndex, IFile *pSink) throw(...)
{
  directory_item_t oDetails;
  oDetails.oFields = false;
  oDetails.oFields.name = true;
  getItemDetails(iIndex, oDetails);
  getStore()->pumpFile(getPath() + oDetails.sName, pSink);
}

IFile* IDirectory::openFileNoThrow(size_t iIndex, eFileOpenMode eMode) throw()
{
  directory_item_t oDetails;
  oDetails.oFields = false;
  oDetails.oFields.name = true;
  try
  {
    getItemDetails(iIndex, oDetails);
    return getStore()->openFileNoThrow(getPath() + oDetails.sName, eMode);
  }
  catch(RainException *e)
  {
    delete e;
    return false;
  }
}

IDirectory* IDirectory::openDirectory(size_t iIndex) throw(...)
{
  directory_item_t oDetails;
  oDetails.oFields = false;
  oDetails.oFields.name = true;
  getItemDetails(iIndex, oDetails);
  return getStore()->openDirectory(getPath() + oDetails.sName);
}

IDirectory* IDirectory::openDirectoryNoThrow(size_t iIndex) throw()
{
  directory_item_t oDetails;
  oDetails.oFields = false;
  oDetails.oFields.name = true;
  try
  {
    getItemDetails(iIndex, oDetails);
    return getStore()->openDirectoryNoThrow(getPath() + oDetails.sName);
  }
  catch(RainException *e)
  {
    delete e;
    return false;
  }
}

file_store_caps_t::file_store_caps_t() throw()
{
  this->operator=(false);
}

file_store_caps_t& file_store_caps_t::operator= (bool bValue) throw()
{
  bCanReadFiles = bCanWriteFiles = bCanDeleteFiles = bCanOpenDirectories = bValue;
  return *this;
}

void IFileStore::pumpFile(const RainString& sPath, IFile* pSink) throw(...)
{
  IFile *pFile = 0;
  try
  {
    pFile = openFile(sPath, FM_Read);

    static const size_t BUFFER_SIZE = 8192;
    unsigned char aBuffer[BUFFER_SIZE];
    while(true)
    {
      size_t iNumBytes = pFile->readArrayNoThrow(aBuffer, BUFFER_SIZE);
      if(iNumBytes == 0)
        break;
      pSink->writeArray(aBuffer, iNumBytes);
    }

    delete pFile;
  }
  CATCH_THROW_SIMPLE_(delete pFile, L"Cannot pump file \'%s\'", sPath.getCharacters());
}

FileSystemStore::FileSystemStore() throw()
{
  m_bKnowEntryPoints = false;
}

FileSystemStore::~FileSystemStore() throw()
{
}

void FileSystemStore::getCaps(file_store_caps_t& oCaps) const throw()
{
  oCaps = false;
  oCaps.bCanReadFiles = true;
  oCaps.bCanWriteFiles = true;
  oCaps.bCanDeleteFiles = true;
  oCaps.bCanOpenDirectories = true;
  oCaps.bCanCreateDirectories = true;
  oCaps.bCanDeleteDirectories = true;
}

IFile* FileSystemStore::openFile(const RainString& sPath, eFileOpenMode eMode) throw(...)
{
  return RainOpenFile(sPath, eMode);
}

IFile* FileSystemStore::openFileNoThrow(const RainString& sPath, eFileOpenMode eMode) throw()
{
  return RainOpenFileNoThrow(sPath, eMode);
}

bool FileSystemStore::doesFileExist(const RainString& sPath) throw()
{
  return RainDoesFileExist(sPath);
}

void FileSystemStore::deleteFile(const RainString& sPath) throw(...)
{
  return RainDeleteFile(sPath);
}

bool FileSystemStore::deleteFileNoThrow(const RainString& sPath) throw()
{
  return RainDeleteFileNoThrow(sPath);
}

size_t FileSystemStore::getEntryPointCount() throw()
{
  _ensureGotEntryPoints();
  return m_vEntryPoints.size();
}

const RainString& FileSystemStore::getEntryPointName(size_t iEntryPointIndex) throw(...)
{
  _ensureGotEntryPoints();
  CHECK_RANGE_LTMAX(0, iEntryPointIndex, m_vEntryPoints.size());
  return m_vEntryPoints[iEntryPointIndex];
}

IDirectory* FileSystemStore::openDirectory(const RainString& sPath) throw(...)
{
  return RainOpenDirectory(sPath);
}

IDirectory* FileSystemStore::openDirectoryNoThrow(const RainString& sPath) throw()
{
  return RainOpenDirectoryNoThrow(sPath);
}

bool FileSystemStore::doesDirectoryExist(const RainString& sPath) throw()
{
  return RainDoesDirectoryExist(sPath);
}

void FileSystemStore::createDirectory(const RainString& sPath) throw(...)
{
  RainCreateDirectory(sPath);
}

bool FileSystemStore::createDirectoryNoThrow(const RainString& sPath) throw()
{
  return RainCreateDirectoryNoThrow(sPath);
}

void FileSystemStore::deleteDirectory(const RainString& sPath) throw(...)
{
  RainDeleteDirectory(sPath);
}

bool FileSystemStore::deleteDirectoryNoThrow(const RainString& sPath) throw()
{
  return RainDeleteDirectoryNoThrow(sPath);
}

void FileSystemStore::_ensureGotEntryPoints() throw()
{
  if(!m_bKnowEntryPoints)
  {
    m_bKnowEntryPoints = true;
    unsigned long iDrives = _getdrives();
    for(char c = 'A'; iDrives; ++c, iDrives >>= 1)
    {
      if(iDrives & 1)
      {
        RainString sPath(L"C:");
        sPath[0] = c;
        m_vEntryPoints.push_back(sPath);
      }
    }
  }
}

class RainFileAdapter : public IFile
{
public:
  RainFileAdapter(FILE* pRawFile, bool bCloseWhenDone = true) throw()
    : m_pFile(pRawFile), m_bCloseWhenDone(bCloseWhenDone) {}
  virtual ~RainFileAdapter() throw()
  {
    if(m_bCloseWhenDone)
      fclose(m_pFile);
  }

  virtual void read(void* pDestination, size_t iItemSize, size_t iItemCount) throw(...)
  {
    if(readNoThrow(pDestination, iItemSize, iItemCount) != iItemCount)
      THROW_SIMPLE(L"Unable to read all items from file");
  }

  virtual size_t readNoThrow(void* pDestination, size_t iItemSize, size_t iItemCount) throw()
  {
    return fread(pDestination, iItemSize, iItemCount, m_pFile);
  }

  virtual void write(const void* pSource, size_t iItemSize, size_t iItemCount) throw(...)
  {
    if(writeNoThrow(pSource, iItemSize, iItemCount) != iItemCount)
      THROW_SIMPLE(L"Unable to write all items to file");
  }

  virtual size_t writeNoThrow(const void* pSource, size_t iItemSize, size_t iItemCount) throw()
  {
    return fwrite(pSource, iItemSize, iItemCount, m_pFile);
  }

  virtual void seek(seek_offset_t iOffset, seek_relative_t eRelativeTo) throw(...)
  {
    if(!seekNoThrow(iOffset, eRelativeTo))
      THROW_SIMPLE(L"Unable to seek to position in file");
  }

  virtual bool seekNoThrow(seek_offset_t iOffset, seek_relative_t eRelativeTo) throw()
  {
    return fseek(m_pFile, iOffset, eRelativeTo) == 0;
  }

  virtual seek_offset_t tell() throw()
  {
    return ftell(m_pFile);
  }

protected:
  FILE* m_pFile;
  bool m_bCloseWhenDone;
};

RAINMAN2_API IFile* RainOpenFilePtr(FILE* pFile, bool bCloseWhenDone) throw(...)
{
  return CHECK_ALLOCATION(new NOTHROW RainFileAdapter(pFile, bCloseWhenDone));
}

IFile* RainOpenFile(const RainString& sPath, eFileOpenMode eMode) throw(...)
{
  const wchar_t* sMode;
  switch(eMode)
  {
  case FM_Read:
    sMode = L"rb";
    break;
  case FM_Write:
    sMode = L"wb";
    break;
  default:
    THROW_SIMPLE_(L"Unsupported file mode for opening \'%s\'", sPath.getCharacters());
  }
  FILE* pRawFile = _wfopen(sPath.getCharacters(), sMode);
  if(pRawFile == 0)
    THROW_SIMPLE_(L"Unable to open \'%s\'", sPath.getCharacters());
  return CHECK_ALLOCATION(new NOTHROW RainFileAdapter(pRawFile));
}

IFile* RainOpenFileNoThrow(const RainString& sPath, eFileOpenMode eMode) throw()
{
  const wchar_t* sMode;
  switch(eMode)
  {
  case FM_Read:
    sMode = L"rb";
    break;
  case FM_Write:
    sMode = L"wb";
    break;
  default:
    return 0;
  }
  FILE* pRawFile = _wfopen(sPath.getCharacters(), sMode);
  if(pRawFile == 0)
    return 0;
  return new NOTHROW RainFileAdapter(pRawFile);
}

bool RainDoesFileExist(const RainString& sPath) throw()
{
  DWORD dwAttributes = GetFileAttributes(sPath.getCharacters());
  return dwAttributes != INVALID_FILE_ATTRIBUTES && (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

void RainDeleteFile(const RainString& sPath) throw(...)
{
  if(!RainDeleteFileNoThrow(sPath))
    THROW_SIMPLE_(L"Cannot delete file \'%s\'", sPath.getCharacters());
}

bool RainDeleteFileNoThrow(const RainString& sPath) throw()
{
  return DeleteFile(sPath.getCharacters()) == TRUE;
}

class RainDirectoryAdapter : public IDirectory
{
public:
  RainDirectoryAdapter(IFileStore* pStore) throw() : m_pStore(pStore), m_iItemCount(0) {}
  virtual ~RainDirectoryAdapter() throw()
  {
    for(std::vector<WIN32_FIND_DATAW*>::iterator itr = m_vItems.begin(); itr != m_vItems.end(); ++itr)
      delete *itr;
  }

  void init(RainString sPath) throw(...)
  {
    m_sPath = sPath;
    m_sPath.replaceAll('/', '\\');
    if(m_sPath.suffix(1).compareCaseless(L"\\") != 0)
      m_sPath += L"\\";
    sPath = m_sPath + L"*";

    m_vItems.push_back(CHECK_ALLOCATION(new NOTHROW WIN32_FIND_DATAW));
    HANDLE hIterator = FindFirstFileW(sPath.getCharacters(), m_vItems[0]);
    if (hIterator == INVALID_HANDLE_VALUE)
      THROW_SIMPLE_(L"Cannot iterate contents of \'%s\'", m_sPath.getCharacters());
    do
    {
      ++m_iItemCount;
    } while(FindNextFile(hIterator, (m_vItems.push_back(CHECK_ALLOCATION(new NOTHROW WIN32_FIND_DATAW)), m_vItems[m_iItemCount])));
    FindClose(hIterator);

    for(size_t i = 0; i < m_iItemCount; ++i)
    {
      if(m_vItems[i]->cFileName[0] == '.' && (m_vItems[i]->cFileName[1] == '.' || m_vItems[i]->cFileName[1] == 0))
      {
        delete m_vItems[i];
        m_vItems.erase(m_vItems.begin() + i);
        --i;
        --m_iItemCount;
      }
    }
  }

  virtual size_t getItemCount() throw() {return m_iItemCount;}
  virtual void getItemDetails(size_t iIndex, directory_item_t& oDetails) throw(...)
  {
    CHECK_RANGE_LTMAX(0, iIndex, m_iItemCount);
    WIN32_FIND_DATAW* pData = m_vItems[iIndex];
    if(oDetails.oFields.dir)
      oDetails.bIsDirectory = pData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? true : false;
    if(oDetails.oFields.name)
      oDetails.sName = pData->cFileName;
    if(oDetails.oFields.size)
      oDetails.iSize.iUpper = pData->nFileSizeHigh, oDetails.iSize.iLower = pData->nFileSizeLow;
    if(oDetails.oFields.time)
    {
      SYSTEMTIME oSysTime;
      FileTimeToSystemTime(&pData->ftLastWriteTime, &oSysTime);
      tm oTM;
      oTM.tm_sec = oSysTime.wSecond;
      oTM.tm_min = oSysTime.wMinute;
      oTM.tm_hour = oSysTime.wHour;
      oTM.tm_mday = oSysTime.wDay;
      oTM.tm_mon = oSysTime.wMonth - 1;
      oTM.tm_year = oSysTime.wYear - 1900;
      oDetails.iTimestamp = mktime(&oTM);
    }
  }

  virtual const RainString& getPath() throw() {return m_sPath;}
  virtual IFileStore* getStore() throw() {return m_pStore;}

protected:
  IFileStore* m_pStore;
  RainString m_sPath;
  size_t m_iItemCount;
  std::vector<WIN32_FIND_DATAW*> m_vItems;
};

IDirectory* RainOpenDirectory(const RainString& sPath) throw(...)
{
  RainDirectoryAdapter* pDirectory = 0;
  CHECK_ALLOCATION(pDirectory = new NOTHROW RainDirectoryAdapter(RainGetFileSystemStore()));
  try
  {
    pDirectory->init(sPath);
  }
  CATCH_THROW_SIMPLE_(delete pDirectory, L"Cannot open directory \'%s\'", sPath.getCharacters());
  return pDirectory;
}

IDirectory* RainOpenDirectoryNoThrow(const RainString& sPath) throw()
{
  try
  {
    return RainOpenDirectory(sPath);
  }
  catch(RainException *e)
  {
    delete e;
    return 0;
  }
}

bool RainDoesDirectoryExist(const RainString& sPath) throw()
{
  DWORD dwAttributes = GetFileAttributes(sPath.getCharacters());
  return dwAttributes != INVALID_FILE_ATTRIBUTES && (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

void RainCreateDirectory(const RainString& sPath) throw(...)
{
  if(_wmkdir(sPath.getCharacters()) == -1)
  {
    const wchar_t* sErr = L"Cannot create directory \'%s\'";
    switch(errno)
    {
    case EEXIST:
      sErr = L"Cannot create directory \'%s\' (POSIX already exists error)";
      break;

    case ENOENT:
      sErr = L"Cannot create directory \'%s\' (POSIX invalid path error)";
      break;
    }
    THROW_SIMPLE_(sErr, sPath.getCharacters());
  }
}

bool RainCreateDirectoryNoThrow(const RainString& sPath) throw()
{
  return _wmkdir(sPath.getCharacters()) == 0;
}

void RainDeleteDirectory(const RainString& sPath) throw(...)
{
  if(_wrmdir(sPath.getCharacters()) == -1)
  {
    const wchar_t* sErr = L"Cannot delete directory \'%s\'";
    switch(errno)
    {
    case ENOTEMPTY:
      sErr = L"Cannot delete directory \'%s\' (POSIX not empty error)";
      break;

    case ENOENT:
      sErr = L"Cannot delete directory \'%s\' (POSIX invalid path error)";
      break;

    case EACCES:
      sErr = L"Cannot delete directory \'%s\' (POSIX access error)";
      break;
    };
    THROW_SIMPLE_(sErr, sPath.getCharacters());
  }
}

bool RainDeleteDirectoryNoThrow(const RainString& sPath) throw()
{
  return _wrmdir(sPath.getCharacters()) == 0;
}

FileSystemStore* RainGetFileSystemStore()
{
  return &g_oRainFSO;
}
