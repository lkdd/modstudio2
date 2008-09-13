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
#include <wx/combobox.h>
#include <wx/listctrl.h>
#include <Rainman2.h>
#include <vector>

class frmLoadProject : public wxFrame
{
public:
  frmLoadProject();
  ~frmLoadProject();

  enum
  {
    BMP_DONATE = wxID_HIGHEST + 1,
    BTN_BROWSE_PIPELINE,
    CMB_PIPELINE_FILES,
    LST_PIPELINE_PROJECTS,
    BTN_LOAD,
  };

  void onBrowse(wxCommandEvent& e);
  void onDonate(wxCommandEvent& e);
  void onLoad  (wxCommandEvent& e);
  void onNew   (wxCommandEvent& e);
  void onPipelineFilesSelect     (wxCommandEvent& e);
  void onPipelineProjectSelected (wxListEvent   & e);
  void onPipelineProjectActivated(wxListEvent   & e);
  void onPipelineProjectsSort    (wxListEvent   & e);
  void onQuit  (wxCommandEvent& e);
  void onResize(wxSizeEvent   & e);

protected:
  void _refreshProjectList();
  void _adjustProjectListItemFont(long iIndex, wxFont (*fnAdjust)(wxFont));
  void _loadEditor(IniFile::Section* pPipelineSection);
  void _loadEditor(IDirectory *pDirectory, IFileStore *pFileStore);

  IniFile m_oPipelineFile;
  wxComboBox* m_pPipelineFileList;
  wxListCtrl* m_pPipelineProjects;
  wxButton* m_pLoadButton,
          * m_pNewButton;
  bool m_bCanLoad;
  long m_iLastColumnSort,
       m_iSelectedProject;
  DECLARE_EVENT_TABLE();
};

