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
#include <wx/combobox.h>
#include <wx/frame.h>
#include <wx/propgrid/propgrid.h>
#include <wx/sizer.h>
#include <wx/treectrl.h>
#include <Rainman2.h>

class frmAttribPreview : public wxFrame
{
#ifdef RAINMAN2_USE_RBF
public:
  frmAttribPreview();
  ~frmAttribPreview();

  void onPrevFile  (wxCommandEvent &e);
  void onNextFile  (wxCommandEvent &e);
  void onResize    (wxSizeEvent    &e);
  void onTreeExpand(wxTreeEvent    &e);
  void onTreeSelect(wxTreeEvent    &e);

  void setFileStore(IFileStore *pStore);

  void loadFile(const RainString& sFilename);

  enum
  {
    BTN_PREV_FILE = wxID_HIGHEST + 1,
    BTN_NEXT_FILE,
  };

protected:
  wxComboBox      *m_pFilenameList;
  wxTreeCtrl      *m_pAttribTree;
  wxPropertyGrid  *m_pPropGrid;
  IFileStore      *m_pFileStore;
  RbfAttributeFile m_oAttribFile;

  void _addTreeItemChildren(wxTreeItemId oParent);

  DECLARE_EVENT_TABLE();
#endif
};
