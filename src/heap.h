/* Array-backed binary min-heap (hand-written, no libraries). */
#ifndef HEAP_H
#define HEAP_H

#include <stdbool.h>

typedef struct {
    double key;   /* priority: smaller pops first          */
    long   seq;   /* tie-breaker: earlier insertion first  */
    int    value; /* payload (node index or candidate index) */
} HeapItem;

typedef struct {
    HeapItem *items;
    int size;
    int cap;
} MinHeap;

void     heap_init(MinHeap *h);
void     heap_free(MinHeap *h);
bool     heap_empty(const MinHeap *h);
void     heap_push(MinHeap *h, double key, long seq, int value);
HeapItem heap_pop(MinHeap *h);   /* caller must ensure the heap is non-empty */

#endif
