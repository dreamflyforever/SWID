#ifndef LINUX_H
#define LINUX_H

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "base.h"
#ifdef LINUX
#include <unistd.h> 
#endif

#include "base.h"

#define size_t unsigned int

cchar* itoa(int value, cchar* result, int base);
inline cchar *_itoa_s(int _Value, cchar * _DstBuf, int _Size, int _Radix);
inline cchar *___gcvt(double _val, int _numOfDigits, cchar *_result);
inline cchar *_gcvt_s(cchar *_buf, int _bufSize, double _Value, int _NumOfDigits);
void Sleep(int _ms);

#define __max(a,b)  (((a) > (b)) ? (a) : (b))
#define __min(a,b)  (((a) < (b)) ? (a) : (b))
#define _tcscmp strcmp
#define _stricmp strcasecmp
#define scanf_s(_sformat, _DstBuf, _SizeInBytes, ...) scanf((_sformat), (_DstBuf), __VA_ARGS__)
#define fopen_s fopen
#define sprintf_s(_DstBuf, _SizeInBytes, ...) sprintf((_DstBuf), __VA_ARGS__)
#define strcpy_s(_dst, _size, _src) strcpy((_dst), (_src))

#endif

