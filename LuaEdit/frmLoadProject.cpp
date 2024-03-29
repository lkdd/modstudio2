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
#include "frmLoadProject.h"
#include "frmNewProject.h"
#include "frmEditor.h"
#include "stdext.h"
#include "utility.h"
#include "application.h"
#include <wx/dirdlg.h>
#include <wx/utils.h>

BEGIN_EVENT_TABLE(frmLoadProject, wxFrame)
  EVT_BUTTON              (BMP_DONATE           , frmLoadProject::onDonate                  )
  EVT_BUTTON              (BTN_BROWSE_PIPELINE  , frmLoadProject::onBrowse                  )
  EVT_BUTTON              (BTN_LOAD             , frmLoadProject::onLoad                    )
  EVT_BUTTON              (wxID_CLOSE           , frmLoadProject::onQuit                    )
  EVT_BUTTON              (wxID_NEW             , frmLoadProject::onNew                     )
  EVT_COMBOBOX            (CMB_PIPELINE_FILES   , frmLoadProject::onPipelineFilesSelect     )
  EVT_LIST_COL_CLICK      (LST_PIPELINE_PROJECTS, frmLoadProject::onPipelineProjectsSort    )
  EVT_LIST_ITEM_ACTIVATED (LST_PIPELINE_PROJECTS, frmLoadProject::onPipelineProjectActivated)
  EVT_LIST_ITEM_DESELECTED(LST_PIPELINE_PROJECTS, frmLoadProject::onPipelineProjectSelected )
  EVT_LIST_ITEM_SELECTED  (LST_PIPELINE_PROJECTS, frmLoadProject::onPipelineProjectSelected )
  EVT_SIZE                (                       frmLoadProject::onResize                  )
  EVT_TEXT                (CMB_PIPELINE_FILES   , frmLoadProject::onPipelineFilesSelect     )
  EVT_TEXT_ENTER          (CMB_PIPELINE_FILES   , frmLoadProject::onPipelineFilesSelect     )
END_EVENT_TABLE()

frmLoadProject::frmLoadProject()
  : wxFrame(0, wxID_ANY, L"Lua Edit", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE)
  , m_pPreloadProgress(0)
  , m_iLastColumnSort(-1), m_iSelectedProject(-1)
{
  wxBoxSizer *pTopSizer = new wxBoxSizer(wxVERTICAL);

  wxBoxSizer *pPipelineFileSizer = new wxBoxSizer(wxHORIZONTAL);
  pPipelineFileSizer->Add(new wxStaticText(this, -1, L"Pipeline file:"), 0, wxALL | wxALIGN_CENTER_VERTICAL, 3);
  pPipelineFileSizer->Add(m_pPipelineFileList = new wxComboBox(this, CMB_PIPELINE_FILES), 1, wxALL | wxALIGN_CENTER_VERTICAL, 3);
  pPipelineFileSizer->Add(new wxButton(this, BTN_BROWSE_PIPELINE, L"Browse..."), 0, wxALL | wxALIGN_CENTER_VERTICAL, 3);
  pTopSizer->Add(pPipelineFileSizer, 0, wxALL | wxEXPAND, 0);

  pTopSizer->Add(m_pPipelineProjects = new wxListCtrl(this, LST_PIPELINE_PROJECTS, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL), 1, wxEXPAND | wxALL, 3);
  m_pPipelineProjects->InsertColumn(0, L"Project");
  m_pPipelineProjects->InsertColumn(1, L"Description");

  wxBoxSizer *pButtonSizer = new wxBoxSizer(wxHORIZONTAL);
  wxBitmap bmpDonate(wxT("IDB_DONATE"));
  bmpDonate.SetMask(new wxMask(bmpDonate, wxColour(255,0,255)));
  pButtonSizer->Add(m_pNewButton = new wxButton(this, wxID_NEW, L"&New"), 0, wxALL | wxALIGN_CENTER_VERTICAL, 3);
  pButtonSizer->Add(m_pLoadButton = new wxButton(this, BTN_LOAD, L"&Load"), 0, wxALL | wxALIGN_CENTER_VERTICAL, 3);
  pButtonSizer->Add(new wxBitmapButton(this, BMP_DONATE, bmpDonate, wxDefaultPosition, wxDefaultSize, 0), 0, wxALL | wxALIGN_CENTER_VERTICAL, 3);
  pButtonSizer->Add(new wxButton(this, wxID_CLOSE, L"&Quit"), 0, wxALL | wxALIGN_CENTER_VERTICAL, 3);
  pTopSizer->Add(pButtonSizer, 0, wxALIGN_RIGHT, 0);
  
  SetBackgroundColour(m_pLoadButton->GetBackgroundColour());

  SetSizer(pTopSizer);
	pTopSizer->SetSizeHints(this);
  Layout();

  SetSize(GetSize().Scale(2.0f, 1.5f));
  Layout();

  m_pPipelineProjects->SetColumnWidth(0, wxLIST_AUTOSIZE_USEHEADER);
  m_pPipelineProjects->SetColumnWidth(0, m_pPipelineProjects->GetColumnWidth(0) * 2);
  m_pPipelineProjects->SetColumnWidth(1, m_pPipelineProjects->GetClientSize().GetWidth() - m_pPipelineProjects->GetColumnWidth(0));

  for(IniFile::Section::iterator itr = LuaEditApp::Config[L"pipelines"].begin(L"pipeline"), end = LuaEditApp::Config[L"pipelines"].end(); itr != end; ++itr)
  {
    m_pPipelineFileList->Append(itr->value());
  }
  if(m_pPipelineFileList->GetCount() > 0)
    m_pPipelineFileList->Select(0);
  _refreshProjectList();
}

