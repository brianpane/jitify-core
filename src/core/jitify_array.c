#include <string.h>
#define JITIFY_INTERNAL
#include "jitify.h"

struct jitify_array_s {
  jitify_pool_t *pool;
  size_t len;
  size_t size;
  size_t element_size;
  char *data;
};

#define INITIAL_ARRAY_SIZE 2

jitify_array_t *jitify_array_create(jitify_pool_t *pool, size_t element_size)
{
  jitify_array_t *array = jitify_malloc(pool, sizeof(*array));
  array->pool = pool;
  array->len = 0;
  array->size = INITIAL_ARRAY_SIZE;
  array->element_size = element_size;
  array->data = jitify_malloc(pool, array->size * element_size);
  return array;
}

size_t jitify_array_length(const jitify_array_t *array)
{
  return array->len;
}

void *jitify_array_get(jitify_array_t *array, size_t index)
{
  if (index >= array->len) {
    return NULL;
  }
  else {
    return array->data + index * array->element_size;
  }
}

void *jitify_array_push(jitify_array_t *array)
{
  if (array->len == array->size) {
    char *new_data;
    array->size *= 2;
    new_data = jitify_malloc(array->pool, array->size * array->element_size);
    memcpy(new_data, array->data, array->len * array->element_size);
    jitify_free(array->pool, array->data);
    array->data = new_data;
  }
  memset(array->data + array->len * array->element_size, 0, array->element_size);
  return array->data + array->len++ * array->element_size;
}

void jitify_array_clear(jitify_array_t *array)
{
  array->len = 0;
}

void jitify_array_destroy(jitify_array_t *array)
{
  jitify_free(array->pool, array->data);
  jitify_free(array->pool, array);
}
