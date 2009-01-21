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
#include "application.h"
#include "frmFileStoreViewer.h"
#include <wx/sysopt.h>

IMPLEMENT_APP(SGAReader2App)

IniFile SGAReader2App::Config;

bool SGAReader2App::OnInit()
{
  // Watch for MSVC CRT memory allocation (tracing memory leaks)
  //_CrtSetBreakAlloc(4874);

	// If we can, force toolbar images to be 32-bit with transparency
  if(GetComCtl32Version() >= 600 && ::wxDisplayDepth() >= 32)
    wxSystemOptions::SetOption(wxT("msw.remap"), 2);
  // Otherwise, simply prevent the colours in toolbar images being changed
  else
	  wxSystemOptions::SetOption(wxT("msw.remap"), 0);

  wxInitAllImageHandlers();

  try
  {
    m_sConfigPath = RainString(argv[0]).beforeLast('\\') + L"\\sga_reader_2_config.ini";
    Config.setSpecialCharacters('#');
    if(RainDoesFileExist(m_sConfigPath))
      Config.load(m_sConfigPath);
    else
      Config.clear();
  }
  CATCH_MESSAGE_BOX(L"Cannot load config", {});

  frmFileStoreViewer *pForm = new frmFileStoreViewer(wxT("SGA Reader 2"));
  pForm->Show(TRUE);
  SetTopWindow(pForm);

  return TRUE;
}

int SGAReader2App::OnExit()
{
  try
  {
    Config.save(m_sConfigPath);
  }
  CATCH_MESSAGE_BOX(L"Cannot save config", {});

  return wxApp::OnExit();
}
