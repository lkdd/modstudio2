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
#include "archive.h"
#include "exception.h"
#include "hash.h"
#include "zlib.h"
#include "memfile.h"
#include <memory.h>
#include <string.h>
#include <windows.h>

struct _entry_point_raw_t
{
  char sDirectoryName[64];
  char sAlias[64];
  unsigned short int iFirstDirectory;
  unsigned short int iLastDirectory;
  unsigned short int iFirstFile;
  unsigned short int iLastFile;
  unsigned long iFolderOffset;
};

struct _directory_raw_t
{
  unsigned long iNameOffset;
  unsigned short int iFirstDirectory;
  unsigned short int iLastDirectory;
  unsigned short int iFirstFile;
  unsigned short int iLastFile;
};

struct _file_raw_t
{
  unsigned long iNameOffset;
  unsigned long iDataOffset;
	unsigned long iDataLengthCompressed;
	unsigned long iDataLength;
	unsigned long iModificationTime;
  union
  {
	  unsigned long iFlags32;
    unsigned short iFlags16;
  };
};

SgaArchive::SgaArchive()
{
  _zeroSelf();
}

SgaArchive::~SgaArchive()
{
  _cleanSelf();
}

void SgaArchive::_zeroSelf() throw()
{
  m_pRawFile = 0;
  m_bDeleteRawFileLater = false;
  memset(&m_oFileHeader, 0, sizeof m_oFileHeader);
  m_pEntryPoints = 0;
  m_pDirectories = 0;
  m_pFiles = 0;
  m_iDataHeaderOffset = 0;
  m_iNumEntryPointsLoaded = 0;
  m_iNumDirectoriesLoaded = 0;
  m_iNumFilesLoaded = 0;
  m_sStringBlob = 0;
}

void SgaArchive::_cleanSelf() throw()
{
  if(m_bDeleteRawFileLater)
    delete m_pRawFile;
  delete[] m_pEntryPoints;
  delete[] m_pDirectories;
  delete[] m_pFiles;
  delete[] m_sStringBlob;

  _zeroSelf();
}

