#ifndef RE2JIT_THREADS_H
#define RE2JIT_THREADS_H
/**
 * An implementation of the re2 threaded-code NFA as a support library.
 *
 * Each possible path of execution is represented by a "thread", which is pretty much
 * a pointer to somewhere within the regular expression. Each step of execution
 * is an attempt to run some code at that pointer; as a result of that, the thread either
 * fails to match an input byte, notifies the NFA of a complete match, or matches
 * a single byte and awaits until all other threads either fail or match that
 * byte in order to advance the input string pointer.
 *
 * Threads are ordered by priority. Of all the threads that match the input,
 * the one with the highest priority provides the answer to the question "where did
 * group N begin/end?". The regex did not match iff none of the threads matched.
 *
 */
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif
    // Must be callable by jitted functions, so no name mangling and other stuff.
    #include "list.h"


    // Maximum number of bytes a thread can consume per one opcode.
    // Normally, re2 only emits opcodes that match a single byte. I got this sweet
    // idea to "invent" some opcodes that can match whole UTF-8 characters, though...
    #define RE2JIT_THREAD_LOOKAHEAD 1


    enum RE2JIT_THREAD_ANCHOR {
        // If start point is unanchored, we want to create a copy of the initial thread
        // each time we advance the input, giving it the lowest priority.
        // That way if none of the threads started at earlier positions match,
        // we may still find a match at some position inside the string.
        RE2JIT_ANCHOR_START = 0x1,
        // Anchoring the end position is easier: if the end is anchored,
        // each time a thread claims to match the input we check whether we have consumed
        // the whole string. If not, the thread has actually failed.
        RE2JIT_ANCHOR_END = 0x2,
    };


    enum RE2JIT_EMPTY_FLAGS {
        // Copied from re2.
        RE2JIT_EMPTY_BEGIN_LINE        = 0x01,  // ^ - beginning of line
        RE2JIT_EMPTY_END_LINE          = 0x02,  // $ - end of line
        RE2JIT_EMPTY_BEGIN_TEXT        = 0x04,  // \A - beginning of text
        RE2JIT_EMPTY_END_TEXT          = 0x08,  // \z - end of text
        RE2JIT_EMPTY_WORD_BOUNDARY     = 0x10,  // \b - word boundary
        RE2JIT_EMPTY_NON_WORD_BOUNDARY = 0x20,  // \B - not \b
    };


    struct st_rejit_thread_t;
    struct st_rejit_thread_ref_t
    {
        RE2JIT_LIST_LINK(struct st_rejit_thread_t);
        // `thread_t` has to be a part of two lists at once, but only one
        // `RE2JIT_LIST_LINK` can be at the beginning of the struct.
        // Thus, this little wrapper.
        struct st_rejit_thread_t *ref;
    };


    struct st_rejit_thread_t
    {
        // Doubly-linked list of all threads, ordered by descending priority.
        // When a thread forks off, it inherits priority from its parent,
        // decreased by an arbitrarily small value. In other words, a new thread
        // is always inserted exactly after its parent. If multiple threads match,
        // the one with the highest priority provides the group-to-index mapping.
        RE2JIT_LIST_LINK(struct st_rejit_thread_t);
        // Doubly-linked list of all threads in a single queue.
        // Queues are rotated in and out as the input string pointer is advanced;
        // see `rejit_thread_dispatch`.
        struct st_rejit_thread_ref_t category;
        // Pointer to the beginning of the thread's code.
        void *entry;
        // The VLA mapping of group indices to offsets in the source string.
        // Inherited from parent thread; has actual length of `threadset.ngroups`.
        // For a group with index `n` in the regex, the starting position is
        // `groups[2 * n]` and the end is at `groups[2 * n + 1]`.
        int groups[0];
    };


    struct st_rejit_threadset_t
    {
        const char *input;
        size_t length;
        // Entry point of the initial thread -- the one which is spawned
        // at the beginning of the input.
        void *entry;
        // Jump back to this point to return to `thread_dispatch`.
        void *return_;
        // Length of `groups` in each thread. NOT the number of groups in the regex.
        // More like double that. That is, +1 for each open and closing parenthesis.
        int ngroups;
        // Options such as anchoring of the regex.
        int flags;
        // Empty-length flags at the current input position.
        int empty;
        // Ring buffer of thread queues. The threads in the active queue are run,
        // in order, until all of them match, fail, or move to one of the inactive
        // queues. Then the input is advanced by a single byte, and the buffer
        // is rotated by one position counterclockwise. Thus, active queue + N
        // is the queue of threads waiting for the N-th byte after the current one.
        size_t active_queue;
        RE2JIT_LIST_ROOT(struct st_rejit_thread_ref_t) queues[RE2JIT_THREAD_LOOKAHEAD + 1];
        RE2JIT_LIST_ROOT(struct st_rejit_thread_t) all_threads;
        // The thread that `rejit_thread_dispatch` has entered last time.
        struct st_rejit_thread_t *running;
        // Linked list of unused, but not yet freed `rejit_thread_t` objects.
        // Performance > *.
        struct st_rejit_thread_t *free;
    };


    typedef struct st_rejit_thread_t rejit_thread_t;
    typedef struct st_rejit_threadset_t rejit_threadset_t;


    // Create a new threaded NFA running on a given input.
    // Initially, a single thread is active, pointing at some entry point.
    rejit_threadset_t *rejit_thread_init(const char *, size_t, void *entry, int flags, int ngroups);

    // Deallocate the NFA, including all the dead threads.
    void rejit_thread_free(rejit_threadset_t *);

    // Pop a thread off the active queue and jump to its entry point, repeat at most
    // `max_steps` times. If at some point no thread is active, advance the input one
    // byte forward and rotate the queue ring. If no thread becomes active until the end
    // of the input, return 0; at this point, `rejit_thread_result` can be used to
    // determine whether the regex matched. Otherwise, return 1.
    int rejit_thread_dispatch(rejit_threadset_t *, int max_steps);

    // Fork a new thread off the currently running one.
    void rejit_thread_fork(rejit_threadset_t *, void *);

    // Claim that the currently running thread has matched the input string.
    void rejit_thread_match(rejit_threadset_t *);

    // Destroy the currently running thread because it failed to match.
    void rejit_thread_fail(rejit_threadset_t *);

    // Make the current thread wait for N more bytes.
    void rejit_thread_wait(rejit_threadset_t *, size_t);

    // Check whether any thread has matched the string, return 1 for a match
    // and 0 otherwise. If a match exists, the thread's array of subgroup locations
    // is returned as an out-parameter. If the NFA has not finished yet (i.e.
    // `rejit_thread_dispatch` did not return 0), behavior is undefined.
    int rejit_thread_result(rejit_threadset_t *, int **);

#ifdef __cplusplus
};
#endif


#endif
