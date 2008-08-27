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
#include <Rainman2.h>
#include <vector>

class IRecentLoadSource
{
public:
  virtual ~IRecentLoadSource();

  virtual RainString getSourceType() = 0;
  virtual RainString getDisplayText() = 0;
  virtual RainString getSaveText() = 0;
  virtual void performLoad(IDirectory **ppDirectory, IFileStore **pFileStore) = 0;
  virtual bool isSameAs(IRecentLoadSource* pOtherSource) = 0;
};

class frmLoadProject : public wxFrame
{
public:
  frmLoadProject();
  ~frmLoadProject();

  enum
  {
    BTN_LOAD_PIPELINE = wxID_HIGHEST + 1,
    BTN_LOAD_MODULE_WA,
    BTN_LOAD_MODULE_DC,
    BTN_LOAD_MODULE_SS,
    BTN_LOAD_MODULE_OF,
    BTN_LOAD_FOLDER,
    LST_LOAD_RECENT,
    BMP_DONATE,
  };

  void onResize(wxSizeEvent& e);
  void onDonate(wxCommandEvent& e);
  void onQuit(wxCommandEvent& e);
  void onLoadFolder(wxCommandEvent& e);
  void onLoadRecent(wxCommandEvent& e);

protected:
  void _loadRecentList();
  void _saveRecentList();
  void _addNewRecentSource(IRecentLoadSource* pRecentSource);
  void _loadEditor(IDirectory *pDirectory, IFileStore *pFileStore);
  void _loadFromRecent(IRecentLoadSource* pRecentSource);

  std::vector<IRecentLoadSource*> m_vRecentSources;
  wxStaticText* m_pText;
  wxListBox* m_pRecentList;
  wxString m_sText;
  DECLARE_EVENT_TABLE();
};

