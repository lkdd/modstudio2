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
#include "spk_archive.h"
#include "zlib.h"
#include "memfile.h"
#include <memory.h>
#include <string.h>
#include <stack>

SpkArchive::SpkArchive()
{
  _zeroSelf();
}

SpkArchive::~SpkArchive()
{
  _cleanSelf();
}

static char* strenchr(char* str, char* en, char chr)
{
  for(; str != en; ++str)
  {
    if(*str == chr)
    {
      break;
    }
  }
  return str;
}

static unsigned long strenulong(char* str, char* en)
{
  unsigned long value = 0;
  for(; str != en; ++str)
  {
    char c = *str;
    if('0' <= c && c <= '9')
    {
      value = value * 10 + static_cast<unsigned long>(c - '0');
    }
    else
    {
      break;
    }
  }
  return value;
}

static void strreadchexcksum(char *str, int iNumBytes, unsigned long* pResult)
{
  --pResult;
  for(int i = 0; i < iNumBytes; ++i, str += 2)
  {
    char cChar = str[0];
    unsigned long iCharValue = 0;
    if('0' <= cChar && cChar <= '9')
      iCharValue += (cChar - '0');
    else if('A' <= cChar && cChar <= 'F')
      iCharValue += (cChar - 'A' + 10);
    else if('a' <= cChar && cChar <= 'f')
      iCharValue += (cChar - 'a' + 10);
    iCharValue <<= 4;
    cChar = str[1];
    if('0' <= cChar && cChar <= '9')
      iCharValue += (cChar - '0');
    else if('A' <= cChar && cChar <= 'F')
      iCharValue += (cChar - 'A' + 10);
    else if('a' <= cChar && cChar <= 'f')
      iCharValue += (cChar - 'a' + 10);

    if((i % 4) == 0)
      *(++pResult) = 0;
    *pResult |= (iCharValue << ((i % 4) << 3));
  }
}

bool SpkArchive::doesFileResemble(IFile* pFile) throw()
{
  char sSig[4];
  size_t iCount = pFile->readArrayNoThrow(sSig, 4);
  pFile->seekNoThrow(-static_cast<seek_offset_t>(iCount), SR_Current);
  return (iCount == 4) && memcmp(sSig, "\xFFSPK", 4) == 0;
}

