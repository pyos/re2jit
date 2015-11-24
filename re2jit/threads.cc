#include <stdlib.h>
#include <string.h>

#include "threads.h"


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
    FREE_LIST(r->threads.first, rejit_list_end(&r->threads));
    #undef FREE_LIST

    rejit_list_init(&r->threads);
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
        return t;
    }

    t = (struct rejit_thread_t *) malloc(sizeof(struct rejit_thread_t)
                                       + sizeof(int) * r->groups);

    if (t == NULL)
        rejit_thread_free(r);

    return t;
}


static struct rejit_thread_t *rejit_thread_fork(struct rejit_threadset_t *r)
{
    struct rejit_thread_t *t = rejit_thread_acquire(r);
    struct rejit_thread_t *c = r->running;
    if (t == NULL) return NULL;

    rejit_list_append(c->prev, t);
    memcpy(t->groups, c->groups, sizeof(int) * r->groups);
    t->queue.bitmap = c->queue.bitmap;
    return c->prev = t;
}


static struct rejit_thread_t *rejit_thread_initial(struct rejit_threadset_t *r)
{
    struct rejit_thread_t *t = rejit_thread_acquire(r);
    if (t == NULL) return NULL;

    memset(t->groups, 255, sizeof(int) * r->groups);
    t->queue.wait   = 0;
    t->queue.bitmap = 0;
    t->groups[0]    = r->offset;
    t->state        = r->initial;
    rejit_list_append(r->threads.last, t);
    rejit_list_append(r->queues[r->queue].last, &t->queue);
    return t;
}


int rejit_thread_dispatch(struct rejit_threadset_t *r, int **groups)
{
    unsigned char queue = 0;
    unsigned char small_map = r->space <= sizeof(size_t);
    size_t __bitmap;

    r->bitmap_id_last = 0;
    r->offset         = 0;
    r->queue          = 0;
    r->free           = NULL;
    rejit_list_init(&r->threads);
    rejit_list_init(&r->queues[0]);
    rejit_list_init(&r->queues[1]);

    if (small_map)
        r->bitmap = (uint8_t *) &__bitmap;
    else if ((r->bitmap = (uint8_t *) malloc(r->space)) == NULL)
        return -1;

    do {
        // is this is volatile, gcc generates better code for some reason.
        volatile unsigned bitmap_id = -1;

        if (!((r->flags & RE2JIT_ANCHOR_START) && r->offset))
            rejit_thread_initial(r);

        struct rejit_thread_t  *t;
        struct rejit_threadq_t *q = r->queues[queue].first;

        if (q == rejit_list_end(&r->queues[queue]))
            // if this queue is empty, the next will be too, and the one after that...
            break;

        do {
            rejit_list_remove(q);

            if (q->wait) {
                q->wait--;
                rejit_list_append(r->queues[!queue].last, q);
                continue;
            }

            if (bitmap_id != q->bitmap) {
                bitmap_id  = q->bitmap;
                if (small_map)
                    __bitmap = 0;
                else
                    memset(r->bitmap, 0, r->space);
            }

            rejit_list_remove(t = rejit_list_container(struct rejit_thread_t, queue, q));
            r->running = t;
            r->entry(r, t->state);
            t->next = r->free;
            r->free = t;
        } while ((q = r->queues[queue].first) != rejit_list_end(&r->queues[queue]));

        r->input++;
        r->offset++;
        r->queue = queue = !queue;
    } while (r->length--);

    if (!small_map)
        free(r->bitmap);

    if (r->flags & RE2JIT_UNDEFINED)
        // XOO < *ac was completely screwed out of memory
        //        and nothing can fix that!!*
        return -1;

    if (r->threads.first == rejit_list_end(&r->threads))
        return 0;

    *groups = r->threads.first->groups;
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

    while (t->next != rejit_list_end(&r->threads)) {
        struct rejit_thread_t *q = t->next;
        // it doesn't matter what less important threads return, so why run them?
        rejit_list_remove(q);
        rejit_list_remove(&q->queue);
        q->next = r->free;
        r->free = q;
    }

    // don't spawn new threads in the initial state for the same reason.
    r->flags |= RE2JIT_ANCHOR_START;
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


int rejit_thread_satisfies(struct rejit_threadset_t *r, enum RE2JIT_EMPTY_FLAGS empty)
{
    if (empty & RE2JIT_EMPTY_BEGIN_TEXT)
        if (r->offset)
            return 0;
    if (empty & RE2JIT_EMPTY_END_TEXT)
        if (r->length)
            return 0;
    if (empty & RE2JIT_EMPTY_BEGIN_LINE)
        if (r->offset && r->input[-1] != '\n')
            return 0;
    if (empty & RE2JIT_EMPTY_END_LINE)
        if (r->length && r->input[0] != '\n')
            return 0;
    if (empty & (RE2JIT_EMPTY_WORD_BOUNDARY | RE2JIT_EMPTY_NON_WORD_BOUNDARY))
        return 0;  // TODO read UTF-8 chars or something
    return 1;
}


struct _bitmap
{
    uint8_t *old_map;
    unsigned old_id;
    uint8_t  bitmap[];
};


void rejit_thread_bitmap_save(struct rejit_threadset_t *r)
{
    struct _bitmap *s = (struct _bitmap *) malloc(sizeof(struct _bitmap) + r->space);

    if (s == NULL) {
        rejit_thread_free(r);
        return;
    }

    memset(s->bitmap, 0, r->space);
    s->old_id  = r->running->queue.bitmap;
    s->old_map = r->bitmap;
    r->bitmap  = s->bitmap;
    r->running->queue.bitmap = ++r->bitmap_id_last;
}


void rejit_thread_bitmap_restore(struct rejit_threadset_t *r)
{
    struct _bitmap *s = (struct _bitmap *) (r->bitmap - offsetof(struct _bitmap, bitmap));
    r->bitmap = s->old_map;
    r->running->queue.bitmap = s->old_id;
    free(s);
}