void SgaArchive::init(IFile* pSgaFile, bool bTakePointerOwnership) throw(...)
{
  _cleanSelf();
  m_pRawFile = pSgaFile;
  m_bDeleteRawFileLater = bTakePointerOwnership;
  
  try
  {
    pSgaFile->readArray(m_oFileHeader.sIdentifier, 8);
    if(strncmp(m_oFileHeader.sIdentifier, "_ARCHIVE", 8) != 0)
      THROW_SIMPLE(L"Identifier is not _ARCHIVE");
    pSgaFile->readOne(m_oFileHeader.iVersionMajor);
    pSgaFile->readOne(m_oFileHeader.iVersionMinor);
    if((m_oFileHeader.iVersionMajor == 2 && m_oFileHeader.iVersionMinor == 0) ||
       (m_oFileHeader.iVersionMajor == 4 && m_oFileHeader.iVersionMinor <= 1) )
    {}
    else
      throw new RainException(__WFILE__, __LINE__, 0, L"Only version 2.0, 4.0 or 4.1 SGA archives are supported, not version %u.%u - Please show this archive to corsix@corsix.org", m_oFileHeader.iVersionMajor, m_oFileHeader.iVersionMinor);
    pSgaFile->readArray(m_oFileHeader.iContentsMD5, 4);
    pSgaFile->readArray(m_oFileHeader.sArchiveName, 64);
    pSgaFile->readArray(m_oFileHeader.iHeaderMD5, 4);
    pSgaFile->readOne(m_oFileHeader.iDataHeaderSize);
    pSgaFile->readOne(m_oFileHeader.iDataOffset);
    if(m_oFileHeader.iVersionMajor == 4)
    {
      pSgaFile->readOne(m_oFileHeader.iPlatform);
      if(m_oFileHeader.iPlatform != 1)
        throw new RainException(__WFILE__, __LINE__, 0, L"Only win32/x86 (platform #1) SGA archives are supported, not platform #%lu. Please show this archive to corsix@corsix.org", m_oFileHeader.iPlatform);
    }
    else
      m_oFileHeader.iPlatform = 1;
  }
  CATCH_THROW_SIMPLE(_cleanSelf(), L"Could not load valid file header")

  m_iDataHeaderOffset = pSgaFile->tell();
  try
  {
    MD5Hash oHeaderHash;
    long iHeaderHash[4];
    oHeaderHash.updateFromString("DFC9AF62-FC1B-4180-BC27-11CCE87D3EFF");
    oHeaderHash.updateFromFile(pSgaFile, m_oFileHeader.iDataHeaderSize);
    oHeaderHash.finalise((unsigned char*)iHeaderHash);
    if(memcmp(iHeaderHash, m_oFileHeader.iHeaderMD5, 16) != 0)
      THROW_SIMPLE(L"Stored header hash does not match computed header hash");
  }
  CATCH_THROW_SIMPLE(_cleanSelf(), L"File is not an SGA archive because of invalid header / hash");

  try
  {
    pSgaFile->seek(m_iDataHeaderOffset, SR_Start);
    pSgaFile->readOne(m_oFileHeader.iEntryPointOffset);
    pSgaFile->readOne(m_oFileHeader.iEntryPointCount);
    pSgaFile->readOne(m_oFileHeader.iDirectoryOffset);
    pSgaFile->readOne(m_oFileHeader.iDirectoryCount);
    pSgaFile->readOne(m_oFileHeader.iFileOffset);
    pSgaFile->readOne(m_oFileHeader.iFileCount);
    pSgaFile->readOne(m_oFileHeader.iStringOffset);
    pSgaFile->readOne(m_oFileHeader.iStringCount);
  }
  CATCH_THROW_SIMPLE(_cleanSelf(), L"Cannot load data header overview");

  if(m_oFileHeader.iVersionMajor == 4 && m_oFileHeader.iVersionMinor == 1)
  {
    // This code reverse-engineered from CoH:Online's Filesystem.dll
    HCRYPTPROV hCryptoProvider = 0;
    HCRYPTKEY hCryptoKey = 0;
    unsigned char* pKeyData = 0;
    try
    {
      pSgaFile->seek(m_iDataHeaderOffset + m_oFileHeader.iStringOffset, SR_Start);
      unsigned long iKeyLength;
      pSgaFile->readOne(iKeyLength);
      CHECK_ALLOCATION(pKeyData = new (std::nothrow) unsigned char[iKeyLength]);
      pSgaFile->readArray(pKeyData, iKeyLength);
      unsigned long iStringsLength;
      pSgaFile->readOne(iStringsLength);
      CHECK_ALLOCATION(m_sStringBlob = new (std::nothrow) char[iStringsLength]);
      pSgaFile->readArray(m_sStringBlob, iStringsLength);

      if(CryptAcquireContext(&hCryptoProvider, NULL, L"Microsoft Enhanced Cryptographic Provider v1.0", PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) == FALSE)
        THROW_SIMPLE(L"Cannot acquire cryptographic context from Windows");
      if(CryptImportKey(hCryptoProvider, pKeyData, iKeyLength, NULL, 0, &hCryptoKey) == FALSE)
        THROW_SIMPLE(L"Cannot import cryptographic key from archive");
      if(CryptDecrypt(hCryptoKey, NULL, TRUE, 0, reinterpret_cast<BYTE*>(m_sStringBlob), &iStringsLength) == FALSE)
        THROW_SIMPLE(L"Could not decrypt archive\'s string table");
    }
    CATCH_THROW_SIMPLE({
      if(hCryptoKey)
        CryptDestroyKey(hCryptoKey);
      if(hCryptoProvider)
        CryptReleaseContext(hCryptoProvider, 0);
      if(pKeyData)
        delete[] pKeyData;
      _cleanSelf();
    }, L"Cannot load file and directory names");
    CryptDestroyKey(hCryptoKey);
    CryptReleaseContext(hCryptoProvider, 0);
    delete[] pKeyData;
  }
  else
  {
    try
    {
      pSgaFile->seek(m_iDataHeaderOffset + m_oFileHeader.iStringOffset, SR_Start);
      size_t iStringsLength = m_oFileHeader.iDataHeaderSize - m_oFileHeader.iStringOffset;
      CHECK_ALLOCATION(m_sStringBlob = new (std::nothrow) char[iStringsLength]);
      pSgaFile->readArray(m_sStringBlob, iStringsLength);
    }
    CATCH_THROW_SIMPLE(_cleanSelf(), L"Cannot load file and directory names");
  }
}