void SpkArchive::init(IFile* pSpkFile, bool bTakePointerOwnership) throw(...)
{
  _cleanSelf();
  m_pRawFile = pSpkFile;
  m_bDeleteRawFileLater = bTakePointerOwnership;

  try
  {
    pSpkFile->readArray(m_oFileHeader.sIdentifier, 4);
    if(memcmp(m_oFileHeader.sIdentifier, "\xFFSPK", 4) != 0)
      THROW_SIMPLE_(L"File is not an SPK archive (signature of \"\xFFSPK\" expected, got \"%.4S\")", m_oFileHeader.sIdentifier);
    pSpkFile->readOne(m_oFileHeader.iVersion);
    pSpkFile->readOne(m_oFileHeader.iHeaderOffset);
    pSpkFile->readOne(m_oFileHeader.iZero);
    pSpkFile->readOne(m_oFileHeader.iHeaderLength);
    pSpkFile->seek(m_oFileHeader.iHeaderOffset, SR_Start);
    CHECK_ALLOCATION(m_sRawInfoHeader = new (std::nothrow) char[m_oFileHeader.iHeaderLength + 1]);
    pSpkFile->readArray(m_sRawInfoHeader, m_oFileHeader.iHeaderLength);
  }
  CATCH_THROW_SIMPLE(_cleanSelf(), L"Could not load valid file header");

  // Handle header line by line
  char *sHeader = m_sRawInfoHeader,
       *sHeaderEnd = sHeader + m_oFileHeader.iHeaderLength,
       *sLineTerminator = strenchr(sHeader, sHeaderEnd, '\n');

  // Check first line
  if((sLineTerminator - sHeader) < 5 || memcmp(sHeader, "SPK: ", 5) != 0)
  {
    THROW_SIMPLE(L"Data header does not indicate an SPK archive");
  }
  sHeader = sLineTerminator + 1;
  m_iNumDirs = 1;
  CHECK_ALLOCATION(m_pRootDir = new (std::nothrow) _dir_t);
  m_pRootDir->sName = L"SPK";
  m_pRootDir->sPath = L"SPK\\";
  m_pRootDir->pDirs = NULL;
  m_pRootDir->pFiles = NULL;
  m_pRootDir->iCountDirs = 0;
  m_pRootDir->iCountFiles = 0;
  m_pRootDir->iAllocdDirs = 0;
  m_pRootDir->iAllocdFiles = 0;

  // Handle file files
  for(; sHeader < sHeaderEnd; sHeader = sLineTerminator + 1)
  {
    sLineTerminator = strenchr(sHeader, sHeaderEnd, '\n');
    char *sDelim = strenchr(sHeader, sLineTerminator, '\t');
    _file_t oFile;
    oFile.iDataOffset = strenulong(sHeader, sDelim) * 0x200;

#define NEXT_TAB() \
  if(sDelim == sLineTerminator) sHeader = sLineTerminator; \
  else sHeader = sDelim + 1, sDelim = strenchr(sHeader, sLineTerminator, '\t')

    NEXT_TAB();
    oFile.iDataLengthCompressed = strenulong(sHeader, sDelim);
    NEXT_TAB();
    oFile.iDataLength = strenulong(sHeader, sDelim);
    NEXT_TAB();
    oFile.iModificationTime = strenulong(sHeader, sDelim);
    NEXT_TAB();
    if((sDelim - sHeader) == 32)
      strreadchexcksum(sHeader, 16, oFile.iChecksum);
    else
      memset(oFile.iChecksum, 0, 16);
    NEXT_TAB();
    oFile.eCompression = _file_t::CT_Unknown;
    if((sDelim - sHeader) == 1 && sHeader[0] == 'Z')
      oFile.eCompression = _file_t::CT_ZLib;
    NEXT_TAB();
    oFile.sName = sHeader;
    *sDelim = 0;

    char *sDirDelim = strrchr(oFile.sName, '\\');
    _dir_t *pDirForThis = _findDir(oFile.sName, sDirDelim ? (sDirDelim - oFile.sName) : 0, true, true);
    if(pDirForThis == NULL)
      THROW_SIMPLE_(L"Cannot allocate directory for \'%S\'", oFile.sName);
    if(sDirDelim)
      oFile.sName = sDirDelim + 1;
    *_allocFile(pDirForThis) = oFile;
  }
}

SpkArchive::_file_t* SpkArchive::_allocFile(_dir_t* pParent) throw(...)
{
  if(pParent->iCountFiles >= pParent->iAllocdFiles)
  {
    size_t iNextAllocd = (pParent->iAllocdFiles + 1) * 2;
    _file_t *pNewFiles = new _file_t[iNextAllocd];
    if(pParent->pFiles)
    {
      memcpy(pNewFiles, pParent->pFiles, sizeof(_file_t) * pParent->iAllocdFiles);
      delete[] pParent->pFiles;
    }
    pParent->pFiles = pNewFiles;
    pParent->iAllocdFiles = iNextAllocd;
  }
  ++m_iNumFiles;
  return pParent->pFiles + (pParent->iCountFiles++);
}

SpkArchive::_dir_t* SpkArchive::_allocDir(_dir_t *pParent) throw(...)
{
  if(pParent->iCountDirs >= pParent->iAllocdDirs)
  {
    size_t iNextAllocd = (pParent->iAllocdDirs + 1) * 2;
    _dir_t *pNewDirs = new _dir_t[iNextAllocd];
    if(pParent->pDirs)
    {
      for(size_t i = 0; i < pParent->iAllocdDirs; ++i)
      {
        pNewDirs[i].sName.swap(pParent->pDirs[i].sName);
        pNewDirs[i].sPath.swap(pParent->pDirs[i].sPath);
        pNewDirs[i].pDirs = pParent->pDirs[i].pDirs;
        pNewDirs[i].pFiles = pParent->pDirs[i].pFiles;
        pNewDirs[i].iCountDirs = pParent->pDirs[i].iCountDirs;
        pNewDirs[i].iCountFiles = pParent->pDirs[i].iCountFiles;
        pNewDirs[i].iAllocdDirs = pParent->pDirs[i].iAllocdDirs;
        pNewDirs[i].iAllocdFiles = pParent->pDirs[i].iAllocdFiles;
      }
      delete[] pParent->pDirs;
    }
    pParent->pDirs = pNewDirs;
    pParent->iAllocdDirs = iNextAllocd;
  }
  _dir_t *pDir = pParent->pDirs + pParent->iCountDirs;
  ++pParent->iCountDirs;
  ++m_iNumDirs;
  pDir->pDirs = NULL;
  pDir->pFiles = NULL;
  pDir->iCountDirs = 0;
  pDir->iCountFiles = 0;
  pDir->iAllocdDirs = 0;
  pDir->iAllocdFiles = 0;
  return pDir;
}

