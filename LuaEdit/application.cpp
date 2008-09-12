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
#include "application.h"
#include "frmLoadProject.h"
#include <wx/sysopt.h>

IMPLEMENT_APP(LuaEditApp)

IniFile LuaEditApp::Config;

bool LuaEditApp::OnInit()
{
	wxSystemOptions::SetOption(wxT("msw.remap"), 0);
  wxInitAllImageHandlers();

  try
  {
    Config.setSpecialCharacters('#');
    if(RainDoesFileExist(L"lua_edit_config.ini"))
      Config.load(L"lua_edit_config.ini");
    else
      Config.clear();
  }
  CATCH_MESSAGE_BOX(L"Cannot load config", {});

  try
  {
    RgdDictionary::checkStaticHashes();
  }
  CATCH_MESSAGE_BOX(L"RGD dictionary code needs updating", {});

  frmLoadProject *pForm = new frmLoadProject();
  pForm->Show(TRUE);
  SetTopWindow(pForm);

  return TRUE;
}

int LuaEditApp::OnExit()
{
  try
  {
    Config.save(L"lua_edit_config.ini");
  }
  CATCH_MESSAGE_BOX(L"Cannot save config", {});

  return wxApp::OnExit();
}
