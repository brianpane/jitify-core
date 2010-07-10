#define JITIFY_INTERNAL
#include "jitify_nginx_glue.h"

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

static ngx_http_output_header_filter_pt jitify_next_header_filter;
static ngx_http_output_body_filter_pt   jitify_next_body_filter;

ngx_module_t jitify_module;

typedef struct {
  ngx_flag_t minify;
} jitify_conf_t;

typedef struct {
  jitify_pool_t *pool;
  jitify_lexer_t *lexer;
  jitify_output_stream_t *out;
} jitify_filter_ctx_t;

static ngx_int_t jitify_header_filter(ngx_http_request_t *r)
{
  ngx_log_t *log = r->connection->log;
  jitify_conf_t *jconf;
  if (r != r->main) {
    return jitify_next_header_filter(r);
  }
  jconf = ngx_http_get_module_loc_conf(r, jitify_module);
  if (!jconf) {
    ngx_log_error(NGX_LOG_WARN, log, 0, "internal error: mod_jitify configuration missing");
    return jitify_next_header_filter(r);
  }
  if (jconf->minify) {
    jitify_filter_ctx_t *jctx = ngx_pcalloc(r->pool, sizeof(*jctx));
    jctx->pool = jitify_nginx_pool_create(r->pool);
    if (r->headers_out.content_type.data) {
      jitify_output_stream_t *out = jitify_nginx_output_stream_create(jctx->pool);
      jctx->lexer = jitify_lexer_for_content_type(jitify_nginx_strdup(jctx->pool, &(r->headers_out.content_type)),
        jctx->pool, out);
      jctx->out = out;
    }
  
    if (jctx->lexer) {
      /* Clear the response headers that might be invalidated
         when the response body is modified */
      ngx_log_error(NGX_LOG_DEBUG, log, 0, "enabling content scanning for uri=%V content-type=%V",
                    &(r->uri), &(r->headers_out.content_type));
      ngx_http_clear_content_length(r);
      ngx_http_clear_last_modified(r);
      ngx_http_clear_accept_ranges(r);
      
      jitify_lexer_set_minify_rules(jctx->lexer, jconf->minify, jconf->minify);
      ngx_http_set_ctx(r, jctx, jitify_module);
      
      r->main_filter_need_in_memory = 1;
    }
    else {
      ngx_log_error(NGX_LOG_DEBUG, log, 0, "no lexer for uri=%V content-type=%V",
                    &(r->uri), &(r->headers_out.content_type));
    }
  }
  return jitify_next_header_filter(r);
}

#define DEFAULT_ERR_LEN 80

static ngx_int_t jitify_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
  ngx_log_t *log = r->connection->log;
  jitify_filter_ctx_t *jctx;
  jitify_nginx_chain_t out;
  int send_flush, send_eof;
  jctx = ngx_http_get_module_ctx(r->main, jitify_module);
  if (!jctx || !jctx->lexer) {
    ngx_log_error(NGX_LOG_DEBUG, log, 0, "jitify filter skipping request for uri=%V",
                  &(r->uri));
    return jitify_next_body_filter(r, in);
  }
  out.first = out.last = NULL;
  out.pool = r->pool;
  jctx->out->state = &out;
  send_flush = send_eof = 0;

  while (in) {
    ngx_buf_t *buf = in->buf;
    if (buf->last_buf) {
      send_eof = 1;
    }
    if (buf->last_buf || (buf->last > buf->pos)) {
      const char *err;
      jitify_lexer_scan(jctx->lexer, buf->pos, buf->last - buf->pos, buf->last_buf);
      err = jitify_lexer_get_err(jctx->lexer);
      if (err) {
        char err_buf[DEFAULT_ERR_LEN + 1];
        size_t err_len = DEFAULT_ERR_LEN;
        size_t max_err_len = (const char*)(buf->last) - err;
        if (err_len > max_err_len) {
          err_len = max_err_len;
        }
        memcpy(err_buf, err, err_len);
        err_buf[err_len] = 0;
        ngx_log_error(NGX_LOG_WARN, log, 0, "parse error in %V near '%s', entering failsafe mode", &(r->uri), err_buf);
      }
    }
    if (buf->flush) {
      send_flush = 1;
    }

    /* Setting buf->pos=buf->last enables the nginx core to recycle this buffer */
    if (buf->pos < buf->last) {
      buf->pos = buf->last;
    }
    in = in->next;
  }

  if (send_eof) {
    size_t processing_time_in_usec = jitify_lexer_get_processing_time(jctx->lexer);
    size_t bytes_in = jitify_lexer_get_bytes_in(jctx->lexer);
    size_t bytes_out = jitify_lexer_get_bytes_out(jctx->lexer);
    ngx_log_error(NGX_LOG_INFO, log, 0, "Jitify stats: bytes_in=%l bytes_out=%l, nsec/byte=%l for %V",
        (long)bytes_in, (long)bytes_out,
        (long)(bytes_in ? processing_time_in_usec * 1000 / bytes_in : 0),
        &(r->uri));
    jitify_nginx_add_eof(&out);
  }
  if (send_flush && out.last) {
    out.last->buf->flush = 1;
  }
  if (out.first) {
    return jitify_next_body_filter(r, out.first);
  }
  else {
    return NGX_OK;
  }
}

static ngx_int_t jitify_post_config(ngx_conf_t *cf)
{
  jitify_next_header_filter = ngx_http_top_header_filter;
  ngx_http_top_header_filter = jitify_header_filter;
  jitify_next_body_filter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = jitify_body_filter;
  return NGX_OK;
}

static void *jitify_create_conf(ngx_conf_t *cf)
{
  jitify_conf_t *conf;
  
  conf = ngx_pcalloc(cf->pool, sizeof(*conf));
  if (conf) {
    conf->minify = NGX_CONF_UNSET;
  }
  return conf;
}

static char *jitify_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
  jitify_conf_t *prev = parent;
  jitify_conf_t *conf = child;
  
  ngx_conf_merge_value(conf->minify, prev->minify, 0);
  return NGX_CONF_OK;
}

static ngx_http_module_t jitify_module_ctx = {
  NULL,                     /* pre-config                            */
  jitify_post_config,       /* post-config                           */
  NULL,                     /* create main (top-level) config struct */
  NULL,                     /* init main (top-level) config struct   */
  NULL,                     /* create server-level config struct     */
  NULL,                     /* merge server-level config struct      */
  jitify_create_conf,       /* create location-level config struct   */
  jitify_merge_conf         /* merge location-level config struct    */
};

static ngx_command_t jitify_commands[] = {
  {
    /* minify on|off */
    ngx_string("minify"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(jitify_conf_t, minify),
    NULL
  },
  ngx_null_command
};

ngx_module_t jitify_module = {
  NGX_MODULE_V1,
  &jitify_module_ctx,
  jitify_commands,
  NGX_HTTP_MODULE,
  NULL,                     /* init master     */
  NULL,                     /* init module     */
  NULL,                     /* init process    */
  NULL,                     /* init thread     */
  NULL,                     /* cleanup thread  */
  NULL,                     /* cleanup process */
  NULL,                     /* cleanup master  */
  NGX_MODULE_V1_PADDING
};
