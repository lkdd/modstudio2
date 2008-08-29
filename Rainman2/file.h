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
#include "string.h"
#include <stdio.h>
#include <vector>
#include <iterator>

//! Type used when specifying the offset in file seek / tell operations
typedef long seek_offset_t;

//! Values passed to IFIle::seek to specify where to seek from
/*!
  These values map to their C standard library equivalents for ease of convertion
*/
enum seek_relative_t
{
  SR_Start   = SEEK_SET, //!< Seek relative to the start of the file
  SR_Current = SEEK_CUR, //!< Seek relative to the current position in the file
  SR_End     = SEEK_END, //!< Seek relative to the end of the file
};

//! Different ways of opening a file
enum eFileOpenMode
{
  FM_Read,  //!< Open a file in read-only binary mode
  FM_Write, //!< Open a file in read & write binary mode
};

//! A generic interface for files and other file-like things
/*!
  RainOpenFile() can be used to open a traditional file and obtain an IFile pointer
*/
class RAINMAN2_API IFile
{
public:
  virtual ~IFile() throw(); //!< virtual destructor

  //! Read bytes from the file
  /*!
    Reads a number of items from a file and copies them to a user-supplied buffer.
    If it cannot read all of the items, then an exception will be thrown, and the file
    position may or may not have been incremented.
    \param pDestination Buffer at least iItemSize*iItemCount bytes big to copy the
           read bytes into.
    \param iItemSize Size, in bytes, of one item
    \param iItemCount Number of items to read from the file
    \sa readNoThrow() readOne() readArray()
  */
  virtual void read(void* pDestination, size_t iItemSize, size_t iItemCount) throw(...) = 0;

  //! Read bytes from the file, without throwing an exception
  /*!
    Reads a number of items from a file and copies them to a user-supplied buffer.
    If it cannot read all of the items, then it will read as many as it can, incrementing
    the file position by that number of items.
    \param pDestination Buffer at least iItemSize*iItemCount bytes big to copy the
           read bytes into.
    \param iItemSize Size, in bytes, of one item
    \param iItemCount Number of items to read from the file
    \return The number of items read
    \sa read() readOneNoThrow() readArrayNoThrow()
  */
  virtual size_t readNoThrow(void* pDestination, size_t iItemSize, size_t iItemCount) throw() = 0;

  //! Read a single item from the file
  /*!
    Reads one item of a user-specified type.
    If the item cannot be read, then an exception will be thrown and the file position
    incremented by an unspecified amount.
    \param T The type of item to read (usually determined automatically by the compiler)
    \param oDestination Variable to store the read item in
    \sa read() readArray() readOneNoThrow()
  */
  template <class T>
  void readOne(T& oDestination) throw(...)
  { read(&oDestination, sizeof(T), 1); }

  //! Read a single item from the file, without throwing an exception
  /*!
    Reads one item of a user-specified type.
    If the item cannot be read, then 0 is returned and the file position is unchanged.
    \param T The type of item to read (usually determined automatically by the compiler)
    \param oDestination Variable to store the read item in
    \return The number of items read (0 or 1)
    \sa readNoThrow() readOne() readArrayNoThrow()
  */
  template <class T>
  size_t readOneNoThrow(T& oDestination) throw()
  { return readNoThrow(&oDestination, sizeof(T), 1); }

  //! Read an array of items from the file
  /*!
    Reads a specified number of iterms of a user-specified type.
    If the requested number of items cannot be read, then an exception will be thrown
    and the file position incremented by an unspecified amount.
    \param T The type of item to read (usually determined automatically by the compiler)
    \param pDestination Pointer to an array of at-lest size iCount
    \param iCount Number of items to read
    \sa read() readOne() readOneNoThrow()
  */
  template <class T>
  void readArray(T* pDestination, size_t iCount) throw(...)
  { read(pDestination, sizeof(T), iCount); }

  //! Load the file as a Lua chunk
  /*!
    Reads the remainder of the file (i.e. the whole file if the file pointer is at the
    start) into a Lua state as a chunk.
    \param L The Lua state to load into
    \param sChunkName The name to give the loaded chunk
    \return Same values as ::lua_load(), with an error or chunk placed on the Lua stack
  */
  int lua_load(lua_State *L, const char* sChunkName) throw();

