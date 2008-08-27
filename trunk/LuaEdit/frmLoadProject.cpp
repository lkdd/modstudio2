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
#include <wx/dirdlg.h>
#include <wx/utils.h>

BEGIN_EVENT_TABLE(frmLoadProject, wxFrame)
  EVT_SIZE(frmLoadProject::onResize)
  EVT_LISTBOX(LST_LOAD_RECENT, frmLoadProject::onLoadRecent)
  EVT_BUTTON(BTN_LOAD_FOLDER, frmLoadProject::onLoadFolder)
  EVT_BUTTON(BMP_DONATE, frmLoadProject::onDonate)
  EVT_BUTTON(wxID_EXIT, frmLoadProject::onQuit)
END_EVENT_TABLE()

IRecentLoadSource::~IRecentLoadSource() {}

class FolderLoadSource : public IRecentLoadSource
{
public:
  FolderLoadSource(RainString sFolder)
    : m_sFolder(sFolder) {}
  virtual RainString getSourceType()
  {
    return L"folder";
  }
  virtual RainString getDisplayText()
  {
    return m_sFolder.afterLast('\\') + L" folder (" + m_sFolder + L")";
  }
  virtual RainString getSaveText()
  {
    return m_sFolder;
  }
  virtual void performLoad(IDirectory **ppDirectory, IFileStore **pFileStore)
  {
    *ppDirectory = RainOpenDirectory(m_sFolder);
    *pFileStore = new (std::nothrow) FileSystemStore;
  }
  virtual bool isSameAs(IRecentLoadSource* pOtherSource)
  {
    return pOtherSource->getSourceType() == getSourceType() &&
      reinterpret_cast<FolderLoadSource*>(pOtherSource)->m_sFolder == m_sFolder;
  }
protected:
  RainString m_sFolder;
};

void frmLoadProject::_loadRecentList()
{
  IFile *pFile = RainOpenFileNoThrow(L"./recent.txt", FM_Read);
  if(pFile)
  {
    RainString sLine;
    while(true)
    {
      try
      {
        sLine = pFile->readString(static_cast<RainChar>('\n'));
      }
      catch(RainException *e)
      {
        delete e;
        break;
      }
      RainString sType = sLine.beforeFirst(';');
      sLine = sLine.afterFirst(';');

      if(sType == L"folder")
      {
        m_vRecentSources.push_back(new (std::nothrow) FolderLoadSource(sLine));
      }
    }
    delete pFile;
  }
  if(!m_vRecentSources.empty())
  {
    m_pRecentList->Clear();
    wxArrayString aItems;
    for(std::vector<IRecentLoadSource*>::iterator itr = m_vRecentSources.begin(); itr != m_vRecentSources.end(); ++itr)
    {
      aItems.Add((*itr)->getDisplayText());
    }
    m_pRecentList->InsertItems(aItems, 0);
  }
}

frmLoadProject::frmLoadProject()
  : wxFrame(0, wxID_ANY, L"Lua Edit", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE & ~(wxMAXIMIZE_BOX|wxRESIZE_BORDER))
{
  wxArrayString aChoices;
  aChoices.Add(L"(no projects loaded recently)");

  wxBitmap bmpDonate(wxT("IDB_DONATE"));
  bmpDonate.SetMask(new wxMask(bmpDonate, wxColour(255,0,255)));

  wxBoxSizer *pTopSizer = new wxBoxSizer(wxVERTICAL);
  pTopSizer->Add(m_pText = new wxStaticText(this, -1,
    L"Welcome to Corsix\'s Lua Editor!\n"
    L"If you find this tool useful, then please consider donating via PayPal (corsix@gmail.com)"
    , wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE), 0, wxEXPAND | wxALIGN_LEFT | wxALL, 2);

  wxBoxSizer *pColumns = new wxBoxSizer(wxHORIZONTAL);

  wxBoxSizer *pLeftSizer = new wxBoxSizer(wxVERTICAL);
  pLeftSizer->Add(new wxButton(this, BTN_LOAD_PIPELINE, L"Load &pipeline.ini project"), 0, wxEXPAND | wxALIGN_LEFT | wxALL, 2);
  pLeftSizer->Add(new wxButton(this, BTN_LOAD_MODULE_WA, L"Load DoW/&WA .module"), 0, wxEXPAND | wxALIGN_LEFT | wxALL, 2);
  pLeftSizer->Add(new wxButton(this, BTN_LOAD_MODULE_DC, L"Load DoW:&DC .module"), 0, wxEXPAND | wxALIGN_LEFT | wxALL, 2);
  pLeftSizer->Add(new wxButton(this, BTN_LOAD_MODULE_SS, L"Load DoW:&SS .module"), 0, wxEXPAND | wxALIGN_LEFT | wxALL, 2);
  pLeftSizer->Add(new wxButton(this, BTN_LOAD_MODULE_OF, L"Load &CoH/OF .module"), 0, wxEXPAND | wxALIGN_LEFT | wxALL, 2);
  pLeftSizer->Add(new wxButton(this, BTN_LOAD_FOLDER, L"Load &single directory"), 0, wxEXPAND | wxALIGN_LEFT | wxALL, 2);
  pLeftSizer->Add(new wxButton(this, wxID_EXIT, L"&Quit"), 0, wxEXPAND | wxALIGN_LEFT | wxALL, 2);

  wxBoxSizer *pRightSizer = new wxBoxSizer(wxVERTICAL);
  pRightSizer->Add(new wxStaticText(this, -1, L"Load recent project:"), 0, wxALIGN_LEFT | wxALL, 2);
  pRightSizer->Add(m_pRecentList = new wxListBox(this, LST_LOAD_RECENT, wxDefaultPosition, wxDefaultSize, aChoices, wxLB_HSCROLL | wxLB_NEEDED_SB), 1, wxALIGN_LEFT | wxALL | wxEXPAND, 2);
  pRightSizer->Add(new wxBitmapButton(this, BMP_DONATE, bmpDonate, wxDefaultPosition, wxDefaultSize, 0), 0, wxALIGN_RIGHT | wxALL, 2);

  pColumns->Add(pLeftSizer, 0, wxEXPAND | wxALL ^ wxRIGHT, 2);
  pColumns->Add(pRightSizer, 1, wxEXPAND | wxALL, 2);
  pTopSizer->Add(pColumns, 1, wxEXPAND);

  wxFont oBold = m_pText->GetFont();
  oBold.SetWeight(wxFONTWEIGHT_BOLD);
  m_pText->SetFont(oBold);
  SetBackgroundColour(m_pText->GetBackgroundColour());
  m_sText = m_pText->GetLabel();
  SetSizer(pTopSizer);
	pTopSizer->SetSizeHints(this);

  _loadRecentList();
}

