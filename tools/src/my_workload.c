#include <stddef.h>
#include <stdlib.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE 16777216
#endif

#ifndef CACHE_LINESIZE
#define CACHE_LINESIZE 64
#endif

#ifndef NTIMES
#define NTIMES 128
#endif

#define ARRAY_ITEM_TYPE double

void malloc_array(ARRAY_ITEM_TYPE **a, ARRAY_ITEM_TYPE **b,
                  size_t array_size) {
  *a = (ARRAY_ITEM_TYPE *)malloc(sizeof(ARRAY_ITEM_TYPE) * array_size);
  *b = (ARRAY_ITEM_TYPE *)malloc(sizeof(ARRAY_ITEM_TYPE) * array_size);
}

void free_array(ARRAY_ITEM_TYPE **a, ARRAY_ITEM_TYPE **b) {
  free(*a);
  free(*b);
}

void copy_array(ARRAY_ITEM_TYPE *a, ARRAY_ITEM_TYPE *b, size_t array_size,
                size_t item_per_line) {
  for (uint offset = 0; offset < item_per_line; ++offset) {
#pragma omp parallel for
    for (uint i = offset; i < ARRAY_SIZE; i += item_per_line) {
      a[i] = b[i];
    }
  }
}

int main() {
  ARRAY_ITEM_TYPE *a, *b;
  int item_per_line = CACHE_LINESIZE / sizeof(ARRAY_ITEM_TYPE);
  malloc_array(&a, &b, ARRAY_SIZE);
  for (uint time = 0; time < NTIMES; ++time) {
    copy_array(a, b, ARRAY_SIZE, item_per_line);
  }
  free_array(&a, &b);
}
