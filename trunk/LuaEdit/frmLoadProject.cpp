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
#include "frmEditor.h"
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
  pButtonSizer->Add(new wxBitmapButton(this, BMP_DONATE, bmpDonate, wxDefaultPosition, wxDefaultSize, 0), 0, wxALL | wxALIGN_CENTER_VERTICAL, 3);
  pButtonSizer->AddStretchSpacer(1);
  pButtonSizer->Add(m_pNewButton = new wxButton(this, wxID_NEW, L"&New"), 0, wxALL | wxALIGN_CENTER_VERTICAL, 3);
  pButtonSizer->Add(m_pLoadButton = new wxButton(this, BTN_LOAD, L"&Load"), 0, wxALL | wxALIGN_CENTER_VERTICAL, 3);
  pButtonSizer->Add(new wxButton(this, wxID_CLOSE, L"&Quit"), 0, wxALL | wxALIGN_CENTER_VERTICAL, 3);
  pTopSizer->Add(pButtonSizer, 0, wxALL | wxEXPAND, 0);
  
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
  std::auto_ptr<wxFileDialog> pBrowser(new wxFileDialog(this, L"Choose the pipeline file", wxEmptyString, wxEmptyString, L"Pipeline files (pipeline.ini)|pipeline.ini|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST));
  if(!m_pPipelineFileList->GetValue().IsEmpty())
  {
    wxFileName oFileName(m_pPipelineFileList->GetValue());
    pBrowser->SetDirectory(oFileName.GetPath());
    pBrowser->SetFilename(oFileName.GetFullName());
    if(oFileName.GetFullName().CmpNoCase(L"pipeline.ini") != 0)
      pBrowser->SetFilterIndex(1);
  }
  if(pBrowser->ShowModal() == wxID_OK)
  {
    int iExistingPosition = m_pPipelineFileList->FindString(pBrowser->GetPath(), false);
    if(iExistingPosition != wxNOT_FOUND)
      m_pPipelineFileList->Delete(iExistingPosition);
    m_pPipelineFileList->Insert(pBrowser->GetPath(), 0);
    m_pPipelineFileList->SetSelection(0);
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
  ::wxMessageBox(L"Not implemented yet, sorry", L"New Pipeline Project", wxOK | wxCENTER | wxICON_ERROR, this);
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

void frmLoadProject::_loadEditor(IniFile::Section* pPipelineSection)
{
  RainString sRootFolder = m_pPipelineFileList->GetValue();
  sRootFolder = sRootFolder.beforeLast('\\') + L"\\";
  std::auto_ptr<FileStoreComposition> pComposition(new FileStoreComposition);

  int iPriority = 1000;
  while(pPipelineSection)
  {
    RainString sGeneric = (*pPipelineSection)["DataGeneric"];
    if(!sGeneric.isEmpty())
    {
      pComposition->addFileStore(RainGetFileSystemStore(), sRootFolder + sGeneric, L"Generic", iPriority--, false);
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
  }

  IDirectory *pDirectory = 0;
  try
  {
    pDirectory = pComposition->openDirectory(L"Generic\\Attrib\\");
  }
  CATCH_MESSAGE_BOX(L"Cannot load project", {});
  _loadEditor(pDirectory, pComposition.release());
}

void frmLoadProject::_loadEditor(IDirectory *pDirectory, IFileStore *pFileStore)
{
  frmLuaEditor* pEditor = 0;
  try
  {
    pEditor = new frmLuaEditor;
    pEditor->setSource(pDirectory, pFileStore);
  }
  CATCH_MESSAGE_BOX(L"Cannot load editor", {delete pEditor; return;});
  pEditor->Show();
  pEditor->Maximize();
  pEditor->Refresh();
  Close();
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
