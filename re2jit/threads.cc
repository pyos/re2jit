#include <stdlib.h>
#include <string.h>

#include <re2jit/threads.h>


static rejit_thread_t *rejit_thread_new(rejit_threadset_t *r)
{
    rejit_thread_t *t;

    if (r->free) {
        // Reuse dead threads to cut on malloc-related costs.
        t = r->free;
        r->free = t->next;
    } else {
        t = (rejit_thread_t *) malloc(sizeof(rejit_thread_t) + sizeof(int) * r->ngroups);

        if (t == NULL) {
            return NULL;
        }

        memset(t->groups, 255, sizeof(int) * r->ngroups);
        t->category.ref = t;
        rejit_list_init(&t->category);
    }

    rejit_list_init(t);
    return t;
}


static rejit_thread_t *rejit_thread_entry(rejit_threadset_t *r)
{
    rejit_thread_t *t = rejit_thread_new(r);

    if (t == NULL) {
        return NULL;
    }

    t->entry = r->entry;
    rejit_list_append(r->all_threads.last, t);
    rejit_list_append(r->queues[r->active_queue].last, &t->category);
    return t;
}


rejit_threadset_t *rejit_thread_init(const char *input, size_t length, void *entry, int flags, int ngroups)
{
    rejit_threadset_t *r = (rejit_threadset_t *) calloc(sizeof(rejit_threadset_t), 1);

    if (r == NULL) {
        return NULL;
    }

    rejit_list_init(&r->all_threads);

    size_t i;

    for (i = 0; i <= RE2JIT_THREAD_LOOKAHEAD; i++) {
        rejit_list_init(&r->queues[i]);
    }

    r->entry = entry;
    r->flags = flags;
    r->input = input;
    r->length = length;
    r->ngroups = ngroups;

    if (rejit_thread_entry(r) == NULL) {
        free(r);
        return NULL;
    }

    return r;
}


void rejit_thread_free(rejit_threadset_t *r)
{
    rejit_thread_t *a, *b;

    #define FREE_LIST(init, end) do { \
        for (a = init; a != end; ) {  \
            b = a;                    \
            a = a->next;              \
            free(b);                  \
        }                             \
    } while (0)

    FREE_LIST(r->free, NULL);
    FREE_LIST(r->all_threads.first, rejit_list_end(&r->all_threads));
    free(r);

    #undef FREE_LIST
}


int rejit_thread_dispatch(rejit_threadset_t *r, int max_steps)
{
    size_t queue = r->active_queue;

    do {
        struct st_rejit_thread_ref_t *t = r->queues[queue].first;

        while (t != rejit_list_end(&r->queues[queue])) {
            r->running = t->ref;

            // TODO something with `t->ref->entry`.
            // NOTE thread should never run `dispatch`! It should return here.

            if (!--max_steps) {
                r->active_queue = queue;
                return 1;
            }

            t = r->queues[queue].first;
        }

        queue = (queue + 1) % (RE2JIT_THREAD_LOOKAHEAD + 1);

        if (!(r->flags & RE2JIT_ANCHOR_START) && r->length) {
            if (rejit_thread_entry(r) == NULL) {
                // XOO < *ac was completely screwed out of memory
                //        and nothing can fix that!!*
            }
        }
    } while (r->input++, r->length--);

    // Doesn't matter which queue is active now...
    return 0;
}


void rejit_thread_fork(rejit_threadset_t *r, void *entry)
{
    rejit_thread_t *t = rejit_thread_new(r);
    rejit_thread_t *p = r->running;

    if (t == NULL) {
        // :33 < oh shit
        return;
    }

    memcpy(t->groups, p->groups, sizeof(int) * r->ngroups);
    t->entry = entry;

    rejit_list_append(p, t);
    rejit_list_append(&p->category, &t->category);
}


void rejit_thread_match(rejit_threadset_t *r)
{
    rejit_thread_t *p = r->running;

    if ((r->flags & RE2JIT_ANCHOR_END) && r->length) {
        // No, it did not. Not EOF yet.
        rejit_thread_fail(r);
        return;
    }

    while (p->next != rejit_list_end(&r->all_threads)) {
        // Can safely fail all less important threads. If they fail, this one
        // has matched, so whatever. If they match, this one contains better results.
        r->running = p->next;
        rejit_thread_fail(r);
    }

    rejit_list_remove(&p->category);
}


void rejit_thread_fail(rejit_threadset_t *r)
{
    rejit_thread_t *p = r->running;
    rejit_list_remove(p);
    rejit_list_remove(&p->category);
    p->next = r->free;
    r->free = p;
}


void rejit_thread_wait(rejit_threadset_t *r, size_t shift)
{
    size_t queue = (r->active_queue + shift) % (RE2JIT_THREAD_LOOKAHEAD + 1);
    rejit_thread_t *p = r->running;
    rejit_list_remove(&p->category);
    rejit_list_append(r->queues[queue].last, &p->category);
}


int rejit_thread_result(rejit_threadset_t *r, int **groups)
{
    if (r->all_threads.first == rejit_list_end(&r->all_threads)) {
        return 0;
    }

    *groups = r->all_threads.first->groups;
    return 1;
}
