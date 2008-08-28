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
#include "memfile.h"
#include "attributes.h"
#include "buffering_streams.h"
#include <map>

class LuaAttribCache;
struct lua_State;
struct Proto;

//! Lua Attribute File
class RAINMAN2_API LuaAttrib
{
public:
  //! Simple constructor
  LuaAttrib() throw();

  //! Default destructor
  ~LuaAttrib() throw();

  //! Retrieve the cache being used to load required files
  /*!
    If no cache has been asigned, then one will be made, at which point an exception may
    be thrown due to memory allocation failing. If a new cache is made, then its base
    folder will be set to the one in this class, so it is advisable to call setBaseFolder
    as early as possible on a LuaAttrib object to ensure that a cache is not constructed
    before the base folder is set.
  */
  LuaAttribCache* getCache() throw(...);

  void setName(const RainString& sName) throw();
  RainString& getName() throw();

  //! Set the cache to use to load required files
  /*!
    Will fail if the class already has a cache (either from a previous call to setCache,
    or one constructed automatically by getCache).
    \param pCache Pointer to the cache to use
    \param bOwnCache If true, the class will delete the cahce in its destructor
  */
  void setCache(LuaAttribCache* pCache, bool bOwnCache) throw(...);

  //! Set the folder from which to load required files
  /*!
    This should be set incase the class has to construct a cache on its own - the value
    is not used otherwise.
  */
  void setBaseFolder(IDirectory *pFolder) throw();

  //! Load data into the GameData and MetaData tables from the given file
  void loadFromFile(IFile *pFile) throw(...);

  //! Save the GameData and MetaData tables to a file in text format
  void saveToTextFile(IFile *pFile) throw(...);

  //! Save the GameData table to a file in binary (RGD) format
  void saveToBinaryFile(IFile *pFile) throw(...);

  IAttributeValue* getGameData() throw(...);
  IAttributeValue* getMetaData() throw(...);

protected:
  friend class LuaAttribCache;
  friend class LuaAttribValueAdapter;
  friend class LuaAttribTableAdapter;

  struct _table_t;
  //! A structure capable of holding any value possible on the execution stack or in a table
  struct _value_t
  {
    //! Simple constructor
    _value_t();
    //! Copy constructor
    _value_t(const _value_t& oOther);
    //! Default destructor
    ~_value_t();
    //! Asignment operator
    _value_t& operator =(const _value_t& oOther);

    //! Free any memory used by the value, and set the value to nil
    void free();

    union
    {
      float fValue;       //!< The actual value when eType == T_Float (Lua uses doubles, but RGDs only store floats)
      const char* sValue; //!< The actual value when eType == T_String (the string itself is owned by the cache's lua_State)
      bool bValue;        //!< The actual value when eType == T_Boolean
      _table_t* pValue;   //!< Pointer to the table when  eType == T_Table
    };
    union
    {
      long iValue;        //!< The actual value when eType == T_Integer
      long iLength;       //!< The length of the string when eType == T_String
    };
    enum eTypes
    {
      T_Float,            //!< A single-precision floating point value
      T_String,           //!< A ASCII string value
      T_Boolean,          //!< A boolean value
      T_Integer,          //!< An integral value
      T_Table,            //!< A table value
      T_Nil,              //!< No value (should not be found in a table)
      T_ReferenceFn,      //!< A function to fetch GameData (should not be found in a table)
      T_InheritMetaFn,    //!< A function to fetch MetaData (should not be found in a table)
    } eType;
  };
  struct _table_t
  {
    _table_t(LuaAttrib* pFile);
    ~_table_t();
    unsigned long iReferenceCount;
    bool bTotallyUnchaged;
    const _table_t *pInheritFrom;
    LuaAttrib *pSourceFile;
    std::map<unsigned long, _value_t> mapContents;

    size_t writeToBinary(IFile* pFile) const;
    void writeToText(BufferingOutputStream<char>& oOutput, const char* sPrefix, size_t iPrefixLength);
  };

  //! Execute a Lua function prototype
  /*!
    Runs a heavily stripped down version of the Lua 5.1 virtual machine suitable for
    quickly evaluating Lua attribute scripts. Designed explicity for attribute scripts,
    and to run them quickly - will not run anything else.
    Supported opcodes: OP_MOVE, OP_LOADK, OP_LOADBOOL, OP_LOADNIL, OP_GETGLOBAL (Only
      for GameData, MetaData, Inherit, Reference and InheritMeta), OP_GETTABLE,
      OP_SETGLOBAL (Only for GameData and MetaData), OP_SETTABLE, OP_NEWTABLE, OP_NOT,
      OP_JMP, OP_CALL (Only with one return value), OP_RETURN
    Unsupported opcodes: OP_GETUPVAL, OP_SETUPVAL, OP_SELF, OP_ADD, OP_SUB, OP_MUL, 
      OP_DIV, OP_MOD, OP_POW, OP_UNM, OP_LEN, OP_CONCAT, OP_EQ, OP_LT, OP_LE, OP_TEST,
      OP_TESTSET, OP_TAILCALL, OP_FORLOOP, OP_FORPREP, OP_TFORLOOP, OP_SETLIST,
      OP_CLOSE, OP_CLOSURE, OP_VARARG
  */
  void _execute(Proto* pFunction);

