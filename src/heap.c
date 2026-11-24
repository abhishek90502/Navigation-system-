#include <stdlib.h>
#include "heap.h"

static bool less(HeapItem a, HeapItem b) {
    return a.key < b.key || (a.key == b.key && a.seq < b.seq);
}

static void swap_items(HeapItem *a, HeapItem *b) {
    HeapItem t = *a;
    *a = *b;
    *b = t;
}

void heap_init(MinHeap *h) {
    h->items = NULL;
    h->size = 0;
    h->cap = 0;
}

void heap_free(MinHeap *h) {
    free(h->items);
    heap_init(h);
}

bool heap_empty(const MinHeap *h) {
    return h->size == 0;
}

void heap_push(MinHeap *h, double key, long seq, int value) {
    if (h->size == h->cap) {
        h->cap = h->cap ? h->cap * 2 : 16;
        h->items = realloc(h->items, (size_t)h->cap * sizeof *h->items);
    }
    int i = h->size++;
    h->items[i] = (HeapItem){key, seq, value};
    while (i > 0) {                       /* sift up */
        int parent = (i - 1) / 2;
        if (!less(h->items[i], h->items[parent])) break;
        swap_items(&h->items[i], &h->items[parent]);
        i = parent;
    }
}

HeapItem heap_pop(MinHeap *h) {
    HeapItem top = h->items[0];
    h->items[0] = h->items[--h->size];
    int i = 0;
    for (;;) {                            /* sift down */
        int l = 2 * i + 1, r = l + 1, s = i;
        if (l < h->size && less(h->items[l], h->items[s])) s = l;
        if (r < h->size && less(h->items[r], h->items[s])) s = r;
        if (s == i) break;
        swap_items(&h->items[i], &h->items[s]);
        i = s;
    }
    return top;
}
