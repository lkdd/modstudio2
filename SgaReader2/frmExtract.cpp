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
#include "frmExtract.h"
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <stack>
#include <queue>

BEGIN_EVENT_TABLE(frmExtract, wxDialog)
  EVT_BUTTON(BTN_BROWSE, frmExtract::onBrowse)
  EVT_BUTTON(wxID_OK, frmExtract::onExtract)
END_EVENT_TABLE()

RainString frmExtract::s_sLastSaveDir;
bool       frmExtract::s_bLastSaveDirHasDir;

frmExtract::frmExtract(wxWindow *pParent, SgaArchive *pArchive, RainString sDefaultExtractFolder,
                       const RainString& sPathToExtract, wxStatusBar *pResultNotification)
  : m_pArchive(pArchive), m_pPositiveResultNoficiationBar(pResultNotification), m_sPathToExtract(sPathToExtract)
{
  m_bIsDirectory = !pArchive->doesFileExist(m_sPathToExtract);
  if(m_bIsDirectory && m_sPathToExtract.suffix(1) != L"\\")
    m_sPathToExtract += L"\\";
  if(sDefaultExtractFolder.suffix(1) != L"\\")
    sDefaultExtractFolder += L"\\";
  Create(pParent, wxID_ANY, m_bIsDirectory ? L"Extract directory" : L"Extract file", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

  wxBoxSizer *pMainSizer = new wxBoxSizer(wxVERTICAL);

  wxFlexGridSizer *pPathsSizer = new wxFlexGridSizer(3, 3, 3);
  pPathsSizer->AddGrowableCol(1, 1); // column 1, 100%
  wxStaticText *pTemp;
  pPathsSizer->Add(pTemp = new wxStaticText(this, wxID_ANY, L"Extracting:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
  wxFont oFont = pTemp->GetFont();
  oFont.SetWeight(wxFONTWEIGHT_BOLD);
  pTemp->SetFont(oFont);
  pPathsSizer->Add(new wxStaticText(this, wxID_ANY, m_sPathToExtract), 1, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT);
  pPathsSizer->AddSpacer(0);
  pPathsSizer->Add(pTemp = new wxStaticText(this, wxID_ANY, L"Destination:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
  pTemp->SetFont(oFont);
  pPathsSizer->Add(m_pDestinationPath = new wxComboBox(this, TXT_FILENAME), 1, wxALIGN_CENTER_VERTICAL | wxEXPAND);
  pPathsSizer->Add(new wxButton(this, BTN_BROWSE, L"Browse..."), 0, wxALIGN_CENTER_VERTICAL);

  {
    sDefaultExtractFolder += m_sPathToExtract;
    RainString sSaved = s_sLastSaveDir;
    if(sSaved.isEmpty())
      sSaved = sDefaultExtractFolder;
    else
    {
      if(s_bLastSaveDirHasDir)
      {
        size_t iLen = 1;
        while(m_sPathToExtract[m_sPathToExtract.length() - iLen - 1] != '\\')
          ++iLen;
        sSaved += m_sPathToExtract.suffix(iLen);
      }
      else
        sSaved += m_sPathToExtract;
    }
    m_pDestinationPath->SetValue(sSaved);
    m_pDestinationPath->Append(sSaved);
    if(sSaved != sDefaultExtractFolder)
      m_pDestinationPath->Append(sDefaultExtractFolder);
  }

  pMainSizer->Add(pPathsSizer, 1, wxEXPAND | wxALL, 3);

  m_pButtonsSizer = CreateSeparatedButtonSizer(wxOK | wxCANCEL);
  if(m_pButtonsSizer)
  {
    pMainSizer->Add(m_pButtonsSizer, 0, wxEXPAND | wxALL, 3);
    FindWindow(wxID_OK)->SetLabel(L"Extract");
  }

  pMainSizer->SetSizeHints(this);
  SetSizer(pMainSizer);
  Layout();
}

frmExtract::~frmExtract()
{
  RainString sPathUsed(m_pDestinationPath->GetValue());
  if(sPathUsed.suffix(m_sPathToExtract.length() + 1).compareCaseless(L"\\" + m_sPathToExtract.toLower()) == 0)
  {
    s_sLastSaveDir = sPathUsed.prefix(sPathUsed.length() - m_sPathToExtract.length());
    s_bLastSaveDirHasDir = false;
  }
  else
  {
    s_sLastSaveDir = sPathUsed.beforeLast('\\') + L"\\";
    s_bLastSaveDirHasDir = true;
  }
}

bool frmExtract::_ensureDirectoryExists(RainString sFolder)
{
  if(!RainDoesDirectoryExist(sFolder))
  {
    if(::wxMessageBox(L"Directory \'" + sFolder + "\' doesn\'t exist, do you want to create it?", L"Extract file", wxOK | wxCANCEL | wxCENTER | wxICON_QUESTION, this) == wxCANCEL)
      return false;

    if(sFolder.suffix(1) == L"\\")
      sFolder = sFolder.prefix(sFolder.length() - 1);
    std::stack<RainString> stkToMake;
    stkToMake.push(sFolder);
    while(true)
    {
      sFolder = stkToMake.top().beforeLast('\\');
      if(RainDoesDirectoryExist(sFolder) || sFolder.isEmpty())
        break;
      stkToMake.push(sFolder);
    }
    while(!stkToMake.empty())
    {
      RainCreateDirectory(stkToMake.top());
      stkToMake.pop();
    }
  }
  return true;
}

void frmExtract::onExtract(wxCommandEvent &e)
{
  if(m_bIsDirectory)
  {
    RainString sFolder = m_pDestinationPath->GetValue();
    if(!_ensureDirectoryExists(sFolder))
        return;

    wxSizer *pMainSizer = GetSizer();
    if(m_pButtonsSizer)
    {
      FindWindow(wxID_OK)->Hide();
      FindWindow(wxID_CANCEL)->Hide();
      pMainSizer->Detach(m_pButtonsSizer);
    }
    pMainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 4);
    wxGauge *pProgressBar = new wxGauge(this, wxID_ANY, 1, wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL | wxGA_SMOOTH);
    wxStaticText *pCurrentItem = new wxStaticText(this, wxID_ANY, L"Initialising extraction...");
    pMainSizer->Add(pProgressBar, 0, wxEXPAND | wxALL, 2);
    pMainSizer->Add(pCurrentItem, 0, wxEXPAND | wxALL, 2);
    pMainSizer->RecalcSizes();
    SetSize(pMainSizer->GetMinSize());
    Layout();
    ::wxSafeYield(this);

    int iTotalCount = 1, iDoneCount = 0;
    std::queue<IDirectory*> qTodo;
    try
    {
      qTodo.push(m_pArchive->openDirectory(m_sPathToExtract));
      iTotalCount += static_cast<int>(qTodo.front()->getItemCount());
      size_t iSkipLength = qTodo.front()->getPath().length();
      pProgressBar->SetRange(iTotalCount);
      while(!qTodo.empty())
      {
        IDirectory *pDirectory = qTodo.front();
        pCurrentItem->SetLabel(pDirectory->getPath());
        pProgressBar->SetValue(++iDoneCount);
        ::wxSafeYield(this);

        RainString sDirectoryBase = pDirectory->getPath();
        sDirectoryBase = sFolder + sDirectoryBase.mid(iSkipLength, sDirectoryBase.length() - iSkipLength);
        if(!RainDoesDirectoryExist(sDirectoryBase))
          RainCreateDirectory(sDirectoryBase);
        for(IDirectory::iterator itr = pDirectory->begin(), itrEnd = pDirectory->end(); itr != itrEnd; ++itr)
        {
          if(itr->isDirectory())
          {
            IDirectory *pChild = itr->open();
            iTotalCount += 1 + static_cast<int>(pChild->getItemCount());
            qTodo.push(pChild);
            pProgressBar->SetRange(iTotalCount);
          }
          else
          {
            std::auto_ptr<IFile> pFile;
            pCurrentItem->SetLabel(pDirectory->getPath() + itr->name());
            pProgressBar->SetValue(++iDoneCount);
            ::wxSafeYield(this);
            pFile.reset(RainOpenFile(sDirectoryBase + itr->name(), FM_Write));
            itr->pump(&*pFile);
          }
        }

        delete pDirectory;
        qTodo.pop();
      }
    }
    catch(RainException *pE)
    {
      while(!qTodo.empty())
      {
        delete qTodo.front();
        qTodo.pop();
      }
      EXCEPTION_MESSAGE_BOX_1(L"Error extracting directory \'%s\'", m_sPathToExtract.getCharacters(), pE);
      SetReturnCode(GetEscapeId());
      return;
    }
  }
  else
  {
    wxFileName oFilename(m_pDestinationPath->GetValue());
    RainString sFolder = oFilename.GetPath();
    IFile *pFile = 0;
    try
    {
      if(!_ensureDirectoryExists(sFolder))
        return;
      pFile = RainOpenFile(m_pDestinationPath->GetValue(), FM_Write);
      m_pArchive->pumpFile(m_sPathToExtract, pFile);
    }
    catch(RainException *pE)
    {
      delete pFile;
      EXCEPTION_MESSAGE_BOX_1(L"Error extracting \'%s\'", m_sPathToExtract.getCharacters(), pE);
      return;
    }
    delete pFile;
  }
  wxString sMessage = L"Extracted " + m_sPathToExtract;
  if(m_pPositiveResultNoficiationBar)
    m_pPositiveResultNoficiationBar->SetStatusText(sMessage);
  else
    ::wxMessageBox(sMessage, GetLabel(), wxOK | wxCENTER | wxICON_INFORMATION, this);
  EndModal(GetAffirmativeId());
}

void frmExtract::onBrowse(wxCommandEvent &e)
{
  wxFileName oFilename(m_pDestinationPath->GetValue());
  if(m_bIsDirectory)
  {
    wxDirDialog oDirSelector(this, wxDirSelectorPromptStr, m_pDestinationPath->GetValue(), wxDD_DEFAULT_STYLE);
    if(oDirSelector.ShowModal() == wxID_OK)
    {
      m_pDestinationPath->SetValue(oDirSelector.GetPath());
    }
  }
  else
  {
    wxFileDialog oFileSelector(this, wxFileSelectorPromptStr, wxEmptyString, wxEmptyString, wxEmptyString, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    oFileSelector.SetMessage(L"Choose where to extract file:");
    oFileSelector.SetDirectory(oFilename.GetPath());
    oFileSelector.SetFilename(oFilename.GetFullName());
    oFileSelector.SetWildcard(L"All files (*.*)|*.*");
    if(oFileSelector.ShowModal() == wxID_OK)
    {
      m_pDestinationPath->SetValue(oFileSelector.GetPath());
    }
  }
}
