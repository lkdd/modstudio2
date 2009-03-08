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
#define RAINMAN2_NO_WX
#define RAINMAN2_NO_LUA
#include <Rainman2.h>
#include <stack>

class TxtReader
{
public:
  TxtReader(IFile *pInputFile, RbfWriter *pWriter);
  ~TxtReader();

  static bool isKeyChar(char c);

  void read();
  void readKey(int iLength);
  void readNumber();
  void readString();

  void pushValueBoolean(bool bValue);
  void pushTable();

  void endTable();
  void endStatement();

  void inputError(const wchar_t *sReason);

  bool isChar(int n);
  bool areChars(const char *s);
  char getChar(int n);
  void consume(int n);

protected:
  IFile     *m_pFile;
  RbfWriter *m_pWriter;
  char      *m_sBuffer,
            *m_sBufferBase;
  int        m_iBufferLength,
             m_iBufferRemaining,
             m_iBytesOfDataInBuffer;
  int        m_iLineNumber;
  std::stack<int> m_stkTableLineNumbers;
  enum
  {
    STATE_WANT_SEPERATOR,
    STATE_WANT_VALUE,
    STATE_WANT_KEY,
    STATE_WANT_END_STATEMENT,
    STATE_WANT_END_FILE,
  } m_eState;
};