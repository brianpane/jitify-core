#define JITIFY_INTERNAL
#include "jitify.h"

void *jitify_malloc(jitify_pool_t *pool, size_t length)
{
  return pool->malloc(pool, length);
}

void *jitify_calloc(jitify_pool_t *pool, size_t length)
{
  return pool->calloc(pool, length);
}

void jitify_free(jitify_pool_t *pool, void *object)
{
  pool->free(pool, object);
}

void jitify_pool_destroy(jitify_pool_t *pool)
{
  if (pool && pool->cleanup) {
    pool->cleanup(pool);
  }
}

static void *malloc_wrapper(jitify_pool_t *pool, size_t length)
{
  return malloc(length);
}

static void *calloc_wrapper(jitify_pool_t *pool, size_t length)
{
  return calloc(1, length);
}

static void free_wrapper(jitify_pool_t *pool, void *object)
{
  free(object);
}

static void malloc_pool_cleanup(jitify_pool_t *pool)
{
  free(pool);
}

jitify_pool_t *jitify_malloc_pool_create()
{
  jitify_pool_t *p = malloc(sizeof(*p));
  if (p) {
    p->state = NULL;
    p->malloc = malloc_wrapper;
    p->calloc = calloc_wrapper;
    p->free = free_wrapper;
    p->cleanup = malloc_pool_cleanup;
  }
  return p;
}