SpkArchive::_dir_t* SpkArchive::_findDir(const char* sName, size_t iNameLength, bool bAssumeInRoot, bool bCreate) throw()
{
  _dir_t *pCurrentDir = m_pRootDir;
  if(!bAssumeInRoot)
  {
    size_t iRootLength = m_pRootDir->sName.length();
    if(iNameLength < iRootLength || m_pRootDir->sName.compareCaseless(sName, iRootLength) != 0)
      return NULL;
    sName += iRootLength;
    iNameLength -= iRootLength;
    if(iNameLength != 0 && *sName == '\\')
      ++sName, --iNameLength;
  }

  for(; iNameLength != 0;)
  {
    size_t iPartLength = iNameLength;
    for(size_t i = 0; i < iNameLength; ++i)
    {
      if(sName[i] == '\\')
      {
        iPartLength = i;
        break;
      }
    }
    if(iPartLength == 0)
    {
      ++sName;
      --iNameLength;
      continue;
    }
    size_t iLower = 0, iUpper = pCurrentDir->iCountDirs;
    _dir_t *pSub = 0;
    while(iLower < iUpper)
    {
      size_t iMid = (iLower + iUpper) / 2;
      pSub = pCurrentDir->pDirs + iMid;
      int iStatus = pSub->sName.compareCaseless(sName, iPartLength);
      if(iStatus == 0)
        break;
      else
      {
        pSub = 0;
        if(iStatus < 0)
          iLower = iMid + 1;
        else
          iUpper = iMid;
      }
    }
    if(pSub == 0 && bCreate)
    {
      if(pCurrentDir->iCountDirs == 0 || pCurrentDir->pDirs[pCurrentDir->iCountDirs - 1].sName.compareCaseless(sName, iPartLength) < 0)
      {
        pSub = _allocDir(pCurrentDir);
        pSub->sName = RainString(sName, iPartLength);
        pSub->sPath = pCurrentDir->sPath + pSub->sName + L"\\";
      }
    }
    if(pSub == 0)
      return NULL;
    pCurrentDir = pSub;
    sName += iPartLength;
    iNameLength -= iPartLength;
    if(iNameLength != 0 && *sName == '\\')
      ++sName, --iNameLength;
  }

  return pCurrentDir;
}

void SpkArchive::_zeroSelf() throw()
{
  memset(m_oFileHeader.sIdentifier, 0, 4);
  m_oFileHeader.iVersion = 0;
  m_oFileHeader.iHeaderOffset = 0;
  m_oFileHeader.iZero = 0;
  m_oFileHeader.iHeaderLength = 0;
  m_oFileHeader.sVersion = 0;
  m_sRawInfoHeader = 0;
  m_pRootDir = 0;
  m_pRawFile = 0;
  m_iNumFiles = 0;
  m_iNumDirs = 0;
  m_bDeleteRawFileLater = false;
}

void SpkArchive::_cleanSelf() throw()
{
  delete[] m_oFileHeader.sVersion;
  delete[] m_sRawInfoHeader;
  if(m_pRootDir)
  {
    std::stack<_dir_t*> stkDirs;
    stkDirs.push(m_pRootDir);
    while(!stkDirs.empty())
    {
      _dir_t *pDir = stkDirs.top();
      stkDirs.pop();
      if(pDir->pDirs)
      {
        if(pDir->iCountDirs != 0)
        {
          stkDirs.push(pDir);
          for(size_t i = 0; i < pDir->iCountDirs; ++i)
          {
            if(pDir->pDirs[i].pDirs)
              stkDirs.push(pDir->pDirs + i);
          }
          pDir->iCountDirs = 0;
        }
        else
        {
          delete[] pDir->pDirs;
          delete[] pDir->pFiles;
        }
      }
    }
    delete m_pRootDir;
  }
  if(m_bDeleteRawFileLater)
    delete m_pRawFile;
  _zeroSelf();
}