frmLoadProject::~frmLoadProject()
{
  IniFile::Section& oPipelines = LuaEditApp::Config[L"pipelines"];
  oPipelines.clear();
  wxString sValue = m_pPipelineFileList->GetValue();
  oPipelines.appendValue(L"pipeline", sValue);
  for(unsigned int i = 0; i < m_pPipelineFileList->GetCount(); ++i)
  {
    wxString sString = m_pPipelineFileList->GetString(i);
    if(sString != sValue)
      oPipelines.appendValue(L"pipeline", sString);
  }
}

void frmLoadProject::onResize(wxSizeEvent& e)
{
  Layout();
}

void frmLoadProject::onPipelineFilesSelect(wxCommandEvent& e)
{
  _refreshProjectList();
}

void frmLoadProject::onBrowse(wxCommandEvent& e)
{
  std::auto_ptr<PipelineFileOpenDialog> pBrowser(new PipelineFileOpenDialog(this, m_pPipelineFileList));
  if(pBrowser->show())
  {
    _refreshProjectList();
  }
}

void frmLoadProject::onPipelineProjectSelected(wxListEvent &e)
{
  long iIndex = e.GetIndex();
  if(m_bCanLoad)
  {
    if(e.GetEventType() == wxEVT_COMMAND_LIST_ITEM_SELECTED)
    {
      m_pLoadButton->Enable();
      m_iSelectedProject = e.GetIndex();
    }
    else
    {
      m_pLoadButton->Disable();
    }
  }
  else
  {
    if(e.GetEventType() == wxEVT_COMMAND_LIST_ITEM_SELECTED)
    {
      m_pPipelineProjects->SetItemState(iIndex, 0, wxLIST_STATE_SELECTED);
    }
  }
}

void frmLoadProject::onPipelineProjectActivated(wxListEvent &e)
{
  _loadEditor(reinterpret_cast<IniFile::Section*>(e.GetData()));
}

void frmLoadProject::onLoad(wxCommandEvent &e)
{
  _loadEditor(reinterpret_cast<IniFile::Section*>(m_pPipelineProjects->GetItemData(m_iSelectedProject)));
}