  //! Read an array of items from the file, without throwing an exception
  /*!
    Reads a specified number of iterms of a user-specified type.
    If the requested number of items cannot be read, then it will read as many
    as it can, incrementing the file position by that number of items.
    \param T The type of item to read (usually determined automatically by the compiler)
    \param pDestination Pointer to an array of at-lest size iCount
    \param iCount Number of items to read
    \return The number of items read (which may be less than the requested number)
    \sa readNoThrow() readOneNoThrow() readArray()
  */
  template <class T>
  size_t readArrayNoThrow(T* pDestination, size_t iCount) throw()
  { return readNoThrow(pDestination, sizeof(T), iCount); }

  virtual void write(const void* pSource, size_t iItemSize, size_t iItemCount) throw(...) = 0;
  virtual size_t writeNoThrow(const void* pSource, size_t iItemSize, size_t iItemCount) throw() = 0;

  template <class T>
  void writeOne(const T& oSource) throw(...)
  { write(&oSource, sizeof(T), 1); }

  template <class T>
  size_t writeOneThrow(const T& oSource) throw()
  { return writeNoThrow(&oSource, sizeof(T), 1); }

  template <class T>
  void writeArray(const T* pSource, size_t iCount) throw(...)
  { write(pSource, sizeof(T), iCount); }

  template <class T>
  size_t writeArrayNoThrow(const T* pSource, size_t iCount) throw()
  { return writeNoThrow(pSource, sizeof(T), iCount); }

  virtual void seek(seek_offset_t iOffset, seek_relative_t eRelativeTo) throw(...) = 0;
  virtual bool seekNoThrow(seek_offset_t iOffset, seek_relative_t eRelativeTo) throw() = 0;

  virtual seek_offset_t tell() throw() = 0;

  template <class T>
  RainString readString(T cDelimiter)
  {
    RainString r;
    T v;
    readOne(v);
    while(v != cDelimiter)
    {
      r += v;
      readOne(v);
    }
    return r;
  }
};

struct filesize_t
{
  unsigned long iUpper, iLower;
};

typedef time_t filetime_t; //!< Unix time stamp; seconds elapsed since Jan 1, 1970

struct RAINMAN2_API directory_item_t
{
  struct field_list_t
  {
    bool name : 1;
    bool dir : 1;
    bool size : 1;
    bool time : 1;

    field_list_t& operator= (bool value) throw();
  } oFields;

  RainString sName;
  bool bIsDirectory;
  filesize_t iSize;
  filetime_t iTimestamp;
};

class RAINMAN2_API IDirectory;

class RAINMAN2_API auto_directory_item
{
public:
  auto_directory_item() throw();
  auto_directory_item(const auto_directory_item&) throw();
  auto_directory_item(IDirectory*, size_t) throw();

  auto_directory_item& operator =  (const auto_directory_item&) throw();
                  bool operator == (const auto_directory_item&) const throw();
                  bool operator != (const auto_directory_item&) const throw();

  void setItem(IDirectory*, size_t) throw();

  bool isValid() const throw();
  IDirectory* getDirectory() const throw();
  size_t getIndex() const throw();

  RainString name() const throw(...);
  bool isDirectory() const throw(...);
  filesize_t size() const throw(...);
  filetime_t timestamp() const throw(...);

  IDirectory* open       ()                    const throw(...);
  IFile*      open       (eFileOpenMode eMode) const throw(...);
  IDirectory* openNoThrow()                    const throw();
  IFile*      openNoThrow(eFileOpenMode eMode) const throw();

protected:
  IDirectory* m_pDirectory;
  size_t m_iIndex;
  mutable directory_item_t m_oItem;
  mutable directory_item_t::field_list_t m_oFieldsPresent;
};

class RAINMAN2_API IFileStore;

class RAINMAN2_API IDirectory
{
public:
  //! virtual destructor
  virtual ~IDirectory() throw();
  
  //! Get the total number of items (files and sub-directories) within the directory
  virtual size_t getItemCount() throw() = 0;

  //! Get certain details about an item in the directory
  /*!
    \param iIndex A value in range [0, getItemCount), specifying the item to get details on
    \param oDetails The details marked as true in the oFields member will be updated to
      reflect the value of that detail for the requested item; other details will be left
      unchanged.
    Will throw an exception only if the index is out of range. Should file-specific details
    be requested for a directory, then an exception shall NOT be thrown, but the value
    assigned to those details is undefined.
  */
  virtual void getItemDetails(size_t iIndex, directory_item_t& oDetails) throw(...) = 0;

