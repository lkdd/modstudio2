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
#include "containers.h"
#include "attributes.h"
#include "memfile.h"
#include <unordered_map>
#ifdef RAINMAN2_USE_RBF

//! Reader for the RBF attribute format used in DoW2
/*!
  The RBF format is a binary file format which is used to store a table (in the
  Lua sense - an associative array), with the following structure:
    * file header
    * the table, key, data, data index and string arrays, in any order

  The file header has the following fields:
    * type and version - the 8 byte ASCII value "RBF V0.1"
    * offset of the table array from the file start - 32 bit unit
    * the number of entries in the table array - 32 bit unit
    * offset of the key array from the file start - 32 bit unit
    * the number of entries in the key array - 32 bit unit
    * offset of the data index array from the file start - 32 bit unit
    * the number of entries in the data index array - 32 bit unit
    * offset of the data array from the file start - 32 bit unit
    * the number of entries in the data array - 32 bit unit
    * offset of the string block from the file start - 32 bit unit
    * the number of bytes in the string block - 32 bit uint

  The table array contains one or more tables, each table having these fields:
    * number of table children - 32 bit uint
    * index of first child (indexes the data array directly if child count is 1,
      indexes the data index array if count is greater than 1) - 32 bit unit

  The key array contains zero or more keys, each of which is an ASCII string
  zero padded to be 64 bytes long. The data index array contains zero or more
  32 bit units, each of which being an index into the data array.

  The data array contains zero or more datum, each datum having these fields:
    * key index - 32 bit uint
    * data type - 32 bit uint
    * value - 32 bit int, uint (bool, table and string indicies) or float

  The string block contains zero or more ASCII strings, each of which is prefixed
  by a length field (32 bit uint) and is NOT zero terminated. Indicies into the
  string block are the offset of the start of the length field relative to the
  start of the string block.
*/
class RAINMAN2_API RbfAttributeFile
{
public:
  RbfAttributeFile() throw();
  ~RbfAttributeFile() throw();

  //! Get the filename previously set by setFilename
  const RainString& getFilename() throw() {return m_sFilename;}

  //! Enable or disable the sorting of table children
  /*!
    By default, sorting is enabled, meaning that when iterating the children of
    an RBF table, they are returned in alphabetical order rather than the order
    from the file (which appears to be psuedo-random). For the end-user, the
    alphabetical order is probably nicer, but access to the unmodified file is
    sometimes preferable.
  */
  void enableTableChildrenSort(bool bEnable = true) {m_bAutoSortTableChildren = bEnable;}

  //! Set the filename
  /*!
    This filename is then returned from all IAttributeValue::getFileSetIn() calls
    made on the values within the file. Note that the filename is not reset when
    load() is called, meaning that the filename can be set before or after a call
    to load().
  */
  void setFilename(const RainString& sFilename) {m_sFilename = sFilename;}

  //! Load a single .RBF file
  void load(IFile *pFile) throw(...);

  //! Get the root-level GameData table
  /*!
    The caller is responsible for deleting the returned pointer.
  */
  IAttributeTable* getRootTable() throw(...);

protected:
  friend class RbfAttrTableAdapter;
  friend class RbfAttrValueAdapter;
  friend class RbfWriter;
  friend class LuaRainmanRbfMethods; // LuaRainman has access to the internals for speed

#pragma pack(push)
#pragma pack(1)
  struct _header_raw_t
  {
    char sTypeAndVer[8]; // Should be "RBF V0.1"
    // Offset of, and number of entries in, the table array
    unsigned long iTablesOffset,
                  iTablesCount,
    // Offset of, and number of entries in, the keys array
                  iKeysOffset,
                  iKeysCount,
    // Offset of, and number of entries in, the data index array
                  iDataIndexOffset,
                  iDataIndexCount,
    // Offset of, and number of entries in, the data array
                  iDataOffset,
                  iDataCount,
    // Offset of, and number of bytes in, the string block
                  iStringsOffset,
                  iStringsLength;
  };

  struct _table_raw_t
  {
    // If count == 0, then no children
    // If count == 1, then child is iChildIndex in the data array
    // Otherwise, the range [iChildIndex, iChildIndex + iChildCount) in the data index array gives the indicies in the data array
    // Note that multiple children may share the same name, as is common in arrays. If there is a
    // $REF child, then it usually (always?) comes first. Remaining children have no apparent order.
    unsigned long iChildCount,
                  iChildIndex;
  };

