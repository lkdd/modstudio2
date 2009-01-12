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
#include <wx/frame.h>
#include <wx/imaglist.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <Rainman2.h>

class frmFileStoreViewer : public wxFrame
{
public:
  frmFileStoreViewer(const wxString& sTitle);
  ~frmFileStoreViewer();

  void onAbout         (wxCommandEvent &e);
  void onExit          (wxCommandEvent &e);
  void onOpen          (wxCommandEvent &e);
  void onResize        (wxSizeEvent    &e);
  void onFileDoAction  (wxListEvent    &e);
  void onTreeExpanding (wxTreeEvent    &e);
  void onTreeSelection (wxTreeEvent    &e);

  enum
  {
    TREE_FILES = wxID_HIGHEST + 1,
    LST_DETAILS,
  };

protected:
  void _doLoadArchive(wxString sPath);

  SgaArchive  *m_pArchive;
  wxTreeCtrl  *m_pFilesTree;
  wxListCtrl  *m_pDetailsView;
  wxChoice    *m_pDirectoryDefaultActionList;
  wxChoice    *m_pFileDefaultActionList;
  wxImageList *m_pIcons;
  wxString     m_sBaseTitle,
               m_sDateFormat,
               m_sCurrentShortName;
  RainString   m_sCurrentArchivePath;
  wxTreeItemId m_oCurrentFolderId;

  DECLARE_EVENT_TABLE();
};