bool SgaArchive::initNoThrow(IFile* pSgaFile, bool bTakePointerOwnership) throw()
{
  try
  {
    init(pSgaFile, bTakePointerOwnership);
    return true;
  }
  catch(RainException* e)
  {
    delete e;
  }
  catch(...)
  {
  }
  return false;
}

void SgaArchive::getCaps(file_store_caps_t& oCaps) const throw()
{
  oCaps = false;
  oCaps.bCanReadFiles = true;
  oCaps.bCanOpenDirectories = true;
}

IFile* SgaArchive::openFile(const RainString& sPath, eFileOpenMode eMode) throw(...)
{
  if(eMode != FM_Read)
    THROW_SIMPLE_(L"Files cannot be written to SGA archives (attempt to write \'%s\')", sPath.getCharacters());
  _directory_info_t* pDirInfo = 0;
  _file_info_t* pFileInfo = 0;
  _resolvePath(sPath, &pDirInfo, &pFileInfo, true);
  if(pFileInfo == 0)
    THROW_SIMPLE_(L"Cannot open file \'%s\' as it is a directory", sPath.getCharacters());
  IFile* pFile = CHECK_ALLOCATION(new (std::nothrow) MemoryWriteFile(pFileInfo->iDataLength));
  try
  {
    _pumpFile(pFileInfo, pFile);
  }
  CATCH_THROW_SIMPLE_(delete pFile, L"Error opening \'%s\' for reading", sPath.getCharacters());
  return pFile;
}

void SgaArchive::_pumpFile(_file_info_t* pInfo, IFile* pSink) throw(...)
{
  m_pRawFile->seek(m_oFileHeader.iDataOffset + pInfo->iDataOffset, SR_Start);

  if(pInfo->iDataLength == pInfo->iDataLengthCompressed)
  {
    static const size_t BUFFER_SIZE = 8192;
    unsigned char aBuffer[BUFFER_SIZE];
    for(size_t iRemaining = static_cast<size_t>(pInfo->iDataLength); iRemaining != 0;)
    {
      size_t iNumBytes = m_pRawFile->readArrayNoThrow(aBuffer, min(BUFFER_SIZE, iRemaining));
      pSink->writeArray(aBuffer, iNumBytes);
      iRemaining -= iNumBytes;
    }
  }
  else
  {
    static const size_t BUFFER_SIZE = 4096;
    static const wchar_t* Z_ERR[] = {
      L"zLib error from C errno",
      L"zLib stream error",
      L"zLib data error",
      L"zLib memory error",
      L"zLib buffer error",
      L"zLib version error"
    };
    unsigned char aBufferComp[BUFFER_SIZE];
    unsigned char aBufferInft[BUFFER_SIZE];

    size_t iRemaining = static_cast<size_t>(pInfo->iDataLengthCompressed);
    size_t iNumBytes;
    z_stream stream;
    int err;

    iNumBytes = m_pRawFile->readArrayNoThrow(aBufferComp, min(BUFFER_SIZE, iRemaining));
    iRemaining -= iNumBytes;

    stream.next_in = (Bytef*)aBufferComp;
    stream.avail_in = (uInt)iNumBytes;
    stream.next_out = (Bytef*)aBufferInft;
    stream.avail_out = (uInt)BUFFER_SIZE;
    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;

    err = inflateInit(&stream);
    if(err != Z_OK)
      THROW_SIMPLE_(L"Cannot initialise zLib stream; %s", Z_ERR[-err-1]);

    try
    {
      while(true)
      {
        err = inflate(&stream, Z_SYNC_FLUSH);
        if(err == Z_STREAM_END)
          break;
        else
        {
          switch(err)
          {
          case Z_NEED_DICT:
            THROW_SIMPLE(L"Cannot decompress file; zLib requesting dictionary");
            break;

          case Z_OK:
            if(stream.avail_in == 0)
            {
              iNumBytes = m_pRawFile->readArrayNoThrow(aBufferComp, min(BUFFER_SIZE, iRemaining));
              iRemaining -= iNumBytes;
              stream.next_in = (Bytef*)aBufferComp;
              stream.avail_in = (uInt)iNumBytes;
            }
            if(stream.next_out != aBufferInft)
            {
              pSink->writeArray(aBufferInft, stream.next_out - aBufferInft);
              stream.next_out = (Bytef*)aBufferInft;
              stream.avail_out = (uInt)BUFFER_SIZE;
            }
            break;

          default:
            THROW_SIMPLE_(L"Cannot decompress file; %s", Z_ERR[-err-1]);
            break;
          }
        }
      }
      if(stream.next_out != aBufferInft)
        pSink->writeArray(aBufferInft, stream.next_out - aBufferInft);
    }
    catch(RainException*)
    {
      inflateEnd(&stream);
      throw;
    }

    err = inflateEnd(&stream);
    if(err != Z_OK)
      THROW_SIMPLE_(L"Cannot cleanly close zLib stream; %s", Z_ERR[-err-1]);
  }
}

