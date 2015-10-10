#include <re2jit/debug.h>

#include <stdio.h>
#include <stdarg.h>

#define RINGBUF_SIZE 16
#define MESSAGE_SIZE 2048

using namespace re2jit;


static thread_local DebugStream _stream;


DebugStream::DebugStream()
{
    #if RE2JIT_DEBUG
        ringbuf = new char* [RINGBUF_SIZE];
        ringbuf_end = ringbuf + RINGBUF_SIZE;

        for (size_t i = 0; i <= RINGBUF_SIZE; i++) {
            ringbuf[i] = new char [MESSAGE_SIZE + 1]();
        }

        head = tail = ringbuf;
        empty = true;
    #endif
}


DebugStream::~DebugStream()
{
    #if RE2JIT_DEBUG
        for (size_t i = 0; i < RINGBUF_SIZE; i++) {
            delete[] ringbuf[i];
        }

        delete[] ringbuf;
    #endif
}


void
DebugStream::Write(const char *fmt, ...)
{
    #if RE2JIT_DEBUG
        va_list vl;
        va_start(vl, fmt);
        vsnprintf(*_stream.tail, MESSAGE_SIZE, fmt, vl);
        va_end(vl);

        if (!_stream.empty && _stream.tail == _stream.head) {
            if (++_stream.head == _stream.ringbuf_end) {
                _stream.head = _stream.ringbuf;
            }
        }

        if (++_stream.tail == _stream.ringbuf_end) {
            _stream.tail = _stream.ringbuf;
        }

        _stream.empty = false;
    #endif
}


void
DebugStream::Clear()
{
    #if RE2JIT_DEBUG
        _stream.head = _stream.tail = _stream.ringbuf;
        _stream.empty = true;
    #endif
}


const char **
DebugStream::Iterate(const char **previous)
{
    #if RE2JIT_DEBUG
        if (_stream.empty) {
            return NULL;  // empty
        }

        if (!previous) {
            return (const char **) _stream.head;
        }

        if (++previous == _stream.ringbuf_end) {
            previous = (const char **) _stream.ringbuf;
        }

        return previous == _stream.tail ? NULL : previous;
    #endif

    return NULL;
}

