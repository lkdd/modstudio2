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
// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWidgets headers)
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif
// ----------------------------
#include <wx/treectrl.h>
#include <Rainman2.h>
#include <vector>
#include <map>
#include "CustomLuaAlloc.h"
#include "TreeCtrlMemory.h"

struct lua_State;
struct Proto;

class InheritTreeItemData : public wxTreeItemData
{
public:
  InheritTreeItemData()
    : pRememberedState(0)
  {}

  virtual ~InheritTreeItemData()
  {
    delete pRememberedState;
  }

  RainString sPath;
  ITreeCtrlMemory *pRememberedState;
};

class InheritanceBuilder
{
public:
  InheritanceBuilder();
  ~InheritanceBuilder();

  void setSource(IDirectory *pDirectory) throw();
  void setTreeCtrl(wxTreeCtrl *pTreeDestination) throw();

  void doBuild() throw(...);

protected:
  void _cleanItems();
  void _recurse(IDirectory* pDirectory, RainString sPath) throw(...);
  void _doFile(IFile* pFile, RainString sPath) throw(...);
  unsigned long _getInheritedFile(Proto* pProto, const char*& sBuffer, size_t& iLength) throw(...);

  IDirectory *m_pRootDirectory;
  wxTreeCtrl *m_pTreeDestination;
  LuaShortSharpAlloc *m_pAllocator;
  lua_State *L;
  size_t m_iCountNonexistantFiles;

  struct _item_t
  {
    void addToTree(wxTreeCtrl *pTree, const wxTreeItemId& oParent);
    RainString sPath;
    std::vector<_item_t*> vChildren;
    _item_t* pParent;
    bool bExists;
  };
  std::map<unsigned long, _item_t*> m_mapAllItems;
  std::vector<_item_t*> m_vRootItems;

  void _addRootItemToTree(_item_t* pItem);
};
