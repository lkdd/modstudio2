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
#include <memory.h>
#include <string.h>

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
    pSgaFile->readOne(m_oFileHeader.iVersion);
    if(m_oFileHeader.iVersion != 2 && m_oFileHeader.iVersion != 4)
      throw new RainException(__WFILE__, __LINE__, 0, L"Only version 2 or 4 SGA archives are supported, not version %lu. Please show this archive to corsix@corsix.org", m_oFileHeader.iVersion);
    pSgaFile->readArray(m_oFileHeader.iContentsMD5, 4);
    pSgaFile->readArray(m_oFileHeader.sArchiveName, 64);
    pSgaFile->readArray(m_oFileHeader.iHeaderMD5, 4);
    pSgaFile->readOne(m_oFileHeader.iDataHeaderSize);
    pSgaFile->readOne(m_oFileHeader.iDataOffset);
    if(m_oFileHeader.iVersion == 4)
    {
      pSgaFile->readOne(m_oFileHeader.iPlatform);
      if(m_oFileHeader.iPlatform != 1)
        throw new RainException(__WFILE__, __LINE__, 0, L"Only platform type 1 SGA archives are supported, not version %lu. Please show this archive to corsix@corsix.org", m_oFileHeader.iPlatform);
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

  try
  {
    pSgaFile->seek(m_iDataHeaderOffset + m_oFileHeader.iStringOffset, SR_Start);
    size_t iStringsLength = m_oFileHeader.iDataHeaderSize - m_oFileHeader.iStringOffset;
    CHECK_ALLOCATION(m_sStringBlob = new (std::nothrow) char[iStringsLength]);
    pSgaFile->readArray(m_sStringBlob, iStringsLength);
  }
  CATCH_THROW_SIMPLE(_cleanSelf(), L"Cannot load file and directory names");
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
  //! todo
  THROW_SIMPLE(L"TODO");
}

IFile* SgaArchive::openFileNoThrow(const RainString& sPath, eFileOpenMode eMode) throw()
{
  //! todo
  return 0;
}

bool SgaArchive::doesFileExist(const RainString& sPath) throw()
{
  //! todo
  return false;
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
        oDetails.sName = pInfo->sName;
      if(oDetails.oFields.dir)
        oDetails.bIsDirectory = false;
      if(oDetails.oFields.size)
        oDetails.iSize.iLower = pInfo->iDataLength, oDetails.iSize.iUpper = 0;
      if(oDetails.oFields.time)
        oDetails.iTimestamp = pInfo->iModificationTime;
    }
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
        m_pRawFile->readOne(oRaw.iFolderOffset);

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

      m_pFiles[iToLoad].sName = RainString(m_sStringBlob + oRaw.iNameOffset);
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

      m_pFiles[iToLoad].sName = RainString(m_sStringBlob + oRaw.iNameOffset);
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
    if(m_oFileHeader.iVersion == 2)
      _loadFilesUpTo_v2(iFirstToLoad, iEnsureLoaded);
    else if(m_oFileHeader.iVersion == 4)
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