void SgaArchive::pumpFile(const RainString& sPath, IFile* pSink) throw(...)
{
  _directory_info_t* pDirInfo = 0;
  _file_info_t* pFileInfo = 0;
  try
  {
    _resolvePath(sPath, &pDirInfo, &pFileInfo, true);
    if(pFileInfo == 0)
      THROW_SIMPLE(L"Cannot pump a directory");

    _pumpFile(pFileInfo, pSink);
  }
  CATCH_THROW_SIMPLE_({}, L"Cannot pump file \'%s\'", sPath.getCharacters());
}

IFile* SgaArchive::openFileNoThrow(const RainString& sPath, eFileOpenMode eMode) throw()
{
  if(eMode != FM_Read)
    return 0;
  _directory_info_t* pDirInfo = 0;
  _file_info_t* pFileInfo = 0;
  if(!_resolvePath(sPath, &pDirInfo, &pFileInfo, false))
    return 0;
  IFile* pFile = new (std::nothrow) MemoryWriteFile(pFileInfo->iDataLength);
  if(!pFile)
    return 0;
  try
  {
    _pumpFile(pFileInfo, pFile);
  }
  catch(RainException *pE)
  {
    delete pE;
    delete pFile;
    return 0;
  }
  return pFile;
}

bool SgaArchive::doesFileExist(const RainString& sPath) throw()
{
  _directory_info_t* pDirInfo = 0;
  _file_info_t* pFileInfo = 0;
  _resolvePath(sPath, &pDirInfo, &pFileInfo, false);
  return pFileInfo != 0;
}

void SgaArchive::deleteFile(const RainString& sPath) throw(...)
{
  THROW_SIMPLE_(L"Files cannot be deleted from SGA archives (attempt to delete \'%s\')", sPath.getCharacters());
}

bool SgaArchive::deleteFileNoThrow(const RainString& sPath) throw()
{
  return false;
}

size_t SgaArchive::getEntryPointCount() throw()
{
  return m_oFileHeader.iEntryPointCount;
}

const RainString& SgaArchive::getEntryPointName(size_t iIndex) throw(...)
{
  CHECK_RANGE((size_t)0, iIndex, getEntryPointCount() - 1);
  _loadEntryPointsUpTo((unsigned short)iIndex);
  return m_pEntryPoints[iIndex].sName;
}

class ArchiveDirectoryAdapter : public IDirectory
{
public:
  ArchiveDirectoryAdapter(SgaArchive* pArchive, SgaArchive::_directory_info_t* pDirectory) throw()
    : m_pArchive(pArchive), m_pDirectory(pDirectory)
  {
    m_iCountSubDir = pDirectory->iLastDirectory - pDirectory->iFirstDirectory;
    m_iCountFiles = pDirectory->iLastFile - pDirectory->iFirstFile;
  }

  virtual const RainString& getPath() throw()
  {
    return m_pDirectory->sPath;
  }

  virtual IFileStore* getStore() throw()
  {
    return m_pArchive;
  }

  virtual size_t getItemCount() throw()
  {
    return m_iCountSubDir + m_iCountFiles;
  }