IFile* SpkArchive::openFile(const RainString& sPath, eFileOpenMode eMode) throw(...)
{
  if(eMode != FM_Read)
    THROW_SIMPLE_(L"Files cannot be written to SGA archives (attempt to write \'%s\')", sPath.getCharacters());
  _file_t *pFileInfo = _findFile(sPath);
  if(pFileInfo == 0)
    THROW_SIMPLE_(L"Cannot open non-existant file \'%s\'", sPath.getCharacters());
  IFile* pFile = CHECK_ALLOCATION(new (std::nothrow) MemoryWriteFile(pFileInfo->iDataLength));
  try
  {
    _pumpFile(pFileInfo, pFile);
    pFile->seek(0, SR_Start);
  }
  CATCH_THROW_SIMPLE_(delete pFile, L"Error opening \'%s\' for reading", sPath.getCharacters());
  return pFile;
}

void SpkArchive::pumpFile(const RainString& sPath, IFile* pSink) throw(...)
{
  _file_t *pFileInfo = _findFile(sPath);
  if(pFileInfo == 0)
    THROW_SIMPLE_(L"Cannot pump non-existant file \'%s\'", sPath.getCharacters());
  _pumpFile(pFileInfo, pSink);
}

IFile* SpkArchive::openFileNoThrow(const RainString& sPath, eFileOpenMode eMode) throw()
{
  if(eMode != FM_Read)
    return 0;
  _file_t *pFileInfo = _findFile(sPath);
  if(pFileInfo == 0)
    return 0;
  IFile* pFile = new (std::nothrow) MemoryWriteFile(pFileInfo->iDataLength);
  if(!pFile)
    return 0;
  try
  {
    _pumpFile(pFileInfo, pFile);
    pFile->seek(0, SR_Start);
  }
  catch(RainException *pE)
  {
    delete pE;
    delete pFile;
    return 0;
  }
  return pFile;
}

bool SpkArchive::doesFileExist(const RainString& sPath) throw()
{
  return _findFile(sPath) != 0;
}

size_t SpkArchive::getEntryPointCount() throw()
{
  if(m_iNumDirs == 0)
    return 0;
  else
    return 1;
}

const RainString& SpkArchive::getEntryPointName(size_t iIndex) throw(...)
{
  CHECK_RANGE_LTMAX(0, iIndex, getEntryPointCount());
  return m_pRootDir->sName;
}

class SpkArchiveDirectoryAdapter : public IDirectory
{
public:
  SpkArchiveDirectoryAdapter(SpkArchive* pArchive, SpkArchive::_dir_t* pDirectory) throw()
    : m_pArchive(pArchive), m_pDirectory(pDirectory)
  {
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
    return m_pDirectory->iCountDirs + m_pDirectory->iCountFiles;
  }