frmLoadProject::~frmLoadProject()
{
  for(std::vector<IRecentLoadSource*>::iterator itr = m_vRecentSources.begin(); itr != m_vRecentSources.end(); ++itr)
  {
    delete *itr;
  }
}

void frmLoadProject::onResize(wxSizeEvent& e)
{
  m_pText->Freeze();
  m_pText->SetLabel(m_sText);
  m_pText->Wrap(e.GetSize().GetWidth());
  m_pText->Thaw();

  Layout();
}

void frmLoadProject::onDonate(wxCommandEvent& e)
{
  ::wxLaunchDefaultBrowser(L"https://www.paypal.com/cgi-bin/webscr?cmd=_xclick&business=corsix%40gmail%2ecom&item_name=Donation%20for%20Corsix%27s%20Lua%20Editor&no_shipping=1&return=http%3a%2f%2fwww%2ecorsix%2eorg%2fgo%2ephp%3fdonate_thankyou&cancel_return=http%3a%2f%2fwww%2ecorsix%2eorg%2f&no_note=1&cn=Optional%20Message&tax=0&currency_code=GBP&bn=PP%2dDonationsBF&charset=UTF%2d8");
}

void frmLoadProject::onQuit(wxCommandEvent& e)
{
  Close();
}

void frmLoadProject::_saveRecentList()
{
  IFile *pRecentList = 0;
  try
  {
    pRecentList = RainOpenFile(L"./recent.txt", FM_Write);
    for(std::vector<IRecentLoadSource*>::iterator itr = m_vRecentSources.begin(); itr != m_vRecentSources.end(); ++itr)
    {
      RainString s = (*itr)->getSourceType();
      pRecentList->write(s.getCharacters(), sizeof(RainChar), s.length());
      s = ";";
      pRecentList->write(s.getCharacters(), sizeof(RainChar), s.length());
      s = (*itr)->getSaveText();
      pRecentList->write(s.getCharacters(), sizeof(RainChar), s.length());
      s = "\n";
      pRecentList->write(s.getCharacters(), sizeof(RainChar), s.length());
    }
  }
  CATCH_MESSAGE_BOX(L"Unable to save list of recent file sources", {});
  delete pRecentList;
}

void frmLoadProject::_addNewRecentSource(IRecentLoadSource* pRecentSource)
{
  m_vRecentSources.insert(m_vRecentSources.begin(), pRecentSource);
  for(size_t i = 1; i < m_vRecentSources.size(); ++i)
  {
    if(m_vRecentSources[i]->isSameAs(m_vRecentSources[0]))
    {
      delete m_vRecentSources[i];
      m_vRecentSources.erase(m_vRecentSources.begin() + i);
      --i;
    }
  }
  _saveRecentList();
  m_pRecentList->Clear();
  wxArrayString aItems;
  for(std::vector<IRecentLoadSource*>::iterator itr = m_vRecentSources.begin(); itr != m_vRecentSources.end(); ++itr)
  {
    aItems.Add((*itr)->getDisplayText().getCharacters());
  }
  m_pRecentList->InsertItems(aItems, 0);
}

void frmLoadProject::_loadFromRecent(IRecentLoadSource* pRecentSource)
{
  IDirectory *pDirectory = 0;
  IFileStore *pFileStore = 0;
  ::wxBeginBusyCursor();
  try
  {
    pRecentSource->performLoad(&pDirectory, &pFileStore);
    _loadEditor(pDirectory, pFileStore);
  }
  CATCH_MESSAGE_BOX(L"Cannot load project", {
    delete pDirectory;
    delete pFileStore;
  });
  ::wxEndBusyCursor();
}

void frmLoadProject::onLoadRecent(wxCommandEvent& e)
{
  if(!m_vRecentSources.empty())
  {
    IRecentLoadSource* pRecentSource = m_vRecentSources[e.GetSelection()];
    if(e.GetSelection() != 0)
    {
      m_vRecentSources.erase(m_vRecentSources.begin() + e.GetSelection());
      m_vRecentSources.insert(m_vRecentSources.begin(), pRecentSource);
      m_pRecentList->Insert(m_pRecentList->GetString(e.GetSelection()), 0);
      m_pRecentList->Delete(e.GetSelection() + 1);
      _saveRecentList();
    }
    _loadFromRecent(pRecentSource);
  }
}

void frmLoadProject::onLoadFolder(wxCommandEvent& e)
{
  wxString sDirectory = ::wxDirSelector(L"Choose the attrib directory containing Lua files:", L"", 0, wxDefaultPosition, this);
  if(!sDirectory.IsEmpty())
  {
    _addNewRecentSource(new FolderLoadSource(sDirectory.c_str()));
    _loadFromRecent(m_vRecentSources[0]);
  }
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