  virtual void getItemDetails(size_t iItemIndex, directory_item_t& oDetails) throw(...)
  {
    CHECK_RANGE_LTMAX((size_t)0, iItemIndex, m_iCountSubDir + m_iCountFiles);
    if(iItemIndex < m_iCountSubDir)
    {
      iItemIndex += m_pDirectory->iFirstDirectory;
      m_pArchive->_loadDirectoriesUpTo((unsigned short)iItemIndex);
      SgaArchive::_directory_info_t* pInfo = m_pArchive->m_pDirectories + iItemIndex;
      if(oDetails.oFields.name)
        oDetails.sName = pInfo->sName;
      if(oDetails.oFields.dir)
        oDetails.bIsDirectory = true;
      if(oDetails.oFields.size)
        oDetails.iSize.iLower = 0, oDetails.iSize.iUpper = 0;
      if(oDetails.oFields.time)
        oDetails.iTimestamp = 0;
    }
    else
    {
      iItemIndex -= m_iCountSubDir;
      iItemIndex += m_pDirectory->iFirstFile;
      m_pArchive->_loadFilesUpTo((unsigned short)iItemIndex);
      SgaArchive::_file_info_t* pInfo = m_pArchive->m_pFiles + iItemIndex;
      if(oDetails.oFields.name)
        oDetails.sName = RainString(m_pArchive->m_sStringBlob + pInfo->iName); //pInfo->sName;
      if(oDetails.oFields.dir)
        oDetails.bIsDirectory = false;
      if(oDetails.oFields.size)
        oDetails.iSize.iLower = pInfo->iDataLength, oDetails.iSize.iUpper = 0;
      if(oDetails.oFields.time)
        oDetails.iTimestamp = pInfo->iModificationTime;
    }
  }

  virtual void pumpFile(size_t iIndex, IFile* pSink) throw(...)
  {
    CHECK_RANGE_LTMAX(m_iCountSubDir, iIndex, m_iCountSubDir + m_iCountFiles);
    iIndex -= m_iCountSubDir;
    iIndex += m_pDirectory->iFirstFile;
    m_pArchive->_loadFilesUpTo((unsigned short)iIndex);
    m_pArchive->_pumpFile(m_pArchive->m_pFiles + iIndex, pSink);
  }

  virtual IDirectory* openDirectory(size_t iIndex) throw(...)
  {
    CHECK_RANGE_LTMAX((size_t)0, iIndex, m_iCountSubDir);
    iIndex += m_pDirectory->iFirstDirectory;
    m_pArchive->_loadDirectoriesUpTo((unsigned short)iIndex);
    return CHECK_ALLOCATION(new (std::nothrow) ArchiveDirectoryAdapter(m_pArchive, m_pArchive->m_pDirectories + iIndex));
  }

  virtual IDirectory* openDirectoryNoThrow(size_t iIndex) throw()
  {
    if(iIndex >= m_iCountSubDir)
      return 0;
    iIndex += m_pDirectory->iFirstDirectory;
    try
    {
      m_pArchive->_loadDirectoriesUpTo((unsigned short)iIndex);
    }
    catch(RainException *pE)
    {
      delete pE;
      return 0;
    }
    return new (std::nothrow) ArchiveDirectoryAdapter(m_pArchive, m_pArchive->m_pDirectories + iIndex);
  }

protected:
  SgaArchive* m_pArchive;
  SgaArchive::_directory_info_t *m_pDirectory;
  size_t m_iCountSubDir;
  size_t m_iCountFiles;
};

IDirectory* SgaArchive::openDirectory(const RainString& sPath) throw(...)
{
  _directory_info_t* pDirectoryInfo;
  _file_info_t* pFileInfo;
  _resolvePath(sPath, &pDirectoryInfo, &pFileInfo, true);
  if(pDirectoryInfo == 0)
    THROW_SIMPLE_(L"Cannot open \'%s\' as a directory because it is a file", sPath.getCharacters());
  return CHECK_ALLOCATION(new (std::nothrow) ArchiveDirectoryAdapter(this, pDirectoryInfo));
}

IDirectory* SgaArchive::openDirectoryNoThrow(const RainString& sPath) throw()
{
  _directory_info_t* pDirectoryInfo;
  _file_info_t* pFileInfo;
  if(!_resolvePath(sPath, &pDirectoryInfo, &pFileInfo, false) || pDirectoryInfo == 0)
    return 0;
  return new (std::nothrow) ArchiveDirectoryAdapter(this, pDirectoryInfo);
}

bool SgaArchive::doesDirectoryExist(const RainString& sPath) throw()
{
  //! todo
  return false;
}

void SgaArchive::createDirectory(const RainString& sPath) throw(...)
{
  THROW_SIMPLE_(L"Directories cannot be created in SGA archives (attempt to create \'%s\')", sPath.getCharacters());
}

bool SgaArchive::createDirectoryNoThrow(const RainString& sPath) throw()
{
  return false;
}

