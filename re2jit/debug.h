#ifndef RE2JIT_DEBUG_H
#define RE2JIT_DEBUG_H
#define RE2JIT_DEBUG_BITS 4

namespace re2jit {
    struct Debug {
        #if RE2JIT_DEBUG
            Debug();
            static void Write(const char *, ...);
            static void Clear();
            static const char *Iterate(const char *);
        #else
            static inline void Write(const char *, ...) {};
            static inline void Clear() {};
            static inline const char *Iterate(const char *) { return NULL; };
        #endif

        protected:
            char buffer[1 << RE2JIT_DEBUG_BITS][2048];
            unsigned head : RE2JIT_DEBUG_BITS;
            unsigned tail : RE2JIT_DEBUG_BITS;
            unsigned empty : 1;
    };
};

#endif
