#define JITIFY_INTERNAL
#include "jitify_apache_glue.h"

static void *apache_malloc_wrapper(jitify_pool_t *pool, size_t len)
{
  return apr_palloc(pool->state, len);
}

static void *apache_calloc_wrapper(jitify_pool_t *pool, size_t len)
{
  return apr_pcalloc(pool->state, len);
}

static void apache_free_wrapper(jitify_pool_t *pool, void *block)
{
}

jitify_pool_t *jitify_apache_pool_create(apr_pool_t *pool)
{
  jitify_pool_t *jpool = apr_pcalloc(pool, sizeof(*jpool));
  jpool->state = pool;
  jpool->malloc = apache_malloc_wrapper;
  jpool->calloc = apache_calloc_wrapper;
  jpool->free = apache_free_wrapper;
  return jpool;
}

#define JITIFY_OUTPUT_BUF_SIZE 8000

static int apache_brigade_write(jitify_output_stream_t *stream, const void *data, size_t len)
{
  apr_bucket_brigade *bb = stream->state;
  /* Case 1 of 3: the target brigade ends with a heap
   * buffer that has enough space left over to hold
   * this write
   */
  if (!APR_BRIGADE_EMPTY(bb)) {
    apr_bucket *b;

    b = APR_BRIGADE_LAST(bb);
    if (APR_BUCKET_IS_HEAP(b)) {
      apr_bucket_heap *h;
      size_t avail;
      
      h = b->data;
      avail = h->alloc_len - (b->start + b->length);
      if (avail >= len) {
        memcpy(h->base + b->start + b->length, data, len);
        b->length += len;
        return len;
      }
    }
  }
  
  /* Case 2 of 3: this write is smaller than the normal
   * buffer size, so we allocate a full-sized buffer in
   * anticipation of more small writes to follow
   */
  if (len <= JITIFY_OUTPUT_BUF_SIZE) {
    apr_bucket *b;
    char *heap_buf;
    
    heap_buf = apr_bucket_alloc(JITIFY_OUTPUT_BUF_SIZE, bb->bucket_alloc);
    b = apr_bucket_heap_create(heap_buf, JITIFY_OUTPUT_BUF_SIZE, apr_bucket_free,
      bb->bucket_alloc);
    memcpy(heap_buf, data, len);
    b->length = len;
    APR_BRIGADE_INSERT_TAIL(bb, b);
  }
  
  /* Case 3 of 3: this write is larger than the normal
   * buffer size, so we wrap it in a bucket as-is
   */
  else {
    apr_bucket *b;
    
    b = apr_bucket_heap_create(data, len, NULL, bb->bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(bb, b);
  }
  return len;
}

jitify_output_stream_t *jitify_apache_output_stream_create(jitify_pool_t *pool)
{
  jitify_output_stream_t *stream = jitify_calloc(pool, sizeof(*stream));
  stream->pool = pool;
  stream->write = apache_brigade_write;
  return stream;
}
