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
#include "frmEditor.h"
#include "frmCommandLineOptions.h"
#include "InheritanceBuilder.h"
#include "utility.h"
#include "application.h"
#include <wx/statusbr.h>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include <wx/artprov.h>
#include <wx/splitter.h>
#include <wx/utils.h>
#include <wx/mstream.h>
#include "TreeCtrlMemory.h"
#include "DynamicPopupMenu.h"
#include "stdext.h"

#define NO_REMEMBER_AUI // For debugging

BEGIN_EVENT_TABLE(frmLuaEditor, wxFrame)
  /* order is alphabetical based on event macro and window ID */
  EVT_AUINOTEBOOK_PAGE_CHANGING(NB_EDITORS, frmLuaEditor::onEditorTabChange)
  EVT_LISTBOX(LST_RECENT,                   frmLuaEditor::onListItemActivate)
  EVT_STC_STYLENEEDED(TXT_CODE,             frmLuaEditor::onStyleNeeded)
  EVT_TREE_ITEM_ACTIVATED(TREE_INHERIT,     frmLuaEditor::onTreeItemActivate)
  EVT_TREE_ITEM_EXPANDING(TREE_ATTRIB,      frmLuaEditor::onAttribTreeItemExpanding)
  EVT_TREE_SEL_CHANGED(TREE_ATTRIB,         frmLuaEditor::onAttribTreeItemActivate)
  EVT_TOOL(TB_SAVEBIN,                      frmLuaEditor::onSaveBinary)
  EVT_TOOL(TB_SAVEBIN_BATCH,                frmLuaEditor::onSaveBinaryBatch)
  EVT_TOOL(TB_SAVELUA,                      frmLuaEditor::onSaveLua)
  EVT_TOOL(TB_SAVELUA_BATCH,                frmLuaEditor::onSaveLuaBatch)
END_EVENT_TABLE()

class GameAppCommandInfo : public wxObjectRefData
{
public:
  GameAppCommandInfo(wxString sCommand, wxString sName)
    : m_sCommand(sCommand), m_sName(sName), m_bWrappedInAnything(false)
    , m_rIniSection(LuaEditApp::Config[L"cmdopt-" + sCommand])
  {
    if(m_rIniSection.empty())
    {
      m_rIniSection.appendValue(L"-modname %MODNAME%", "true");
      m_rIniSection.appendValue(L"-dev", "true");
      m_rIniSection.appendValue(L"-nomovies", "true");
      m_rIniSection.appendValue(L"-window", "false");
    }

    for(IniFile::Section::iterator itrValue = m_rIniSection.begin(), itrValueEnd = m_rIniSection.end(); itrValue != itrValueEnd; ++itrValue)
    {
      m_oParameters.addParameter(itrValue->key(), itrValue->value() == L"true");
    }

    m_oParameters.addPlaceholder(L"%MODNAME%", L"ModNameTODO");
  }

  const wxString& name() const {return m_sName;}
  const wxString& command() const {return m_sCommand;}
  CommandLineParameters& parameters() {return m_oParameters;}
  IniFile::Section& iniSection() {return m_rIniSection;}

  wxObject *wrapInObject()
  {
    wxObject *p = new wxObject;
    if(!m_bWrappedInAnything)
    {
      // wxObjectRefData initialises the reference count to 1, so first time round, do not
      // increment the reference count
      p->SetRefData(this);
      m_bWrappedInAnything = true;
    }
    else
    {
      // This is seriously retarded, and there must be a nicer way of doing it <_<
      // In order to increment the reference count, Ref must be called, but it can only be
      // called with an existing object :/
      wxObject o;
      o.SetRefData(this);
      p->Ref(o);
      o.SetRefData(0);
    }
    return p;
  }

protected:
  CommandLineParameters m_oParameters;
  IniFile::Section& m_rIniSection;
  wxString m_sCommand,
           m_sName;
  bool m_bWrappedInAnything;
};

class RecentLuaListData : public wxClientData
{
public:
  wxTreeItemId oTreeItem;
};

class LuaAttribTreeData : public wxTreeItemData
{
public:
  LuaAttribTreeData(IAttributeValue *_pValue, IAttributeTable *_pTable)
    : pValue(_pValue), pTable(_pTable), pReference(0)
  {
    if(pValue && !pTable && pValue->isTable())
      pTable = pValue->getValueTable();
    if(pTable)
    {
      unsigned long iRefKey = pTable->findChildIndex(RgdDictionary::_REF);
      if(iRefKey != IAttributeTable::NO_INDEX)
      {
        pReference = pTable->getChild(iRefKey);
      }
    }
  }
  ~LuaAttribTreeData()
  {
    delete pValue;
    delete pTable;
    delete pReference;
  }
  IAttributeValue *pValue;
  IAttributeTable *pTable;
  IAttributeValue *pReference;
};

#define mySTC_STYLE_BOLD 1
#define mySTC_STYLE_ITALIC 2
#define mySTC_STYLE_UNDERL 4
#define mySTC_STYLE_HIDDEN 8

struct StyleInfo {
    const wxChar *name;
    const wxChar *foreground;
    const wxChar *background;
    const wxChar *fontname;
    int fontsize;
    int fontstyle;
    int lettercase;
};

