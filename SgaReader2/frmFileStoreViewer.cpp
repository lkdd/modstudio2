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
#include "application.h"
#include "frmFileStoreViewer.h"
#include "frmExtract.h"
#include <wx/datetime.h>
#include <wx/display.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/menu.h>
#include <wx/mimetype.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <memory>

BEGIN_EVENT_TABLE(frmFileStoreViewer, wxFrame)
  EVT_LIST_ITEM_ACTIVATED(LST_DETAILS, frmFileStoreViewer::onFileDoAction )
  EVT_MENU               (wxID_OPEN  , frmFileStoreViewer::onOpen         )
  EVT_MENU               (wxID_EXIT  , frmFileStoreViewer::onExit         )
  EVT_MENU               (wxID_ABOUT , frmFileStoreViewer::onAbout        )
  EVT_SIZE               (             frmFileStoreViewer::onResize       )
  EVT_TREE_ITEM_EXPANDING(TREE_FILES , frmFileStoreViewer::onTreeExpanding)
  EVT_TREE_SEL_CHANGED   (TREE_FILES , frmFileStoreViewer::onTreeSelection)
END_EVENT_TABLE()

class SGATreeData : public wxTreeItemData
{
public:
  SGATreeData(const RainString& sPath, bool bLoaded = false)
    : m_sPath(sPath), m_bLoaded(bLoaded)
  {
  }

  const RainString& getPath() const {return m_sPath;}
  bool isLoaded() const {return m_bLoaded;}

  void setLoaded(bool bLoaded = true) {m_bLoaded = bLoaded;}

protected:
  RainString m_sPath;
  bool m_bLoaded;
};

