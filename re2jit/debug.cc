#include <stdio.h>
#include <stdarg.h>

#ifndef RE2JIT_DEBUG
/* Force the header file to use extern definitions.
   Not defining this in another compilation unit will make it use empty
   inline definitions instead of the ones in this file, effectively turning off
   debugging for that unit. */
#define RE2JIT_DEBUG
#endif

#include "debug.h"


static thread_local re2jit::debug _stream;


namespace re2jit
{
    debug::debug(void) : head(0), tail(0), empty(1)
    {
    }


    void debug::write(const char *fmt, ...)
    {
        va_list vl;
        va_start(vl, fmt);
        vsnprintf(_stream.buffer[_stream.tail], sizeof(*buffer) - 1, fmt, vl);
        va_end(vl);

        if (_stream.tail++ == _stream.head && !_stream.empty) {
            _stream.head++;
        }

        _stream.empty = 0;
    }


    void debug::clear(void)
    {
        _stream.head = _stream.tail = 0;
        _stream.empty = 1;
    }


    const char *debug::iterate(const char *previous)
    {
        if (_stream.empty) {
            return NULL;
        }

        if (previous == NULL) {
            return &_stream.buffer[_stream.head][0];
        }

        unsigned i = (previous - &_stream.buffer[0][0]) / sizeof(*buffer);
        i = (i + 1) & ((1 << RE2JIT_DEBUG_BITS) - 1);

        return i == _stream.tail ? NULL : &_stream.buffer[i][0];
    }
};
