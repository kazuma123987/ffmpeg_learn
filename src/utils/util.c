#include "util.h"
// utf8或gbk(ansi)转unicode
void char_to_wchar(char *src, wchar_t *dst, size_t bufferlen, unsigned int codePage)
{
#ifdef WIN32
    size_t dstlen = MultiByteToWideChar(codePage, 0, src, -1, NULL, 0);
    if (dstlen >= bufferlen)
    {
        printf("buffer size too small\n");
        return;
    }
    MultiByteToWideChar(codePage, 0, src, -1, dst, dstlen);
    dst[dstlen] = L'\0';
#else
    size_t dstlen = mbstowcs(NULL, src, 0);
    if (dstlen >= bufferlen)
    {
        printf("buffer size too small\n");
        return;
    }
    mbstowcs(dst,src,dstlen);
    dst[dstlen]=L'\0';
#endif
}

// unicode转utf8或gbk(ansi)
void wchar_to_char(wchar_t *src, char *dst, size_t bufferlen,unsigned int codePage)
{
#ifdef WIN32
    size_t dstlen = WideCharToMultiByte(codePage, 0, src, -1, NULL, 0,NULL,NULL);
    if (dstlen >= bufferlen)
    {
        printf("buffer size too small\n");
        return;
    }
    WideCharToMultiByte(codePage, 0, src, -1, dst, dstlen,NULL,NULL);
    dst[dstlen] = '\0';
#else
    size_t dstlen = wcstombs(NULL, src, 0);
    if (dstlen >= bufferlen)
    {
        printf("buffer size too small\n");
        return;
    }
    wcstombs(dst,src,dstlen);
    dst[dstlen]='\0';
#endif
}