void SgaArchive::deleteDirectory(const RainString& sPath) throw(...)
{
  THROW_SIMPLE_(L"Directories cannot be deleted from SGA archives (attempt to delete \'%s\')", sPath.getCharacters());
}

bool SgaArchive::deleteDirectoryNoThrow(const RainString& sPath) throw()
{
  return false;
}

void SgaArchive::_loadEntryPointsUpTo(unsigned short int iEnsureLoaded) throw(...)
{
  unsigned short int iFirstToLoad = m_iNumEntryPointsLoaded + 1;
  if(m_pEntryPoints == 0)
  {
    iFirstToLoad = 0;
    CHECK_ALLOCATION(m_pEntryPoints = new _directory_info_t[getEntryPointCount()]);
  }
  if(iFirstToLoad <= iEnsureLoaded)
  {
    CHECK_RANGE_LTMAX((unsigned short)0, iEnsureLoaded, m_oFileHeader.iEntryPointCount);
    try
    {
      m_pRawFile->seek(m_iDataHeaderOffset + m_oFileHeader.iEntryPointOffset + iFirstToLoad * 140, SR_Start);
      for(unsigned short int iToLoad = iFirstToLoad; iToLoad <= iEnsureLoaded; m_iNumEntryPointsLoaded = iToLoad++)
      {
        _entry_point_raw_t oRaw;
        m_pRawFile->readArray(oRaw.sDirectoryName, 64);
        m_pRawFile->readArray(oRaw.sAlias, 64);
        m_pRawFile->readOne(oRaw.iFirstDirectory);
        m_pRawFile->readOne(oRaw.iLastDirectory);
        m_pRawFile->readOne(oRaw.iFirstFile);
        m_pRawFile->readOne(oRaw.iLastFile);
        if(m_oFileHeader.iVersionMajor == 4 && m_oFileHeader.iVersionMinor == 1)
        {
          unsigned short iFolderOffset;
          m_pRawFile->readOne(iFolderOffset);
          oRaw.iFolderOffset = iFolderOffset;
        }
        else
        {
          m_pRawFile->readOne(oRaw.iFolderOffset);
        }

        m_pEntryPoints[iToLoad].sName = oRaw.sDirectoryName;
        m_pEntryPoints[iToLoad].sPath = m_pEntryPoints[iToLoad].sName + L"\\";
        m_pEntryPoints[iToLoad].iFirstDirectory = oRaw.iFirstDirectory;
        m_pEntryPoints[iToLoad].iLastDirectory = oRaw.iLastDirectory;
        m_pEntryPoints[iToLoad].iFirstFile = oRaw.iFirstFile;
        m_pEntryPoints[iToLoad].iLastFile = oRaw.iLastFile;

        _loadDirectoriesUpTo(oRaw.iFirstDirectory);
      }
    }
    CATCH_THROW_SIMPLE({}, L"Unable to load entry point details")
  }
}

const RainString& SgaArchive::_getDirectoryEntryPoint(_directory_info_t* pDirectory) throw()
{
  _loadEntryPointsUpTo(m_oFileHeader.iEntryPointCount - 1);
  if(m_oFileHeader.iEntryPointCount != 1)
  {
    unsigned short int iIndex = (unsigned short)(pDirectory - m_pDirectories);
    for(unsigned short int iEntryPoint = 0; iEntryPoint < m_oFileHeader.iEntryPointCount; ++iEntryPoint)
    {
      if(m_pEntryPoints[iEntryPoint].iFirstDirectory <= iIndex && iIndex < m_pEntryPoints[iEntryPoint].iLastDirectory)
        return m_pEntryPoints[iEntryPoint].sPath;
    }
  }
  return m_pEntryPoints->sPath;
}