frmFileStoreViewer::frmFileStoreViewer(const wxString& sTitle)
  : wxFrame(NULL, wxID_ANY, sTitle)
  , m_pArchive(0)
  , m_sBaseTitle(sTitle)
{
  m_sDateFormat = TheConfig[L"Dates"][L"Format"].value();
  if(m_sDateFormat.IsEmpty())
  {
    m_sDateFormat = L"%d/%m/%Y %H:%M:%S";
  }

  {
    m_pIcons = new wxImageList(16, 16);
    wxBitmap oIcons(L"IDB_ICONS", wxBITMAP_TYPE_BMP_RESOURCE);
    for(int x = 0; x < oIcons.GetWidth(); x += 16)
    {
      m_pIcons->Add(oIcons.GetSubBitmap(wxRect(x, 0, 16, 16)), wxColour(255, 0, 255));
    }
  }

  wxSizer *pMainSizer = new wxBoxSizer(wxVERTICAL);

  CreateStatusBar()->SetStatusText(L"Ready");

  wxMenuBar *pMenuBar = new wxMenuBar;
  {
    wxMenu *pFileMenu = new wxMenu;
    pMenuBar->Append(pFileMenu, L"&File");
    pFileMenu->Append(wxID_OPEN, wxEmptyString, L"Opens an SGA archive for browsing");
    pFileMenu->AppendSeparator();
    IniFile::Section& oRecent = TheConfig[L"Recent"];
    if(!oRecent.empty())
    {
      int i = 1;
      for(IniFile::Section::iterator itr = oRecent.begin(), itrEnd = oRecent.end(); itr != itrEnd && i <= 5; ++itr, ++i)
      {
        pFileMenu->Append(wxID_FILE1 - 1 + i, RainString().printf(L"%i ", i) + itr->value(), RainString(L"Opens ") + itr->value());
      }
      pFileMenu->AppendSeparator();
    }
    pFileMenu->Append(wxID_EXIT, wxEmptyString, L"Closes the application");
  }
  {
    wxMenu *pHelpMenu = new wxMenu;
    pMenuBar->Append(pHelpMenu, L"&Help");
    pHelpMenu->Append(wxID_ABOUT, wxEmptyString, L"Shows details about the application");
  }
  SetMenuBar(pMenuBar);

  wxSplitterWindow *pSplitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
  wxPanel *pRightPanel = new wxPanel(pSplitter);
  m_pFilesTree = new wxTreeCtrl(pSplitter, TREE_FILES, wxDefaultPosition, wxDefaultSize, wxBORDER_SUNKEN | wxTR_HAS_BUTTONS | wxTR_HIDE_ROOT | wxTR_SINGLE | wxTR_LINES_AT_ROOT);
  m_pFilesTree->SetImageList(m_pIcons);
  m_pDetailsView = new wxListCtrl(pRightPanel, LST_DETAILS, wxDefaultPosition, wxDefaultSize, wxBORDER_SUNKEN | wxLC_REPORT | /*wxLC_VIRTUAL |*/ wxLC_SINGLE_SEL);
  m_pDetailsView->SetImageList(m_pIcons,  wxIMAGE_LIST_SMALL);
  m_pDetailsView->InsertColumn(0, L"Name");
  m_pDetailsView->InsertColumn(1, L"Date modified");
  m_pDetailsView->InsertColumn(2, L"Type");
  m_pDetailsView->InsertColumn(3, L"Size");
  for(int i = 0; i < 4; ++i)
  {
    m_pDetailsView->SetColumnWidth(i, wxLIST_AUTOSIZE_USEHEADER);
  }
  m_pDetailsView->SetColumnWidth(0, m_pDetailsView->GetColumnWidth(0) * 5);
  m_pDetailsView->SetColumnWidth(1, m_pDetailsView->GetColumnWidth(1) * 3 / 2);
  m_pDetailsView->SetColumnWidth(2, m_pDetailsView->GetColumnWidth(2) * 3);
  m_pDetailsView->SetColumnWidth(3, m_pDetailsView->GetColumnWidth(3) * 3);

  wxString aActions[] = {
    L"Navigate",
    L"Extract",
  };

  wxSizer *pRightSizer = new wxBoxSizer(wxVERTICAL);
  wxSizer *pDefaultActionSizer = new wxBoxSizer(wxHORIZONTAL);
  pDefaultActionSizer->Add(new wxStaticText(pRightPanel, wxID_ANY, L"Default directory action:"), 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
  pDefaultActionSizer->Add(m_pDirectoryDefaultActionList = new wxChoice(pRightPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, 2, aActions), 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 2);
  m_pDirectoryDefaultActionList->SetSelection(0);
  pDefaultActionSizer->AddStretchSpacer(1);
  pDefaultActionSizer->Add(new wxStaticText(pRightPanel, wxID_ANY, L"Default file action:"), 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
  pDefaultActionSizer->Add(m_pFileDefaultActionList = new wxChoice(pRightPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, 1, aActions + 1), 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 2);
  m_pFileDefaultActionList->SetSelection(0);
  pDefaultActionSizer->AddStretchSpacer(1);
  pRightSizer->Add(pDefaultActionSizer, 0, wxEXPAND);
  pRightSizer->Add(m_pDetailsView, 1, wxEXPAND);
  pRightSizer->SetSizeHints(pRightPanel);
  pRightPanel->SetSizer(pRightSizer);

  pSplitter->SplitVertically(m_pFilesTree, pRightPanel, 260);
  pMainSizer->Add(pSplitter, 1, wxALL | wxEXPAND, 0);

  pMainSizer->SetSizeHints(this);
  SetSizer(pMainSizer);

  {
    wxRect rcScreen = wxDisplay(wxDisplay::GetFromPoint(GetPosition())).GetClientArea();
    int iWidth  = (rcScreen.width  * 3) / 4;
    int iHeight = (rcScreen.height * 3) / 4;
    SetSize(rcScreen.x + (rcScreen.width - iWidth) / 2, rcScreen.y + (rcScreen.height - iHeight) / 2, iWidth, iHeight);
  }
  Layout();
}

frmFileStoreViewer::~frmFileStoreViewer()
{
  delete m_pIcons;
  if(m_pArchive)
  {
    GetStatusBar()->SetStatusText(L"Closing " + m_sCurrentShortName + L"...");
    delete m_pArchive;
  }
  TheConfig["Dates"]["Format"] = m_sDateFormat;
}

void frmFileStoreViewer::onExit(wxCommandEvent &e)
{
  Close();
}

void frmFileStoreViewer::onOpen(wxCommandEvent &e)
{
  wxFileDialog oFileSelector(this, wxFileSelectorPromptStr, wxEmptyString, wxEmptyString, wxEmptyString, wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_CHANGE_DIR);
  oFileSelector.SetMessage(L"Choose an archive");
  oFileSelector.SetWildcard(L"SGA archives (*.sga)|*.sga|All files (*.*)|*.*");
  oFileSelector.SetFilterIndex(0);
  if(oFileSelector.ShowModal() == wxID_CANCEL)
    return;
  else
    _doLoadArchive(oFileSelector.GetPath());
}

void frmFileStoreViewer::onFileDoAction(wxListEvent &e)
{
  long iIndex = e.GetIndex();
  if(iIndex < 0 || iIndex >= m_pDetailsView->GetItemCount() || !m_oCurrentFolderId.IsOk())
    return;

  SGATreeData *pFolderData = reinterpret_cast<SGATreeData*>(m_pFilesTree->GetItemData(m_oCurrentFolderId));

  switch(m_pDetailsView->GetItemData(iIndex))
  {
  case 0: // up
    {
      wxTreeItemId oParent = m_pFilesTree->GetItemParent(m_oCurrentFolderId);
      if(oParent.IsOk())
      {
        m_pFilesTree->SelectItem(oParent);
        m_pFilesTree->SetFocus();
      }
      break;
    }

  case 1: // folder
    if(m_pDirectoryDefaultActionList->GetSelection() == 0)
    {
      if(!m_pFilesTree->IsExpanded(m_oCurrentFolderId))
        m_pFilesTree->Expand(m_oCurrentFolderId);
      wxTreeItemIdValue oCookie;
      wxTreeItemId oChild = m_pFilesTree->GetFirstChild(m_oCurrentFolderId, oCookie);
      wxString sLookingFor = m_pDetailsView->GetItemText(iIndex);
      while(oChild.IsOk())
      {
        if(m_pFilesTree->GetItemText(oChild) == sLookingFor)
        {
          m_pFilesTree->SelectItem(oChild);
          m_pFilesTree->SetFocus();
          break;
        }
        oChild = m_pFilesTree->GetNextChild(m_oCurrentFolderId, oCookie);
      }
      break;
    }
    // fall-through

  case 2: // file
    frmExtract oExtractor(this, m_pArchive, m_sCurrentArchivePath, pFolderData->getPath() + L"\\" + RainString(m_pDetailsView->GetItemText(iIndex)), GetStatusBar());
    oExtractor.ShowModal();
    break;
  }
}

void frmFileStoreViewer::_doLoadArchive(wxString sPath)
{
  if(m_pArchive)
  {
    GetStatusBar()->SetStatusText(L"Closing " + m_sCurrentShortName + L"...");
    delete m_pArchive;
    m_pArchive = 0;
    m_oCurrentFolderId = wxTreeItemId();
    m_pFilesTree->DeleteAllItems();
    m_pDetailsView->DeleteAllItems();
  }

  {
    wxFileName oFileName(sPath);
    m_sCurrentShortName = oFileName.GetFullName();
    m_sCurrentArchivePath = RainString(oFileName.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR));
  }

  GetStatusBar()->SetStatusText(L"Loading " + m_sCurrentShortName + L"...");

  SetTitle(m_sBaseTitle);
  RainString sArchiveFile = sPath;
  try
  {
    CHECK_ALLOCATION(m_pArchive = new (std::nothrow) SgaArchive);
    m_pArchive->init(RainOpenFile(sArchiveFile, FM_Read));
  }
  CATCH_MESSAGE_BOX_1(L"Unable to open archive file \'%s\'", sArchiveFile.getCharacters(), {
    delete m_pArchive;
    m_pArchive = 0;
    return;
  });

  SetTitle(m_sBaseTitle + L" - " + m_sCurrentShortName);

  wxTreeItemId oRoot = m_pFilesTree->AddRoot(wxEmptyString);
  size_t iEntryCount = m_pArchive->getEntryPointCount();
  if(iEntryCount == 0)
  {
    ::wxMessageBox(L"Archive was loaded successfully, but it contains no files.", L"Open archive", wxICON_INFORMATION, this);
    return;
  }
  wxTreeItemId oEntry;
  for(size_t i = 0; i < iEntryCount; ++i)
  {
    const RainString& sName = m_pArchive->getEntryPointName(i);
    oEntry = m_pFilesTree->AppendItem(oRoot, sName, 1, -1, new SGATreeData(sName));
    m_pFilesTree->SetItemHasChildren(oEntry);
    m_pFilesTree->Expand(oEntry);
  }
  m_pFilesTree->SelectItem(oEntry);
  GetStatusBar()->SetStatusText(L"Loaded " + m_sCurrentShortName + wxString::Format(L", %u files in %u directories", m_pArchive->getFileCount(), m_pArchive->getDirectoryCount()));
}

void frmFileStoreViewer::onAbout(wxCommandEvent &e)
{
  ::wxMessageBox(
    L"SGA Reader 2 by Corsix, part of the Mod Studio 2 suite\n"
    L"Available from http://code.google.com/p/modstudio2/\n"
    L"Uses wxWidgets, Rainman2 and famfamfam \'silk\' icons"
  , m_sBaseTitle, wxICON_INFORMATION, this);
}

void frmFileStoreViewer::onResize(wxSizeEvent &e)
{
  Layout();
}

void frmFileStoreViewer::onTreeSelection(wxTreeEvent &e)
{
  m_pDetailsView->DeleteAllItems();

  wxTreeItemId oItem = e.GetItem();
  if(!oItem.IsOk())
    return;

  m_oCurrentFolderId = oItem;

  SGATreeData *pData = reinterpret_cast<SGATreeData*>(m_pFilesTree->GetItemData(oItem));
  wxString sDirectory(L"Directory");

  std::auto_ptr<IDirectory> pDirectory;
  try
  {
    if(pData->getPath().indexOf('\\') != RainString::NOT_FOUND)
    {
      m_pDetailsView->InsertItem(0, L"Up one level");
      m_pDetailsView->SetItem(0, 1, wxEmptyString);
      m_pDetailsView->SetItem(0, 2, sDirectory);
      m_pDetailsView->SetItem(0, 3, wxEmptyString);
      m_pDetailsView->SetItemImage(0, 0);
      m_pDetailsView->SetItemData(0, 0);
    }

    pDirectory.reset(m_pArchive->openDirectory(pData->getPath()));
    for(IDirectory::iterator itr = pDirectory->begin(), itrEnd = pDirectory->end(); itr != itrEnd; ++itr)
    {
      long iIndex = m_pDetailsView->GetItemCount();
      m_pDetailsView->InsertItem(iIndex, itr->name());

      wxString sType;
      int iIcon;
      if(itr->isDirectory())
      {
        m_pDetailsView->SetItem(iIndex, 1, wxEmptyString);
        sType = sDirectory;
        m_pDetailsView->SetItem(iIndex, 3, wxEmptyString);
        m_pDetailsView->SetItemData(iIndex, 1);
        iIcon = 2;
      }
      else
      {
        m_pDetailsView->SetItemData(iIndex, 2);
        wxDateTime oTimestamp(itr->timestamp());
        m_pDetailsView->SetItem(iIndex, 1, oTimestamp.Format(m_sDateFormat));

        wxString sExt = itr->name().afterLast('.').toUpper();
        sType = sExt + L" file";
        iIcon = 3;
        bool bGotType = false;

        if(!bGotType)
        {
          const IniFile::Section& oSection = TheConfig[L"Types"];
          if(oSection.hasValue(sExt, false))
          {
            sType = oSection[sExt].value();
            bGotType = true;
            if(sType.Find(';') != wxNOT_FOUND)
            {
              wxString sIconIndex = sType.BeforeFirst(';');
              long iIconIndexLong;
              if(sIconIndex.ToLong(&iIconIndexLong))
              {
                iIcon = static_cast<int>(iIconIndexLong);
                sType = sType.AfterFirst(';');
              }
            }
          }
        }
        if(!bGotType)
        {
          wxFileType *pType = wxTheMimeTypesManager->GetFileTypeFromExtension(sExt);
          if(pType)
          {
            wxString sDesc;
            if(pType->GetDescription(&sDesc) && !sDesc.IsEmpty())
            {
              sType = sDesc;
              bGotType = true;
            }
          }
        }

        const wchar_t* aSuffixs[] = {L"Bytes", L"KB", L"MB", L"GB", NULL};
        int iSuffix = 0;
        double fSize = static_cast<double>(itr->size().iLower) + static_cast<double>(itr->size().iUpper) * 4294967296.0;
        while(aSuffixs[iSuffix + 1] && fSize >= 1024.0)
        {
          ++iSuffix;
          fSize = fSize / 1024;
        }
        m_pDetailsView->SetItem(iIndex, 3, wxString::Format(L"%.1f %s", fSize, aSuffixs[iSuffix]));
      }
      m_pDetailsView->SetItem(iIndex, 2, sType);
      m_pDetailsView->SetItemImage(iIndex, iIcon);
    }
  }
  CATCH_MESSAGE_BOX(L"Unable to list directory contents", {});
}

void frmFileStoreViewer::onTreeExpanding(wxTreeEvent &e)
{
  wxTreeItemId oItem = e.GetItem();
  SGATreeData *pData = reinterpret_cast<SGATreeData*>(m_pFilesTree->GetItemData(oItem));
  if(!pData->isLoaded())
  {
    std::auto_ptr<IDirectory> pDirectory;
    try
    {
      pDirectory.reset(m_pArchive->openDirectory(pData->getPath()));
      for(IDirectory::iterator itr = pDirectory->begin(), itrEnd = pDirectory->end(); itr != itrEnd; ++itr)
      {
        if(itr->isDirectory())
        {
          wxTreeItemData *pNewData = new SGATreeData(pData->getPath() + L"\\" + itr->name());
          wxTreeItemId oChild = m_pFilesTree->AppendItem(oItem, itr->name(), 2, -1, pNewData);
          IDirectory *pChild = 0;
          try
          {
            pChild = itr->open();
            for(IDirectory::iterator itr = pChild->begin(), itrEnd = pChild->end(); itr != itrEnd; ++itr)
            {
              if(itr->isDirectory())
              {
                m_pFilesTree->SetItemHasChildren(oChild);
                break;
              }
            }
            delete pChild;
          }
          catch(RainException *pE)
          {
            delete pE;
            m_pFilesTree->SetItemHasChildren(oChild); // best guess
          }
        }
      }
      pData->setLoaded();
    }
    CATCH_MESSAGE_BOX(L"Unable to determine directory children", {});
  }
}
