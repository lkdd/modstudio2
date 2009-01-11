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

class RAINMAN2_API SgaArchive : public IFileStore
{
public:
  SgaArchive() throw();
  virtual ~SgaArchive() throw();

  void init(IFile* pSgaFile, bool bTakePointerOwnership = true) throw(...);
  bool initNoThrow(IFile* pSgaFile, bool bTakePointerOwnership = true) throw();

  virtual void getCaps(file_store_caps_t& oCaps) const throw();

  virtual IFile* openFile         (const RainString& sPath, eFileOpenMode eMode) throw(...);
  virtual IFile* openFileNoThrow  (const RainString& sPath, eFileOpenMode eMode) throw();
  virtual bool   doesFileExist    (const RainString& sPath) throw();
  virtual void   deleteFile       (const RainString& sPath) throw(...);
  virtual bool   deleteFileNoThrow(const RainString& sPath) throw();

  virtual size_t            getEntryPointCount() throw();
  virtual const RainString& getEntryPointName(size_t iIndex) throw(...);

  virtual IDirectory* openDirectory         (const RainString& sPath) throw(...);
  virtual IDirectory* openDirectoryNoThrow  (const RainString& sPath) throw();
  virtual bool        doesDirectoryExist    (const RainString& sPath) throw();
  virtual void        createDirectory       (const RainString& sPath) throw(...);
  virtual bool        createDirectoryNoThrow(const RainString& sPath) throw();
  virtual void        deleteDirectory       (const RainString& sPath) throw(...);
  virtual bool        deleteDirectoryNoThrow(const RainString& sPath) throw();

protected:
  friend class ArchiveDirectoryAdapter;
  friend class ArchiveFileAdapter;

  struct _file_header_raw_t
  {
    char sIdentifier[8];
    unsigned short iVersionMajor;
    unsigned short iVersionMinor;
    long iContentsMD5[4];
    wchar_t sArchiveName[64];
    long iHeaderMD5[4];
    unsigned long iDataHeaderSize;
    unsigned long iDataOffset;
    unsigned long iPlatform;
    unsigned long iEntryPointOffset;
    unsigned short int iEntryPointCount;
    unsigned long iDirectoryOffset;
    unsigned short int iDirectoryCount;
    unsigned long iFileOffset;
    unsigned short int iFileCount;
    unsigned long iStringOffset;
    unsigned short int iStringCount;
  };

  struct _directory_info_t
  {
    RainString sName;
    RainString sPath;
    unsigned short int iFirstDirectory;
    unsigned short int iLastDirectory;
    unsigned short int iFirstFile;
    unsigned short int iLastFile;
  };

  struct _file_info_t
  {
    RainString sName;
    unsigned long iDataOffset;
		unsigned long iDataLengthCompressed;
		unsigned long iDataLength;
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
  bool _resolvePath(const RainString& sPath, _directory_info_t** ppDirectory, _file_info_t **ppFile, bool bThrow) throw(...);

  template <class T>
  T* _resolveArray(T* pArray, unsigned short iStartIndex, unsigned short iEndIndex, const RainString& sPart, const RainString& sPath, bool bThrow) throw(...)
  {
    for(unsigned short i = iStartIndex; i < iEndIndex; ++i)
    {
      if(pArray[i].sName.compareCaseless(sPart) == 0)
        return pArray + i;
    }
    if(bThrow)
      THROW_SIMPLE_(L"Unable to find \'%s\' of path \'%s\'", sPart.getCharacters(), sPath.getCharacters());
    return 0;
  }

  _file_header_raw_t m_oFileHeader;
  _directory_info_t *m_pEntryPoints;
  _directory_info_t *m_pDirectories;
  _file_info_t *m_pFiles;
  char* m_sStringBlob;
  IFile* m_pRawFile;
  seek_offset_t m_iDataHeaderOffset;
  unsigned short int m_iNumEntryPointsLoaded;
  unsigned short int m_iNumDirectoriesLoaded;
  unsigned short int m_iNumFilesLoaded;
  bool m_bDeleteRawFileLater;
};