void SgaArchive::_loadDirectoriesUpTo(unsigned short int iEnsureLoaded) throw(...)
{
  unsigned short int iFirstToLoad = m_iNumDirectoriesLoaded + 1;
  if(m_pDirectories == 0)
  {
    iFirstToLoad = 0;
    CHECK_ALLOCATION(m_pDirectories = new _directory_info_t[m_oFileHeader.iDirectoryCount]);
  }
  if(iFirstToLoad <= iEnsureLoaded)
  {
    CHECK_RANGE_LTMAX((unsigned short)0, iEnsureLoaded, m_oFileHeader.iDirectoryCount);
    try
    {
      m_pRawFile->seek(m_iDataHeaderOffset + m_oFileHeader.iDirectoryOffset + iFirstToLoad * 12, SR_Start);
      for(unsigned short int iToLoad = iFirstToLoad; iToLoad <= iEnsureLoaded; m_iNumDirectoriesLoaded = iToLoad++)
      {
        _directory_raw_t oRaw;
        m_pRawFile->readOne(oRaw.iNameOffset);
        m_pRawFile->readOne(oRaw.iFirstDirectory);
        m_pRawFile->readOne(oRaw.iLastDirectory);
        m_pRawFile->readOne(oRaw.iFirstFile);
        m_pRawFile->readOne(oRaw.iLastFile);

        m_pDirectories[iToLoad].sPath = RainString(m_sStringBlob + oRaw.iNameOffset);
        if(m_pDirectories[iToLoad].sPath.isEmpty())
        {
          m_pDirectories[iToLoad].sPath = _getDirectoryEntryPoint(m_pDirectories + iToLoad);
          m_pDirectories[iToLoad].sName = m_pDirectories[iToLoad].sPath.beforeLast('\\');
        }
        else
        {
          m_pDirectories[iToLoad].sPath = _getDirectoryEntryPoint(m_pDirectories + iToLoad) + m_pDirectories[iToLoad].sPath;
          m_pDirectories[iToLoad].sName = m_pDirectories[iToLoad].sPath.afterLast('\\');
          m_pDirectories[iToLoad].sPath += L"\\";
        }
        m_pDirectories[iToLoad].iFirstDirectory = oRaw.iFirstDirectory;
        m_pDirectories[iToLoad].iLastDirectory = oRaw.iLastDirectory;
        m_pDirectories[iToLoad].iFirstFile = oRaw.iFirstFile;
        m_pDirectories[iToLoad].iLastFile = oRaw.iLastFile;
      }
    }
    CATCH_THROW_SIMPLE({}, L"Unable to load directory details")
  }
}

void SgaArchive::_loadFilesUpTo_v2(unsigned short int iFirstToLoad, unsigned short int iEnsureLoaded) throw(...)
{
  try
  {
    m_pRawFile->seek(m_iDataHeaderOffset + m_oFileHeader.iFileOffset + iFirstToLoad * 20, SR_Start);
    for(unsigned short int iToLoad = iFirstToLoad; iToLoad <= iEnsureLoaded; m_iNumFilesLoaded = iToLoad++)
    {
      _file_raw_t oRaw;
      m_pRawFile->readOne(oRaw.iNameOffset);
      m_pRawFile->readOne(oRaw.iFlags32);
      m_pRawFile->readOne(oRaw.iDataOffset);
      m_pRawFile->readOne(oRaw.iDataLengthCompressed);
      m_pRawFile->readOne(oRaw.iDataLength);

      m_pFiles[iToLoad].iName = oRaw.iNameOffset; //RainString(m_sStringBlob + oRaw.iNameOffset);
      m_pFiles[iToLoad].iDataOffset = oRaw.iDataOffset;
      m_pFiles[iToLoad].iDataLength = oRaw.iDataLength;
      m_pFiles[iToLoad].iDataLengthCompressed = oRaw.iDataLengthCompressed;
      m_pFiles[iToLoad].iModificationTime = 0;
    }
  }
  CATCH_THROW_SIMPLE({}, L"Unable to load file details")
}

void SgaArchive::_loadFilesUpTo_v4(unsigned short int iFirstToLoad, unsigned short int iEnsureLoaded) throw(...)
{
  try
  {
    m_pRawFile->seek(m_iDataHeaderOffset + m_oFileHeader.iFileOffset + iFirstToLoad * 22, SR_Start);
    for(unsigned short int iToLoad = iFirstToLoad; iToLoad <= iEnsureLoaded; m_iNumFilesLoaded = iToLoad++)
    {
      _file_raw_t oRaw;
      m_pRawFile->readOne(oRaw.iNameOffset);
      m_pRawFile->readOne(oRaw.iDataOffset);
      m_pRawFile->readOne(oRaw.iDataLengthCompressed);
      m_pRawFile->readOne(oRaw.iDataLength);
      m_pRawFile->readOne(oRaw.iModificationTime);
      m_pRawFile->readOne(oRaw.iFlags16);

      m_pFiles[iToLoad].iName = oRaw.iNameOffset; //RainString(m_sStringBlob + oRaw.iNameOffset);
      m_pFiles[iToLoad].iDataOffset = oRaw.iDataOffset;
      m_pFiles[iToLoad].iDataLength = oRaw.iDataLength;
      m_pFiles[iToLoad].iDataLengthCompressed = oRaw.iDataLengthCompressed;
      m_pFiles[iToLoad].iModificationTime = oRaw.iModificationTime;
    }
  }
  CATCH_THROW_SIMPLE({}, L"Unable to load file details")
}