  struct _data_raw_t
  {
    enum eTypes
    {
      // boolean; uValue is 1 or 0
      T_Bool = 0,
      // single-precision floating point; fValue
      T_Float,
      // 32-bit signed int; iValue
      T_Int,
      // ASCII string; iValue gives the position in the string block of the length of the string (as a 32-bit uint)
      //               iValue+4 is the position in the string block of the actual string data (NOT null terminated)
      T_String,
      // table; uValue is the index in the table array
      T_Table,
      // force eTypes to take 32 bits of storage
      T_force_dword = 0xffffffff
    } eType;

    // Index in the key array of the key
    unsigned long iKeyIndex;

    union
    {
     long iValue;
     unsigned long uValue;
     float fValue;
    };
  };
#pragma pack(pop)

  typedef char _key_raw_t[64];

  void _zeroSelf() throw();
  void _cleanSelf() throw();
  void _buildKeyOrdering() throw(...);

  // Contents of a .rbf file on disk
  _header_raw_t  m_oHeader;
  _table_raw_t  *m_pTables;
  _data_raw_t   *m_pData;
  _key_raw_t    *m_pKeys;
  unsigned long *m_pDataIndex;
  char          *m_sStringBlock;

  // Fields not on disk
  RainString     m_sFilename;
  unsigned long *m_pKeyOrdering;
  bool           m_bAutoSortTableChildren;
};

//! Writer for the RBF attribute format used in DoW2
/*!
  General usage of this class is as follows:
    * call initialise()
    * call pushTable() to begin the root table
    * for each key/value pair in the root table:
      * call setKey()
      * if value is a table, call pushTable(), repeat this process for that table, then call popTable()
      * call one of the setValue functions
      * call setTable()
    * call popTable() to end the root table
    * call writeToFile()
*/
class RAINMAN2_API RbfWriter
{
public:
  //! Standard constructor
  /*!
    initialise() should be called immediately on any newly constructed instance of this
    class. This two-stage construction is to seperate the code which can and cannot
    throw exceptions.
  */
  RbfWriter() throw();
  ~RbfWriter() throw();

  //! Initialise the write state
  /*!
    This method should be called exactly once immediately after class construction. Calling
    it at any other time will have no effect, nor will calling it more than once. Failure to
    call it will result in invalid output, and will also cause a segfault if setValueString
    is called.
  */
  void initialise() throw(...);

  //! Enable or disable caching of values
  /*!
    The RBF format allows many things to be re-used rather than being present multiple times.
    For example, if the same string value is used twice, it can be written to the file once
    and then referenced twice rather than being written to the file twice. This practice of
    only writing the minimum amount required is refered to as caching.

    Caching is enabled by default. Disabling caching will increase file size (unless there is
    no data suitable for caching). On the other hand, Relic's RBF files do not use caching,
    so if binary compatibility with them is required, caching should be disabled. Also note
    that keys are always cached, regardless of the value of this flag, as that is part of the
    RBF specification (and done by Relic).
  */
  void enableCaching(bool bEnable = true) {m_bEnableCaching = bEnable;}

  //! Set the key of the next key/value pair to be inserted
  /*!
    Keys are ASCII strings up to 64 characters in length - if iKeyLength is larger than 64,
    an exception will be thrown.

    The current key is saved when pushTable() is called, and then restored by popTable(),
    meaning that if the value of the key/value pair is a table, setKey() can be called
    before loading the table, even though the next pair to be inserted may be within the
    child table.
  */
  void setKey(const char *sKey, size_t iKeyLength) throw(...);

  //! Set the value of the next key/value pair to a boolean value
  void setValueBoolean(bool bValue) throw();

  //! Set the value of the next key/value pair to a floating point value
  void setValueFloat(float fValue) throw();

  //! Set the value of the next key/value pair to an integer value
  void setValueInteger(long iValue) throw();

  //! Set the value of the next key/value pair to an ASCII string value
  /*!
    If a unicode value is required, then the game's UCS mechanism should be used instead,
    with an index into the UCS (UniCode Strings) file being set with setValueInteger().
  */
  void setValueString(const char *sString, size_t iStringLength) throw(...);

  //! Set the value of the next key/value pair to a table value
  /*!
    \param iIndex Return value from popTable() specifying the table index to use
  */
  void setValueTable(unsigned long iIndex) throw();

  //! Store the current key/value pair in the current table
  /*!
    The current key/value pair is set by calling setKey() and one of:
      * setValueBoolean()
      * setValueFloat()
      * setValueInteger()
      * setValueString()
      * setValueTable()
    The current table is whichever table is on the top of the stack from calls to:
      * pushTable()
      * popTable()
    
    If called without a table on the stack, then an exception is thrown. Note that
    the current key/value pair is not reset after calling, meaning that if either
    of them are not changed before calling setTable() again, then the old values
    will be reused.
  */
  void setTable() throw(...);

