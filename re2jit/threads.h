#ifndef RE2JIT_THREADS_H
#define RE2JIT_THREADS_H

#ifdef __cplusplus
extern "C" {
#endif
    #include <stddef.h>
    #include <stdlib.h>
    #include <stdint.h>

    #include "list.h"


    enum RE2JIT_THREAD_FLAGS {
        RE2JIT_ANCHOR_START = 0x1,  // all matches must start at 0
        RE2JIT_ANCHOR_END   = 0x2,  // all matches must end at EOF
        RE2JIT_UNDEFINED    = 0x4,  // set when regex can't match because of an exception
                                    // (e.g. ran out of memory while splitting)
    };


    enum RE2JIT_EMPTY_FLAGS {
        // this enum must be the same as its re2 counterpart.
        RE2JIT_EMPTY_BEGIN_LINE        = 0x01,  // (?m)^
        RE2JIT_EMPTY_END_LINE          = 0x02,  // (?m)$
        RE2JIT_EMPTY_BEGIN_TEXT        = 0x04,  // \A or ^
        RE2JIT_EMPTY_END_TEXT          = 0x08,  // \z or $
        RE2JIT_EMPTY_WORD_BOUNDARY     = 0x10,  // \b
        RE2JIT_EMPTY_NON_WORD_BOUNDARY = 0x20,  // \B
    };


    #if RE2JIT_ENABLE_SUBROUTINES
    struct rejit_subcall_t
    {
        struct rejit_subcall_t *next;
        const void *state;
        unsigned group;
        unsigned refcnt;
        unsigned groups[];
    };
    #endif


    struct rejit_threadq_t
    {
        RE2JIT_LIST_LINK(struct rejit_threadq_t);
        // if non-zero, decrement and move to the next queue; don't run.
        unsigned wait;
        // threads with different bitmap ids may have matched some backreferenced groups
        // at different locations and should never be considered equal.
        unsigned bitmap;
    };


    struct rejit_thread_t
    {
        RE2JIT_LIST_LINK(struct rejit_thread_t);
        struct rejit_threadq_t queue;
        // actual meaning of state determined by closure computation algorithm used.
        const void *state;
        #if RE2JIT_ENABLE_SUBROUTINES
        struct rejit_subcall_t *substack;
        #endif
        // group `i` spans the input from `groups[2i]`-th character to `groups[2i+1]`-th.
        // if either bound is -1, the group did not match. 0-th group is the whole match.
        unsigned groups[];
    };


    struct rejit_threadset_t
    {
  /*0*/ const char *input;
        unsigned offset;
        unsigned length;
        // actual length of `rejit_thread_t.groups`. must be at least 2 to store
        // the location of the whole match, + 2 per capturing group, if needed.
        unsigned groups;
        // the one to run on the next input byte.
        unsigned char queue;
        unsigned char flags;  // enum RE2JIT_THREAD_FLAGS
        // size of `bitmap`. 64 KB is enough for everyone.
        unsigned short space;
        // mark visited states here. resets automatically after advancing the input ptr
        // or when encountering a thread with a different bitmap id.
        uint8_t *bitmap;
        // linked list of unused thread objects.
        struct rejit_thread_t *free;
        // the thread for which `entry` is currently computing an epsilon closure.
        struct rejit_thread_t *running;
        // function to call to compute the epsilon closure of a single state.
        // must call `rejit_thread_match` if a matching state is reachable and
        // `rejit_thread_wait` for each non-epsilon transition we can take.
        void (*entry)(struct rejit_threadset_t *, const void *);
        // the initial state of the automaton, duh.
        const void *initial;
        // threads in the active queue should be run, threads in the other one
        // wait until input is advanced one byte.
  /*8*/ RE2JIT_LIST_ROOT(struct rejit_threadq_t) queues[2];
        // the threads. ALL of them, ordered by descending priority.
        // there is no match if this becomes empty at any point.
        RE2JIT_LIST_ROOT(struct rejit_thread_t) threads;
        // last assigned `rejit_threadq_t.bitmap`. incremented each time a backreferenced
        // group matches at a new offset.
        unsigned bitmap_id_last;
        // arbitrary additional data.
        void *data;
    };


    /* Run the NFA. Returns an array of group boundaries if matched, NULL if not.
     * `input`, `length`, `groups`, `flags`, `space`, `entry`, and `initial`
     * must be set prior to calling this. Array is only valid until `rejit_thread_free`. */
    const unsigned *rejit_thread_dispatch(struct rejit_threadset_t *);

    /* Release any lingering threads. The array returned by dispatch becomes invalid. */
    void rejit_thread_free(struct rejit_threadset_t *);

    /* Claim that the currently running thread has matched the input string.
     * Returns 1 if there is no point in following the remaining epsilon transitions. */
    int rejit_thread_match(struct rejit_threadset_t *);

    /* Create a copy of the current thread and place it onto the waiting queue
     * until N more bytes of input are consumed. Returns 1 in same cases as `match`. */
    int rejit_thread_wait(struct rejit_threadset_t *, const void *, size_t);

    /* Check that all empty flags match at the current character. */
    int rejit_thread_satisfies(struct rejit_threadset_t *r, enum RE2JIT_EMPTY_FLAGS empty);

    /* Save the current state bitmap and create a new, zero-filled one
     * because there was some change in state that is impossible to record
     * (thus revisiting states we've already seen may be worthwhile.) */
    void rejit_thread_bitmap_save(struct rejit_threadset_t *);

    /* Restore a previously saved bitmap because we've reverted to the previous state. */
    void rejit_thread_bitmap_restore(struct rejit_threadset_t *);

    #if RE2JIT_ENABLE_SUBROUTINES
    /* Push a state onto the stack. A group id is used to determine when to pop it
     * and jump to the return address. Returns 1 on error (out of memory). */
    int rejit_thread_subcall_push(struct rejit_threadset_t *, const void *state,
                                                              const void *ret, unsigned);

    /* Try to pop a state off the stack. Returns 1 if the id is wrong, meaning
     * we should simply continue on to the next state. */
    int rejit_thread_subcall_pop(struct rejit_threadset_t *, unsigned);
    #endif

#ifdef __cplusplus
}
#endif


#endif
