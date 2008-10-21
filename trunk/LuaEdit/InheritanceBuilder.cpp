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
#include "InheritanceBuilder.h"
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lobject.h>
#include <lopcodes.h>
#include <lstate.h>
}
#include <memory>
#include <wx/progdlg.h>
#include "stdext.h"

InheritanceBuilder::InheritanceBuilder()
{
  m_pRootDirectory = 0;
  m_pTreeDestination = 0;
}

InheritanceBuilder::~InheritanceBuilder()
{
  _cleanItems();
}

void InheritanceBuilder::setSource(IDirectory *pDirectory) throw()
{
  m_pRootDirectory = pDirectory;
}

void InheritanceBuilder::setTreeCtrl(wxTreeCtrl *pTreeDestination) throw()
{
  m_pTreeDestination = pTreeDestination;
}

//#define LUA_TRACE_ALLOC

#ifdef LUA_TRACE_ALLOC
static void* tracing_lua_allocator(void *ud, void *ptr, size_t osize, size_t nsize)
{
  void* ret;
  if (nsize == 0)
  {
    free(ptr);
    ret = NULL;
  }
  else
    ret = realloc(ptr, nsize);
  fprintf(reinterpret_cast<FILE*>(ud), "%p (%li) => %p (%li)\n", ptr, static_cast<long>(osize), ret, static_cast<long>(nsize));
  return ret;
}
#endif

void InheritanceBuilder::doBuild() throw(...)
{
  CHECK_ASSERT(m_pRootDirectory != 0);
  CHECK_ASSERT(m_pTreeDestination != 0);

  m_iCountNonexistantFiles = 0;
  {
  m_pAllocator = 0;
#ifdef LUA_TRACE_ALLOC
  FILE *fTracingLog = fopen("lua_alloc_trace.log", "wt");
  L = lua_newstate(tracing_lua_allocator, reinterpret_cast<void*>(fTracingLog));
#else
  std::auto_ptr<LuaShortSharpAlloc> pAllocator(new LuaShortSharpAlloc);
  m_pAllocator = &*pAllocator;
  L = lua_newstate(LuaShortSharpAlloc::allocf, reinterpret_cast<void*>(m_pAllocator));
#endif
  _recurse(m_pRootDirectory, L"");
  lua_close(L);
#ifdef LUA_TRACE_ALLOC
  fclose(fTracingLog);
#endif
  m_pAllocator = 0;
  }

  if(m_iCountNonexistantFiles)
  {
    for(std::map<unsigned long, _item_t*>::iterator itr = m_mapAllItems.begin(); itr != m_mapAllItems.end(); ++itr)
    {
      if(!itr->second->bExists)
        THROW_SIMPLE_(L"Lua file \'%s\' was referenced by \'%s\' (and %i other files), but does not itself exist!",
                      itr->second->sPath.getCharacters(),
                      itr->second->vChildren[1]->sPath.getCharacters(),
                      static_cast<int>(itr->second->vChildren.size() - 1));
    }
  }

  const wxTreeItemId& oRoot = m_pTreeDestination->AddRoot(L"Inheritance Tree");
  std::for_each(m_vRootItems.begin(), m_vRootItems.end(), std::bind1st(std::mem_fun(&InheritanceBuilder::_addRootItemToTree), this));
  m_pTreeDestination->SortChildren(oRoot);
}

void InheritanceBuilder::_addRootItemToTree(InheritanceBuilder::_item_t* pItem)
{
  pItem->addToTree(m_pTreeDestination, m_pTreeDestination->GetRootItem());
}

void InheritanceBuilder::_recurse(IDirectory* pDirectory, RainString sPath) throw(...)
{
  wxProgressDialog* pProgress = 0;
  int iMaximum;
  if(sPath.isEmpty())
  {
    iMaximum = static_cast<int>(pDirectory->getItemCount());
    pProgress = new wxProgressDialog(L"Loading Luas", L"Please wait, initialising... ", iMaximum, 0, wxPD_APP_MODAL | wxPD_SMOOTH);
    pProgress->SetSize(pProgress->GetSize().Scale(2.0f, 1.0f));
  }
  for(IDirectory::iterator itr = pDirectory->begin(), end = pDirectory->end(); itr != end; ++itr)
  {
    if(pProgress)
    {
      int iNewIndex = static_cast<int>(itr->getIndex());
      /*
        I am really not happy with the visual appearance of this progress bar under vista (and perhaps
        the same is true with XP). It updates too quickly for some things (i.e. the large array of TYPE_
        folders) and visually, the bar lags behind the actual progress such that the text may say "28 of 30",
        but the bar would only be 1/4 of the way across. Cannot find reason or solution to this.
      */
      pProgress->Update(iNewIndex, wxString::Format(L"Loading folder %i of %i: ", iNewIndex + 1, iMaximum) + implicit_cast<wxString>(itr->name()));
    }
    if(itr->isDirectory())
    {
      IDirectory *pChild = 0;
      try
      {
        pChild = itr->open();
        _recurse(pChild, sPath + itr->name() + L"\\");
      }
      CATCH_THROW_SIMPLE_(delete pChild, L"Cannot build inheritance for folder \'%s\' due to sub-folder", sPath.getCharacters());
      delete pChild;
    }
    else
    {
      RainString sExtension = itr->name().afterLast('.').toLower();
      if(sExtension == L"lua" || sExtension == L"nil")
      {
        IFile *pFile = 0;
        try
        {
          pFile = itr->open(FM_Read);
          _doFile(pFile, sPath + itr->name());
        }
        CATCH_THROW_SIMPLE_(delete pFile, L"Cannot build inheritance for folder \'%s\' due to file", sPath.getCharacters());
        delete pFile;
      }
    }
  }
  if(pProgress)
    delete pProgress;
}

