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
#include "frmNewProject.h"
#include "application.h"
#include "stdext.h"
#include "utility.h"
#include <wx/html/htmlwin.h>

NewPipelineProjectWizard::NewPipelineProjectWizard(wxWindow* pParent, int iID)
  : wxWizard(pParent, iID, L"Create new pipeline project", wxNullBitmap, wxDefaultPosition, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
}

bool NewPipelineProjectWizard::RunWizard()
{
  WelcomePage *pWelcomePage = new WelcomePage(this);
  GamePipelineSelect *pPipelinePage = new GamePipelineSelect(this);

  wxWizardPageSimple::Chain(pWelcomePage, pPipelinePage);

  GetPageAreaSizer()->Add(pWelcomePage);

  return wxWizard::RunWizard(pWelcomePage);
}

NewPipelineProjectWizard::WelcomePage::WelcomePage(NewPipelineProjectWizard* pWizard)
  : wxWizardPageSimple(pWizard)
{
  wxBoxSizer *pMainSizer = new wxBoxSizer(wxVERTICAL);

  wxHtmlWindow *pInfoBox = new wxHtmlWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHW_DEFAULT_STYLE | wxHW_NO_SELECTION);
  pMainSizer->Add(pInfoBox, 1, wxEXPAND | wxALL, 0);
  pInfoBox->SetPage(L"<html><body><p>"
    L"<b>Welcome to the pipeline project creation wizard. </b>"
    L"This wizard will guide you through the process of creating a new pipeline project. "
    L"Before you begin, it is important that the following requirements are met:</p><ol>"
    L"<li>The game is installed and patched</li>"
    L"<li>The game has a pipeline.ini file</li>"
    L"<li>The game has it\'s Luas in the DataGeneric folder</li>"
    L"</ol><p>"
    L"If any of these are not met, then please consult the program help files on how to proceed."
    L"</p></body></html>"
  );

  SetSizer(pMainSizer);
}

BEGIN_EVENT_TABLE(NewPipelineProjectWizard::GamePipelineSelect, wxWizardPageSimple)
  EVT_BUTTON  (BTN_BROWSEGAMEFOLDER, NewPipelineProjectWizard::GamePipelineSelect::onBrowseGameFolder)
  EVT_BUTTON  (BTN_BROWSEPIPELINE  , NewPipelineProjectWizard::GamePipelineSelect::onBrowsePipeline  )
  EVT_COMBOBOX(CMB_PIPELINES       , NewPipelineProjectWizard::GamePipelineSelect::onPipelineSelect  )
END_EVENT_TABLE()

