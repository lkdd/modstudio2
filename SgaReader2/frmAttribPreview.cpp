/*
Copyright (c) 2009 Peter "Corsix" Cawley

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
#include "frmAttribPreview.h"
#include <wx/button.h>
#include <wx/splitter.h>
#include <wx/statline.h>
#ifdef RAINMAN2_USE_RBF

BEGIN_EVENT_TABLE(frmAttribPreview, wxFrame)
  EVT_SIZE(frmAttribPreview::onResize)
  EVT_TREE_ITEM_EXPANDING(wxID_ANY, frmAttribPreview::onTreeExpand)
  EVT_TREE_SEL_CHANGED(wxID_ANY, frmAttribPreview::onTreeSelect)
END_EVENT_TABLE()

struct AttribTreeItemData : public wxTreeItemData
{
  AttribTreeItemData(IAttributeValue *pValue, IAttributeTable *pTable)
    : pValue(pValue), pTable(pTable)
  {
  }

  ~AttribTreeItemData()
  {
    delete pValue;
    delete pTable;
  }

  IAttributeValue *pValue;
  IAttributeTable *pTable;
};

frmAttribPreview::frmAttribPreview()
  : wxFrame(NULL, wxID_ANY, L"Attrib Preview")
  , m_pFileStore(0)
{
  wxBoxSizer *pMainSizer = new wxBoxSizer(wxVERTICAL);

  {
    wxBoxSizer *pTopSizer = new wxBoxSizer(wxHORIZONTAL);
    pTopSizer->Add(new wxButton(this, BTN_PREV_FILE, L"<  Prev"), 0, wxEXPAND | wxALL, 2);
    pTopSizer->Add(m_pFilenameList = new wxComboBox(this, wxID_ANY), 1, wxALIGN_CENTER_VERTICAL | wxALL, 2);
    pTopSizer->Add(new wxButton(this, BTN_NEXT_FILE, L"Next  >"), 0, wxEXPAND | wxALL, 2);
    pMainSizer->Add(pTopSizer, 0, wxEXPAND);
  }
  pMainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 2);

  wxSplitterWindow *pSplitter = new wxSplitterWindow(this);
  m_pAttribTree = new wxTreeCtrl(pSplitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE);
  m_pPropGrid = new wxPropertyGrid(pSplitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxPG_SPLITTER_AUTO_CENTER | wxPG_HIDE_MARGIN);
  pSplitter->SplitVertically(m_pAttribTree, m_pPropGrid);
  pMainSizer->Add(pSplitter, 1, wxEXPAND | wxALL, 0);

  SetBackgroundColour(m_pAttribTree->GetBackgroundColour());

  SetSizer(pMainSizer);
  Layout();
}

frmAttribPreview::~frmAttribPreview()
{
}

void frmAttribPreview::setFileStore(IFileStore *pStore)
{
  m_pFileStore = pStore;
}

void frmAttribPreview::loadFile(const RainString& sFilename)
{
  try
  {
    std::auto_ptr<IFile> pFile(m_pFileStore->openFile(sFilename, FM_Read));
    m_oAttribFile.load(&*pFile);
  }
  CATCH_THROW_SIMPLE({}, L"Cannot load file as RBF data");

  m_pFilenameList->SetValue(sFilename);
  m_pAttribTree->DeleteAllItems();

  try
  {
    IAttributeTable *pTable = m_oAttribFile.getRootTable();
    wxTreeItemId oRoot = m_pAttribTree->AddRoot(L"GameData", -1, -1, new AttribTreeItemData(0, pTable));
    _addTreeItemChildren(oRoot);
    m_pAttribTree->SelectItem(oRoot);
  }
  CATCH_THROW_SIMPLE({}, L"Error populating attrib tree");
}

void frmAttribPreview::_addTreeItemChildren(wxTreeItemId oParent)
{
  AttribTreeItemData *pData = reinterpret_cast<AttribTreeItemData*>(m_pAttribTree->GetItemData(oParent));
  if(pData == 0 || pData->pTable == 0)
    return;
  
  unsigned long iCount = pData->pTable->getChildCount();
  for(unsigned long i = 0; i < iCount; ++i)
  {
    IAttributeValue *pValue = pData->pTable->getChild(i);
    if(!pValue->isTable())
    {
      delete pValue;
      continue;
    }
    IAttributeTable *pTable = pValue->getValueTable();
    wxTreeItemId oChild = m_pAttribTree->AppendItem(oParent, *RgdDictionary::getSingleton()->hashToString(pValue->getName()), -1, -1, new AttribTreeItemData(pValue, pTable));
    unsigned long iSubCount = pTable->getChildCount();
    for(unsigned long j = 0; j < iSubCount; ++j)
    {
      IAttributeValue *pChild = pTable->getChild(j);
      if(pChild->isTable())
      {
        delete pChild;
        m_pAttribTree->SetItemHasChildren(oChild);
        break;
      }
      delete pChild;
    }
  }
}

void frmAttribPreview::onTreeExpand(wxTreeEvent& e)
{
  wxTreeItemId oItem = e.GetItem();
  if(oItem.IsOk() && m_pAttribTree->GetChildrenCount(oItem, false) == 0)
  {
    try
    {
      _addTreeItemChildren(oItem);
    }
    CATCH_MESSAGE_BOX(L"Error expanding tree item", {});
  }
}

void frmAttribPreview::onTreeSelect(wxTreeEvent& e)
{
  m_pPropGrid->Clear();

  wxTreeItemId oItem = e.GetItem();
  if(!oItem.IsOk())
    return;
  AttribTreeItemData *pData = reinterpret_cast<AttribTreeItemData*>(m_pAttribTree->GetItemData(oItem));
  if(pData == 0 || pData->pTable == 0)
    return;

  unsigned long iCount = pData->pTable->getChildCount();
  for(unsigned long i = 0; i < iCount; ++i)
  {
    try
    {
      std::auto_ptr<IAttributeValue> pValue(pData->pTable->getChild(i));
      wxString sName = *RgdDictionary::getSingleton()->hashToString(pValue->getName());
      while(m_pPropGrid->GetProperty(sName))
        sName << L" ";
      wxPGProperty *pProp = 0;
      switch(pValue->getType())
      {
      case VT_Boolean:
        pProp = m_pPropGrid->Append(new wxBoolProperty(sName, wxPG_LABEL, pValue->getValueBoolean()));
        break;

      case VT_String:
        pProp = m_pPropGrid->Append(new wxStringProperty(sName, wxPG_LABEL, pValue->getValueString()));
        break;

      case VT_Integer:
        pProp = m_pPropGrid->Append(new wxIntProperty(sName, wxPG_LABEL, pValue->getValueInteger()));
        break;

      case VT_Float:
        pProp = m_pPropGrid->Append(new wxFloatProperty(sName, wxPG_LABEL, pValue->getValueFloat()));
        break;

      case VT_Table:
        pProp = m_pPropGrid->Append(new wxStringProperty(sName, wxPG_LABEL, L"(table)"));
        break;

      default:
        pProp = m_pPropGrid->Append(new wxStringProperty(sName, wxPG_LABEL, L"(unknown)"));
        break;
      };
      m_pPropGrid->SetPropertyReadOnly(pProp);
    }
    CATCH_MESSAGE_BOX_1(L"Error fetching child %lu", i, {});
  }
  m_pPropGrid->Refresh();
}

void frmAttribPreview::onResize(wxSizeEvent &e)
{
  Layout();
}

#endif
