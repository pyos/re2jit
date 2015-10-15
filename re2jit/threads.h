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
    #include <stdlib.h>

    #include "list.h"

    /* Maximum number of bytes a thread can consume per one opcode.
     * Normally, re2 only emits opcodes that match a single byte. I got this sweet
     * idea to "invent" some opcodes that can match whole UTF-8 characters, though... */
    #define RE2JIT_THREAD_LOOKAHEAD 1


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


    enum RE2JIT_THREAD_FLAGS {
        /* If `thread_free` is called in the middle of `thread_dispatch`, this flag
         * is set. It means that the a thread has encountered an invalid instruction. */
        RE2JIT_THREAD_FAILED = 0x4,
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
        typedef size_t rejit_entry_t;
    #else
        /* While the JIT-compiled code is not expected to be a function,
         * it should at least behave like one. A thread's entry point should expect
         * to receive a pointer to the NFA where the first argument should be,
         * and use the standard return mechanisms to get back to `thread_dispatch`. */
        typedef void (*rejit_entry_t)(struct rejit_threadset_t *);
    #endif


    struct rejit_thread_ref_t
    {
        RE2JIT_LIST_LINK(struct rejit_thread_t);
        /* `thread_t` has to be a part of two lists at once, but only one
         * `LIST_LINK` can be at the beginning of the struct.
         * Thus, this little wrapper. */
        struct rejit_thread_t *ref;
    };


    struct rejit_thread_t
    {
        /* Doubly-linked list of all threads, ordered by descending priority.
         * When a thread forks off, it inherits priority from its parent,
         * decreased by an arbitrarily small value. In other words, a new thread
         * is always inserted exactly after its parent. If multiple threads match,
         * the one with the highest priority provides the group-to-index mapping. */
        RE2JIT_LIST_LINK(struct rejit_thread_t);
        /* Doubly-linked list of all threads in a single queue.
         * Queues are rotated in and out as the input string pointer advances;
         * see `thread_dispatch`. */
        struct rejit_thread_ref_t category;
        /* Pointer to the beginning of the thread's code. */
        rejit_entry_t entry;
        /* VLA mapping of group indices to indices into the input string.
         * Subgroup N matched the substring indexed by [groups[2N]:groups[2N+1]).
         * Subgroup 0 is special -- it is the whole match. */
        int groups[0];
    };


    struct rejit_threadset_t
    {
        const char *input;
        size_t length;
        /* Entry point of the initial thread. */
        rejit_entry_t entry;
        /* Actual length of `thread_t.groups`. Must be at least 2. */
        size_t groups;
        /* Ring buffer of thread queues. The threads in the active queue are ready to
         * run; the rest are waiting for the input pointer to advance. When the active
         * queue becomes empty (due to all threads in it failing, matching, or reading
         * an input byte and moving themselves to a different queue), the input string
         * is advanced one byte and the queue buffer is rotated one position. */
        size_t active_queue;
        RE2JIT_LIST_ROOT(struct rejit_thread_ref_t) queues[RE2JIT_THREAD_LOOKAHEAD + 1];
        /* Doubly-linked list of threads ordered by descending priority. Again.
         * There is no match iff this becomes empty at some point, and there is a match
         * iff there is exactly one thread, and it is not in any of the queues. */
        RE2JIT_LIST_ROOT(struct rejit_thread_t) all_threads;
        /* Currently active thread, set by `thread_dispatch`. */
        struct rejit_thread_t *running;
        /* Linked list of failed threads. These can be reused to avoid allocations. */
        struct rejit_thread_t *free;

        unsigned /* enum RE2JIT_THREAD_ANCHOR */ flags;
        unsigned /* enum RE2JIT_EMPTY_FLAGS   */ empty;
    };


    /* Finish initialization of an NFA. Input, its length, optional flags, number of
     * capturing parentheses, and the initial entry point should be populated
     * prior to calling this. */
    void rejit_thread_init(struct rejit_threadset_t *);

    /* Deallocate all threads and reset the NFA to a failed state. */
    void rejit_thread_free(struct rejit_threadset_t *);

    /* Pop a thread off the active queue and jump to its entry point, repeat at most
     * `max_steps` times. If at some point no thread is active, advance the input one
     * byte forward and rotate the queue ring. If no thread becomes active until the end
     * of the input, return 0; `thread_result` can then be used to determine whether
     * the regex matched. Otherwise, return 1. */
    int rejit_thread_dispatch(struct rejit_threadset_t *, int max_steps);

    /* Fork a new thread off the currently running one. */
    void rejit_thread_fork(struct rejit_threadset_t *, rejit_entry_t);

    /* Claim that the currently running thread has matched the input string. */
    void rejit_thread_match(struct rejit_threadset_t *);

    /* Destroy the currently running thread because it failed to match. */
    void rejit_thread_fail(struct rejit_threadset_t *);

    /* Make the current thread wait for N more bytes. */
    void rejit_thread_wait(struct rejit_threadset_t *, size_t);

    /* Check whether any thread has matched the string, return 1 for a match
     * and 0 otherwise. If a match exists, the thread's array of subgroup locations
     * is returned as an out-parameter. If the NFA has not finished yet (i.e.
     * `rejit_thread_dispatch` did not return 0), behavior is undefined. */
    int rejit_thread_result(struct rejit_threadset_t *, int **);

#ifdef __cplusplus
};
#endif


#endif