void frmLoadProject::onNew(wxCommandEvent& e)
{
  NewPipelineProjectWizard oWizard(this);
  oWizard.RunWizard();
}

static int wxCALLBACK ColumnAlphaSort(IniFile::Section *pProject1, IniFile::Section *pProject2, std::pair<long, bool>* pData)
{ 
  RainString sItem1 = pData->first ? (*pProject1)["Description"] : pProject1->name().afterFirst(':');
  RainString sItem2 = pData->first ? (*pProject2)["Description"] : pProject2->name().afterFirst(':');

  int iResult = sItem1.compareCaseless(sItem2);
  if(pData->second)
    iResult = -iResult;
  return iResult;
}

void frmLoadProject::onPipelineProjectsSort(wxListEvent &e)
{
  if(m_pPipelineProjects->GetItemCount() > 1 && m_bCanLoad)
  {
    std::pair<long, bool> oSortData(e.GetColumn(), false);
    if(m_iLastColumnSort == e.GetColumn())
    {
      m_iLastColumnSort = -1;
      oSortData.second = true;
    }
    else
    {
      m_iLastColumnSort = e.GetColumn();
    }
    m_pPipelineProjects->SortItems(reinterpret_cast<int(wxCALLBACK*)(long,long,long)>(ColumnAlphaSort), reinterpret_cast<long>(&oSortData));
  }
}

void frmLoadProject::onDonate(wxCommandEvent& e)
{
  ::wxLaunchDefaultBrowser(L"https://www.paypal.com/cgi-bin/webscr?cmd=_xclick&business=corsix%40gmail%2ecom&item_name=Donation%20for%20Corsix%27s%20Lua%20Editor&no_shipping=1&no_note=0&cn=Optional%20Message&tax=0&currency_code=GBP&bn=PP%2dDonationsBF&charset=UTF%2d8");
}

void frmLoadProject::onQuit(wxCommandEvent& e)
{
  Close();
}

wxProgressDialog *frmLoadProject::_getPreloadProgressDialog(bool bCreateIfNeeded)
{
  if(m_pPreloadProgress)
    return m_pPreloadProgress;
  else if(!bCreateIfNeeded)
    return 0;

  m_pPreloadProgress = new wxProgressDialog(L"Loading project", L"Please wait, loading...", 20, this, wxPD_APP_MODAL | wxPD_SMOOTH);
  return m_pPreloadProgress;
}

void frmLoadProject::_cancelPreloadProgressDialog()
{
  if(m_pPreloadProgress)
  {
    delete m_pPreloadProgress;
    m_pPreloadProgress = 0;
  }
}

