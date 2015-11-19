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
        // group `i` spans the input from `groups[2i]`-th character to `groups[2i+1]`-th.
        // if either bound is -1, the group did not match. 0-th group is the whole match.
        int groups[];
    };


    struct rejit_threadset_t
    {
  /*0*/ const char *input;
        size_t offset;
        size_t length;
        // actual length of `rejit_thread_t.groups`. must be at least 2 to store
        // the location of the whole match, + 2 per capturing group, if needed.
        unsigned groups;
        // inverted combination of RE2JIT_EMPTY_FLAGS,
        // i.e. a flag is set if &-ing with it yields 0.
        unsigned char  empty;
        // the one to run on the next input byte.
        unsigned char  queue : 1;
        // RE2JIT_THREAD_FLAGS. not inverted this time.
        unsigned char  flags : 7;
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
  /*8*/ const void *initial;
        // threads in the active queue should be run, threads in the other one
        // wait until input is advanced one byte.
        RE2JIT_LIST_ROOT(struct rejit_threadq_t) queues[2];
        // the threads. ALL of them, ordered by descending priority.
        // there is no match if this becomes empty at any point.
        RE2JIT_LIST_ROOT(struct rejit_thread_t) threads;
        // id of the bitmap currently in use. if a thread with a different id
        // is encountered, the bitmap must be reset because that thread
        // (and the ones it will spawn) is different from any we've already run.
        unsigned bitmap_id;
        // last assigned bitmap id. incremented each time a backreferenced
        // group matches at a new offset.
        unsigned bitmap_id_last;
        // arbitrary additional data.
 /*16*/ void *data;
    };


    /* Finish initialization of an NFA. `input`, `length`, `groups`, `flags`, `space`,
     * `entry`, and `initial` must be set prior to calling this. */
    void rejit_thread_init(struct rejit_threadset_t *);
    void rejit_thread_free(struct rejit_threadset_t *);

    /* Run the NFA. Returns -1 if ran out of memory, 0 if failed, and 1 if matched,
     * in which case the provided pointer will be set to an array of group boundaries. */
    int rejit_thread_dispatch(struct rejit_threadset_t *, int **);

    /* Claim that the currently running thread has matched the input string.
     * Returns 1 if there is no point in following the remaining epsilon transitions. */
    int rejit_thread_match(struct rejit_threadset_t *);

    /* Create a copy of the current thread and place it onto the waiting queue
     * until N more bytes of input are consumed. Returns 1 in same cases as `match`. */
    int rejit_thread_wait(struct rejit_threadset_t *, const void *, size_t);

    /* Save the current state bitmap and create a new, zero-filled one
     * because there was some change in state that is impossible to record
     * (thus revisiting states we've already seen may be worthwhile.) */
    int rejit_thread_bitmap_save(struct rejit_threadset_t *);

    /* Restore a previously saved bitmap because we've reverted to the previous state. */
    void rejit_thread_bitmap_restore(struct rejit_threadset_t *);

#ifdef __cplusplus
}
#endif


#endif
