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
#include "file.h"
#include "exception.h"

class RAINMAN2_API IArchiveFileStore : public IFileStore
{
public:
  virtual void init(IFile* pArchiveFile, bool bTakePointerOwnership = true) throw(...) = 0;
  virtual bool initNoThrow(IFile* pArchiveFile, bool bTakePointerOwnership = true) throw();

  virtual size_t getFileCount() const throw() = 0;
  virtual size_t getDirectoryCount() const throw() = 0;

  virtual void getCaps(file_store_caps_t& oCaps) const throw();

  virtual void   deleteFile            (const RainString& sPath) throw(...);
  virtual bool   deleteFileNoThrow     (const RainString& sPath) throw();
  virtual void   createDirectory       (const RainString& sPath) throw(...);
  virtual bool   createDirectoryNoThrow(const RainString& sPath) throw();
  virtual void   deleteDirectory       (const RainString& sPath) throw(...);
  virtual bool   deleteDirectoryNoThrow(const RainString& sPath) throw();
};

/*
  All Relic games since Impossible Creatures have used SGA archives to store the game data.
  The implementation of SgaArchive in Rainman2 allows for the reading of version 2.0, 4.0 and
  4.1 SGA archives, which covers the following games:
    * Warhammer 40,000: Dawn of War (uses 2.0)
    * Dawn of War: Winter Assault, Dark Crusade and Soulstorm (use 2.0)
    * Company of Heroes (uses 4.0)
    * Company of Heroes: Opposing Fronts (uses 4.0)
    * Company of Heroes Online (uses 4.1, also uses SPK archives)
    * Warhammer 40,000: Dawn of War II (uses 5.0)
  It is currently assumed that Company of Heroes: Tales of Valor will use version 4.x archives.
*/
class RAINMAN2_API SgaArchive : public IArchiveFileStore
{
public:
  SgaArchive() throw();
  virtual ~SgaArchive() throw();

  static bool doesFileResemble(IFile* pFile) throw();

  virtual void init(IFile* pSgaFile, bool bTakePointerOwnership = true) throw(...);

  virtual size_t getFileCount() const throw() {return m_oFileHeader.iFileCount;}
  virtual size_t getDirectoryCount() const throw() {return m_oFileHeader.iDirectoryCount;}

  virtual IFile* openFile         (const RainString& sPath, eFileOpenMode eMode) throw(...);
  virtual void   pumpFile         (const RainString& sPath, IFile* pSink) throw(...);
  virtual IFile* openFileNoThrow  (const RainString& sPath, eFileOpenMode eMode) throw();
  virtual bool   doesFileExist    (const RainString& sPath) throw();

  virtual size_t            getEntryPointCount() throw();
  virtual const RainString& getEntryPointName(size_t iIndex) throw(...);

  virtual IDirectory* openDirectory         (const RainString& sPath) throw(...);
  virtual IDirectory* openDirectoryNoThrow  (const RainString& sPath) throw();
  virtual bool        doesDirectoryExist    (const RainString& sPath) throw();

protected:
  friend class ArchiveDirectoryAdapter;

  struct _file_header_raw_t
  {
    // Formally, these fields are the 'file' header:
    char sIdentifier[8];
    unsigned short iVersionMajor;
    unsigned short iVersionMinor;
    long iContentsMD5[4];
    wchar_t sArchiveName[64];
    long iHeaderMD5[4];
    unsigned long iDataHeaderSize;
    unsigned long iDataOffset;
    unsigned long iPlatform;
    // Formally, thse fields are the start of the 'data' header:
    // All offsets are from the start of this data header
    unsigned long iEntryPointOffset;
    unsigned short int iEntryPointCount;
    unsigned long iDirectoryOffset;
    unsigned short int iDirectoryCount;
    unsigned long iFileOffset;
    unsigned short int iFileCount;
    // The string block contains the name of every directory and file in the archive. Note that the string count is
    // the number of strings (i.e. iDirectoryCount + iFileCount), *not* the length (in bytes) of the string block.
    // In version 4.1 SGA files, the string block is encrypted to prevent casual viewing of file names. In this case,
    // placed before the string block is the encryption algorithm ID and key required to decrypt the block.
    unsigned long iStringOffset;
    unsigned short int iStringCount;
  };

  struct _directory_info_t
  {
    RainString sName;
    RainString sPath;
    // The subdirectories of this directory are those in the range [first, last)
    unsigned short int iFirstDirectory;
    unsigned short int iLastDirectory;
    // The files in this directory (excluding those in subdirectories) are in the range [first, last)
    unsigned short int iFirstFile;
    unsigned short int iLastFile;
  };

