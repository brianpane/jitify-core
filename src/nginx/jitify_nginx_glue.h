#ifndef jitify_nginx_glue_h
#define jitify_nginx_glue_h

#include "jitify.h"

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

extern jitify_pool_t *jitify_nginx_pool_create(ngx_pool_t *pool);

extern jitify_output_stream_t *jitify_nginx_output_stream_create(jitify_pool_t *pool);

extern jitify_output_stream_t *jitify_nginx_output_stream_create(jitify_pool_t *pool);

/* Utility function to allocate, from a pool, a null-terminated copy of one of nginx's pointer/length strings */
extern char *jitify_nginx_strdup(jitify_pool_t *pool, ngx_str_t *str);

/* Wrapper for an expandable list of buffers */
typedef struct {
  ngx_chain_t      *first;
  ngx_chain_t      *last;
  ngx_pool_t       *pool;
} jitify_nginx_chain_t;

extern void jitify_nginx_add_eof(jitify_nginx_chain_t *chain);

#endif /* !defined(jitify_nginx_glue_h) */