  virtual void getItemDetails(size_t iItemIndex, directory_item_t& oDetails) throw(...)
  {
    CHECK_RANGE_LTMAX(0, iItemIndex, getItemCount());
    if(iItemIndex < m_pDirectory->iCountDirs)
    {
      SpkArchive::_dir_t* pInfo = m_pDirectory->pDirs + iItemIndex;
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
      iItemIndex -= m_pDirectory->iCountDirs;
      SpkArchive::_file_t* pInfo = m_pDirectory->pFiles + iItemIndex;
      if(oDetails.oFields.name)
        oDetails.sName = RainString(pInfo->sName);
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
    CHECK_RANGE_LTMAX(m_pDirectory->iCountDirs, iIndex, getItemCount());
    iIndex -= m_pDirectory->iCountDirs;
    try
    {
      m_pArchive->_pumpFile(m_pDirectory->pFiles + iIndex, pSink);
    }
    CATCH_THROW_SIMPLE_({}, L"Cannot pump file \'%s\\%S\'", m_pDirectory->sPath.getCharacters(), m_pDirectory->pFiles[iIndex].sName);
  }

  virtual IDirectory* openDirectory(size_t iIndex) throw(...)
  {
    CHECK_RANGE_LTMAX(0, iIndex, m_pDirectory->iCountDirs);
    return CHECK_ALLOCATION(new (std::nothrow) SpkArchiveDirectoryAdapter(m_pArchive, m_pDirectory->pDirs + iIndex));
  }

  virtual IDirectory* openDirectoryNoThrow(size_t iIndex) throw()
  {
    if(iIndex >= m_pDirectory->iCountDirs)
      return 0;
    return new (std::nothrow) SpkArchiveDirectoryAdapter(m_pArchive, m_pDirectory->pDirs + iIndex);
  }

protected:
  SpkArchive *m_pArchive;
  SpkArchive::_dir_t *m_pDirectory;
};

IDirectory* SpkArchive::openDirectory(const RainString& sPath) throw(...)
{
  _dir_t *pDirectoryInfo = _findDir(sPath);
  if(pDirectoryInfo == 0)
    THROW_SIMPLE_(L"Cannot open non-existant directory \'%s\'", sPath.getCharacters());
  return CHECK_ALLOCATION(new (std::nothrow) SpkArchiveDirectoryAdapter(this, pDirectoryInfo));
}

IDirectory* SpkArchive::openDirectoryNoThrow(const RainString& sPath) throw()
{
  _dir_t *pDirectoryInfo = _findDir(sPath);
  if(pDirectoryInfo == 0)
    return 0;
  return new (std::nothrow) SpkArchiveDirectoryAdapter(this, pDirectoryInfo);
}

bool SpkArchive::doesDirectoryExist(const RainString& sPath) throw()
{
  return _findDir(sPath) != 0;
}

SpkArchive::_dir_t* SpkArchive::_findDir(RainString sName) throw()
{
  if(m_pRootDir == 0)
    return 0;
  if(m_pRootDir->sName.compareCaseless(sName.beforeFirst('\\')) != 0)
    return 0;
  sName = sName.afterFirst('\\');
  _dir_t *pDir = m_pRootDir;
  for(; !sName.isEmpty();)
  {
    RainString sPart = sName.beforeFirst('\\');
    sName = sName.afterFirst('\\');

    size_t iLower = 0, iUpper = pDir->iCountDirs;
    while(iLower < iUpper)
    {
      size_t iMid = (iLower + iUpper) / 2;
      _dir_t *pChild = pDir->pDirs + iMid;
      int iStatus = sPart.compareCaseless(pChild->sName);
      if(iStatus == 0)
      {
        pDir = pChild;
        goto next_for;
      }
      else if(iStatus < 0)
        iUpper = iMid;
      else
        iLower = iMid + 1;
    }
    return 0;
next_for: ;
  }
  return pDir;
}

SpkArchive::_file_t* SpkArchive::_findFile(const RainString& sName) throw()
{
  _dir_t *pTheDir = _findDir(sName.beforeLast('\\'));
  if(pTheDir == 0)
    return 0;
  RainString sFile = sName.afterLast('\\');

  size_t iLower = 0, iUpper = pTheDir->iCountFiles;
  while(iLower < iUpper)
  {
    size_t iMid = (iLower + iUpper) / 2;
    _file_t *pFile = pTheDir->pFiles + iMid;
    int iStatus = sFile.compareCaseless(pFile->sName);
    if(iStatus == 0)
      return pFile;
    else if(iStatus < 0)
      iUpper = iMid;
    else
      iLower = iMid + 1;
  }
  return 0;
}

void SpkArchive::_pumpFile(SpkArchive::_file_t* pInfo, IFile* pSink) throw(...)
{
  switch(pInfo->eCompression)
  {
  case _file_t::CT_ZLib:
    break;

  case _file_t::CT_Unknown:
  default:
    THROW_SIMPLE(L"Unknown file compression method");
  };

  m_pRawFile->seek(pInfo->iDataOffset, SR_Start);

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

  iNumBytes = m_pRawFile->readArrayNoThrow(aBufferComp, std::min(BUFFER_SIZE, iRemaining));
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
            iNumBytes = m_pRawFile->readArrayNoThrow(aBufferComp, std::min(BUFFER_SIZE, iRemaining));
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
        if(stream.total_out == pInfo->iDataLength)
          break;
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
