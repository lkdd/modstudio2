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
#include "frmCommandLineOptions.h"
#include "stdext.h"
#include "utility.h"
#include "application.h"

void CommandLineParameters::addParameter(RainString sParameter, bool bEnabled)
{
  m_vParameters.push_back(Parameters::value_type(bEnabled, sParameter));
}

void CommandLineParameters::addPlaceholder(RainString sPlaceholder, RainString sValue)
{
  m_vPlaceholders[sPlaceholder] = sValue;
}

CommandLineParameters::Parameters& CommandLineParameters::getParameters()
{
  return m_vParameters;
}

CommandLineParameters::Placeholders& CommandLineParameters::getPlaceholders()
{
  return m_vPlaceholders;
}

RainString CommandLineParameters::realiseParameter(RainString sParameter)
{
  for(Placeholders::iterator itr = m_vPlaceholders.begin(), itrEnd = m_vPlaceholders.end(); itr != itrEnd; ++itr)
  {
    sParameter.replaceAll(itr->first, itr->second);
  }
  return sParameter;
}

RainString CommandLineParameters::realise()
{
  RainString sCommandLine;

  for(Parameters::iterator itr = m_vParameters.begin(), itrEnd = m_vParameters.end(); itr != itrEnd; ++itr)
  {
    if(itr->first)
      sCommandLine = sCommandLine + L" " + realiseParameter(itr->second);
  }

  return sCommandLine;
}
