#include <stdlib.h>
#include <string.h>

#include "threads.h"


static struct rejit_thread_t *rejit_thread_acquire(struct rejit_threadset_t *r)
{
    struct rejit_thread_t *t;

    if (r->free) {
        t = r->free;
        r->free = t->next;
        t->next = t;
        return t;
    }

    t = (struct rejit_thread_t *) malloc(sizeof(struct rejit_thread_t)
                                       + sizeof(int) * r->groups);

    if (t == NULL) {
        return NULL;
    }

    rejit_list_init(&t->category);
    rejit_list_init(t);
    return t;
}


static struct rejit_thread_t *rejit_thread_fork(struct rejit_threadset_t *r)
{
    struct rejit_thread_t *t = rejit_thread_acquire(r);

    if (t == NULL) {
        return NULL;
    }

    memcpy(t->groups, r->running->groups, sizeof(int) * r->groups);
    rejit_list_append(r->forked, t);
    return r->forked = t;
}


// Spawn a copy of the initial thread, pointing at the regexp's entry point.
static struct rejit_thread_t *rejit_thread_entry(struct rejit_threadset_t *r)
{
    struct rejit_thread_t *t = rejit_thread_acquire(r);

    if (t == NULL) {
        return NULL;
    }

    memset(t->groups, 255, sizeof(int) * r->groups);
    t->entry = r->entry;
    t->groups[0] = r->offset;
    rejit_list_append(r->all_threads.last, t);
    rejit_list_append(r->queues[r->active_queue].last, &t->category);
    return t;
}


int rejit_thread_init(struct rejit_threadset_t *r)
{
    rejit_list_init(&r->all_threads);

    r->visited = (uint8_t *) calloc(sizeof(uint8_t), (r->states + 7) / 8);

    if (r->visited == NULL) {
        return 0;
    }

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
        free(r->visited);
        r->visited = NULL;
        return 0;
    }

    return 1;
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

    FREE_LIST(r->free, NULL);
    FREE_LIST(r->all_threads.first, rejit_list_end(&r->all_threads));
    free(r->visited);
    r->visited = NULL;
    #undef FREE_LIST
}


int rejit_thread_dispatch(struct rejit_threadset_t *r)
{
    #if RE2JIT_VM
        if (r->running) {
            r->running->next = r->free;
            r->free = r->running;
            r->running = NULL;
        }
    #endif

    size_t queue = r->active_queue;

    while (1) {
        struct rejit_thread_t *t = r->queues[queue].first;

        while (t != rejit_list_end(&r->queues[queue])) {
            struct rejit_thread_t *q = RE2JIT_DEREF_THREAD(t);

            r->running = q;
            r->forked  = q->prev;

            rejit_list_remove(q);
            rejit_list_remove(&q->category);

            #if RE2JIT_VM
                return 1;
            #else
                q->entry(r);
            #endif

            q->next = r->free;
            r->free = q;

            t = r->queues[queue].first;
        }

        r->running = NULL;

        if (!r->length) {
            return 0;
        }

        // this bit vector is shared across all threads on a single queue.
        // whichever thread first enters a state gets to own that state.
        memset(r->visited, 0, (r->states + 7) / 8);

        r->active_queue = queue = (queue + 1) % (RE2JIT_THREAD_LOOKAHEAD + 1);
        r->offset++;
        r->empty = 0;

        if (*r->input++ == '\n') {
            r->empty |= RE2JIT_EMPTY_BEGIN_LINE;
        }

        if (! --(r->length)) {
            r->empty |= RE2JIT_EMPTY_END_LINE | RE2JIT_EMPTY_END_TEXT;
        } else if (*r->input == '\n') {
            r->empty |= RE2JIT_EMPTY_END_LINE;
        }

        // Word boundaries not supported because UTF-8.

        if (!(r->flags & RE2JIT_ANCHOR_START)) {
            if (rejit_thread_entry(r) == NULL) {
                // XOO < *ac was completely screwed out of memory
                //        and nothing can fix that!!*
            }
        }
    }
}


int rejit_thread_match(struct rejit_threadset_t *r)
{
    if ((r->flags & RE2JIT_ANCHOR_END) && r->length) {
        // No, it did not. Not EOF yet.
        return 0;
    }

    struct rejit_thread_t *t = rejit_thread_fork(r);

    t->groups[1] = r->offset;

    while (t->next != rejit_list_end(&r->all_threads)) {
        struct rejit_thread_t *q = t->next;
        // Can safely fail all less important threads. If they fail, this one
        // has matched, so whatever. If they match, this one contains better results.
        rejit_list_remove(q);
        rejit_list_remove(&q->category);
        q->next = r->free;
        r->free = q;
    }

    return 1;
}


int rejit_thread_wait(struct rejit_threadset_t *r, rejit_entry_t entry, size_t shift)
{
    struct rejit_thread_t *t = rejit_thread_fork(r);

    if (t == NULL) {
        // :33 < oh shit
        return 0;
    }

    t->entry = entry;

    size_t queue = (r->active_queue + shift) % (RE2JIT_THREAD_LOOKAHEAD + 1);
    rejit_list_append(r->queues[queue].last, &t->category);
    return 0;
}


int rejit_thread_result(struct rejit_threadset_t *r, int **groups)
{
    if (r->all_threads.first == rejit_list_end(&r->all_threads)) {
        return 0;
    }

    *groups = r->all_threads.first->groups;
    return 1;
}