void SgaArchive::_loadFilesUpTo(unsigned short int iEnsureLoaded) throw(...)
{
  unsigned short int iFirstToLoad = m_iNumFilesLoaded + 1;
  if(m_pFiles == 0)
  {
    iFirstToLoad = 0;
    CHECK_ALLOCATION(m_pFiles = new _file_info_t[m_oFileHeader.iFileCount]);
  }
  if(iFirstToLoad <= iEnsureLoaded)
  {
    CHECK_RANGE_LTMAX((unsigned short)0, iEnsureLoaded, m_oFileHeader.iFileCount);
    if(m_oFileHeader.iVersionMajor == 2)
      _loadFilesUpTo_v2(iFirstToLoad, iEnsureLoaded);
    else if(m_oFileHeader.iVersionMajor == 4)
      _loadFilesUpTo_v4(iFirstToLoad, iEnsureLoaded);
    else
      THROW_SIMPLE(L"Unsupported SGA version for file info");
  }
}

void SgaArchive::_loadChildren(_directory_info_t* pInfo, bool bJustDirectories) throw(...)
{
  if(pInfo->iLastDirectory != 0)
    _loadDirectoriesUpTo(pInfo->iLastDirectory - 1);
  if(!bJustDirectories && pInfo->iLastFile != 0)
    _loadFilesUpTo(pInfo->iLastFile - 1);
}

bool SgaArchive::_resolvePath(const RainString& sPath, _directory_info_t** ppDirectory, _file_info_t **ppFile, bool bThrow) throw(...)
{
  // Clear return values
  *ppDirectory = 0;
  *ppFile = 0;

  // Load all the entry points
  try { _loadEntryPointsUpTo(m_oFileHeader.iEntryPointCount - 1); }
  CATCH_THROW_SIMPLE({ if(!bThrow){delete e; return false;} }, L"Unable to load entry point details");

  // Find entry point
  RainString sPart = sPath.beforeFirst('\\');
  RainString sPathRemain = sPath.afterFirst('\\');
  _directory_info_t* pDirectory = _resolveArray(m_pEntryPoints, 0, m_oFileHeader.iEntryPointCount, sPart, sPath, bThrow);
  if(!pDirectory)
    return false;
  pDirectory = m_pDirectories + pDirectory->iFirstDirectory;
  if(sPathRemain.isEmpty())
  {
    *ppDirectory = pDirectory;
    return true;
  }

  // Find subsequent directories
  sPart = sPathRemain.beforeFirst('\\');
  sPathRemain = sPathRemain.afterFirst('\\');
  while(!sPathRemain.isEmpty())
  {
    try { _loadChildren(pDirectory, true); }
    CATCH_THROW_SIMPLE({ if(!bThrow){delete e; return false;} }, L"Unable to load directory details");

    pDirectory = _resolveArray(m_pDirectories, pDirectory->iFirstDirectory, pDirectory->iLastDirectory, sPart, sPath, bThrow);
    if(!pDirectory)
      return false;
    sPart = sPathRemain.beforeFirst('\\');
    sPathRemain = sPathRemain.afterFirst('\\');
  }

  // Find last directory / file
  try { _loadChildren(pDirectory, false); }
  CATCH_THROW_SIMPLE({ if(!bThrow){delete e; return false;} }, L"Unable to load directory details");

  *ppFile = _resolveArray(m_pFiles, pDirectory->iFirstFile, pDirectory->iLastFile, sPart, sPath, false);
  if(*ppFile == 0)
  {
    *ppDirectory = _resolveArray(m_pDirectories, pDirectory->iFirstDirectory, pDirectory->iLastDirectory, sPart, sPath, bThrow);
    if(*ppDirectory == 0)
      return false;
  }
  return true;
}
