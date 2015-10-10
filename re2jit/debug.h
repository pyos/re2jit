#ifndef RE2JIT_DEBUG_H
#define RE2JIT_DEBUG_H

#include <iostream>
#include <sstream>


namespace re2jit {
    class DebugStream {

    public:
        char **ringbuf;
        char **ringbuf_end;
        char **head;
        char **tail;
        bool empty;

        DebugStream();
       ~DebugStream();

        static void Write(const char *fmt, ...);
        static void Clear();
        static const char **Iterate(const char **previous);
    };

    #if RE2JIT_DEBUG
        #define DEBUG(...) re2jit::DebugStream::Write(__VA_ARGS__)
    #else
        #define DEBUG(...)
    #endif
};

#endif
