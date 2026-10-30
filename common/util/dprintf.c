#ifndef NDEBUG

#include <windows.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/dprintf.h"

static const char dbg_buf_trunc[] = " [truncated]\n";
static long dbg_buf_lock_init;
static CRITICAL_SECTION dbg_buf_lock;
static char dbg_buf[16384];
static size_t dbg_buf_pos;

void dprintf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    dprintfv(fmt, ap);
    va_end(ap);
}

void dprintfv(const char *fmt, va_list ap)
{
    long init;
    int ret;

    /* Static constructors in C are difficult to do in a way that works under
       both GCC and MSVC, so we have to use atomic ops to ensure that the
       buffer mutex is correctly initialized instead. */

    do {
        init = InterlockedCompareExchange(&dbg_buf_lock_init, 0, 1);

        if (init == 0) {
            /* We won the init race, global variable is now set to 1, other
               threads will spin until it becomes -1. */
            InitializeCriticalSection(&dbg_buf_lock);
            dbg_buf_lock_init = -1;
            init = -1;
        }
    } while (init >= 0);

    EnterCriticalSection(&dbg_buf_lock);

    /* vsnprintf_s returns -1 on truncation and when %ls text cannot be
       converted in the C locale, and msvcrt may report a length that it did
       not write. So never add the return value to the position; measure what
       actually ended up in the buffer instead. */

    ret = vsnprintf_s(
            dbg_buf + dbg_buf_pos,
            sizeof(dbg_buf) - dbg_buf_pos,
            sizeof(dbg_buf) - dbg_buf_pos - 1,
            fmt,
            ap);

    dbg_buf[sizeof(dbg_buf) - 1] = '\0';
    dbg_buf_pos += strlen(dbg_buf + dbg_buf_pos);

    if (dbg_buf_pos + 1 >= sizeof(dbg_buf) &&
            (ret < 0 || dbg_buf[dbg_buf_pos - 1] != '\n')) {
        /* Out of space: flush what fits and mark it as cut off, so that the
           position always leaves room for the next call. */
        memcpy(
                dbg_buf + sizeof(dbg_buf) - sizeof(dbg_buf_trunc),
                dbg_buf_trunc,
                sizeof(dbg_buf_trunc));
        OutputDebugStringA(dbg_buf);
        dbg_buf_pos = 0;
        dbg_buf[0] = '\0';
    } else if (strchr(dbg_buf, '\n') != NULL) {
        OutputDebugStringA(dbg_buf);
        dbg_buf_pos = 0;
        dbg_buf[0] = '\0';
    }

    LeaveCriticalSection(&dbg_buf_lock);
}

void dwprintf(const wchar_t *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    dwprintfv(fmt, ap);
    va_end(ap);
}

void dwprintfv(const wchar_t *fmt, va_list ap)
{
    wchar_t msg[512];

    _vsnwprintf_s(msg, _countof(msg), _countof(msg) - 1, fmt, ap);
    OutputDebugStringW(msg);
}

#endif
