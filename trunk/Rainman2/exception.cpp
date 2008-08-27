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
#include "exception.h"

RainException::RainException(const RainString sFile, unsigned long iLine, const RainString sMessage, RainException* pPrevious)
  : m_sFile(sFile), m_sMessage(sMessage), m_iLine(iLine), m_pPrevious(pPrevious)
{
}

RainException::~RainException()
{
  delete m_pPrevious;
}

RainException::RainException(const RainString sFile, unsigned long iLine, RainException* pPrevious, const RainString sFormat, ...)
  : m_sFile(sFile), m_iLine(iLine), m_pPrevious(pPrevious)
{
  va_list vArgs;
  va_start(vArgs, sFormat);
  m_sMessage.printfV(sFormat, vArgs);
  va_end(vArgs);
}

void RainException::checkAssert(const RainChar* sFile, unsigned long iLine, bool bAssertion, const RainChar* sCheck) throw(...)
{
  if(!bAssertion)
  {
    RainString sFile(sFile);
    RainString sMessage(L"Assertion failed: ");
    sMessage += sCheck;
    throw new RainException(RainString(sFile), iLine, sMessage);
  }
}
