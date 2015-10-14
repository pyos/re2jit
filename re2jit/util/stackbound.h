#ifndef RE2JIT_UTIL_STACKBOUND_H
#define RE2JIT_UTIL_STACKBOUND_H


/* Automatic memory management for heap-allocated C objects.
 *
 * It's customary in C to have constructors that allocate objects on the heap and
 * destructors that do the reverse. It's customary to use RIAA in C++. This is a bridge
 * from the former to the latter:
 *
 *     auto *heap_object = clib_create_whatever();
 *     auto _heap_object_deallocator = util::stackbound<clib_object_t>
 *                         { heap_object, clib_destroy_whatever };
 *
 * Now `heap_object` will be deallocated automatically on return! Yay!
 *
 */


namespace re2jit
{
    namespace util
    {
        template <typename objt> struct stackbound
        {
            objt  *object;
            void (*deallocator)(objt *);

           ~stackbound()
            {
                deallocator(object);
            }
        };
    };
};


#endif
