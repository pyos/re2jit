#ifndef RE2JIT_LIST_H
#define RE2JIT_LIST_H

#ifdef __cplusplus
extern "C" {
#endif
    /* Generic circular doubly-linked list.
     *
     * A `struct T` that contains a `RE2JIT_LIST_LINK(struct T)` can form a doubly linked list
     * with other objects of same type. The `RE2JIT_LIST_LINK` is not required to have a name;
     * if it does not, `struct T` is extended with members `next` and `prev`.
     *
     * Optionally, another `struct R` may start with `RE2JIT_LIST_ROOT(struct T)`.
     * The `struct R` is then also a part of the cycle; it's basically the beginning,
     * and also the end, of the list. If `struct R` is the root, `first` and `last`
     * point into the list proper; compare them to `rejit_list_end(root)` to determine
     * whether they point to valid `struct T`s or you have gone full circle to the root.
     *
     * NOTE: `first`/`last`/`next`/`prev` point to `RE2JIT_LIST_LINK`/`RE2JIT_LIST_ROOT` of
     *       another element, not to the beginning. Either put `RE2JIT_LIST_LINK` as
     *       the first member of the struct, or do some pointer arithmetic manually.
     *
     * Stolen from libcno. https://github.com/pyos/libcno/blob/master/cno/common.h#L104
     * (HTTP still sucks.)
     *
     */
    typedef struct rejit_st_list_link_t { struct rejit_st_list_link_t *prev, *next; } rejit_list_link_t;


    #define RE2JIT_LIST_LINK(T) union { struct { T *prev, *next;  }; rejit_list_link_t __list_handle[1]; }
    #define RE2JIT_LIST_ROOT(T) union { struct { T *last, *first; }; rejit_list_link_t __list_handle[1]; }


    #define rejit_list_end(x)  (void *) (x)->__list_handle
    #define rejit_list_init(x)       __rejit_list_init((x)->__list_handle)
    #define rejit_list_append(x, y)  __rejit_list_append((x)->__list_handle, (y)->__list_handle)
    #define rejit_list_remove(x)     __rejit_list_remove((x)->__list_handle)


    static inline void __rejit_list_init(rejit_list_link_t *node)
    {
        node->next = node;
        node->prev = node;
    }


    static inline void __rejit_list_append(rejit_list_link_t *node, rejit_list_link_t *next)
    {
        next->prev = node;
        next->next = node->next;
        node->next = next;
        next->next->prev = next;
    }


    static inline void __rejit_list_remove(rejit_list_link_t *node)
    {
        node->next->prev = node->prev;
        node->prev->next = node->next;
    }

#ifdef __cplusplus
};
#endif

#endif
