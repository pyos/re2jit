#if RE2JIT_DEBUG
    #include <re2jit/debug.h>

    #include <stdio.h>
    #include <stdarg.h>

    using namespace re2jit;


    static thread_local DebugStream _stream;


    DebugStream::DebugStream() : head(0), tail(0), empty(1) {}


    void
    DebugStream::Write(const char *fmt, ...)
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


    void
    DebugStream::Clear()
    {
        _stream.head = _stream.tail = 0;
        _stream.empty = 1;
    }


    const char *
    DebugStream::Iterate(const char *previous)
    {
        if (_stream.empty) {
            return NULL;
        }

        if (previous == NULL) {
            return &_stream.buffer[_stream.head][0];
        }

        struct { unsigned int i : RE2JIT_DEBUG_BITS; } b;
        b.i = (previous - &_stream.buffer[0][0]) / sizeof(*buffer);
        b.i++;

        return b.i == _stream.tail ? NULL : &_stream.buffer[b.i][0];
    }
#endif
