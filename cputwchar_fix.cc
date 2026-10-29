// Workaround for NetBSD curses bug: __cputwchar calls putwc() on a
// byte-oriented FILE*, which truncates wchar_t > 0xFF to its lower byte.
// This wrapper converts wchar_t to UTF-8 bytes and outputs via __cputchar.
// Define this function in the application to interpose the curses version.

#include <wchar.h>

// Forward declaration of __cputchar from curses
extern "C" void __cputchar(int);

extern "C" void __cputwchar(wchar_t wc) {
    if (wc < 0x80) {
        __cputchar((int)wc);
        return;
    }

    unsigned char buf[4];
    int len;

    if (wc < 0x800) {
        buf[0] = 0xC0 | (wc >> 6);
        buf[1] = 0x80 | (wc & 0x3F);
        len = 2;
    } else if (wc < 0x10000) {
        buf[0] = 0xE0 | (wc >> 12);
        buf[1] = 0x80 | ((wc >> 6) & 0x3F);
        buf[2] = 0x80 | (wc & 0x3F);
        len = 3;
    } else {
        buf[0] = 0xF0 | (wc >> 18);
        buf[1] = 0x80 | ((wc >> 12) & 0x3F);
        buf[2] = 0x80 | ((wc >> 6) & 0x3F);
        buf[3] = 0x80 | (wc & 0x3F);
        len = 4;
    }

    for (int i = 0; i < len; i++)
        __cputchar((int)buf[i]);
}
