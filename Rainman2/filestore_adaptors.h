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

class RAINMAN2_API ReadOnlyFileStoreAdaptor : public IFileStore
{
public:
  ReadOnlyFileStoreAdaptor(IFileStore *pFileStore, bool bTakeOwnership = true) throw();
  virtual ~ReadOnlyFileStoreAdaptor() throw();

  // IFileStore interface
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
  IFileStore *m_pFileStore;
  bool m_bOwnsFileStore;
};