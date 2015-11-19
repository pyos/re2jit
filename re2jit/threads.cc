#include <stdlib.h>
#include <string.h>

#include "threads.h"


static void rejit_thread_free_lists(struct rejit_threadset_t *r)
{
    struct rejit_thread_t *a, *b;

    #define FREE_LIST(init, end) do { \
        void *__e = end;              \
        for (a = init; a != __e; ) {  \
            b = a;                    \
            a = a->next;              \
            free(b);                  \
        }                             \
    } while (0)
    FREE_LIST(r->free, NULL);
    FREE_LIST(r->all_threads.first, rejit_list_end(&r->all_threads));
    #undef FREE_LIST

    rejit_list_init(&r->all_threads);
    rejit_list_init(&r->queues[0]);
    rejit_list_init(&r->queues[1]);
    r->flags |= RE2JIT_UNDEFINED;
}


static struct rejit_thread_t *rejit_thread_acquire(struct rejit_threadset_t *r)
{
    if (r->flags & RE2JIT_UNDEFINED)
        return NULL;

    struct rejit_thread_t *t = r->free;

    if (t) {
        r->free = t->next;
        t->next = t;
        return t;
    }

    t = (struct rejit_thread_t *) malloc(sizeof(struct rejit_thread_t)
                                       + sizeof(int) * r->groups);

    if (t == NULL)
        // Well, shit.
        rejit_thread_free_lists(r);

    return t;
}


static struct rejit_thread_t *rejit_thread_fork(struct rejit_threadset_t *r)
{
    struct rejit_thread_t *t = rejit_thread_acquire(r);
    struct rejit_thread_t *c = r->running;
    if (t == NULL) return NULL;

    rejit_list_append(r->forked, t);
    memcpy(t->groups, c->groups, sizeof(int) * r->groups);
    t->bitmap_id = c->bitmap_id;
    return r->forked = t;
}


static struct rejit_thread_t *rejit_thread_initial(struct rejit_threadset_t *r)
{
    struct rejit_thread_t *t = rejit_thread_acquire(r);
    if (t == NULL) return NULL;

    memset(t->groups, 255, sizeof(int) * r->groups);
    t->queue.wait = 0;
    t->bitmap_id  = 0;
    t->groups[0]  = r->offset;
    t->state      = r->initial;
    rejit_list_append(r->all_threads.last, t);
    rejit_list_append(r->queues[r->queue].last, &t->queue);
    return t;
}


void rejit_thread_init(struct rejit_threadset_t *r)
{
    r->bitmap         = (uint8_t *) malloc(r->space);
    r->bitmap_id      = 0;
    r->bitmap_id_last = 0;
    r->offset         = 0;
    r->queue          = 0;
    r->free           = NULL;
    r->running        = NULL;
    r->empty          = ~(RE2JIT_EMPTY_BEGIN_LINE | RE2JIT_EMPTY_BEGIN_TEXT);
    rejit_list_init(&r->all_threads);
    rejit_list_init(&r->queues[0]);
    rejit_list_init(&r->queues[1]);
    rejit_thread_initial(r);
}


void rejit_thread_free(struct rejit_threadset_t *r)
{
    rejit_thread_free_lists(r);
    free(r->bitmap);
}


int rejit_thread_dispatch(struct rejit_threadset_t *r, int **groups)
{
    unsigned char queue = r->queue;

    if (!r->bitmap)
        return -1;

    do {
        r->bitmap_id = -1;

        if (!(r->flags & RE2JIT_ANCHOR_START) && r->offset)
            // don't spawn a new initial thread if we already have a match.
            if (r->all_threads.last == rejit_list_end(&r->all_threads) || r->all_threads.last->groups[1] == -1)
                rejit_thread_initial(r);

        if (!r->length)
            r->empty &= ~(RE2JIT_EMPTY_END_LINE | RE2JIT_EMPTY_END_TEXT);
        else if (*r->input == '\n')
            r->empty &= ~RE2JIT_EMPTY_END_LINE;

        struct rejit_thread_t  *t;
        struct rejit_threadq_t *q   = r->queues[queue].first;
        struct rejit_threadq_t *end = rejit_list_end(&r->queues[queue]);

        if (q == end)
            // if this queue is empty, the next will be too, and the one after that...
            break;

        do {
            rejit_list_remove(q);

            if (q->wait) {
                q->wait--;
                rejit_list_append(r->queues[!queue].last, q);
                continue;
            }

            r->running = t = rejit_list_container(struct rejit_thread_t, queue, q);
            r->forked  = t->prev;
            rejit_list_remove(t);

            if (r->bitmap_id != t->bitmap_id) {
                r->bitmap_id  = t->bitmap_id;
                memset(r->bitmap, 0, r->space);
            }

            r->entry(r, t->state);
            t->next = r->free;
            r->free = t;
        } while ((q = r->queues[queue].first) != end);

        r->input++;
        r->offset++;
        r->queue = queue = !queue;
        r->empty = r->empty & RE2JIT_EMPTY_END_LINE ? ~0 : ~RE2JIT_EMPTY_BEGIN_LINE;
        // Word boundaries not supported because UTF-8.
    } while (r->length--);

    if (r->flags & RE2JIT_UNDEFINED)
        // XOO < *ac was completely screwed out of memory
        //        and nothing can fix that!!*
        return -1;

    if (r->all_threads.first == rejit_list_end(&r->all_threads))
        return 0;

    *groups = r->all_threads.first->groups;
    return 1;
}


int rejit_thread_match(struct rejit_threadset_t *r)
{
    if ((r->flags & RE2JIT_ANCHOR_END) && r->length)
        // no, it did not. not EOF yet.
        return 0;

    struct rejit_thread_t *t = rejit_thread_fork(r);

    if (t == NULL)
        // visiting other paths will not help us now.
        return 1;

    rejit_list_init(&t->queue);
    t->groups[1] = r->offset;

    while (t->next != rejit_list_end(&r->all_threads)) {
        struct rejit_thread_t *q = t->next;
        // can safely fail all less important threads. If they fail, this one
        // has matched, so whatever. if they match, this one contains better results.
        rejit_list_remove(q);
        rejit_list_remove(&q->queue);
        q->next = r->free;
        r->free = q;
    }

    return 1;
}


int rejit_thread_wait(struct rejit_threadset_t *r, const void *state, size_t shift)
{
    struct rejit_thread_t *t = rejit_thread_fork(r);
    if (t == NULL) return 1;
    t->state      = state;
    t->queue.wait = shift - 1;
    rejit_list_append(r->queues[!r->queue].last, &t->queue);
    return 0;
}


struct _bitmap
{
    unsigned old_id;
    uint8_t *old_map;
    uint8_t  bitmap[0];
};


int rejit_thread_bitmap_save(struct rejit_threadset_t *r)
{
    struct _bitmap *s = (struct _bitmap *) malloc(sizeof(struct _bitmap) + r->space);

    if (s == NULL) {
        rejit_thread_free_lists(r);
        return 0;
    }

    memset(s->bitmap, 0, r->space);
    s->old_id  = r->running->bitmap_id;
    s->old_map = r->bitmap;
    r->bitmap  = s->bitmap;
    r->running->bitmap_id = ++r->bitmap_id_last;
    return 1;
}


void rejit_thread_bitmap_restore(struct rejit_threadset_t *r)
{
    struct _bitmap *s = (struct _bitmap *) (r->bitmap - offsetof(struct _bitmap, bitmap));
    r->bitmap = s->old_map;
    r->running->bitmap_id = s->old_id;
    free(s);
}
