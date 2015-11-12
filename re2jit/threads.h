#ifndef RE2JIT_THREADS_H
#define RE2JIT_THREADS_H

#ifdef __cplusplus
extern "C" {
#endif
    /**
     * An implementation of the re2 threaded-code NFA as a support library.
     *
     * Possible paths of execution are represented by "threads", which are pretty much
     * pointers to somewhere within the regular expression. Each step of execution
     * is an attempt to run some code at that pointer; as a result of that, the thread either
     * fails to match an input byte, notifies the NFA of a complete match, or matches
     * a single byte and awaits until all other threads either fail or match that same
     * byte in order to advance the input pointer.
     *
     * Threads are ordered by priority. Of all the threads that match the input,
     * the one with the highest priority provides the answer to the question "where did
     * group N begin/end?". The regex did not match iff none of the threads matched.
     *
     */
    #include <stddef.h>
    #include <stdlib.h>
    #include <stdint.h>

    #include "list.h"


    enum RE2JIT_ANCHOR_FLAGS {
        /* If start point is unanchored, we want to create a copy of the initial thread
         * each time we advance the input, giving it the lowest priority.
         * That way if none of the threads started at earlier positions match,
         * we may still find a match at some position inside the string. */
        RE2JIT_ANCHOR_START = 0x1,
        /* Anchoring the end position is easier: if the end is anchored,
         * each time a thread claims to match the input we check whether we have consumed
         * the whole string. If not, the thread has actually failed. */
        RE2JIT_ANCHOR_END = 0x2,
    };


    enum RE2JIT_EMPTY_FLAGS {  // has to match its re2 counterpart.
        RE2JIT_EMPTY_BEGIN_LINE        = 0x01,  // ^ - beginning of line
        RE2JIT_EMPTY_END_LINE          = 0x02,  // $ - end of line
        RE2JIT_EMPTY_BEGIN_TEXT        = 0x04,  // \A - beginning of text
        RE2JIT_EMPTY_END_TEXT          = 0x08,  // \z - end of text
        RE2JIT_EMPTY_WORD_BOUNDARY     = 0x10,  // \b - word boundary
        RE2JIT_EMPTY_NON_WORD_BOUNDARY = 0x20,  // \B - not \b
    };


    struct rejit_thread_t;
    struct rejit_threadset_t;

    /* An entry point is a function that takes an NFA and a pointer to a state
     * and computes an epsilon closure of that state, calling `thread_wait` for each
     * acceptable non-empty transition reachable from said state. Exactly what
     * a "state" is is backend-defined -- the pointer passed to this function
     * is whatever `thread_wait` was called with, no modifications applied.
     * For example, it may be a pointer to an instruction counter. */
    typedef void (*rejit_entry_t)(struct rejit_threadset_t *, const void *);


    struct rejit_thread_t
    {
        /* Doubly-linked list of all threads, ordered by descending priority.
         * When a thread forks off, it is inserted directly after its parent. */
        RE2JIT_LIST_LINK(struct rejit_thread_t);
        /* Doubly-linked list of all threads in a single queue.
         * Queues are rotated in and out as the input string pointer advances;
         * see `thread_dispatch`. */
        RE2JIT_LIST_LINK(struct rejit_thread_t) category;
        /* Since list links point to each other, not to actual objects that contain them,
         * and this reference is not at the beginning of `struct rejit_thread_t`,
         * we'll have to do some pointer arithmetic. */
        #define RE2JIT_DEREF_THREAD(ref) ((struct rejit_thread_t *)(((char *)(ref)) - offsetof(struct rejit_thread_t, category)))
        /* Pointer to something describing the thread's current state. */
        const void *state;
        /* If non-zero, decrement and move to the next queue instead of running. */
        size_t wait;
        /* Unique identifier of the state bitmap used by this thread.
         * Threads with identical bitmap ids are guaranteed to have matched
         * all backreferenced groups at the same locations. */
        unsigned bitmap_id;
        /* VLA mapping of group indices to indices into the input string.
         * Subgroup N matched the substring indexed by [groups[2N]:groups[2N+1]).
         * Subgroup 0 is special -- it is the whole match. Unmatched subgroups
         * have at least one boundary set to -1. */
        int groups[0];
    };


    struct rejit_threadset_t
    {
        const char *input;
        size_t offset;
        size_t length;
        size_t states;
        unsigned int queue : 1;
        /* Set to 1 if malloc fails during matching. */
        unsigned int oom : 1;
        /* Anchoring mode; enum RE2JIT_ANCHOR_FLAGS. */
        unsigned int flags : 2;
        /* State flags for zero-width checks; enum RE2JIT_EMPTY_FLAGS.
         * Inverted, so `empty & expected` is zero if every expected flag is set. */
        unsigned int empty;
        /* Actual length of `thread_t.groups`. Must be at least 2 to store
         * the location of the whole match, + 2 per subgroup needed. */
        unsigned int groups;
        /* A bitmap that may be used to mark visited states. It is reset
         * automatically every time the input pointer advances. */
        uint8_t *bitmap;
        /* Currently active thread, set by `thread_dispatch`. */
        struct rejit_thread_t *running;
        /* Last (so far) thread forked off the currently running one. Threads are created
         * in descending priority, so the next one should be inserted after this one. */
        struct rejit_thread_t *forked;
        /* Linked list of failed threads. These can be reused to avoid allocations. */
        struct rejit_thread_t *free;
        /* The state to spawn the initial thread with. */
        const void *initial;
        /* Function to call to compute the epsilon closure of a single state. */
        void (*entry)(struct rejit_threadset_t *, const void *);
        /* Doubly-linked list of threads ordered by descending priority. Again.
         * There is no match iff this becomes empty at some point, and there is a match
         * iff there is exactly one thread, and it is not in any of the queues. */
        RE2JIT_LIST_ROOT(struct rejit_thread_t) all_threads;
        /* Threads in queue with index `queue` are currently active; threads
         * in the other queue are waiting for them to read a single byte. */
        RE2JIT_LIST_ROOT(struct rejit_thread_t) queues[2];
        /* ID of the bitmap currently in use. If a thread with a different ID becomes
         * active, the whole bitmap should be reset, else we may never enter some
         * valid states. */
        unsigned bitmap_id;
        /* Last assigned bitmap ID. A new ID is assigned whenever a thread
         * requests a new bitmap because one of the backreferenced groups
         * has changed its position. Unlike `bitmap_id`, this value is not restored
         * after that thread terminates and releases the bitmap. */
        unsigned bitmap_id_last;
    };


    /* Finish initialization of an NFA. Input, its length, the number of capturing
     * parentheses and states, as well as the initial state and the entry point
     * should all be set prior to calling this. */
    void rejit_thread_init(struct rejit_threadset_t *);
    void rejit_thread_free(struct rejit_threadset_t *);

    /* Run all threads on the active queue, switch to the other one, repeat
     * until either both queues are empty or there is no input to consume.
     * Return -1 if ran out of memory, 0 if failed, and 1 if matched, in which
     * case a provided pointer will be set to the array of subgroup boundaries. */
    int rejit_thread_dispatch(struct rejit_threadset_t *, int **);

    /* Claim that the currently running thread has matched the input string.
     * Return 1 if no point in following the remaining epsilon transitions. */
    int rejit_thread_match(struct rejit_threadset_t *);

    /* Create a copy of the current thread and place it onto the waiting queue
     * until N more bytes of input are consumed. Return 1 in same cases as `match`. */
    int rejit_thread_wait(struct rejit_threadset_t *, const void *, size_t);

    /* Save the current state bitmap and create a new, zero-filled one
     * because there was some change in state that is impossible to record.
     * (thus revisiting states we've already seen may be worthwhile.) */
    int rejit_thread_bitmap_save(struct rejit_threadset_t *);

    /* Restore a previously saved bitmap because we've reverted to the previous state. */
    void rejit_thread_bitmap_restore(struct rejit_threadset_t *);

#ifdef __cplusplus
};
#endif


#endif
