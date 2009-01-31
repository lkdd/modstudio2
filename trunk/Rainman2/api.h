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
   Definitions of macros for the Rainman2 API.
   If RAINMAN2_STATIC is set, then RAINMAN2_API is gets replaced with nothing.
   Otherwise, it becomes the appropriate command for the compiler to import or
   export symbols from a DLL. Set RAINMAN2_EXPORTS when building as a DLL, but
   not when using Rainman2 as a DLL.
*/
#pragma once

#ifndef _BIND_TO_CURRENT_VCLIBS_VERSION
#define _BIND_TO_CURRENT_VCLIBS_VERSION 1
#endif

#ifdef RAINMAN2_STATIC
#define RAINMAN2_API
#else
#pragma warning (disable:4251)
#ifdef RAINMAN2_EXPORTS
#define RAINMAN2_API __declspec(dllexport)
#else
#define RAINMAN2_API __declspec(dllimport)
#endif
#endif

#ifndef RAINMAN2_NO_WX
#define RAINMAN2_USE_WX
#endif

#ifndef RAINMAN2_NO_LUA
#define RAINMAN2_USE_LUA
#endif

#ifndef RAINMAN2_NO_CRYPTO_WIN32
#define RAINMAN2_USE_CRYPTO_WIN32
#endif

#ifndef RAINMAN2_NO_RBF
#define RAINMAN2_USE_RBF
#endif
