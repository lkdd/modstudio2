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
#include "TreeCtrlMemory.h"
#include <Rainman2.h>

ITreeCtrlMemory::~ITreeCtrlMemory() {}

struct RememberedChildren;

struct RememberedChild
{
  RememberedChild()
  {
    pChildren = 0;
    bExpanded = false;
    bSelected = false;
    bVisible = false;
  }

  ~RememberedChild();

  wxString sLabel;
  RememberedChildren *pChildren;
  bool bExpanded;
  bool bSelected;
  bool bVisible;
};

struct RememberedChildren
{
  RememberedChildren()
  {
    pChildren = 0;
    iCount = 0;
  }

  ~RememberedChildren()
  {
    delete[] pChildren;
  }

  RememberedChild *pChildren;
  size_t iCount;
};

RememberedChild::~RememberedChild()
{
  if(pChildren)
    delete pChildren;
}

class TreeCtrlMemory : public ITreeCtrlMemory
{
public:
  TreeCtrlMemory()
  {
    m_pRoot = 0;
    m_bIsRootExpanded = false;
  }

  virtual ~TreeCtrlMemory()
  {
    delete m_pRoot;
  }

  virtual void store(wxTreeCtrl *pTree) throw(...)
  {
    delete m_pRoot;
    m_pRoot = 0;
    m_bIsRootExpanded = false;
    bool bTmp;
    m_pRoot = storeRecursive(pTree, pTree->GetRootItem(), &bTmp);
    m_bIsRootExpanded = pTree->IsExpanded(pTree->GetRootItem());
  }

  virtual void retrieve(wxTreeCtrl *pTree) throw(...)
  {
    pTree->Freeze();
    try
    {
      if(m_bIsRootExpanded)
        pTree->Expand(pTree->GetRootItem());
      pTree->SelectItem(pTree->GetRootItem());
      if(m_pRoot)
        retrieveRecursive(pTree, pTree->GetRootItem(), m_pRoot);
    }
    catch(...)
    {
      pTree->Thaw();
      throw;
    }
    pTree->Thaw();
  }

  bool doesItemNeedStorage(wxTreeCtrl *pTree, const wxTreeItemId& oItem)
  {
    return pTree->IsExpanded(oItem) || pTree->IsVisible(oItem) || pTree->IsSelected(oItem) || pTree->GetChildrenCount(oItem, false) > 0;
  }

  RememberedChildren* storeRecursive(wxTreeCtrl *pTree, const wxTreeItemId& oParent, bool *pIsSelected)
  {
    wxTreeItemIdValue oCookie;
    wxTreeItemId oChild;
    size_t iCount = 0;

    for(oChild = pTree->GetFirstChild(oParent, oCookie); oChild.IsOk(); oChild = pTree->GetNextChild(oParent, oCookie))
    {
      if(doesItemNeedStorage(pTree, oChild))
        ++iCount;
    }

    if(iCount == 0)
      return 0;

    RememberedChildren *pRemembered = new RememberedChildren;
    pRemembered->iCount = iCount;
    pRemembered->pChildren = new RememberedChild[iCount];

    iCount = 0;
    bool bAnythingSelected = false;
    for(oChild = pTree->GetFirstChild(oParent, oCookie); oChild.IsOk(); oChild = pTree->GetNextChild(oParent, oCookie))
    {
      if(doesItemNeedStorage(pTree, oChild))
      {
        RememberedChild *pChild = pRemembered->pChildren + iCount++;
        pChild->sLabel = pTree->GetItemText(oChild);
        pChild->bExpanded = pTree->IsExpanded(oChild);
        pChild->bSelected = pTree->IsSelected(oChild);
        pChild->bVisible = pTree->IsVisible(oChild);
        pChild->pChildren = storeRecursive(pTree, oChild, &pChild->bSelected);
        bAnythingSelected = bAnythingSelected || pChild->bSelected;
      }
    }
    if(bAnythingSelected)
      *pIsSelected = true;

    return pRemembered;
  }

  void retrieveRecursive(wxTreeCtrl *pTree, const wxTreeItemId& oParent, RememberedChildren *pRemembered)
  {
    size_t iRememberedIndex = 0;
    RememberedChild *pChild = pRemembered->pChildren;
    wxTreeItemIdValue oCookie;
    wxTreeItemId oChild;

    for(oChild = pTree->GetFirstChild(oParent, oCookie); oChild.IsOk() && iRememberedIndex < pRemembered->iCount; oChild = pTree->GetNextChild(oParent, oCookie))
    {
      if(pTree->GetItemText(oChild) == pChild->sLabel)
      {
        if(pChild->bSelected)
          pTree->SelectItem(oChild);

        if(pChild->pChildren && pTree->ItemHasChildren(oChild))
        {
          pTree->Expand(oChild); // need to expand to get load-on-demand children
          retrieveRecursive(pTree, oChild, pChild->pChildren);
          if(!pChild->bExpanded)
            pTree->Collapse(oChild);
        }
        else if(pChild->bExpanded)
          pTree->Expand(oChild);

        if(pChild->bVisible)
          pTree->EnsureVisible(pChild);

        ++iRememberedIndex;
        ++pChild;
      }
    }
  }

protected:
  RememberedChildren *m_pRoot;
  bool m_bIsRootExpanded;
};

ITreeCtrlMemory* CreateTreeCtrlMemory()
{
  return new TreeCtrlMemory;
}
