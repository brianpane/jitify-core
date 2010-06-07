#include <ap_config.h>
#include <httpd.h>
#include <http_config.h>
#include <http_log.h>
#include <http_request.h>
#include <apr_strings.h>

#define JITIFY_INTERNAL
#include "jitify_apache_glue.h"

module AP_MODULE_DECLARE_DATA jitify_module;

#define JITIFY_FILTER_KEY "JITIFY"

#define JITIFY_NOTE_KEY "jitify_ctx"

typedef struct {
  int minify; /* 0 for false, >0 for true, <0 for unset */
} jitify_dir_conf_t;

typedef struct {
  const char *orig_accept_encoding;
} jitify_request_ctx_t;

typedef struct {
  bool response_started;
  jitify_pool_t *pool;
  jitify_lexer_t *lexer;
  jitify_output_stream_t *out;
} jitify_filter_ctx_t;

static jitify_filter_ctx_t *jitify_filter_init(ap_filter_t *f)
{
  jitify_pool_t *pool = jitify_apache_pool_create(f->r->pool);
  jitify_filter_ctx_t *ctx = jitify_calloc(pool, sizeof(*ctx));
  jitify_dir_conf_t *jconf = ap_get_module_config(f->r->per_dir_config, &jitify_module);
  jitify_request_ctx_t *jctx = (jitify_request_ctx_t *)apr_table_get(f->r->notes, JITIFY_NOTE_KEY);
  ctx->pool = pool;
  if (jconf->minify > 0) {
    jitify_output_stream_t *out = jitify_apache_output_stream_create(pool);
    ctx->lexer = jitify_lexer_for_content_type(f->r->content_type, pool, out);
    if (ctx->lexer) {
      ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r, "found lexer for content-type %s for %s", f->r->content_type, f->r->uri);
      jitify_lexer_set_minify_rules(ctx->lexer, true, true);
    }
    else {
      ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r, "no lexer for content-type %s for %s", f->r->content_type, f->r->uri);
    }
    ctx->out = out;
  }
  else {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r, "no transforms enabled for %s, skipping lexer", f->r->uri);
  }
  if (jctx && jctx->orig_accept_encoding) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r, "Restoring Accept-Encoding");
    apr_table_setn(f->r->headers_in, "Accept-Encoding", jctx->orig_accept_encoding);
  }
  return ctx;
}

#define DEFAULT_ERR_LEN 80

static apr_status_t jitify_filter(ap_filter_t *f, apr_bucket_brigade *bb)
{
  apr_status_t rv;
  jitify_filter_ctx_t *ctx;
  apr_bucket_brigade *out;
  ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r, "in jitify filter for %s", f->r->uri);
  if (f->ctx == NULL) {
    f->ctx = jitify_filter_init(f);
  }
  ctx = f->ctx;
  if (!ctx || !ctx->lexer) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r, "no lexer for %s, skipping", f->r->uri);
    return ap_pass_brigade(f->next, bb);
  }
  
  if (!ctx->response_started) {
    apr_table_unset(f->r->headers_out, "Content-Length");
    apr_table_unset(f->r->headers_out, "Last-modified");
    apr_table_unset(f->r->headers_out, "Accept-Ranges");
    ctx->response_started = true;
  }
  out = apr_brigade_create(f->r->pool, f->c->bucket_alloc);
  ctx->out->state = out;
  rv = APR_SUCCESS;
  while (!APR_BRIGADE_EMPTY(bb)) {
    apr_bucket *b;
    const char *data;
    apr_size_t len;
    b = APR_BRIGADE_FIRST(bb);
    APR_BUCKET_REMOVE(b);
    rv = apr_bucket_read(b, &data, &len, APR_BLOCK_READ);
    if (rv != APR_SUCCESS) {
      break;
    }
    if (len > 0) {
      const char *err;
      ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r, "scanning %d bytes of %s", (int)len, f->r->uri);
      jitify_lexer_scan(ctx->lexer, data, len, false);
      err = jitify_lexer_get_err(ctx->lexer);
      if (err) {
        char err_buf[DEFAULT_ERR_LEN + 1];
        size_t err_len = DEFAULT_ERR_LEN;
        size_t max_err_len = (data + len) - err;
        if (err_len > max_err_len) {
          err_len = max_err_len;
        }
        memcpy(err_buf, err, err_len);
        err_buf[err_len] = 0;
        ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, f->r, "parse error in %s near '%s', entering failsafe mode", f->r->uri, err_buf);
      }
    }
    if (APR_BUCKET_IS_EOS(b)) {
      size_t processing_time_in_usec = jitify_lexer_get_processing_time(ctx->lexer);
      size_t bytes_in = jitify_lexer_get_bytes_in(ctx->lexer);
      size_t bytes_out = jitify_lexer_get_bytes_out(ctx->lexer);
      ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, f->r, "Jitify stats: bytes_in=%lu bytes_out=%lu, nsec/byte=%lu for %s",
        bytes_in, bytes_out, bytes_in ? processing_time_in_usec * 1000 / bytes_in : 0, f->r->uri);
      jitify_lexer_scan(ctx->lexer, NULL, 0, true);
      APR_BRIGADE_INSERT_TAIL(out, b);
    }
    else {
      apr_bucket_destroy(b);
    }
  }
  ctx->out->state = NULL;
  if (APR_BRIGADE_EMPTY(out)) {
    return APR_SUCCESS;
  }
  else {
    return ap_pass_brigade(f->next, out);
  }
}

