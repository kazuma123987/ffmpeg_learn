#ifndef __UTIL__H
#define __UTIL__H
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#ifdef WIN32
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif
// utf8或gbk(ansi)转unicode
void char_to_wchar(char *src, wchar_t *dst, size_t bufferlen, unsigned int codePage);
// unicode转utf8或gbk(ansi)
void wchar_to_char(wchar_t *src, char *dst, size_t bufferlen,unsigned int codePage);
#ifdef __cplusplus
}
#endif
#endif