  virtual const RainString& getPath() throw() = 0;
  virtual IFileStore* getStore() throw() = 0;

  virtual IFile* openFile(size_t iIndex, eFileOpenMode eMode) throw(...);
  virtual IFile* openFileNoThrow(size_t iIndex, eFileOpenMode eMode) throw();
  virtual IDirectory* openDirectory(size_t iIndex) throw(...);
  virtual IDirectory* openDirectoryNoThrow(size_t iIndex) throw();

  //! Input iterator for easily traversing the contents of a directory
  /*!
    Use IDirectory::begin() and IDirectory::end() to get iterators for a
    particular directory. These iterators should be able to be used with
    any STL functions which expect input iterators, for example, the following
    construction will print a list of items in pDirectory to stdout:
    std::transform(pDirectory->begin(), pDirectory->end(),
      std::ostream_iterator<RainString, wchar_t>(std::wcout, L"\n"),
      std::mem_fun_ref(&auto_directory_item::name)
    );
    The following has a similar effect, but without much of the STL:
    for(IDirectory::iterator itr = pDirectory->begin(); itr != pDirectory->end(); ++itr)
      std::wcout << itr->name() << std::endl;
  */
  class RAINMAN2_API iterator
  {
  public:
    iterator() throw();
    iterator(const iterator&) throw();
    iterator(IDirectory*, size_t) throw();

    typedef std::input_iterator_tag iterator_category;
    typedef const auto_directory_item value_type;
    typedef ptrdiff_t difference_type;
    typedef value_type* pointer;
    typedef value_type& reference;

    iterator& operator =  (const iterator&) throw();
    iterator& operator ++ ()                throw();
    iterator& operator -- ()                throw();
    bool      operator == (const iterator&) const throw();
    bool      operator != (const iterator&) const throw();
    reference operator *  ()                const throw();
    pointer   operator -> ()                const throw();

  protected:
    auto_directory_item m_item;
  };

  iterator begin();
  iterator end();
};

//! Definition of the capabilities of a file store
/*!
  Call IFileStore::getCaps() to fill this structure
*/
struct RAINMAN2_API file_store_caps_t
{
  //! Default constructor; sets all capabilities flags to false
  file_store_caps_t() throw();

  //! Set all capabilities to the same value
  /*!
    A common pattern is to set all capabilities to false using this,
    then flag as true the capabilities which are supported. This is
    future-proof, as if any new capabilities are added later, then
    this method will cause them to be set to false.
  */
  file_store_caps_t& operator= (bool bValue) throw();

  bool bCanReadFiles : 1;         //!< true if files can be opened for reading
  bool bCanWriteFiles : 1;        //!< true if files can be opened for writing
  bool bCanDeleteFiles : 1;       //!< true if files can be deleted
  bool bCanOpenDirectories : 1;   //!< true if directories can be opened
  bool bCanCreateDirectories : 1; //!< true if directories can be created
  bool bCanDeleteDirectories : 1; //!< true if directories can be deleted
};

//! Interface for entities which contain files
/*!
  Any entity containing files (e.g. file archive, hard disks, etc.) must implement this
  interface to make the files within it available to the application.
*/
class RAINMAN2_API IFileStore
{
public:
  virtual ~IFileStore() throw(); //!< virtual destructor
  
  //! Get details of the capabilities of the file store
  /*!
  	Not all file stores support all operations, so it may be useful to query the file
  	store to determine what it can do.
  	\param oCaps A capabilites structure to be filled
  */
  virtual void getCaps(file_store_caps_t& oCaps) const throw() = 0;
  
  //! Open a file within the store for reading for writing
  /*!
  	\param sPath The full path to the file in question
  	\param eMode The mode (i.e. read or write) in which to open the file
  	\return A valid file interface pointer, which the caller must later delete
  */
  virtual IFile* openFile(const RainString& sPath, eFileOpenMode eMode) throw(...) = 0;

