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
#pragma once
#include <wx/dialog.h>
#include <wx/combobox.h>
#include <wx/sizer.h>
#include <wx/statusbr.h>
#include <Rainman2.h>

class frmExtract : public wxDialog
{
public:
  frmExtract(wxWindow *pParent, SgaArchive *pArchive, RainString sDefaultExtractFolder, 
             const RainString& sPathToExtract, wxStatusBar *pResultNotification = 0);
  ~frmExtract();

  void onExtract(wxCommandEvent &e);
  void onBrowse (wxCommandEvent &e);

  enum
  {
    TXT_FILENAME = wxID_HIGHEST + 1,
    BTN_BROWSE,
  };

protected:
  bool _ensureDirectoryExists(RainString sFolder);

  SgaArchive  *m_pArchive;
  wxStatusBar *m_pPositiveResultNoficiationBar;
  wxComboBox  *m_pDestinationPath;
  wxSizer     *m_pButtonsSizer;
  RainString   m_sPathToExtract;
  bool         m_bIsDirectory;

  static RainString s_sLastSaveDir;
  static bool       s_bLastSaveDirHasDir;

  DECLARE_EVENT_TABLE();
};
