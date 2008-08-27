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
/*
   Redefines the C++ `new` operator to add file names and line numbers to
   all memory allocations. This is useful when tracking down a memory
   leak, as the leak report will now list the location of the allocation.
   Also updates the C `strdup` function to use the tracing new, so that
   allocations done by it are also easier to track down.

   Simply include this file in any .cpp file and compile/run in debug
   mode to activate tracing new (the standard new operator is always used
   in release mode). A note of caution though; if the .cpp file uses
   placement new, or nothrow new, then it should use the PLACEMENT_NEW
   and NOTHROW macros provided in order to not conflict with the tracing.
*/
#pragma once

#ifdef _DEBUG
	#include <crtdbg.h>
	#include <memory.h>
	#include <string.h>
	#define DEBUG_NEW new(_NORMAL_BLOCK ,__FILE__, __LINE__)
	#define strdup(s) strdup_memdebug(s, __FILE__, __LINE__)
	static char* strdup_memdebug(const char* s, const char* sFile, unsigned long iLine)
	{
		size_t iL = strlen(s);
		char* sR = new(_NORMAL_BLOCK, sFile, iLine) char[iL + 1];
		memcpy((void*)sR, (const void*)s, iL);
		sR[iL] = 0;
		return sR;
	}
#else
	#define DEBUG_NEW new
#endif

#ifndef NEW_TRACE_DEFINE_ONLY
#define PLACEMENT_NEW(place) new (place)
#ifdef _DEBUG
	#define new DEBUG_NEW
  #define NOTHROW
#else // (not) _DEBUG
  #define NOTHROW (std::nothrow)
#endif // _DEBUG
#else // (not) NEW_TRACE_DEFINE_ONLY
  #define NOTHROW (std::nothrow)
#endif // NEW_TRACE_DEFINE_ONLY