  //! Open a file within the store for reading for writing, never throwing an exception
  /*!
  	Same as openFile(), except it will never throw an exception, instead returning a null
  	pointer.
  	\param sPath The full path to the file in question
  	\param eMode The mode (i.e. read or write) in which to open the file
  	\return A valid file interface pointer, which the caller must later delete, or in the
  	  case of an error, a null pointer
  */
  virtual IFile* openFileNoThrow(const RainString& sPath, eFileOpenMode eMode) throw() = 0;

  virtual bool doesFileExist(const RainString& sPath) throw() = 0;
  
  //! Delete a file from the store
  /*!
  	\param sPath The full path to the file in question
  */
  virtual void deleteFile(const RainString& sPath) throw(...) = 0;
  
  //! Delete a file from the store, never throwing an exception
  /*!
    Same as deleteFile(), except it will never throw an exception, instead returning
    false
    \return true if the file was deleted, false in case of error
  */
  virtual bool deleteFileNoThrow(const RainString& sPath) throw() = 0;
  
  //! Get the number of entry points into the file store
  /*!
  	The root level folders in the file store are called entry points, which may corespond
  	to drive letters (win32 file system), tables of contents (SGA archives), etc.
  */
  virtual size_t getEntryPointCount() throw() = 0;
  
  //! Get the name of one of the entry points
  /*!
    Only throws an exception when the index is out of bounds.
  	\param iIndex A value between 0 and getEntryPointCount() - 1 (inclusive)
  	\return Name of the entry point, which can be passed to openDirectory()
  */
  virtual const RainString& getEntryPointName(size_t iIndex) throw(...) = 0;
  
  //! Open a directory within the store for enumeration
  /*!
  	\param sPath The full path to the directory in question (trailing slash optional)
  	\return A valid directory interface pointer, which the caller must later delete
  */
  virtual IDirectory* openDirectory(const RainString& sPath) throw(...) = 0;
  
  //! Open a directory within the store for enumeration, never throwing an exception
  /*!
  	Same as openDirectory(), except it will never throw an exception, instead returning a
  	null pointer.
  	\param sPath The full path to the directory in question (trailing slash optional)
  	\return A valid directory interface pointer, which the caller must later delete, or
  	  in the case of an error, a null pointer
  */
  virtual IDirectory* openDirectoryNoThrow(const RainString& sPath) throw() = 0;

  virtual bool doesDirectoryExist(const RainString& sPath) throw() = 0;

  virtual void createDirectory(const RainString& sPath) throw(...) = 0;
  virtual void createDirectoryNoThrow(const RainString& sPath) throw() = 0;
  virtual void deleteDirectory(const RainString& sPath) throw(...) = 0;
  virtual void deleteDirectoryNoThrow(const RainString& sPath) throw() = 0;
};

//! Implementation of the IFileStore interface for the standard physical filesystem
/*!
  RainGetFileSystemStore() can be used to get a instance of this class. Functions
  like RainOpenFile(), RainDeleteFile(), RainOpenDirectory() and their NoThrow
  equivalents can also be used instead of this class when an IFileStore interface
  is not required.
*/
class RAINMAN2_API FileSystemStore : public IFileStore
{
public:
  FileSystemStore() throw();
  ~FileSystemStore() throw();

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
  virtual void        createDirectoryNoThrow(const RainString& sPath) throw();
  virtual void        deleteDirectory       (const RainString& sPath) throw(...);
  virtual void        deleteDirectoryNoThrow(const RainString& sPath) throw();

public:
  void _ensureGotEntryPoints() throw();

  bool m_bKnowEntryPoints;
  std::vector<RainString> m_vEntryPoints;
};

RAINMAN2_API FileSystemStore* RainGetFileSystemStore();

RAINMAN2_API IFile* RainOpenFile(const RainString& sPath, eFileOpenMode eMode) throw(...);
RAINMAN2_API IFile* RainOpenFileNoThrow(const RainString& sPath, eFileOpenMode eMode) throw();
RAINMAN2_API void RainDeleteFile(const RainString& sPath) throw(...);
RAINMAN2_API bool RainDeleteFileNoThrow(const RainString& sPath) throw();
RAINMAN2_API IDirectory* RainOpenDirectory(const RainString& sPath) throw(...);
RAINMAN2_API IDirectory* RainOpenDirectoryNoThrow(const RainString& sPath) throw();

RAINMAN2_API filesize_t operator+ (const filesize_t& a, const filesize_t& b);
