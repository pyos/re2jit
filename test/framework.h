#include <time.h>


#define MEASURE_START(__n)                  \
  struct timespec __start;                  \
  struct timespec __finish;                 \
  clock_gettime(CLOCK_MONOTONIC, &__start); \
  for (int __i = 0; __i < __n; __i++)


#define MEASURE_FINISH do {                                  \
  clock_gettime(CLOCK_MONOTONIC, &__finish);                 \
  time_t __secdiff = __finish.tv_sec  - __start.tv_sec;      \
  long   __nscdiff = __finish.tv_nsec - __start.tv_nsec;     \
  printf("=> %.9f s", __secdiff * 1.0 + __nscdiff * 1.0e-9); \
} while (0)


#define MEASURE(__n, __xs) do { MEASURE_START(__n) { __xs; } MEASURE_FINISH; } while (0)
