#define JITIFY_INTERNAL
#include "jitify_nginx_glue.h"

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

extern ngx_module_t jitify_module;

static void *nginx_malloc_wrapper(jitify_pool_t *pool, size_t len)
{
  return ngx_palloc(pool->state, len);
}

static void *nginx_calloc_wrapper(jitify_pool_t *pool, size_t len)
{
  return ngx_pcalloc(pool->state, len);
}

static void nginx_free_wrapper(jitify_pool_t *pool, void *block)
{
}

jitify_pool_t *jitify_nginx_pool_create(ngx_pool_t *pool)
{
  jitify_pool_t *jpool = ngx_pcalloc(pool, sizeof(*jpool));
  jpool->state = pool;
  jpool->malloc = nginx_malloc_wrapper;
  jpool->calloc = nginx_calloc_wrapper;
  jpool->free = nginx_free_wrapper;
  return jpool;
}

static int nginx_buf_write(jitify_output_stream_t *stream, const void *data, size_t len)
{
  jitify_nginx_chain_t *chain = stream->state;
  const char *cdata = data;
  size_t bytes_remaining;
  if (!data) {
    return 0;
  }
  bytes_remaining = len;
  while (bytes_remaining) {
    ngx_chain_t *link;
    size_t write_size;
    link = chain->last;
    if (!link || !link->buf->temporary || (link->buf->last == link->buf->end)) {
      ngx_buf_t *buf = ngx_create_temp_buf(chain->pool, ngx_pagesize);
      link = ngx_alloc_chain_link(chain->pool);
      link->next = NULL;
      link->buf = buf;
      if (chain->last) {
        chain->last->next = link;
      }
      else {
        chain->first = link;
      }
      chain->last = link;
    }
    write_size = link->buf->end - link->buf->last;
    if (write_size > bytes_remaining) {
      write_size = bytes_remaining;
    }
    memcpy(link->buf->last, cdata, write_size);
    cdata += write_size;
    link->buf->last += write_size;
    bytes_remaining -= write_size;
  }
  return len;
}

jitify_output_stream_t *jitify_nginx_output_stream_create(jitify_pool_t *pool)
{
  jitify_output_stream_t *stream = jitify_calloc(pool, sizeof(*stream));
  stream->pool = pool;
  stream->write = nginx_buf_write;
  return stream;
}

char *jitify_nginx_strdup(jitify_pool_t *pool, ngx_str_t *str)
{
  size_t len;
  char *buf;
  if (!str || !str->data) {
    return NULL;
  }
  len = str->len;
  buf = jitify_malloc(pool, len + 1);
  memcpy(buf, str->data, len);
  buf[len] = 0;
  return buf;
}

void jitify_nginx_add_eof(jitify_nginx_chain_t *chain)
{
  ngx_chain_t *link = ngx_pcalloc(chain->pool, sizeof(*chain));
  ngx_buf_t *buf = ngx_calloc_buf(chain->pool);
  buf->last_buf = 1;
  link->buf = buf;
  if (chain->last) {
    chain->last->next = link;
  }
  else {
    chain->first = link;
  }
  chain->last = link;
}
