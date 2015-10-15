#ifndef RE2JIT_DEBUG_H
#define RE2JIT_DEBUG_H
#define RE2JIT_DEBUG_BITS 4


namespace re2jit
{
    struct debug
    {
        #ifdef RE2JIT_DEBUG
            debug();
            static void write(const char *, ...);
            static void clear();
            static const char *iterate(const char *);
        #else
            /* compile with -O1 or higher to completely eliminate overhead */
            static inline void write(const char *, ...) {};
            static inline void clear() {};
            static inline const char *iterate(const char *) { return NULL; };
        #endif

        protected:
            char buffer[1 << RE2JIT_DEBUG_BITS][2048];
            unsigned head : RE2JIT_DEBUG_BITS;
            unsigned tail : RE2JIT_DEBUG_BITS;
            unsigned empty : 1;
    };
};


#endif