void InheritanceBuilder::_doFile(IFile* pFile, RainString sPath) throw(...)
{
  _item_t* pItem = 0;
  unsigned long iHash = CRCCaselessHashSimpleAsciiFromUnicode(sPath.getCharacters(), sPath.length());
  pItem = m_mapAllItems[iHash];
  if(!pItem)
  {
    CHECK_ALLOCATION(pItem = new (std::nothrow) _item_t);
    m_mapAllItems[iHash] = pItem;
    pItem->sPath = sPath;
    pItem->pParent = 0;
    pItem->bExists = true;
  }
  else if(!pItem->bExists)
  {
    --m_iCountNonexistantFiles;
    pItem->bExists = true;
  }

  int iErr = pFile->lua_load(L, "lua file");
  if(iErr)
    THROW_SIMPLE_(L"Cannot load lua file \'%s\': %S", sPath.getCharacters(), lua_tostring(L, -1));

  Proto *pPrototype = reinterpret_cast<Closure*>(const_cast<void*>(lua_topointer(L, -1)))->l.p;
  const char* sInheritedFileBuffer;
  size_t iInheritedFileLength;
  unsigned long iInheritedFileHash;
  try
  {
    iInheritedFileHash = _getInheritedFile(pPrototype, sInheritedFileBuffer, iInheritedFileLength);
  }
  CATCH_THROW_SIMPLE_({}, L"Cannot determine inheritance for lua file \'%s\'", sPath.getCharacters());
  if(iInheritedFileLength != 0)
  {
    _item_t *pParentItem = m_mapAllItems[iInheritedFileHash];
    if(!pParentItem)
    {
      CHECK_ALLOCATION(pParentItem = new (std::nothrow) _item_t);
      m_mapAllItems[iInheritedFileHash] = pParentItem;
      pParentItem->pParent = 0;
      pParentItem->sPath = RainString(sInheritedFileBuffer, iInheritedFileLength);
      pParentItem->bExists = false;
      ++m_iCountNonexistantFiles;
    }
    pParentItem->vChildren.push_back(pItem);
    pItem->pParent = pParentItem;
  }
  else
  {
    m_vRootItems.push_back(pItem);
  }

  lua_pop(L, 1);
  if(m_pAllocator->isPoolRunningLow())
    lua_gc(L, LUA_GCCOLLECT, 0);
}

unsigned long InheritanceBuilder::_getInheritedFile(Proto* pPrototype, const char*& sBuffer, size_t& iLength) throw(...)
{
  /*
    GameData = Inherit([[filename]])
    Will become the following opcodes:
    GETGLOBAL Inherit
    LOADK filename
    CALL 1 1
    SETGLOBAL GameData
    Thus we scan through the function looking for them, and extract the filename.
    These instructions are almost always the first instructions in the array. Thus
    placing the loop termination check at the end of the loop rather than in the
    'for' instruction saves on a few clock cycles for virtually every input.
  */
  Instruction* I = pPrototype->code;
  for(;; ++I)
  {
    if(GET_OPCODE(*I) == OP_GETGLOBAL && tsvalue(pPrototype->k + GETARG_Bx(*I))->hash == LuaCompileTimeHashes::Inherit
      && GET_OPCODE(I[1]) == OP_LOADK && GET_OPCODE(I[2]) == OP_CALL)
    {
      TString* str = rawtsvalue(pPrototype->k + GETARG_Bx(I[1]));
      sBuffer = getstr(&str->tsv);
      iLength = str->tsv.len;
      return CRCCaselessHashSimple(sBuffer, iLength);
    }
    else if(GET_OPCODE(*I) == OP_RETURN)
    {
      THROW_SIMPLE(L"Lua file does not contain Reference([[filename]])");
    }
  }
}

void InheritanceBuilder::_cleanItems()
{
  for(std::map<unsigned long, _item_t*>::iterator itr = m_mapAllItems.begin(); itr != m_mapAllItems.end(); ++itr)
    delete itr->second;
  m_mapAllItems.clear();
}

void InheritanceBuilder::_item_t::addToTree(wxTreeCtrl *pTree, const wxTreeItemId& oParent)
{
  int iIcon = (sPath.afterLast('.').toLower() == L"nil") ? 0 : 1;
  InheritTreeItemData* pData = new (std::nothrow) InheritTreeItemData;
  if(pData)
    pData->sPath = sPath;
  RainString sTitle = sPath.afterLast('\\').beforeLast('.').toLower();
  wxTreeItemId oId = pTree->AppendItem(oParent, sTitle.getCharacters(), iIcon, iIcon, pData);
  for(std::vector<_item_t*>::iterator itr = vChildren.begin(); itr != vChildren.end(); ++itr)
    (*itr)->addToTree(pTree, oId);
  pTree->SortChildren(oId);
}
