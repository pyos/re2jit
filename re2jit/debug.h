#ifndef RE2JIT_DEBUG_H
#define RE2JIT_DEBUG_H
#define RE2JIT_DEBUG_BITS 4

#include <iostream>
#include <sstream>


namespace re2jit {
    struct DebugStream {
        #if RE2JIT_DEBUG
            #define DEBUG(...) re2jit::DebugStream::Write(__VA_ARGS__)

            char buffer[1 << RE2JIT_DEBUG_BITS][2048];
            unsigned head : RE2JIT_DEBUG_BITS;
            unsigned tail : RE2JIT_DEBUG_BITS;
            unsigned empty : 1;

            DebugStream();
            static void Write(const char *fmt, ...);
            static void Clear();
            static const char *Iterate(const char *previous);
        #else
            #define DEBUG(...)

            DebugStream() = delete;
            static inline void Write(const char *fmt, ...) {};
            static inline void Clear() {};
            static inline const char *Iterate(const char *previous) { return NULL; };
        #endif
    };
};

#endif
