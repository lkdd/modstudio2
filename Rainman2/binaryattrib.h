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
#include "attributes.h"

struct BinaryAttribTable;

template <class T> struct BinaryAttribDataTypeCode {};
template <> struct BinaryAttribDataTypeCode<float> {static const long code = 0;};
template <> struct BinaryAttribDataTypeCode<long> {static const long code = 1;};
template <> struct BinaryAttribDataTypeCode<bool> {static const long code = 2;};
template <> struct BinaryAttribDataTypeCode<char*> {static const long code = 3;};
template <> struct BinaryAttribDataTypeCode<wchar_t*> {static const long code = 4;};
template <> struct BinaryAttribDataTypeCode<BinaryAttribTable> {static const long code = 100;};

template <bool b> struct BinaryAttribBooleanEncoding {};
template <> struct BinaryAttribBooleanEncoding<true> {static const unsigned char value = 1;};
template <> struct BinaryAttribBooleanEncoding<false> {static const unsigned char value = 0;};