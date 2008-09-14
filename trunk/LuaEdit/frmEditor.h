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
#include <wx/aui/aui.h>
#include <wx/stc/stc.h>
#pragma warning(push)
#pragma warning(disable: 4251)
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/manager.h>
#pragma warning(pop)
#include <wx/treectrl.h>
#include <Rainman2.h>

class LuaAttribTreeData;

class frmLuaEditor : public wxFrame
{
public:
  frmLuaEditor();
  ~frmLuaEditor();

  enum
  {
    LST_RECENT = wxID_HIGHEST + 1,
    NB_EDITORS,
    TB_SAVELUA,
    TB_SAVEBIN,
    TREE_INHERIT,
    TREE_ATTRIB,
    TXT_CODE,
  };

  void onAttribTreeItemActivate (wxTreeEvent        &e);
  void onAttribTreeItemExpanding(wxTreeEvent        &e);
  void onEditorTabChange        (wxAuiNotebookEvent &e);
  void onListItemActivate       (wxCommandEvent     &e); 
  void onResize                 (wxSizeEvent        &e);
  void onSaveBinary             (wxCommandEvent     &e);
  void onSaveLua                (wxCommandEvent     &e);
  void onStyleNeeded            (wxStyledTextEvent  &e);
  void onTreeItemActivate       (wxTreeEvent        &e);

  void setSource(IDirectory* pRootDirectory, IFileStore *pDirectoryStore) throw(...);

protected:
  void _initToolbar();
  void _initTextControl();
  void _populateInheritanceTree();
  void _populatePropertyGrid(LuaAttribTreeData *pData);
  void _doLoadLua(IFile *pFile, RainString sPath);
  wxImage _loadPng(wxString sName) throw(...);
  wxString _getValueName(IAttributeValue* pValue);
  wxPGProperty* _getValueEditor(IAttributeValue* pValue, wxString sName = L"", wxString sNameId = L"");

  wxAuiManager m_oManager;
  wxAuiNotebook *m_pNotebook;
  wxStyledTextCtrl *m_pLuaCode;
  wxPropertyGridManager *m_pLuaProperties;
  wxTreeCtrl *m_pInheritanceTree,
             *m_pAttribTree;
  wxTreeItemId m_oCurrentAttribTreeItem;
  wxListBox *m_pRecentLuas;
  IDirectory *m_pRootDirectory;
  IFileStore *m_pDirectoryStore;
  LuaAttrib *m_pAttributeFile;
  bool m_bAttributeFileChanged;

  DECLARE_EVENT_TABLE();
};
