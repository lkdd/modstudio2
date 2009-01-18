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
#include "archive.h"

class RAINMAN2_API SpkArchive : public IArchiveFileStore
{
public:
  SpkArchive() throw();
  virtual ~SpkArchive() throw();

  static bool doesFileResemble(IFile* pFile) throw();

  virtual void init(IFile* pSpkFile, bool bTakePointerOwnership = true) throw(...);

  virtual size_t getFileCount() const throw() {return m_iNumFiles;}
  virtual size_t getDirectoryCount() const throw() {return m_iNumDirs;}

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
  friend class SpkArchiveDirectoryAdapter;

  struct _file_header_t
  {
    char sIdentifier[4];
    unsigned long iVersion;
    unsigned long iHeaderOffset;
    unsigned long iZero;
    unsigned long iHeaderLength;
    char *sVersion;
  };

  struct _file_t
  {
    unsigned long iDataOffset;
    unsigned long iDataLengthCompressed;
		unsigned long iDataLength;
		unsigned long iModificationTime;
    unsigned long iChecksum[4];
    char         *sName;
    enum eCompressionType
    {
      CT_Unknown = 0,
      CT_ZLib = 'Z',
    } eCompression;
  };

  struct _dir_t
  {
    RainString sName,
               sPath;
    _dir_t    *pDirs;
    _file_t   *pFiles;
    size_t     iCountDirs,
               iCountFiles,
               iAllocdDirs,
               iAllocdFiles;
  };

  void     _zeroSelf () throw();
  void     _cleanSelf() throw();
  _file_t* _allocFile(_dir_t* pParent) throw(...);
  _dir_t*  _allocDir (_dir_t* pParent) throw(...);
  _dir_t*  _findDir  (const char* sName, size_t iNameLength, bool bAssumeInRoot, bool bCreate) throw();
  _dir_t*  _findDir  (RainString sName) throw();
  _file_t* _findFile (const RainString& sName) throw();
  void     _pumpFile (_file_t* pFile, IFile* pSink) throw(...);

  _file_header_t m_oFileHeader;
  char          *m_sRawInfoHeader;
  _dir_t        *m_pRootDir;
  IFile         *m_pRawFile;
  size_t         m_iNumFiles;
  size_t         m_iNumDirs;
  bool           m_bDeleteRawFileLater;
};