static StyleInfo g_StylePrefs [] = {
/*      identifer                    name                foreground       background   font  size       style    case*/
/* wxSTC_LUA_DEFAULT */       {_T("Default"       ), _T("BLACK"       ), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_LUA_COMMENT */       {_T("Comment"       ), _T("FOREST GREEN"), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_LUA_COMMENTLINE */   {_T("Comment line"  ), _T("FOREST GREEN"), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_LUA_COMMENTDOC */    {_T("Comment (Doc)" ), _T("FOREST GREEN"), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_LUA_NUMBER */        {_T("Number"        ), _T("ORANGE"      ), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_LUA_WORD */          {_T("Keyword1"      ), _T("BLUE"        ), _T("WHITE"), _T(""), 10, mySTC_STYLE_BOLD, 0},
/* wxSTC_LUA_STRING */        {_T("String"        ), _T("GREY"        ), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_LUA_CHARACTER */     {_T("Character"     ), _T("GREY"        ), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_LUA_LITERALSTRING */ {_T("Literal String"), _T("PURPLE"      ), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_LUA_PREPROCESSOR */  {_T("Preprocessor"  ), _T("GREY"        ), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_LUA_OPERATOR */      {_T("Operator"      ), _T("RED"         ), _T("WHITE"), _T(""), 10, mySTC_STYLE_BOLD, 0},
/* wxSTC_LUA_IDENTIFIER */    {_T("Identifier"    ), _T("BLACK"       ), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_LUA_STRINGEOL */     {_T("String (EOL)"  ), _T("PURPLE"      ), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_TYPE_WORD2 */        {_T("Keyword2"      ), _T("MEDIUM BLUE" ), _T("WHITE"), _T(""), 10, mySTC_STYLE_BOLD, 0},
/* wxSTC_TYPE_WORD3 */        {_T("Keyword3"      ), _T("TAN"         ), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_TYPE_WORD4 */        {_T("Keyword4"      ), _T("FIREBRICK"   ), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_TYPE_WORD5 */        {_T("Keyword5"      ), _T("DARK GREY"   ), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_TYPE_WORD6 */        {_T("Keyword6"      ), _T("GREY"        ), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_TYPE_WORD7 */        {_T("Keyword7"      ), _T("GREY"        ), _T("WHITE"), _T(""), 10, 0               , 0},
/* wxSTC_TYPE_WORD8 */        {_T("Keyword8"      ), _T("GREY"        ), _T("WHITE"), _T(""), 10, 0               , 0},
/*      identifer                    name                foreground       background   font  size       style    case*/
};

struct HightlightSet
{
	int type;
	const wxChar* words;
};

static HightlightSet g_LuaWords[] = {
	{wxSTC_LUA_DEFAULT      , 0},
	{wxSTC_LUA_COMMENTLINE  , 0},
	{wxSTC_LUA_COMMENTDOC   , 0},
	{wxSTC_LUA_NUMBER       , 0},
	{wxSTC_LUA_STRING       , 0},
	{wxSTC_LUA_CHARACTER    , 0},
	{wxSTC_LUA_LITERALSTRING, 0},
	{wxSTC_LUA_PREPROCESSOR , 0},
	{wxSTC_LUA_OPERATOR     , 0},
	{wxSTC_LUA_IDENTIFIER   , 0},
	{wxSTC_LUA_STRINGEOL    , 0},
	{wxSTC_LUA_WORD         , _T("true false nil") },
	{wxSTC_LUA_WORD2        , _T("and break do else elseif end false for function if in local nil not or repeat ")
						                _T("return then true until while _G") },
	{wxSTC_LUA_WORD3        , _T("Reference Inherit GameData") },
	{wxSTC_LUA_WORD4        , _T("InheritMeta MetaData") },
	{wxSTC_LUA_WORD5        , 0},
	{wxSTC_LUA_WORD6        , 0},
	{wxSTC_LUA_WORD7        , 0},
	{wxSTC_LUA_WORD8        , 0},
	{-1, 0}
};

void frmLuaEditor::_initTextControl()
{
  lua_State *L = luaL_newstate();
  int iErr = luaL_loadfile(L, "./editor_config.lua");
  if(iErr == 0)
    iErr = lua_pcall(L, 0, 0, 0);
  if(iErr)
  {
    RainString sError;
    sError.printf(L"Cannot load editor configuration file (editor_config.lua): %S", lua_tostring(L, -1));
    lua_close(L);
    L = 0;
  }

  m_pLuaCode->StyleClearAll();
	m_pLuaCode->SetTabWidth(2);
	m_pLuaCode->SetLexer(wxSTC_LEX_LUA);
	m_pLuaCode->SetProperty(wxT("fold.compact"), wxT("0"));
  m_pLuaCode->SetViewEOL(false);
  m_pLuaCode->SetIndentationGuides(true);
  m_pLuaCode->SetEdgeMode(wxSTC_EDGE_NONE);
  m_pLuaCode->SetViewWhiteSpace(wxSTC_WS_INVISIBLE);
  m_pLuaCode->SetOvertype(false);
  m_pLuaCode->SetReadOnly(false);
  m_pLuaCode->SetWrapMode(wxSTC_WRAP_NONE);
  wxFont oFont(10, wxMODERN, wxNORMAL, wxNORMAL);
  m_pLuaCode->StyleSetFont(wxSTC_STYLE_DEFAULT, oFont);
  m_pLuaCode->StyleSetBackground(wxSTC_STYLE_DEFAULT, *wxWHITE);
  m_pLuaCode->StyleSetForeground(wxSTC_STYLE_LINENUMBER, wxColour(_T("DARK GREY")));
  m_pLuaCode->StyleSetBackground(wxSTC_STYLE_LINENUMBER, wxColour(_T("WHEAT")));
  m_pLuaCode->SetUseTabs(false);
  m_pLuaCode->SetTabIndents(true);
  m_pLuaCode->SetBackSpaceUnIndents(true);
  m_pLuaCode->SetIndent(2);

  for(int i = 0; i < wxSTC_STYLE_LASTPREDEFINED; ++i)
    m_pLuaCode->StyleSetFont(i, oFont);
  m_pLuaCode->StyleSetForeground(wxSTC_STYLE_DEFAULT, wxColour (_T("DARK GREY")));
  m_pLuaCode->StyleSetForeground(wxSTC_STYLE_INDENTGUIDE, wxColour (_T("DARK GREY")));
  for(int i = 0; g_LuaWords[i].type != -1; ++i)
  {
    StyleInfo &curType = g_StylePrefs [g_LuaWords[i].type];
    wxFont oFont(curType.fontsize, wxMODERN, wxNORMAL, wxNORMAL, false, curType.fontname);
    RainString sTemp1, sTemp2, sTemp3;

    if(L)
    {
      lua_getglobal(L, "colours");
      if(lua_type(L, -1) != LUA_TNIL)
      {
        luaX_pushwstring(L, curType.name);
        lua_gettable(L, -2);
        if(lua_type(L, -1) != LUA_TNIL)
        {
          lua_rawgeti(L, -1, 1);
          if(lua_type(L, -1) != LUA_TNIL)
          {
            sTemp1 = lua_tostring(L, -1);
            curType.foreground = sTemp1.getCharacters();
          }
          lua_pop(L, 1);
          lua_rawgeti(L, -1, 2);
          if(lua_type(L, -1) != LUA_TNIL)
          {
            sTemp2 = lua_tostring(L, -1);
            curType.background = sTemp2.getCharacters();
          }
          lua_pop(L, 1);
          lua_rawgeti(L, -1, 3);
          if(lua_type(L, -1) != LUA_TNIL)
          {
            curType.fontstyle = lua_tointeger(L, -1);
          }
          lua_pop(L, 1);
        }
        lua_pop(L, 1);
      }
      lua_pop(L, 1);
    }

    m_pLuaCode->StyleSetFont(g_LuaWords[i].type, oFont);
    if(curType.foreground)
      m_pLuaCode->StyleSetForeground(g_LuaWords[i].type, wxColour (curType.foreground));
    if(curType.background)
      m_pLuaCode->StyleSetBackground(g_LuaWords[i].type, wxColour (curType.background));
    m_pLuaCode->StyleSetBold(g_LuaWords[i].type, (curType.fontstyle & mySTC_STYLE_BOLD) > 0);
    m_pLuaCode->StyleSetItalic(g_LuaWords[i].type, (curType.fontstyle & mySTC_STYLE_ITALIC) > 0);
    m_pLuaCode->StyleSetUnderline(g_LuaWords[i].type, (curType.fontstyle & mySTC_STYLE_UNDERL) > 0);
    m_pLuaCode->StyleSetVisible(g_LuaWords[i].type, (curType.fontstyle & mySTC_STYLE_HIDDEN) == 0);
    m_pLuaCode->StyleSetCase(g_LuaWords[i].type, curType.lettercase);
    int iWordIndex = -1;
    if(g_LuaWords[i].type == wxSTC_LUA_WORD)
      iWordIndex = 0;
    else if(g_LuaWords[i].type >= wxSTC_LUA_WORD2 && g_LuaWords[i].type <= wxSTC_LUA_WORD8)
      iWordIndex = g_LuaWords[i].type - wxSTC_LUA_WORD2 + 1;     
    if(iWordIndex != -1)
    {
      if(L)
      {
        lua_getglobal(L, "keywords");
        if(lua_type(L, -1) != LUA_TNIL)
        {
          lua_rawgeti(L, -1, iWordIndex + 1);
          if(lua_type(L, -1) != LUA_TNIL)
          {
            sTemp3 = lua_tostring(L, -1);
            g_LuaWords[i].words = sTemp3.getCharacters();
          }
          lua_pop(L, 1);
        }
        lua_pop(L, 1);
      }
      m_pLuaCode->SetKeyWords(iWordIndex, g_LuaWords[i].words);
    }
  }
	m_pLuaCode->SetMarginWidth(0, m_pLuaCode->TextWidth(wxSTC_STYLE_LINENUMBER, _T("_999999")));

  if(L)
    lua_close(L);
}

frmLuaEditor::frmLuaEditor()
  : wxFrame(0, wxID_ANY, L"Lua Edit"), m_pRootDirectory(0), m_pDirectoryStore(0), m_pAttributeFile(0)
{
  m_oManager.SetManagedWindow(this);

  CreateStatusBar();
  GetStatusBar()->SetStatusText(L"Ready");

  SetMinSize(wxSize(400,300));

  m_pInheritanceTree = new wxTreeCtrl(this, TREE_INHERIT, wxPoint(0,0), wxSize(250,250), wxTR_DEFAULT_STYLE | wxNO_BORDER | wxTR_HIDE_ROOT);
  m_pRecentLuas = new wxListBox(this, LST_RECENT, wxPoint(0,0), wxSize(250,250), 0, NULL, wxLB_SINGLE | wxNO_BORDER | wxLB_HSCROLL | wxLB_NEEDED_SB);

  wxSize client_size = GetClientSize();
  m_pNotebook = new wxAuiNotebook(this, NB_EDITORS, wxPoint(client_size.x, client_size.y), wxSize(430,200), wxAUI_NB_TOP | wxAUI_NB_TAB_MOVE | wxAUI_NB_TAB_FIXED_WIDTH);

  wxSplitterWindow *pLuaEditSplit = new wxSplitterWindow(m_pNotebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D);
  m_pAttribTree = new wxTreeCtrl(pLuaEditSplit, TREE_ATTRIB, wxDefaultPosition, wxDefaultSize, wxTR_DEFAULT_STYLE | wxNO_BORDER);
  m_pLuaProperties = new wxPropertyGridManager(pLuaEditSplit, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxPG_SPLITTER_AUTO_CENTER | wxPG_TOOLTIPS | wxPG_HIDE_MARGIN | wxTAB_TRAVERSAL | wxPG_DESCRIPTION | wxNO_BORDER); 
  pLuaEditSplit->SplitVertically(m_pAttribTree, m_pLuaProperties);

  wxBitmap bmpIcons(wxT("IDB_ATTRIBICONS"));
  wxImageList* pImageList = new wxImageList(16, 16);
  for(int i = 0; i < bmpIcons.GetWidth(); i += 16)
    pImageList->Add(bmpIcons.GetSubBitmap(wxRect(i, 0, 16, 16)), *wxWHITE);
  m_pAttribTree->AssignImageList(pImageList);

  m_pNotebook->AddPage(pLuaEditSplit, L"Lua Tree", false );
  m_pNotebook->AddPage(m_pLuaCode = new wxStyledTextCtrl(m_pNotebook, TXT_CODE, wxDefaultPosition, wxDefaultSize, wxNO_BORDER) , L"Lua Code", false);
  m_oManager.AddPane(m_pInheritanceTree, wxAuiPaneInfo().Name(L"inheritance").Caption(L"Inheritance Tree").Left().Layer(1).Position(1).CloseButton(false).MaximizeButton(true));
  m_oManager.AddPane(m_pRecentLuas, wxAuiPaneInfo().Name(L"recent").Caption(L"Recent Luas").Left().Layer(1).Position(2).CloseButton(false).MaximizeButton(true));
  m_oManager.AddPane(m_pNotebook, wxAuiPaneInfo().Name(L"notebook_main").CenterPane().PaneBorder(false));

  _initTextControl();
  _initToolbar();
}

wxImage frmLuaEditor::_loadPng(wxString sName) throw(...)
{
  HRSRC hResource = FindResource(wxGetInstance(), sName, wxT("PNG"));
  if(hResource == 0)
   THROW_SIMPLE_(L"Cannot find PNG resource \'%s\'", sName.c_str());

  HGLOBAL hData = LoadResource(wxGetInstance(), hResource);
  if(hData == 0)
    THROW_SIMPLE_(L"Cannot load PNG resource \'%s\'", sName.c_str());

  const void *pData = LockResource(hData);
  if(pData == 0)
    THROW_SIMPLE_(L"Cannot lock PNG resource \'%s\'", sName.c_str());

  wxMemoryInputStream oInput(pData, SizeofResource(wxGetInstance(), hResource));
  wxImage oImg;
  if(!oImg.LoadFile(oInput, wxBITMAP_TYPE_PNG))
    THROW_SIMPLE_(L"Cannot load PNG resource \'%s\' from stream", sName.c_str());

  return oImg;
}

template <class T>
struct array_dtor
{
  void operator()(T* p) const
  {
    delete[] p;
  }
};

void frmLuaEditor::onEditorTabChange(wxAuiNotebookEvent &e)
{
  if(e.GetOldSelection() == -1) // not changing tab
    return;

  try
  {
    if(e.GetSelection() == 0)
    {
      // Code view -> Tree view
      if(!m_pLuaCode->GetModify())
        return;

      std::auto_ptr<ITreeCtrlMemory> pTreeMemory(CreateTreeCtrlMemory());

      m_pLuaProperties->Clear();
      pTreeMemory->store(m_pAttribTree);
      m_pAttribTree->DeleteAllItems();
      wxString sCode = m_pLuaCode->GetText();
      size_t iAsciiLength = wxConvUTF8.FromWChar(NULL, 0, sCode.c_str(), sCode.size());
      std::tr1::shared_ptr<char> sAsciiCode(CHECK_ALLOCATION(new (std::nothrow) char[iAsciiLength]), array_dtor<char>());
      wxConvUTF8.FromWChar(&*sAsciiCode, iAsciiLength, sCode.c_str(), sCode.size());
      std::auto_ptr<IFile> pFile(CHECK_ALLOCATION(new (std::nothrow) MemoryReadFile(&*sAsciiCode, iAsciiLength - 1, false)));
      try
      {
        m_pAttributeFile->loadFromFile(&*pFile);
      }
      CATCH_MESSAGE_BOX(L"Cannot switch to tree view due to invalid code", {e.Veto(); return;});
      wxTreeItemId oRoot = m_pAttribTree->AddRoot(L"GameData", (int)VI_Table, -1, new LuaAttribTreeData(m_pAttributeFile->getGameData(), 0));
      m_pAttribTree->SetItemHasChildren(oRoot, true);
      m_pAttribTree->Expand(oRoot);
      m_pAttribTree->SelectItem(oRoot);
      pTreeMemory->retrieve(m_pAttribTree);
    }
    else
    {
      // Tree view -> Code view
      if(!m_bAttributeFileChanged)
        return;

      std::auto_ptr<MemoryWriteFile> pFile(new MemoryWriteFile);
      m_pAttributeFile->saveToTextFile(&*pFile);
      m_pLuaCode->ClearAll();
      m_pLuaCode->EmptyUndoBuffer();
      m_pLuaCode->AddText(wxString(pFile->getBuffer(), wxConvUTF8, pFile->tell()));
      m_pLuaCode->SetSavePoint();
    }
    m_bAttributeFileChanged = false;
  }
  CATCH_MESSAGE_BOX(L"Error while changing tab", {});
}

void frmLuaEditor::onSaveLuaBatch(wxCommandEvent &e)
{
  ::wxMessageBox(L"Not implemented yet", L"Save Lua Batch", wxOK | wxCENTER | wxICON_ERROR, this);
}

void frmLuaEditor::onSaveLua(wxCommandEvent &e)
{
  try
  {
    // TODO: Remove .test once happy with saving
    std::auto_ptr<IFile> pFile(m_pDirectoryStore->openFile(m_pRootDirectory->getPath() + m_pAttributeFile->getName() + L".test", FM_Write));
    m_pAttributeFile->saveToTextFile(&*pFile);
  }
  CATCH_MESSAGE_BOX(L"Cannot save file", return);

  ::wxMessageBox(L"File saved", L"Save Lua", wxOK | wxICON_INFORMATION, this);
}

void frmLuaEditor::onSaveBinaryBatch(wxCommandEvent &e)
{
  ::wxMessageBox(L"Not implemented yet", L"Save Binary Batch", wxOK | wxCENTER | wxICON_ERROR, this);
}

void frmLuaEditor::onSaveBinary(wxCommandEvent &e)
{
  if(m_pAttributeFile->getName().afterLast('.') == L"nil")
  {
    if(wxMessageBox(L".nil files are not normally saved as binary files. Continue anyway?", L"Save Binary", wxOK | wxCANCEL, this) == wxCANCEL)
    {
      return;
    }
  }

  std::auto_ptr<IFile> pFile(0);
  try
  {
    // TODO: Change this to proper binary file output location
    pFile.reset(m_pDirectoryStore->openFile(m_pRootDirectory->getPath() + m_pAttributeFile->getName().beforeLast('.') + L".rgd", FM_Write));
  }
  CATCH_MESSAGE_BOX(L"Cannot open binary file for writing", return);

  ::wxMessageBox(L"TODO", L"Save Binary");
}

static RainString ParentFolder(RainString sPath, int iNumLevelsUp = 1)
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

void frmLuaEditor::setPipeline(IniFile::Section *pPipelineSection, RainString sPipelineFile) throw(...)
{
  wxToolBar *pRunToolbar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_FLAT | wxTB_NODIVIDER);
  pRunToolbar->SetToolBitmapSize(wxSize(48,48));

  // Usually, the pipeline file is in the game folder
  RainString sAppFolder1 = sPipelineFile.beforeLast('\\') + L"\\";
  // Sometimes it is not though, in which case two levels up from DataFinal (to remove ModName\Data\) should be it
  RainString sAppFolder2 = ParentFolder(sAppFolder1 + (*pPipelineSection)[L"DataFinal"], 2);

  try
  {
    _addGameApps(pRunToolbar, sAppFolder1);
    if(sAppFolder1 != sAppFolder2)
      _addGameApps(pRunToolbar, sAppFolder2);
  }
  CATCH_THROW_SIMPLE(delete pRunToolbar, L"Cannot populate the \'Run\' toolbar");

  if(pRunToolbar->GetToolsCount() == 0)
  {
    delete pRunToolbar;
  }
  else
  {
    pRunToolbar->Realize();
    m_oManager.AddPane(pRunToolbar, wxAuiPaneInfo().Name(wxT("tbRun")).Caption(wxT("Run")).Top().Fixed().Dock().Row(5).CloseButton(false).ToolbarPane());
  }
}

void frmLuaEditor::_addGameApps(wxToolBar *pToolbar, RainString sPath) throw(...)
{
  std::auto_ptr<IDirectory> pDirectory(RainOpenDirectory(sPath));
  for(IDirectory::iterator itr = pDirectory->begin(), end = pDirectory->end(); itr != end; ++itr)
  {
    if(itr->name().afterLast('.').compareCaseless(L"exe") == 0)
    {
      Win32PEFile oFile;
      RainString sFullPath = pDirectory->getPath() + itr->name();

      try
      {
        oFile.load(sFullPath);
      }
      catch(RainException *pE)
      {
#ifdef _DEBUG
        EXCEPTION_MESSAGE_BOX_1(L"Cannot load information on PE file \'%s\'", itr->name().getCharacters(), pE);
#else
        delete pE;
#endif
        continue;
      }

      bool bUsesSimEngine = false;
      for(size_t iLibIndex = 0; !bUsesSimEngine && iLibIndex < oFile.getImportedLibraryCount(); ++iLibIndex)
      {
        const RainString& sLibrary = oFile.getImportedLibrary(iLibIndex);
        if(sLibrary.compareCaseless(L"SimEngine.dll") == 0)
          bUsesSimEngine = true;
      }
      if(bUsesSimEngine)
      {
        wxBitmap bmpIcon;
        wxIcon icoIcon;
        SHFILEINFO oShellInfo;

        if(SHGetFileInfo(sFullPath.getCharacters(), 0, &oShellInfo, sizeof( SHFILEINFOW ), SHGFI_ICON | SHGFI_SHELLICONSIZE))
        {
          icoIcon.SetHICON(oShellInfo.hIcon);
          bmpIcon.CopyFromIcon(icoIcon);
        }
        else
        {
          bmpIcon = wxArtProvider::GetBitmap(wxART_EXECUTABLE_FILE, wxART_TOOLBAR);
          oShellInfo.hIcon = 0;
        }
        wxImage imgIcon(bmpIcon.ConvertToImage());
        if(imgIcon.GetWidth() < 48 && imgIcon.GetHeight() < 48)
        {
          imgIcon.Resize(wxSize(48, 48), wxPoint(24 - imgIcon.GetWidth() / 2, 24 - imgIcon.GetHeight() / 2));
        }
        else
        {
          imgIcon.Rescale(48, 48, wxIMAGE_QUALITY_HIGH);
        }
        if(!imgIcon.HasAlpha())
          imgIcon.InitAlpha();
        bmpIcon = wxBitmap(imgIcon);
        if(imgIcon.HasMask())
          bmpIcon.SetMask(new wxMask(bmpIcon, wxColour(imgIcon.GetMaskRed(), imgIcon.GetMaskGreen(), imgIcon.GetMaskBlue())));
        if(oShellInfo.hIcon)
          DestroyIcon(oShellInfo.hIcon);

        int iToolId = pToolbar->AddTool(wxID_ANY, L"", bmpIcon, wxString(L"Launch ") + (wxString)itr->name())->GetId();
        GameAppCommandInfo *pInfo = new GameAppCommandInfo(sFullPath, itr->name());
        this->Connect(iToolId, iToolId, wxEVT_COMMAND_TOOL_CLICKED , wxCommandEventHandler(frmLuaEditor::onGameAppClicked     ), pInfo->wrapInObject());
        this->Connect(iToolId, iToolId, wxEVT_COMMAND_TOOL_RCLICKED, wxCommandEventHandler(frmLuaEditor::onGameAppRightClicked), pInfo->wrapInObject());
      }
    }
  }
}

void frmLuaEditor::_initToolbar()
{
  wxToolBar *pSaveToolbar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_FLAT | wxTB_NODIVIDER);
  pSaveToolbar->SetToolBitmapSize(wxSize(48,48));

  try
  {
    pSaveToolbar->AddTool(TB_SAVELUA      , wxT("Save Lua")         , wxBitmap(_loadPng(wxT("IDP_SAVELUA")))   , wxT("Save the current file as Lua (editing format)"));
    pSaveToolbar->AddTool(TB_SAVELUA_BATCH, wxT("Save All Luas")    , wxBitmap(_loadPng(wxT("IDP_SAVELUAALL"))), wxT("Save all currently modified files as Lua (editing format)"));
    pSaveToolbar->AddTool(TB_SAVEBIN      , wxT("Save Binary")      , wxBitmap(_loadPng(wxT("IDP_SAVEBIN")))   , wxT("Save the current file as RGD (binary game format)"));
    pSaveToolbar->AddTool(TB_SAVEBIN_BATCH, wxT("Save All Binaries"), wxBitmap(_loadPng(wxT("IDP_SAVEBINALL"))), wxT("Save all Luas which are newer than their RGD equivalents as RGD (binary game format)"));
  }
  catch(RainException *pE)
  {
    EXCEPTION_MESSAGE_BOX(L"Cannot load toolbar icon(s)", pE);
  }
  pSaveToolbar->Realize();

  m_oManager.AddPane(pSaveToolbar, wxAuiPaneInfo().Name(wxT("tbSave")).Caption(wxT("Save")).Top().Fixed().Dock().Row(5).CloseButton(false).ToolbarPane());
}

void frmLuaEditor::onGameAppClicked(wxCommandEvent &e)
{
  GameAppCommandInfo *pCommandInfo = reinterpret_cast<GameAppCommandInfo*>(e.m_callbackUserData->GetRefData());

  wxString sExecutable = pCommandInfo->command();
  wxString sRunIn = sExecutable.BeforeLast('\\');
  wxString sCommandLine = pCommandInfo->parameters().realise();

#ifdef _WIN32
  ShellExecuteW(0, L"open", sExecutable.c_str(), sCommandLine.c_str(), sRunIn.c_str(), SW_SHOW);
#else
  wxExecute(sExecutable + L" " + sCommandLine);
#endif
}

void frmLuaEditor::onGameAppRightClicked(wxCommandEvent &e)
{
  std::map<int, CommandLineParameters::Parameters::value_type*> mapCheckItems;
  int iConfigureId, iLaunchId;
  GameAppCommandInfo *pCommandInfo = reinterpret_cast<GameAppCommandInfo*>(e.m_callbackUserData->GetRefData());
  CommandLineParameters& oParams = pCommandInfo->parameters();
  DynamicPopupMenu oMenu;

  iLaunchId = oMenu.append(L"Launch " + implicit_cast<wxString>(pCommandInfo->name()));
  oMenu.appendSeparator();
  for(CommandLineParameters::Parameters::iterator itrParam = oParams.getParameters().begin(), itrParamEnd = oParams.getParameters().end(); itrParam != itrParamEnd; ++itrParam)
  {
    mapCheckItems[oMenu.appendCheckItem(oParams.realiseParameter(itrParam->second), wxEmptyString, itrParam->first)] = &*itrParam;
  }
  oMenu.appendSeparator();
  iConfigureId = oMenu.append(L"Configure...");

  oMenu.popup(this);

  if(oMenu.getId() == wxID_ANY)
    return;
  else if(mapCheckItems.count(oMenu.getId()))
  {
    bool &b = mapCheckItems[oMenu.getId()]->first;
    pCommandInfo->iniSection()[ mapCheckItems[oMenu.getId()]->second ] = (b = !b) ? L"true" : L"false";
  }
  else if(oMenu.getId() == iConfigureId)
  {
    ::wxMessageBox(L"Not implemented yet", L"Configure", wxOK | wxICON_ERROR | wxCENTER, this);
  }
  else if(oMenu.getId() == iLaunchId)
  {
    onGameAppClicked(e);
  }
}

void frmLuaEditor::onStyleNeeded(wxStyledTextEvent &e)
{
	int iEndStyle = m_pLuaCode->GetEndStyled();
	m_pLuaCode->Colourise(iEndStyle, e.GetPosition());
}

void frmLuaEditor::onListItemActivate(wxCommandEvent &e)
{
  wxTreeItemId oItem = reinterpret_cast<RecentLuaListData*>(m_pRecentLuas->GetClientObject(e.GetSelection()))->oTreeItem;
  m_pInheritanceTree->EnsureVisible(oItem);
  m_pInheritanceTree->SelectItem(oItem);
  wxTreeEvent eNewEvent;
  eNewEvent.SetItem(oItem);
  onTreeItemActivate(eNewEvent);
}

static char wxChar_to_char(wxChar c)
{
  return c & ~0xFF ? '?' : c;
}

void frmLuaEditor::onTreeItemActivate(wxTreeEvent &e)
{
  if(m_oCurrentAttribTreeItem.IsOk())
  {
    // Save tree state
    InheritTreeItemData* pData = reinterpret_cast<InheritTreeItemData*>(m_pInheritanceTree->GetItemData(m_oCurrentAttribTreeItem));
    if(pData->pRememberedState == 0)
      pData->pRememberedState = CreateTreeCtrlMemory();
    pData->pRememberedState->store(m_pAttribTree);

    // Save code (to memory store)
    if(m_bAttributeFileChanged || m_pLuaCode->GetModify())
    {
      RainString sPath = m_pRootDirectory->getPath() + pData->sPath;
      IFile* pFile = 0;
      try
      {
        pFile = m_pDirectoryStore->openFile(sPath, FM_Write);
        BufferingOutputStream<char> oOutput(pFile);
        oOutput.writeConverting(m_pLuaCode->GetText().c_str(), m_pLuaCode->GetText().Len(), wxChar_to_char);
      }
      catch(RainException *pE)
      {
        delete pE;
        delete pFile;
      }
      delete pFile;
    }
  }

  wxTreeItemId oItem = e.GetItem();
  if(oItem.IsOk())
  {
    m_oCurrentAttribTreeItem = oItem;

    InheritTreeItemData* pData = reinterpret_cast<InheritTreeItemData*>(m_pInheritanceTree->GetItemData(oItem));
    if(pData)
    {
      RecentLuaListData* pItemData = new RecentLuaListData;
      pItemData->oTreeItem = oItem;
      m_pRecentLuas->Insert(m_pInheritanceTree->GetItemText(oItem) + L" (" + implicit_cast<wxString>(pData->sPath) + L")", 0, pItemData);
      for(unsigned int n = 1; n < m_pRecentLuas->GetCount(); ++n)
      {
        if(reinterpret_cast<RecentLuaListData*>(m_pRecentLuas->GetClientObject(n))->oTreeItem == oItem)
        {
          m_pRecentLuas->Delete(n);
          break;
        }
      }
      m_pRecentLuas->SetSelection(0);

      RainString sPath = m_pRootDirectory->getPath() + pData->sPath;
      IFile* pFile = 0;
      try
      {
        pFile = m_pDirectoryStore->openFile(sPath, FM_Read);
      }
      CATCH_MESSAGE_BOX(L"Cannot read Lua file", {return;});
      m_pAttribTree->Freeze();
      m_pLuaProperties->Freeze();
      try
      {
        _doLoadLua(pFile, pData->sPath);
      }
      CATCH_MESSAGE_BOX(L"Cannot load Lua file", {});
      m_pAttribTree->Thaw();
      m_pLuaProperties->Thaw();
      delete pFile;

      if(pData->pRememberedState)
        pData->pRememberedState->retrieve(m_pAttribTree);

      m_pInheritanceTree->SetItemBold(oItem, true);
      if(m_pRecentLuas->GetCount() >= 2)
      {
        m_pInheritanceTree->SetItemBold(reinterpret_cast<RecentLuaListData*>(m_pRecentLuas->GetClientObject(1))->oTreeItem, false);
      }

      if(m_pNotebook->GetSelection() == 0)
        m_pAttribTree->SetFocus();
      else
        m_pLuaCode->SetFocus();
    }
  }
}

wxPGProperty* frmLuaEditor::_getValueEditor(IAttributeValue* pValue, wxString sName, wxString sNameId)
{
  switch(pValue->getType())
  {
  case VT_Boolean:
    return new wxBoolProperty(sName, sNameId, pValue->getValueBoolean());

  case VT_String:
    return new wxStringProperty(sName, sNameId, pValue->getValueString());

  case VT_Float:
    return new wxFloatProperty(sName, sNameId, pValue->getValueFloat());

  case VT_Table: {
    wxPGProperty *pProperty = 0;
    IAttributeTable *pTable = pValue->getValueTable();
    unsigned long iRefKey = pTable->findChildIndex(RgdDictionary::_REF);
    if(iRefKey != IAttributeTable::NO_INDEX)
    {
      IAttributeValue *pReference = pTable->getChild(iRefKey);
      pProperty = _getValueEditor(pReference, sName, sNameId);
      delete pReference;
    }
    delete pTable;
    if(pProperty)
      return pProperty; }
    // fall through

  default:
    return new wxStringProperty(sName, sNameId, L"(unknown)");
  }
}

wxString frmLuaEditor::_getValueName(IAttributeValue* pValue)
{
  wxString sName;
  const RainString *pName = RgdDictionary::getSingleton()->hashToStringNoThrow(pValue->getName());
  if(pName)
    sName = *pName;
  else
    sName.Printf(L"0x%lX", pValue->getName());
  return sName;
}

void frmLuaEditor::onAttribTreeItemExpanding(wxTreeEvent &e)
{
  wxTreeItemId oNode = e.GetItem();
  if(m_pAttribTree->ItemHasChildren(oNode) && m_pAttribTree->GetChildrenCount(oNode, false) == 0)
  {
    LuaAttribTreeData *pData = reinterpret_cast<LuaAttribTreeData*>(m_pAttribTree->GetItemData(oNode));
    if(!pData || !pData->pTable || pData->pTable->getChildCount() == 0)
    {
      m_pAttribTree->SetItemHasChildren(oNode, false);
      return;
    }
    for(unsigned long iIndex = 0; iIndex < pData->pTable->getChildCount(); ++iIndex)
    {
      IAttributeValue *pChild = pData->pTable->getChild(iIndex);
      if(pChild->getName() == RgdDictionary::_REF)
      {
        delete pChild;
        continue;
      }
      IAttributeTable *pTable = 0;
      if(pChild && pChild->isTable())
        pTable = pChild->getValueTable();
      
      wxTreeItemId oChild = m_pAttribTree->AppendItem(oNode, _getValueName(pChild), static_cast<int>(pChild->getIcon()), -1, new LuaAttribTreeData(pChild, pTable));
      if(pTable && pTable->getChildCount() > 0)
      {
        if(pTable->getChildCount() != 1 || pTable->findChildIndex(RgdDictionary::_REF) == IAttributeTable::NO_INDEX)
          m_pAttribTree->SetItemHasChildren(oChild);
      }
    }
  }
}

void frmLuaEditor::_doLoadLua(IFile *pFile, RainString sPath)
{
  m_pLuaCode->ClearAll();
  m_pLuaProperties->Clear();
  m_pLuaCode->EmptyUndoBuffer();
  m_pAttribTree->DeleteAllItems();
  delete m_pAttributeFile;
  m_pAttributeFile = 0;

  pFile->seek(0, SR_Start);
  const int buffer_size = 1024;
  char sBuffer[buffer_size];
  size_t iBytes;
  do
  {
    iBytes = pFile->readArrayNoThrow(sBuffer, buffer_size);
    m_pLuaCode->AddText(wxString(sBuffer, wxConvUTF8, iBytes));
  } while(iBytes);

  m_pLuaCode->SetSavePoint();
  m_bAttributeFileChanged = false;

  pFile->seek(0, SR_Start);
  m_pAttributeFile = new LuaAttrib;
  m_pAttributeFile->setName(sPath);
  m_pAttributeFile->setBaseFolder(m_pRootDirectory);
  m_pAttributeFile->loadFromFile(pFile);
  wxTreeItemId oRoot = m_pAttribTree->AddRoot(L"GameData", (int)VI_Table, -1, new LuaAttribTreeData(m_pAttributeFile->getGameData(), 0));
  m_pAttribTree->SetItemHasChildren(oRoot, true);
  m_pAttribTree->Expand(oRoot);
  m_pAttribTree->SelectItem(oRoot);
}

void frmLuaEditor::onAttribTreeItemActivate(wxTreeEvent &e)
{
  wxTreeItemId oItem = e.GetItem();
  if(oItem.IsOk())
  {
    LuaAttribTreeData *pData = (LuaAttribTreeData*)m_pAttribTree->GetItemData(oItem);
    if(pData)
      _populatePropertyGrid(pData);
    else
      m_pLuaProperties->Clear();
  }
}

void frmLuaEditor::_populatePropertyGrid(LuaAttribTreeData *pData)
{
  m_pLuaProperties->Freeze();
  m_pLuaProperties->Clear();
  m_pLuaProperties->AddPage();
  try
  {
    m_pLuaProperties->Append(new wxPropertyCategory(L"Properties"));
    m_pLuaProperties->Append(new wxStringProperty(L"Name", L"Name", _getValueName(pData->pValue)));

    wxArrayString asDataTypes; wxArrayInt aiDataTypes;
	  asDataTypes.Add(wxT("Text"   )); aiDataTypes.Add(VT_String);
	  asDataTypes.Add(wxT("Number" )); aiDataTypes.Add(VT_Float);
	  asDataTypes.Add(wxT("Boolean")); aiDataTypes.Add(VT_Boolean);
	  asDataTypes.Add(wxT("Table"  )); aiDataTypes.Add(VT_Table);
	  asDataTypes.Add(wxT("Unknown")); aiDataTypes.Add(VT_Unknown);

    m_pLuaProperties->Append(new wxEnumProperty(L"Data Type", L"DataType", asDataTypes, aiDataTypes, pData->pValue->getType()));

    if(!pData->pTable)
    {
      m_pLuaProperties->Append(_getValueEditor(pData->pValue, L"Value", L"Value"));

      wxPGProperty *pSetBy = m_pLuaProperties->Append(new wxStringProperty(L"Set by", L"SetBy", pData->pValue->getFileSetIn().afterLast('\\')));
      m_pLuaProperties->EnableProperty(pSetBy, false);
      m_pLuaProperties->SetPropertyHelpString(pSetBy, pData->pValue->getFileSetIn());

      const IAttributeValue *pParent = pData->pValue->getParentNoThrow();
      if(pParent)
      {
        m_pLuaProperties->Append(new wxPropertyCategory(L"Parent Values"));
        while(pParent)
        {
          wxString sShortParentName = pParent->getFileSetIn().afterLast('\\');
          wxPGProperty *pProperty = m_pLuaProperties->Append(_getValueEditor((IAttributeValue*)pParent, sShortParentName, pParent->getFileSetIn()));
          m_pLuaProperties->EnableProperty(pProperty, false);
          m_pLuaProperties->SetPropertyHelpString(pProperty, pParent->getFileSetIn());
          const IAttributeValue *pNextParent = pParent->getParentNoThrow();
          delete pParent;
          pParent = pNextParent;
        }
      }
    }
    else
    {
      if(pData->pReference)
      {
        m_pLuaProperties->Append(_getValueEditor(pData->pReference, L"Reference", L"Value"));

        m_pLuaProperties->EnableProperty(
          m_pLuaProperties->Append(new wxStringProperty(L"Set by", L"SetBy", pData->pReference->getFileSetIn().getCharacters())),
          false);
      }

      m_pLuaProperties->Append(new wxPropertyCategory(L"Children"));
      for(unsigned long iChild = 0; iChild < pData->pTable->getChildCount(); ++iChild)
      {
        IAttributeValue *pValue = pData->pTable->getChild(iChild);
        if(pValue->getName() != RgdDictionary::_REF)
        {
          m_pLuaProperties->Append(_getValueEditor(pValue, _getValueName(pValue), L""));
        }
        delete pValue;
      }
    }
  }
  CATCH_MESSAGE_BOX(L"Cannot populate property grid", {});
  m_pLuaProperties->Thaw();
  m_pLuaProperties->Refresh();
}

void frmLuaEditor::setSource(IDirectory* pRootDirectory, IFileStore *pDirectoryStore) throw(...)
{
  CHECK_ASSERT(pRootDirectory != 0);
  CHECK_ASSERT(pDirectoryStore != 0);
  CHECK_ASSERT(m_pRootDirectory == 0);

  m_oManager.Update();
#ifndef NO_REMEMBER_AUI
  wxString sPerspective = LuaEditApp::Config[L"editor"][L"aui_perspective"].value();
  if(!sPerspective.IsEmpty())
    m_oManager.LoadPerspective(sPerspective, true);
#endif

  FileStoreComposition *pComposition = CHECK_ALLOCATION(new (std::nothrow) FileStoreComposition);
  pComposition->addFileStore(new ReadOnlyFileStoreAdaptor(pDirectoryStore), L"", L"", 10, true);
  pComposition->addFileStore(new MemoryFileStore, L"", L"", 20, true);

  m_pDirectoryStore = pComposition;
  m_pRootDirectory = pComposition->openDirectory(pRootDirectory->getPath());
  delete pRootDirectory;

  try
  {
    _populateInheritanceTree();
  }
  CATCH_THROW_SIMPLE({}, L"Cannot build inheritance tree");
}

void frmLuaEditor::_populateInheritanceTree()
{
  wxBitmap bmpIcons(wxT("IDB_TREEICONS"));
  wxImageList* pImageList = new wxImageList(14, 14);
  for(int i = 0; i < bmpIcons.GetWidth(); i += 14)
    pImageList->Add(bmpIcons.GetSubBitmap(wxRect(i, 0, 14, 14)), *wxWHITE);
  m_pInheritanceTree->AssignImageList(pImageList);

  {
    InheritanceBuilder oBuild;
    oBuild.setSource(m_pRootDirectory);
    oBuild.setTreeCtrl(m_pInheritanceTree);
    oBuild.doBuild();
  }

  wxTreeItemIdValue oCookie;
  wxTreeItemId oFirstItem = m_pInheritanceTree->GetFirstChild(m_pInheritanceTree->GetRootItem(), oCookie);
  if(oFirstItem.IsOk())
  {
    m_pInheritanceTree->SelectItem(oFirstItem, true);
    wxTreeEvent e;
    e.SetItem(oFirstItem);
    onTreeItemActivate(e);
  }
}

frmLuaEditor::~frmLuaEditor()
{
  delete m_pRootDirectory;
  delete m_pDirectoryStore;
  delete m_pAttributeFile;
  RainString sPerspective = m_oManager.SavePerspective();
  if(sPerspective.indexOf('\n') == RainString::NOT_FOUND && sPerspective.indexOf('#') == RainString::NOT_FOUND)
    LuaEditApp::Config[L"editor"][L"aui_perspective"] = sPerspective;
  else
    ::wxMessageBox(L"Cannot save AUI perspective, please inform programmer");
  m_oManager.UnInit();
}
