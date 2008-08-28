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
#include "exception_dialog.h"
#include <wx/artprov.h>
#include <wx/clipbrd.h>

BEGIN_EVENT_TABLE(RainExceptionDialog, wxDialog)
  EVT_BUTTON(wxID_OK, RainExceptionDialog::onOK)
  EVT_BUTTON(wxID_MORE, RainExceptionDialog::onDetails)
  EVT_BUTTON(wxID_SAVE, RainExceptionDialog::onSave)
  EVT_LIST_ITEM_SELECTED(wxID_ANY, RainExceptionDialog::onListSelect)
END_EVENT_TABLE()

RainExceptionDialog::RainExceptionDialog(wxWindow *parent, RainException *pException)
  : wxDialog(parent, wxID_ANY, wxT("Error"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
  , m_pDetailList(0), m_bShowingDetails(false), m_pDetailsBtn(0), m_pStaticLine(0), m_pSaveBtn(0)
{
  for(RainException *e = pException; e; e = e->getPrevious())
  {
    m_aMessages.Add(e->getMessage());
    m_aFiles.Add(e->getFile());
    m_aLines.Add(e->getLine());
  }

  wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
  wxBoxSizer *sizerButtons = new wxBoxSizer(wxVERTICAL);
  wxBoxSizer *sizerAll = new wxBoxSizer(wxHORIZONTAL);

  wxButton *btnOk = new wxButton(this, wxID_OK);
  sizerButtons->Add(btnOk, 0, wxCENTRE | wxBOTTOM, 5);
  m_pDetailsBtn = new wxButton(this, wxID_MORE, wxT("&Details >>"));
  sizerButtons->Add(m_pDetailsBtn, 0,wxCENTRE | wxTOP, 4);

  wxBitmap bitmap = wxArtProvider::GetBitmap(wxART_ERROR, wxART_MESSAGE_BOX);
  sizerAll->Add(new wxStaticBitmap(this, wxID_ANY, bitmap), 0, wxALIGN_CENTRE_VERTICAL);

  sizerAll->Add(CreateTextSizer(pException->getMessage()), 1, wxALIGN_CENTRE_VERTICAL | wxLEFT | wxRIGHT, 10);
  sizerAll->Add(sizerButtons, 0, wxALIGN_RIGHT | wxLEFT, 10);
  sizerTop->Add(sizerAll, 0, wxALL | wxEXPAND, 10);

  SetSizer(sizerTop);

  wxSize size = sizerTop->Fit(this);
  m_maxHeight = size.y;
  SetSizeHints(size.x, size.y, m_maxWidth, m_maxHeight);

  btnOk->SetFocus();
  Centre();
}

void RainExceptionDialog::_addExtraControls()
{
  m_pSaveBtn = new wxButton(this, wxID_SAVE, wxT("Copy to Clipboard"));
  m_pStaticLine = new wxStaticLine(this, wxID_ANY);

  m_pDetailList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER | wxLC_REPORT | wxLC_SINGLE_SEL);

  m_pDetailList->InsertColumn(0, wxT("Message"));
  m_pDetailList->InsertColumn(1, wxT("File"));
  m_pDetailList->InsertColumn(2, wxT("Ln."));

  size_t count = m_aMessages.GetCount();
  for(size_t n = 0; n < count; ++n)
  {
    m_pDetailList->InsertItem(n, m_aMessages[n]);
    m_pDetailList->SetItem(n, 1, m_aFiles[n]);
    m_pDetailList->SetItem(n, 2, wxString::Format(wxT("%li"), m_aLines[n]));
  }

  m_pDetailList->SetColumnWidth(0, wxLIST_AUTOSIZE);
  m_pDetailList->SetColumnWidth(1, wxLIST_AUTOSIZE);
  m_pDetailList->SetColumnWidth(2, wxLIST_AUTOSIZE);

  int height = GetCharHeight()*(count + 4);
  int heightMax = wxGetDisplaySize().y - GetPosition().y - 2*GetMinHeight();

  heightMax *= 9;
  heightMax /= 10;

  m_pDetailList->SetSize(wxDefaultCoord, wxMin(height, heightMax));
}


void RainExceptionDialog::onListSelect(wxListEvent& e)
{
  m_pDetailList->SetItemState(e.GetIndex(), 0, wxLIST_STATE_SELECTED);
}

void RainExceptionDialog::onOK(wxCommandEvent& WXUNUSED(e))
{
  EndModal(wxID_OK);
}

void RainExceptionDialog::onSave(wxCommandEvent& WXUNUSED(e))
{
  if(wxTheClipboard->Open())
  {
    wxString sMessage;

    size_t count = m_aMessages.GetCount();
    for(size_t n = 0; n < count; ++n)
    {
      sMessage << m_aFiles[n] << wxT(" line ") << m_aLines[n] << wxT(": ") << m_aMessages[n] << wxT("\n");
    }

    wxTheClipboard->SetData( new wxTextDataObject(sMessage) );
    wxTheClipboard->Close();
    wxMessageBox(wxT("Error copied to clipboard"), wxT("Copy to Clipboard"), wxOK | wxICON_INFORMATION, this);
  }
  else
  {
    wxMessageBox(wxT("Cannot access clipboard"), wxT("Copy to Clipboard"), wxOK | wxICON_ERROR, this);
  }
}

void RainExceptionDialog::onDetails(wxCommandEvent& WXUNUSED(e))
{
  wxSizer *sizer = GetSizer();

  if(m_bShowingDetails)
  {
    m_pDetailsBtn->SetLabel(wxT("&Details >>"));
    sizer->Detach(m_pDetailList);
    sizer->Detach(m_pStaticLine);
    sizer->Detach(m_pSaveBtn);
  }
  else
  {
    m_pDetailsBtn->SetLabel(wxT("<< &Details"));
    if(!m_pDetailList)
    {
      _addExtraControls();
    }

    sizer->Add(m_pStaticLine, 0, wxEXPAND | (wxALL & ~wxTOP), 10);
    sizer->Add(m_pDetailList, 1, wxEXPAND | (wxALL & ~wxTOP), 10);
    sizer->Add(m_pSaveBtn, 0, wxALIGN_RIGHT | (wxALL & ~wxTOP), 10);
  }

  m_bShowingDetails = !m_bShowingDetails;

  m_minHeight = m_maxHeight = -1;

  wxSize sizeTotal = GetSize(), sizeClient = GetClientSize();
  wxSize size = sizer->GetMinSize();
  size.x += sizeTotal.x - sizeClient.x;
  size.y += sizeTotal.y - sizeClient.y;

  if(m_bShowingDetails)
  {
    size.x *= 3;
  }
  else
  {
    m_maxHeight = size.y;
  }

  SetSizeHints(size.x, size.y, m_maxWidth, m_maxHeight);
  SetSize(size.x, size.y);
}

void RainExceptionDialog::show(RainException* pE, bool bDeleteException)
{
  RainExceptionDialog oDialog(wxGetActiveWindow(), pE);
  oDialog.ShowModal();
  if(bDeleteException)
    delete pE;
}
