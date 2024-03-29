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
#include "utility.h"
#include <wx/filename.h>
extern "C" {
#include <lauxlib.h>
}

void luaX_pushrstring(lua_State *L, const RainString& s)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  for(size_t i = 0; i < s.length(); ++i)
    luaL_addchar(&b, s.getCharacters()[i]);
  luaL_pushresult(&b);
}

void luaX_pushwstring(lua_State *L, const wchar_t* s)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  size_t iLen = wcslen(s);
  for(size_t i = 0; i < iLen; ++i)
    luaL_addchar(&b, s[i]);
  luaL_pushresult(&b);
}

RainString GetParentFolder(RainString sPath, int iNumLevelsUp)
{
  if(sPath.suffix(1) == L"\\")
    sPath = sPath.mid(0, sPath.length() - 1);
  for(; iNumLevelsUp; --iNumLevelsUp)
  {
    sPath = sPath.beforeLast('\\');
  }
  sPath += L"\\";
  return sPath;
}

PipelineFileOpenDialog::PipelineFileOpenDialog(wxWindow *pParent, wxComboBox *pFileList)
  : wxFileDialog(pParent, L"Choose the pipeline file", wxEmptyString, wxEmptyString, L"Pipeline files (pipeline.ini)|pipeline.ini|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST)
  , m_pFileList(pFileList)
{
  if(!pFileList->GetValue().IsEmpty())
  {
    wxFileName oFileName(pFileList->GetValue());
    SetDirectory(oFileName.GetPath());
    SetFilename(oFileName.GetFullName());
    if(oFileName.GetFullName().CmpNoCase(L"pipeline.ini") != 0)
      SetFilterIndex(1);
  }
}

bool PipelineFileOpenDialog::show()
{
  if(ShowModal() != wxID_OK)
    return false;
  
  int iExistingPosition = m_pFileList->FindString(GetPath(), false);
  if(iExistingPosition != wxNOT_FOUND)
    m_pFileList->Delete(iExistingPosition);
  m_pFileList->Insert(GetPath(), 0);
  m_pFileList->SetSelection(0);

  return true;
}
