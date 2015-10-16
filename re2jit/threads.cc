#include <stdlib.h>
#include <string.h>

#include "threads.h"


static struct rejit_thread_t *rejit_thread_acquire(struct rejit_threadset_t *r)
{
    struct rejit_thread_t *t;

    if (r->free) {
        t = r->free;
        r->free = t->next;
        return t;
    }

    t = (struct rejit_thread_t *) malloc(sizeof(struct rejit_thread_t)
                                       + sizeof(int) * r->groups);

    if (t == NULL) {
        return NULL;
    }

    t->category.ref = t;
    rejit_list_init(&t->category);
    rejit_list_init(t);
    return t;
}


static void rejit_thread_release(struct rejit_threadset_t *r, struct rejit_thread_t *t)
{
    t->_prev_before_reclaiming = t->prev;
    rejit_list_remove(t);
    rejit_list_remove(&t->category);
    t->next = r->free;
    r->free = t;
}


static struct rejit_thread_t *rejit_thread_reclaim(struct rejit_threadset_t *r)
{
    struct rejit_thread_t *t = rejit_thread_acquire(r);

    if (t == NULL) {
        return NULL;
    }

    if (t == r->running) {
        // `groups` has not been modified since `release`.
        rejit_list_append(t->_prev_before_reclaiming, t);
    } else {
        // `r->running` has already been reclaimed.
        memcpy(t->groups, r->running->groups, sizeof(int) * r->groups);
        rejit_list_append(r->running, t);
    }

    return t;
}


static struct rejit_thread_t *rejit_thread_entry(struct rejit_threadset_t *r)
{
    struct rejit_thread_t *t = rejit_thread_acquire(r);

    if (t == NULL) {
        return NULL;
    }

    t->entry = r->entry;
    memset(t->groups, 255, sizeof(int) * r->groups);
    rejit_list_append(r->all_threads.last, t);
    rejit_list_append(r->queues[r->active_queue].last, &t->category);
    return t;
}


void rejit_thread_init(struct rejit_threadset_t *r)
{
    rejit_list_init(&r->all_threads);

    size_t i;

    for (i = 0; i <= RE2JIT_THREAD_LOOKAHEAD; i++) {
        rejit_list_init(&r->queues[i]);
    }

    r->empty = RE2JIT_EMPTY_BEGIN_LINE | RE2JIT_EMPTY_BEGIN_TEXT;
    r->offset = 0;
    r->active_queue = 0;
    r->free = NULL;
    r->running = NULL;

    if (!r->length) {
        r->empty |= RE2JIT_EMPTY_END_LINE | RE2JIT_EMPTY_END_TEXT;
    }

    if (rejit_thread_entry(r) == NULL) {
        // Dammit.
    }
}


void rejit_thread_free(struct rejit_threadset_t *r)
{
    struct rejit_thread_t *a, *b;

    #define FREE_LIST(init, end) do { \
        for (a = init; a != end; ) {  \
            b = a;                    \
            a = a->next;              \
            free(b);                  \
        }                             \
    } while (0)

    r->flags |= RE2JIT_THREAD_FAILED;
    r->input = NULL;
    r->length = 0;  // force `thread_dispatch` to stop
    FREE_LIST(r->free, NULL);
    FREE_LIST(r->all_threads.first, rejit_list_end(&r->all_threads));
    #undef FREE_LIST
}


int rejit_thread_dispatch(struct rejit_threadset_t *r, int max_steps)
{
    size_t queue = r->active_queue;

    while (1) {
        struct rejit_thread_ref_t *t = r->queues[queue].first;

        while (t != rejit_list_end(&r->queues[queue])) {
            r->running = t->ref;

            rejit_thread_release(r, r->running);

            #if !RE2JIT_VM
                r->running->entry(r);
            #endif

            if (!--max_steps) {
                return 1;
            }

            t = r->queues[queue].first;
        }

        r->active_queue = queue = (queue + 1) % (RE2JIT_THREAD_LOOKAHEAD + 1);

        if (!r->length) {
            if (r->queues[queue].first != rejit_list_end(&r->queues[queue])) {
                // Allow remaining greedy threads to fail.
                continue;
            }

            return 0;
        }

        if (!(r->flags & RE2JIT_ANCHOR_START)) {
            if (rejit_thread_entry(r) == NULL) {
                // XOO < *ac was completely screwed out of memory
                //        and nothing can fix that!!*
            }
        }

        r->empty &= ~(RE2JIT_EMPTY_BEGIN_LINE | RE2JIT_EMPTY_BEGIN_TEXT);

        if (*r->input++ == '\n') {
            r->empty |= RE2JIT_EMPTY_BEGIN_LINE;
        }

        if (! --(r->length)) {
            r->empty |= RE2JIT_EMPTY_END_LINE | RE2JIT_EMPTY_END_TEXT;
        }

        r->offset++;
        // Word boundaries not supported because UTF-8.
    }
}


int rejit_thread_match(struct rejit_threadset_t *r)
{
    if ((r->flags & RE2JIT_ANCHOR_END) && r->length) {
        // No, it did not. Not EOF yet.
        return 0;
    }

    struct rejit_thread_t *t = rejit_thread_reclaim(r);

    while (t->next != rejit_list_end(&r->all_threads)) {
        // Can safely fail all less important threads. If they fail, this one
        // has matched, so whatever. If they match, this one contains better results.
        rejit_thread_release(r, t->next);
    }

    // Remove this thread from the queue, but leave it in the list of all threads.
    rejit_list_remove(&t->category);
    return 0;
}


void rejit_thread_wait(struct rejit_threadset_t *r, rejit_entry_t entry, size_t shift)
{
    struct rejit_thread_t *t = rejit_thread_reclaim(r);

    if (t == NULL) {
        // :33 < oh shit
        return;
    }

    t->entry = entry;

    size_t queue = (r->active_queue + shift) % (RE2JIT_THREAD_LOOKAHEAD + 1);
    rejit_list_append(r->queues[queue].last, &t->category);
}


int rejit_thread_result(struct rejit_threadset_t *r, int **groups)
{
    if (r->flags & RE2JIT_THREAD_FAILED) {
        return 0;
    }

    if (r->all_threads.first == rejit_list_end(&r->all_threads)) {
        return 0;
    }

    *groups = r->all_threads.first->groups;
    return 1;
}