void frmLoadProject::_loadEditor(IniFile::Section* pPipelineSection)
{
  IniFile::Section* pOriginalSection = pPipelineSection;
  RainString sRootFolder = m_pPipelineFileList->GetValue();
  sRootFolder = sRootFolder.beforeLast('\\') + L"\\";
  std::auto_ptr<FileStoreComposition> pComposition(new FileStoreComposition);

  int iPriority = 1000;
  IFileStore *pStoreToUse = RainGetFileSystemStore(), *pReadOnly = 0;
  bool bNextOwnershipOfStore = false;
  while(pPipelineSection)
  {
    _getPreloadProgressDialog()->Pulse(wxString(L"Preparing to load pipeline project \'") + implicit_cast<wxString>(pPipelineSection->name()) + L"\'");
    RainString sData = (*pPipelineSection)["DataFinal"];
    if(pReadOnly == 0 && !sData.isEmpty())
    {
      sData = sRootFolder + sData;
      if(sData.suffix(1) != L"\\")
        sData += L"\\";
      try
      {
        pComposition->addFileStore(pStoreToUse, sData, L"DataRGD", iPriority--, bNextOwnershipOfStore);
        bNextOwnershipOfStore = false;
      }
      CATCH_MESSAGE_BOX_1(L"Cannot load %s (cannot mount DataFinal)", pPipelineSection->name().getCharacters(), {
        if(bNextOwnershipOfStore)
          delete pStoreToUse;
        _cancelPreloadProgressDialog();
        return;
      });
    }

    RainString sGeneric = (*pPipelineSection)["DataGeneric"];
    if(!sGeneric.isEmpty())
    {
      sGeneric = sRootFolder + sGeneric;
      if(sGeneric.suffix(1) != L"\\")
        sGeneric += L"\\";

      // Relic store the CoH Lua files alongside the RGD files in DataFinal\Attrib (DataGeneric\Attrib doesn't exist)
      // Detect this and map that as Generic if it looks like the case
      if(!RainDoesDirectoryExist(sGeneric + L"Attrib\\") && !sData.isEmpty() && _isSideBySideAttrib(sData + L"Attrib\\"))
      {
        sGeneric = sData;
      }

      try
      {
        pComposition->addFileStore(pStoreToUse, sGeneric, L"Generic", iPriority--, bNextOwnershipOfStore);
        bNextOwnershipOfStore = false;
      }
      CATCH_MESSAGE_BOX_1(L"Cannot load %s (cannot mount DataGeneric)", pPipelineSection->name().getCharacters(), {
        if(bNextOwnershipOfStore)
          delete pStoreToUse;
        _cancelPreloadProgressDialog();
        return;
      });
    }

    RainString sParent = (*pPipelineSection)["Parent"];
    if(sParent.isEmpty())
      pPipelineSection = 0;
    else
    {
      sParent = RainString(L"project:") + sParent;
      IniFile::iterator itr = pPipelineSection->getFile()->begin(), end = pPipelineSection->getFile()->end();
      pPipelineSection = 0;
      for(; itr != end; ++itr)
      {
        if(itr->name().compareCaseless(sParent) == 0)
        {
          pPipelineSection = &*itr;
          break;
        }
      }
    }

    if(pReadOnly == 0)
    {
      pReadOnly = new ReadOnlyFileStoreAdaptor(pStoreToUse, false);
      bNextOwnershipOfStore = true;
      pStoreToUse = pReadOnly;
    }
  }
  if(bNextOwnershipOfStore)
    delete pStoreToUse;

  IDirectory *pDirectory = 0;
  try
  {
    pDirectory = pComposition->openDirectory(L"Generic\\Attrib\\");
  }
  CATCH_MESSAGE_BOX(L"Cannot load project", {});
  _loadEditor(pDirectory, pComposition.release(), pOriginalSection);
}

void frmLoadProject::_loadEditor(IDirectory *pDirectory, IFileStore *pFileStore, IniFile::Section* pPipelineSection)
{
  _getPreloadProgressDialog()->Pulse(L"Preparing to load Luas...");
  frmLuaEditor* pEditor = 0;
  try
  {
    pEditor = new frmLuaEditor;
    if(pPipelineSection)
    {
      pEditor->setPipeline(pPipelineSection, m_pPipelineFileList->GetValue());
    }
    _cancelPreloadProgressDialog(); // setSource has it's own dialog
    pEditor->setSource(pDirectory, pFileStore);
  }
  CATCH_MESSAGE_BOX(L"Cannot load editor", {
    delete pEditor;
    _cancelPreloadProgressDialog();
    return;
  });
  _cancelPreloadProgressDialog();
  pEditor->Show();
  pEditor->Maximize();
  pEditor->Refresh();
  Close();
}

enum EThreeStateBool
{
  TSB_False = 0,
  TSB_True = 1,
  TSB_Undetermined = 2,
};