  struct _file_info_t
  {
    // Unlike directories, the name is stored as an index into the archive's string block rather than as a RainString.
    // For archives with upward of 30,000 files, this would have meant 30000 RainStrings comprising of 30000 internal
    // RainString buffers and 30000 individual raw character buffers. Thus this would have caused 60000 small allocations
    // when the archive was loaded, and 60000 dealocations when unloaded.
    /* RainString sName; */
    unsigned long iName;
    // The data for this file is located at position ArchiveHeader.iDataOffset + ThisFile.iDataOffset
    unsigned long iDataOffset;
    // The file's data may be compressed with DEFLATE (zLib), in which case the length of the compressed data and the
    // length of the uncompressed data will vary. In some cases, the file data may be uncompressed, in which case the
    // length of the compressed data equals the length of the uncompressed data.
		unsigned long iDataLengthCompressed;
		unsigned long iDataLength;
    // From version 4.0, files in SGA archives have individual unix timestamps on them (i.e. seconds elapsed since
    // January 1st 1970). Version 2 archives do not, so all files report a modification time of Jan 1st 1970 ;)
		unsigned long iModificationTime;
  };

  void _zeroSelf() throw();
  void _cleanSelf() throw();
  const RainString& _getDirectoryEntryPoint(_directory_info_t* pDirectory) throw();
  void _loadEntryPointsUpTo(unsigned short int iEnsureLoaded) throw(...);
  void _loadDirectoriesUpTo(unsigned short int iEnsureLoaded) throw(...);
  void _loadFilesUpTo(unsigned short int iEnsureLoaded) throw(...);
  void _loadFilesUpTo_v2(unsigned short int iFirstToLoad, unsigned short int iEnsureLoaded) throw(...);
  void _loadFilesUpTo_v4(unsigned short int iFirstToLoad, unsigned short int iEnsureLoaded) throw(...);
  void _loadChildren(_directory_info_t* pInfo, bool bJustDirectories) throw(...);
  void _pumpFile(_file_info_t* pInfo, IFile* pSink) throw(...);

  /*!
    Attempts to locate a directory/file within the archive from a given file name / path.
    \param sPath The file path to try and locate
    \param ppDirectory If the file path refers to a directory in the archive, then a pointer to the information on it
                       will be written here, otherwise NULL will be written. ppDirectory itself must not be NULL.
    \param ppFile If the file path refers to a file in the archive, then a pointer to the information on it will be
                  written here, otherwise NULL will be written. ppFile itself must not be NULL.
    \param bThrow If true, then an exception is thrown if nothing is found. If false, then false is returned instead.
  */
  bool _resolvePath(const RainString& sPath, _directory_info_t** ppDirectory, _file_info_t **ppFile, bool bThrow) throw(...);

  template <class T>
  inline const RainString& _getName(const T& o) {return o.sName;}

  inline const char* _getName(const _file_info_t& o) {return m_sStringBlob + o.iName;}

  template <class T>
  T* _resolveArray(T* pArray, unsigned short iStartIndex, unsigned short iEndIndex, const RainString& sPart, const RainString& sPath, bool bThrow) throw(...)
  {
    // Assume array is sorted, and try
    {
      unsigned long iStart = iStartIndex;
      unsigned long iEnd = iEndIndex;
      while(iStart < iEnd)
      {
        unsigned long iTry = (iStart + iEnd) >> 1;
        int iResult = sPart.compareCaseless(_getName(pArray[iTry]));
        if(iResult < 0)
          iEnd = iTry;
        else if(iResult > 0)
          iStart = iTry + 1;
        else
          return pArray + iTry;
      }
    }

    // Retry without the assumption of sorting
    for(unsigned short i = iStartIndex; i < iEndIndex; ++i)
    {
      if(sPart.compareCaseless(_getName(pArray[i])) == 0)
        return pArray + i;
    }
    if(bThrow)
      THROW_SIMPLE_(L"Unable to find \'%s\' of path \'%s\'", sPart.getCharacters(), sPath.getCharacters());
    return 0;
  }

  _file_header_raw_t m_oFileHeader;
  _directory_info_t *m_pEntryPoints;
  _directory_info_t *m_pDirectories;
  _file_info_t      *m_pFiles;
  char              *m_sStringBlob;
  IFile             *m_pRawFile;
  seek_offset_t      m_iDataHeaderOffset;
  unsigned short int m_iNumEntryPointsLoaded;
  unsigned short int m_iNumDirectoriesLoaded;
  unsigned short int m_iNumFilesLoaded;
  bool               m_bDeleteRawFileLater;
};