static int jitify_translate_name(request_rec *r)
{
  jitify_dir_conf_t *jconf;
  ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "In jitify_translate_name for %s", r->uri);
  jconf = ap_get_module_config(r->per_dir_config, &jitify_module);
  if (jconf && (jconf->minify > 0)) {
    jitify_request_ctx_t *jctx;
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "Preparing request for Jitify processing");
    jctx = (jitify_request_ctx_t *)apr_table_get(r->notes, JITIFY_NOTE_KEY);
    if (!jctx) {
      jctx = apr_pcalloc(r->pool, sizeof(*jctx));
      apr_table_setn(r->notes, JITIFY_NOTE_KEY, (void *)jctx);
    }
    jctx->orig_accept_encoding = apr_table_get(r->headers_in, "Accept-Encoding");
    if (jctx->orig_accept_encoding) {
      ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "Hiding Accept-Encoding until output filter");
      apr_table_unset(r->headers_in, "Accept-Encoding");
    }
  }
  return DECLINED;
}

static int jitify_fixup(request_rec *r)
{
  ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "in jitify fixup for %s", r->uri);
  ap_add_output_filter(JITIFY_FILTER_KEY, NULL, r, r->connection);
  return DECLINED;
}

static const command_rec jitify_cmds[] =
{
  AP_INIT_FLAG("Minify", ap_set_flag_slot, APR_OFFSETOF(jitify_dir_conf_t, minify),
               RSRC_CONF|ACCESS_CONF, "Enable dynamic content minification"),
/*
  AP_SOMETHING("CDNify", something, something,
               RSRC_CONF|ACCESS_CONF, "Rewrite links to use a new base URL"),
*/
  {NULL}
};

static void register_jitify_hooks(apr_pool_t *p)
{
  ap_hook_translate_name(jitify_translate_name, NULL, NULL, APR_HOOK_REALLY_FIRST);
  ap_hook_fixups(jitify_fixup, NULL, NULL, APR_HOOK_REALLY_FIRST);
  ap_register_output_filter(JITIFY_FILTER_KEY, jitify_filter, NULL, AP_FTYPE_RESOURCE);
}

static void *create_jitify_dir_config(apr_pool_t *pool, char *unused)
{
  jitify_dir_conf_t *conf = apr_pcalloc(pool, sizeof(*conf));
  conf->minify = -1;
  return conf;
}

static void *merge_jitify_dir_config(apr_pool_t *pool, void *base_conf, void *add_conf)
{
  jitify_dir_conf_t *merged = apr_pcalloc(pool, sizeof(*merged));
  jitify_dir_conf_t *base = base_conf;
  jitify_dir_conf_t *add = add_conf;
  if (add->minify < 0) {
    merged->minify = base->minify;
  }
  else {
    merged->minify = add->minify;
  }
  return merged;
}

module AP_MODULE_DECLARE_DATA jitify_module =
{
  STANDARD20_MODULE_STUFF,
  create_jitify_dir_config,     /* create directory-level config */
  merge_jitify_dir_config,      /* merge directory-level config */
  NULL,                         /* create server-level config */
  NULL,                         /* merge server-level config */
  jitify_cmds,                  /* configuration commands */
  register_jitify_hooks         /* register request lifecycle hooks */
};
