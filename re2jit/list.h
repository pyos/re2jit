#ifndef RE2JIT_LIST_H
#define RE2JIT_LIST_H

#ifdef __cplusplus
extern "C" {
#endif
    /* Generic circular doubly-linked list.
     *
     * A `struct T` that starts with a `RE2JIT_LIST_LINK(struct T);` can form
     * a doubly linked list with other objects of same type.
     *
     * Optionally, a different `struct R` may start with `RE2JIT_LIST_ROOT(struct T)`.
     * The `struct R` is then also a part of the cycle; it's the beginning and the end
     * of the list. If `struct R` is the root, its `first` and `last` fields point into
     * the list proper; compare each element to `rejit_list_end(root)` to check whether
     * you've reached the end.
     *
     */
    struct rejit_list_link_t { struct rejit_list_link_t *prev, *next; };
    #define RE2JIT_LIST_LINK(T) union { struct { T *prev, *next;  }; struct rejit_list_link_t __list_handle[1]; }
    #define RE2JIT_LIST_ROOT(T) union { struct { T *last, *first; }; struct rejit_list_link_t __list_handle[1]; }

    #define rejit_list_end(x)        (x)->first->prev
    #define rejit_list_init(x)       __rejit_list_init((x)->__list_handle)
    #define rejit_list_append(x, y)  __rejit_list_append((x)->__list_handle, (y)->__list_handle)
    #define rejit_list_remove(x)     __rejit_list_remove((x)->__list_handle)
    #define rejit_list_container(t, f, x) (t *) ((t *) ((char *)(x) - offsetof(t, f)))


    static inline void __rejit_list_init(struct rejit_list_link_t *node)
    {
        node->next = node;
        node->prev = node;
    }


    static inline void __rejit_list_append(struct rejit_list_link_t *node, struct rejit_list_link_t *next)
    {
        next->prev = node;
        next->next = node->next;
        node->next->prev = next;
        node->next = next;
    }


    static inline void __rejit_list_remove(struct rejit_list_link_t *node)
    {
        node->next->prev = node->prev;
        node->prev->next = node->next;
    }

#ifdef __cplusplus
}
#endif

#endif