  //! Load a constant from Lua
  /*!
    \param pDestination Value structure to hold the constant
    \param pFunction Function prototype containing the constant table
    \param iK Index of the constant in the function's constant table
  */
  void _loadk(_value_t *pDestination, Proto* pFunction, int iK);

  void _writeInheritLine(BufferingOutputStream<char>& oOutput, _value_t& oTable, bool bIsMetaData);

  //! Generate a hash code from a value
  unsigned long _hashvalue(_value_t* pValue);

  //! Take a table value and replace it with a new table value inheriting from the old one
  void _wraptable(_value_t* pValue) throw(...);

  _value_t m_oGameData;
  _value_t m_oMetaData;
  IDirectory *m_pDirectory;
  LuaAttribCache *m_pCache;
  bool m_bOwnCache;
  RainString m_sName;
};

class RAINMAN2_API LuaAttribCache
{
public:
  LuaAttribCache();
  ~LuaAttribCache();

  void setBaseFolder(IDirectory *pFolder);
  LuaAttrib::_value_t getGameData(const char* sFilename);
  LuaAttrib::_value_t getMetaData(const char* sFilename);
  lua_State* getLuaState();

  size_t writeTableToBinaryFile(const LuaAttrib::_table_t* pTable, IFile* pFile, bool bCacheResult);

protected:
  LuaAttrib* _performGet(const char* sFilename);

  std::map<unsigned long, LuaAttrib*> m_mapFiles;
  std::map<const LuaAttrib::_table_t*, MemoryWriteFile*> m_mapTables;
  lua_State *m_L;
  IDirectory *m_pDirectory;
  LuaAttrib::_value_t m_oEmptyTable;
  LuaAttrib* m_pEmptyFile;
};

//! Lua hashes of commonly used strings, calculated by the compiler at compile-time
/*!
  This allows for nice readable code when working with Lua string hashes, such that,
  for example, LuaCompileTimeHashes::Inherit can be written instead of 0x42b6997b.
  The former has semantic meaning, while the latter is a random number.

  The Lua code for computing a string's hash (from lua-5.1.4's lstring.c):
    unsigned int h = cast(unsigned int, l);  // seed
    size_t step = (l>>5)+1;  // if string is too long, don't hash all its chars
    size_t l1;
    for (l1=l; l1>=step; l1-=step)  // compute hash
      h = h ^ ((h<<5)+(h>>2)+cast(unsigned char, str[l1-1]));
  For short strings (< 32 characters), this basically becomes that last statement
  repeated for each character in reverse order, using the string length as the
  starting hash value.
*/
class RAINMAN2_API LuaCompileTimeHashes
{
public:
  // MSVC complains about constant overflows (which are what we want), so stop it for a while
#pragma warning (push)
#pragma warning (disable: 4307)

  // Template class for appending a character to a hash
  // Comes from the main body of Lua's hashing algorithm
  template <unsigned char _char, unsigned int _hash>
  struct hash_char { static const unsigned int _hash = _hash ^ ((_hash << 5) + (_hash >> 2) + _char); };

  // Define some macros to make the code shorter and bareable to read
#define hash hash_char<
#define u ,hash
#define x1 >::_hash
#define x2 x1 x1
#define x4 x2 x2
#define x8 x4 x4

  /* These are not elegant code-wise, but string literals are not compile-time constants, and as such
     the hashes need to calculated character by character, followed by the total length, then a number
     of x commands equal to the length
  */
  static const unsigned int Inherit     = hash'I'u'n'u'h'u'e'u'r'u'i'u't'                , 7 x4 x2 x1;
  static const unsigned int GameData    = hash'G'u'a'u'm'u'e'u'D'u'a'u't'u'a'            , 8 x8;
  static const unsigned int MetaData    = hash'M'u'e'u't'u'a'u'D'u'a'u't'u'a'            , 8 x8;
  static const unsigned int Reference   = hash'R'u'e'u'f'u'e'u'r'u'e'u'n'u'c'u'e'        , 9 x8 x1;
  static const unsigned int InheritMeta = hash'I'u'n'u'h'u'e'u'r'u'i'u't'u'M'u'e'u't'u'a',11 x8 x2 x1;

  // Remove the macros, as they are no longer needed
#undef hash
#undef u
#undef x1
#undef x2
#undef x4
#undef x8

  // Re-enable the constant overflow warning
#pragma warning (pop)
};
