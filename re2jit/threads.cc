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
        t->wait = 0;
        return t;
    }

    t = (struct rejit_thread_t *) malloc(sizeof(struct rejit_thread_t)
                                       + sizeof(int) * r->groups);

    RE2JIT_NULL_CHECK(t) return NULL;
    rejit_list_init(&t->category);
    rejit_list_init(t);
    t->wait = 0;
    return t;
}


static struct rejit_thread_t *rejit_thread_fork(struct rejit_threadset_t *r)
{
    struct rejit_thread_t *t = rejit_thread_acquire(r);

    RE2JIT_NULL_CHECK(t) return NULL;
    memcpy(t->groups, r->running->groups, sizeof(int) * r->groups);
    rejit_list_append(r->forked, t);
    return r->forked = t;
}


static struct rejit_thread_t *rejit_thread_entry(struct rejit_threadset_t *r)
{
    struct rejit_thread_t *t = rejit_thread_acquire(r);

    RE2JIT_NULL_CHECK(t) return NULL;
    memset(t->groups, 255, sizeof(int) * r->groups);
    t->state = r->initial;
    t->groups[0] = r->offset;
    rejit_list_append(r->all_threads.last, t);
    rejit_list_append(r->queues[r->active_queue].last, &t->category);
    return t;
}


int rejit_thread_init(struct rejit_threadset_t *r)
{
    rejit_list_init(&r->all_threads);

    r->visited = (uint8_t *) calloc(sizeof(uint8_t), (r->states + 7) / 8);
    RE2JIT_NULL_CHECK(r->visited) return 0;
    rejit_list_init(&r->queues[0]);
    rejit_list_init(&r->queues[1]);
    r->empty = RE2JIT_EMPTY_BEGIN_LINE | RE2JIT_EMPTY_BEGIN_TEXT;
    r->offset = 0;
    r->active_queue = 0;
    r->free = NULL;
    r->running = NULL;

    if (!r->length)
        r->empty |= RE2JIT_EMPTY_END_LINE | RE2JIT_EMPTY_END_TEXT;

    RE2JIT_NULL_CHECK(rejit_thread_entry(r)) {
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
    unsigned char queue = r->active_queue;

    while (1) {
        struct rejit_thread_t *t = r->queues[queue].first;

        for (; t != rejit_list_end(&r->queues[queue]); t = r->queues[queue].first) {
            struct rejit_thread_t *q = RE2JIT_DEREF_THREAD(t);

            if (q->wait) {
                q->wait--;
                rejit_list_remove(&q->category);
                rejit_list_append(r->queues[!queue].last, &q->category);
                continue;
            }

            r->running = q;
            r->forked  = q->prev;
            rejit_list_remove(q);
            rejit_list_remove(&q->category);

            r->entry(r, q->state);
            q->next = r->free;
            r->free = q;
        }

        if (!r->length)
            return 0;

        memset(r->visited, 0, (r->states + 7) / 8);

        r->active_queue = queue = !queue;
        r->offset++;
        r->empty = 0;

        if (*r->input++ == '\n')
            r->empty |= RE2JIT_EMPTY_BEGIN_LINE;

        if (--r->length == 0)
            r->empty |= RE2JIT_EMPTY_END_LINE | RE2JIT_EMPTY_END_TEXT;
        else if (*r->input == '\n')
            r->empty |= RE2JIT_EMPTY_END_LINE;

        // Word boundaries not supported because UTF-8.

        if (!(r->flags & RE2JIT_ANCHOR_START))
            RE2JIT_NULL_CHECK(rejit_thread_entry(r)) {
                // XOO < *ac was completely screwed out of memory
                //        and nothing can fix that!!*
            }
    }
}


int rejit_thread_match(struct rejit_threadset_t *r)
{
    if ((r->flags & RE2JIT_ANCHOR_END) && r->length)
        // No, it did not. Not EOF yet.
        return 0;

    struct rejit_thread_t *t = rejit_thread_fork(r);
    RE2JIT_NULL_CHECK(t) return 0;
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


int rejit_thread_wait(struct rejit_threadset_t *r, void *state, size_t shift)
{
    struct rejit_thread_t *t = rejit_thread_fork(r);
    RE2JIT_NULL_CHECK(t) return 0;
    t->state = state;
    t->wait = shift - 1;
    rejit_list_append(r->queues[!r->active_queue].last, &t->category);
    return 0;
}


int rejit_thread_result(struct rejit_threadset_t *r, int **groups)
{
    if (r->all_threads.first == rejit_list_end(&r->all_threads))
        return 0;

    *groups = r->all_threads.first->groups;
    return 1;
}