NewPipelineProjectWizard::GamePipelineSelect::GamePipelineSelect(NewPipelineProjectWizard* pWizard)
  : wxWizardPageSimple(pWizard)
{
  wxBoxSizer *pMainSizer = new wxBoxSizer(wxVERTICAL);

  wxFlexGridSizer *pDirectoriesSizer = new wxFlexGridSizer(3);
  pDirectoriesSizer->SetFlexibleDirection(wxHORIZONTAL);
  pDirectoriesSizer->AddGrowableCol(1, 1);

  pDirectoriesSizer->Add(new wxStaticText(this, wxID_ANY, L"Pipeline file:"), 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
  m_pPipelineFileList = new wxComboBox(this, CMB_PIPELINES);
  pDirectoriesSizer->Add(m_pPipelineFileList, 1, wxALL | wxALIGN_CENTER_VERTICAL | wxEXPAND, 2);
  pDirectoriesSizer->Add(new wxButton(this, BTN_BROWSEPIPELINE, L"Browse..."), 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);

  pDirectoriesSizer->Add(new wxStaticText(this, wxID_ANY, L"Game folder:"), 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
  m_pGameFolder = new wxTextCtrl(this, wxID_ANY);
  pDirectoriesSizer->Add(m_pGameFolder, 1, wxALL | wxALIGN_CENTER_VERTICAL | wxEXPAND, 2);
  pDirectoriesSizer->Add(new wxButton(this, BTN_BROWSEGAMEFOLDER, L"Browse..."), 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);

  pMainSizer->Add(pDirectoriesSizer, 0, wxEXPAND);

  wxArrayString aModFileOptions;
  aModFileOptions.Add(L"Create a new module file");
  aModFileOptions.Add(L"Use an existing module file");
  aModFileOptions.Add(L"Do not create a module file");
  pMainSizer->Add(new wxRadioBox(this, wxID_ANY, L"Module file for this project", wxDefaultPosition, wxDefaultSize, aModFileOptions, 1, wxRA_SPECIFY_COLS), 0, wxEXPAND | wxALL, 2);

  pMainSizer->AddStretchSpacer();

  SetSizer(pMainSizer);

  for(IniFile::Section::iterator itr = LuaEditApp::Config[L"pipelines"].begin(L"pipeline"), end = LuaEditApp::Config[L"pipelines"].end(); itr != end; ++itr)
  {
    m_pPipelineFileList->Append(itr->value());
  }
  if(m_pPipelineFileList->GetCount() > 0)
  {
    // Minimum size will be calculated using selection
    m_pPipelineFileList->Select(0);
    m_pGameFolder->SetValue(_findGameFolder(m_pPipelineFileList->GetValue()));
  }
  else
  {
    // Minimum size needs manually expanding
    pWizard->GetPageAreaSizer()->SetMinSize(pWizard->GetPageAreaSizer()->GetMinSize().Scale(1.4f, 1.0f));
  }
}

static size_t GetExeFileCount(RainString sPath)
{
  std::auto_ptr<IDirectory> pDirectory(RainOpenDirectoryNoThrow(sPath));
  if(pDirectory.get() == 0)
    return 0;

  size_t iCount = 0;
  for(IDirectory::iterator itr = pDirectory->begin(), itrEnd = pDirectory->end(); itr != itrEnd; ++itr)
  {
    if(itr->isDirectory() == false && itr->name().afterLast('.').compareCaseless(L"exe") == 0)
      ++iCount;
  }
  return iCount;
}

wxString NewPipelineProjectWizard::GamePipelineSelect::_findGameFolder(wxString sPipelineFile)
{
  RainString sGameFolder1, sGameFolder2;
  sGameFolder1 = implicit_cast<RainString>(sPipelineFile.BeforeLast('\\')) + L"\\";
  if(!sPipelineFile.IsEmpty() && wxFileExists(sPipelineFile))
  {
    try
    {
      IniFile oFile;
      oFile.load(sPipelineFile);
      for(IniFile::iterator itrSection = oFile.begin(), itrSectionEnd = oFile.end(); itrSection != itrSectionEnd; ++itrSection)
      {
        if(wcsncmp(itrSection->name().getCharacters(), L"project:", 8) == 0)
        {
          if(itrSection->hasValue(L"DataFinal", false))
          {
            sGameFolder2 = GetParentFolder(sGameFolder1 + (*itrSection)[L"DataFinal"], 2);
            break;
          }
        }
      }
    }
    catch(RainException *pE)
    {
      delete pE;
    }
  }

  if(!sGameFolder2.isEmpty() && GetExeFileCount(sGameFolder2) > GetExeFileCount(sGameFolder1))
    return sGameFolder2;
  else
    return sGameFolder1;
}

void NewPipelineProjectWizard::GamePipelineSelect::onBrowsePipeline(wxCommandEvent& e)
{
  std::auto_ptr<PipelineFileOpenDialog> pBrowser(new PipelineFileOpenDialog(this, m_pPipelineFileList));
  if(pBrowser->show())
  {
    onPipelineSelect(e);
  }
}

void NewPipelineProjectWizard::GamePipelineSelect::onPipelineSelect(wxCommandEvent& e)
{
  m_pGameFolder->SetValue(_findGameFolder(m_pPipelineFileList->GetValue()));
}

void NewPipelineProjectWizard::GamePipelineSelect::onBrowseGameFolder(wxCommandEvent& e)
{
  wxString sValue = ::wxDirSelector(L"Please locate the game directory", m_pGameFolder->GetValue(), wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST, wxDefaultPosition, this);
  if(!sValue.IsEmpty())
  {
    m_pGameFolder->SetValue(sValue);
  }
}
