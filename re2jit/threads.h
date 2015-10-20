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

    #if RE2JIT_NO_EXTCODES
        /* Maximum number of bytes a thread can consume per one opcode.
         * Normally, re2 only emits opcodes that match a single byte. I got this sweet
         * idea to "invent" some opcodes that can match whole UTF-8 characters, though... */
        #define RE2JIT_THREAD_LOOKAHEAD 1
    #else
        #define RE2JIT_THREAD_LOOKAHEAD 7
    #endif


    enum RE2JIT_THREAD_ANCHOR {
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


    #if RE2JIT_VM
        // In VM mode, "entry points" are indices into the opcode array.
        // TODO not handle that as a special case somehow.
        typedef size_t rejit_entry_t;
    #else
        /* The JIT-compiled code is expected to behave like a function at each opcode,
         * accepting an NFA as the first "argument" and "returning" when it has exhausted
         * all empty-length arrows. */
        typedef void (*rejit_entry_t)(struct rejit_threadset_t *);
    #endif


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
        /* Pointer to the beginning of the thread's code. */
        rejit_entry_t entry;
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
        unsigned /* enum RE2JIT_THREAD_ANCHOR */ flags;
        unsigned /* enum RE2JIT_EMPTY_FLAGS   */ empty;
        /* A vector of bits, one for each state, marking whether that state was already
         * visited while handling this input character. Used to avoid infinite
         * loops consisting purely of empty transitions. */
        uint8_t *visited;
        /* Entry point of the initial thread. */
        rejit_entry_t entry;
        /* Actual length of `thread_t.groups`. Must be at least 2 to store
         * the location of the whole match, + 2 for each subgroup if needed. */
        size_t groups;
        /* Currently active thread, set by `thread_dispatch`. */
        struct rejit_thread_t *running;
        /* Last (so far) thread forked off the currently running one. Threads are created
         * in descending priority, so the next one should be inserted after this one. */
        struct rejit_thread_t *forked;
        /* Linked list of failed threads. These can be reused to avoid allocations. */
        struct rejit_thread_t *free;
        /* Ring buffer of thread queues. The threads in the active queue are ready to
         * run; the rest are waiting for the input pointer to advance. When the active
         * queue becomes empty (due to all threads in it failing, matching, or reading
         * an input byte and moving themselves to a different queue), the input string
         * is advanced one byte and the queue buffer is rotated one position. */
        size_t active_queue;
        // NOTE use RE2JIT_DEREF_THREAD on objects from these lists to get valid pointers.
        RE2JIT_LIST_ROOT(struct rejit_thread_t) queues[RE2JIT_THREAD_LOOKAHEAD + 1];
        /* Doubly-linked list of threads ordered by descending priority. Again.
         * There is no match iff this becomes empty at some point, and there is a match
         * iff there is exactly one thread, and it is not in any of the queues. */
        RE2JIT_LIST_ROOT(struct rejit_thread_t) all_threads;
    };


    /* Finish initialization of an NFA. Input, its length, optional flags, number of
     * capturing parentheses, and the initial entry point should be populated
     * prior to calling this. Returns 0 on failure. */
    int rejit_thread_init(struct rejit_threadset_t *);

    /* Deallocate all threads. */
    void rejit_thread_free(struct rejit_threadset_t *);

    /* While there's a thread on the active queue, pop it off and jump to its entry
     * point. If at some point no thread is active, advance the input one byte
     * forward and rotate the queue ring. When the end of the input is reached
     * and no more threads are active, return 0. In VM mode, return 1 instead of
     * jumping to the entry point. */
    int rejit_thread_dispatch(struct rejit_threadset_t *);

    /* Claim that the currently running thread has matched the input string.
     * Returns 0 if it has actually failed, 1 otherwise. */
    int rejit_thread_match(struct rejit_threadset_t *);

    /* Fork a new thread off the currently running one and make it wait for N more bytes.
     * Always returns 0. */
    int rejit_thread_wait(struct rejit_threadset_t *, rejit_entry_t, size_t);

    /* Check whether any thread has matched the string, return 1 for a match
     * and 0 otherwise. If a match exists, the thread's array of subgroup locations
     * is returned as an out-parameter. If the NFA has not finished yet (i.e.
     * `rejit_thread_dispatch` did not return 0), behavior is undefined. */
    int rejit_thread_result(struct rejit_threadset_t *, int **);

#ifdef __cplusplus
};
#endif


#endif