static EThreeStateBool SideBySideRecurse(IDirectory *pDir)
{
  size_t iCountLua = 0, iCountRGD = 0;
  EThreeStateBool eStatus = TSB_Undetermined;
  for(IDirectory::iterator itr = pDir->begin(), end = pDir->end(); eStatus == TSB_Undetermined && itr != end; ++itr)
  {
    if(itr->isDirectory())
    {
      std::auto_ptr<IDirectory> pSubDir(itr->open());
      eStatus = SideBySideRecurse(&*pSubDir);
    }
    else
    {
      RainString sExt = itr->name().afterLast('.');
      if(sExt.compareCaseless(L"lua") == 0)
        ++iCountLua;
      else if(sExt.compareCaseless(L"rgd") == 0)
        ++iCountRGD;
    }
  }
  if(eStatus == TSB_Undetermined)
  {
    if(iCountLua != iCountRGD)
      eStatus = TSB_False;
    else if(iCountLua >= 3)
      eStatus = TSB_True;
  }
  return eStatus;
}

bool frmLoadProject::_isSideBySideAttrib(RainString sPath)
{
  try
  {
    std::auto_ptr<IDirectory> pDir(RainOpenDirectory(sPath));
    return SideBySideRecurse(&*pDir) == TSB_True;
  }
  catch(RainException *pE)
  {
    delete pE;
    return false;
  }
}

void frmLoadProject::_adjustProjectListItemFont(long iIndex, wxFont (*fnAdjust)(wxFont))
{
  m_pPipelineProjects->SetItemFont(iIndex, fnAdjust(m_pPipelineProjects->GetItemFont(iIndex)));
}

static wxFont MakeFontBold(wxFont f)
{
  f.SetWeight(wxFONTWEIGHT_BOLD);
  return f;
}

void frmLoadProject::_refreshProjectList()
{
  m_pPipelineProjects->DeleteAllItems();
  m_pLoadButton->Disable();
  m_pNewButton->Disable();
  m_oPipelineFile.clear();
  m_bCanLoad = false;

  RainString sPipelineFile = m_pPipelineFileList->GetValue();
  if(sPipelineFile.isEmpty() || !RainDoesFileExist(sPipelineFile))
  {
    m_pPipelineProjects->InsertItem(0, L"none");
    m_pPipelineProjects->SetItem(0, 1, L"Please choose a pipeline file to continue");
    m_pPipelineProjects->SetItemData(0, 0);
    _adjustProjectListItemFont(0, MakeFontBold);
    return;
  }

  try
  {
    m_oPipelineFile.load(sPipelineFile);
  }
  catch(RainException *pE)
  {
    m_pPipelineProjects->InsertItem(0, L"none");
    m_pPipelineProjects->SetItem(0, 1, L"Error while loading pipeline file: ");
    m_pPipelineProjects->SetItemData(0, 0);
    _adjustProjectListItemFont(0, MakeFontBold);
    long iDetailIndex = 1;
    for(RainException *e = pE; e; e = e->getPrevious(), ++iDetailIndex)
    {
      m_pPipelineProjects->InsertItem(iDetailIndex, L"");
      m_pPipelineProjects->SetItem(iDetailIndex, 1, e->getMessage());
      m_pPipelineProjects->SetItemData(iDetailIndex, iDetailIndex);
    }
    EXCEPTION_MESSAGE_BOX(L"Cannot load pipeline file", pE);
    return;
  }
  m_pNewButton->Enable();

  long iProjectIndex = 0;
  for(IniFile::iterator itr = m_oPipelineFile.begin(); itr != m_oPipelineFile.end(); ++itr)
  {
    if(wcsncmp(itr->name().getCharacters(), L"project:", 8) == 0)
    {
      m_pPipelineProjects->InsertItem(iProjectIndex, itr->name().afterFirst(':'));
      m_pPipelineProjects->SetItem(iProjectIndex, 1, (*itr)[L"Description"].value());
      m_pPipelineProjects->SetItemData(iProjectIndex, reinterpret_cast<long>(&*itr));
      ++iProjectIndex;
    }
  }

  if(iProjectIndex > 0)
  {
    m_bCanLoad = true;
  }
  else
  {
    m_pPipelineProjects->InsertItem(0, L"none");
    m_pPipelineProjects->SetItem(0, 1, L"No pipeline projects in file");
    m_pPipelineProjects->SetItemData(0, 0);
    _adjustProjectListItemFont(0, MakeFontBold);
  }
}