  //! Create a new table and push it onto the table stack
  /*!
    Also pushes the current key onto a stack.
  */
  void pushTable() throw(...);

  //! Finish writing a table and pop it from the table stack
  /*!
    Also pops the key saved by the matching call to pushTable() and makes it the
    current key.
    
    \return The index of the table within the file, to be passed to setValueTable()
  */
  unsigned long popTable() throw(...);

  //! Rewrite an RBF file in the Relic style
  /*!
    Makes a copy of the data stored by this writer, and re-orders the various
    internal arrays in the process so that they conform to the Relic style. For
    example, in a Relic-made RBF file:
      * In the data index array, A, A[x] > A[y] implies x > y (and vice versa)
      * A table's children will be continuous in the data array (rendering the
        data index array fairly redundant)
      * If a table contains child tables, the indicies of the child tables will
        be continuous in the table array
    
    \param pDestination A fresh RbfWriter instance on which only initialise() has
      been called.

    After calling, this writer will be unchanged, and pDestination will contain
    a re-ordered copy of this writer's data.
  */
  void rewriteInRelicStyle(RbfWriter *pDestination) const throw(...);

  //! Writes an RBF file
  /*!
    Should only be called once the root table has been completely written (i.e.
    popTable() was called, leaving nothing in the table stack).

    Writing is done sequentially; no seeks are done.

    \param bRelicStyle Controls the order in which arrays are written to the RBF
    file (does not change the order of the _contents_ of the arrays, rewriteInRelicStyle()
    should be called to do that). When true, the order used is the same as Relic:
      tables, keys, data index, data, strings
    When false, arrays are ordered in decreasing size of their elements:
      keys (64 bytes each), data (12), tables (8), data index (4), strings (1)
  */
  void writeToFile(IFile *pFile, bool bRelicStyle = false) const throw(...);

  //! Get the number of unique keys used
  unsigned long getKeyCount() const {return m_vKeys.size();}

  unsigned long getTableCount() const {return m_vTables.size();}
  unsigned long getDataCount() const {return m_vData.size();}

  //! Get the number of entries in the data index array
  /*!
    This value may be smaller or larger or equal to the length of the data array.
  */
  unsigned long getDataIndexCount() const {return m_vDataIndex.size();}

  //! Get the number of unique strings in the string block
  /*!
    If caching is enabled, then the number of unique strings is equal to the
    number of strings. If caching is disabled, then repeated strings will be in
    the string block multiple times, but will only count once between them
    toward the unique string count.
  */
  unsigned long getStringCount() const {return m_mapStrings.size();}

  //! Get the number of bytes used by the string block
  /*!
    This includes the actual ASCII data and the bytes used to store string lengths.
    If caching is disabled, then all instances of a repeated string will be counted
    by this function (unlike getStringCount(), which works with unique strings).
  */
  unsigned long getStringsLength() const {return m_pStringBlock->getLengthUsed();}

  //! Get the number of bytes saved by caching
  unsigned long getAmountSavedByCache() const {return m_iAmountSavedByCaching;}

protected:
  typedef RainSimpleContainer<RbfAttributeFile::_key_raw_t, unsigned long> _keys_buffer_t;
  typedef RainSimpleContainer<RbfAttributeFile::_data_raw_t, unsigned long> _data_buffer_t;
  typedef RainSimpleContainer<RbfAttributeFile::_table_raw_t, unsigned long> _table_buffer_t;
  typedef RainSimpleContainer<unsigned long, unsigned long> _index_buffer_t;
  typedef RainSimpleContainer<unsigned long, unsigned long> _table_children_buffer_t;
  typedef RainSimpleContainer<std::pair<unsigned long, _table_children_buffer_t*> > _tables_in_progress_stack_t;

  //! Maps a CRC32 value to an array index (used for caching)
  typedef std::tr1::unordered_map<unsigned long, unsigned long> _cache_map_t;

  _tables_in_progress_stack_t m_stkTables;
  _keys_buffer_t m_vKeys;
  _data_buffer_t m_vData;
  _table_buffer_t m_vTables;
  _index_buffer_t m_vDataIndex;
  _cache_map_t m_mapKeys;
  _cache_map_t m_mapData;
  _cache_map_t m_mapStrings;
  RbfAttributeFile::_data_raw_t m_oDataValue;
  MemoryWriteFile* m_pStringBlock;
  size_t m_iAmountSavedByCaching;
  bool m_bEnableCaching;
};

#endif
