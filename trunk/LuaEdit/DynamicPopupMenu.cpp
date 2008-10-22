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
#include "DynamicPopupMenu.h"

DynamicPopupMenu::DynamicPopupMenu()
  : wxMenu(), m_iIdSelected(wxID_ANY)
{
  SetEventHandler(this);
}

int DynamicPopupMenu::append(const wxString& sText, const wxString& sHelpString)
{
  return _connect(Append(wxID_ANY, sText, sHelpString)->GetId());
}

int DynamicPopupMenu::appendCheckItem(const wxString& sText, const wxString& sHelpString, bool bChecked)
{
  wxMenuItem *pItem = AppendCheckItem(wxID_ANY, sText, sHelpString);
  pItem->Check(bChecked);
  return _connect(pItem->GetId());
}

void DynamicPopupMenu::appendSeparator()
{
  AppendSeparator();
}

int DynamicPopupMenu::_connect(int iID)
{
  Connect(iID, iID, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(DynamicPopupMenu::_onClick));
  return iID;
}

void DynamicPopupMenu::_onClick(wxCommandEvent &e)
{
  m_iIdSelected = e.GetId();
}

int DynamicPopupMenu::popup(wxWindow *pParent)
{
  m_iIdSelected = wxID_ANY;
  pParent->PopupMenu(this);
  return m_iIdSelected;
}

int DynamicPopupMenu::getId()
{
  return m_iIdSelected;
}
