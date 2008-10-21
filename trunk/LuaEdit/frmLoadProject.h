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
#include <wx/progdlg.h>
#include <Rainman2.h>
#include <vector>

//! Dialog for choosing a pipeline project to load
class frmLoadProject : public wxFrame
{
public:
  //! Default constructor
  frmLoadProject();

  //! Virtual destructor
  ~frmLoadProject();

  enum
  {
    BMP_DONATE = wxID_HIGHEST + 1, //!< Donation bitmap button
    BTN_BROWSE_PIPELINE,           //!< "Browse..." button for pipeline.ini file
    BTN_LOAD,                      //!< Load button for pipeline project
    CMB_PIPELINE_FILES,            //!< Combobox for pipeline.ini file
    LST_PIPELINE_PROJECTS,         //!< List of pipeline projects
  };

  //! Event handler for "Browse..." button click
  void onBrowse(wxCommandEvent& e);
  //! Event handler for donation button click
  void onDonate(wxCommandEvent& e);
  //! Event handler for "Load" button click
  void onLoad  (wxCommandEvent& e);
  //! Event handler for "New" button click
  void onNew   (wxCommandEvent& e);
  //! Event handler for whenever the pipeline file combobox changes value
  /*!
    This gets all events which cause a change of value; list (de)selections and text input.
    Hence if the user types a stream of characters, this will be called after each character,
    as the value changes each time.
  */
  void onPipelineFilesSelect     (wxCommandEvent& e);
  //! Event handler for (de)selection of a pipeline project
  void onPipelineProjectSelected (wxListEvent   & e);
  //! Event handler for the activation (e.g. double clicking) of a pipeline project
  void onPipelineProjectActivated(wxListEvent   & e);
  //! Event handler for clicking of the headers in the pipeline project list (which sorts the list on that column)
  void onPipelineProjectsSort    (wxListEvent   & e);
  //! Event handler for the "Quit" button (does not catch the user closing the window via the window chrome, logging off, Alt+F4, etc.)
  void onQuit  (wxCommandEvent& e);
  //! Event handler for window resize
  void onResize(wxSizeEvent   & e);

protected:
  void _refreshProjectList();
  void _adjustProjectListItemFont(long iIndex, wxFont (*fnAdjust)(wxFont));
  void _loadEditor(IniFile::Section* pPipelineSection);
  void _loadEditor(IDirectory *pDirectory, IFileStore *pFileStore, IniFile::Section* pPipelineSection = 0);
  bool _isSideBySideAttrib(RainString sPath);

  wxProgressDialog *_getPreloadProgressDialog(bool bCreateIfNeeded = true);
  void _cancelPreloadProgressDialog();

  IniFile m_oPipelineFile;
  wxComboBox* m_pPipelineFileList;
  wxListCtrl* m_pPipelineProjects;
  wxButton* m_pLoadButton,
          * m_pNewButton;
  wxProgressDialog* m_pPreloadProgress;
  bool m_bCanLoad;
  long m_iLastColumnSort,
       m_iSelectedProject;

  DECLARE_EVENT_TABLE();
};

