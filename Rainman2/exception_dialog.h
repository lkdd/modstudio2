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
#include "exception.h"
#ifdef RAINMAN2_USE_WX
#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/statline.h>
#include "exception.h"
#pragma warning(push)
#pragma warning(disable: 4275) // non dll-interface class used as base

class RAINMAN2_API RainExceptionDialog : public wxDialog
{
public:
  RainExceptionDialog(wxWindow *parent, RainException *pException);

  static void show(RainException* pE, bool bDeleteException = true);

  void onOK(wxCommandEvent& e);
  void onDetails(wxCommandEvent& e);
  void onSave(wxCommandEvent& e);
  void onListSelect(wxListEvent& e);

private:
  void _addExtraControls();

  wxArrayString m_aMessages;
  wxArrayString m_aFiles;
  wxArrayLong   m_aLines;

  wxButton *m_pDetailsBtn;
  bool      m_bShowingDetails;

  wxListCtrl   *m_pDetailList;
  wxStaticLine *m_pStaticLine;
  wxButton     *m_pSaveBtn;

  DECLARE_EVENT_TABLE()
};

#define EXCEPTION_MESSAGE_BOX(message, e) RainExceptionDialog::show(new RainException(__WFILE__, __LINE__, message, e));
#define EXCEPTION_MESSAGE_BOX_1(message, a1, e) RainExceptionDialog::show(new RainException(__WFILE__, __LINE__, e, message, a1));
#define CATCH_MESSAGE_BOX(message, cleanup) catch (RainException *pRainError) { EXCEPTION_MESSAGE_BOX(message, pRainError); cleanup; }
#define CATCH_MESSAGE_BOX_1(message, a1, cleanup) catch (RainException *pRainError) { EXCEPTION_MESSAGE_BOX_1(message, a1, pRainError); cleanup; }
#pragma warning(pop)
#endif
