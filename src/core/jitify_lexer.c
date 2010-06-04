#include <string.h>
#include <sys/time.h>
#define JITIFY_INTERNAL
#include "jitify_lexer.h"

jitify_token_type_t jitify_token_type_misc = "Miscellaneous";

static int failsafe_send(jitify_lexer_t *lexer, const void *data, size_t len, size_t offset)
{
  lexer->token_type = jitify_token_type_misc;
  switch (lexer->transform(lexer, data, len, offset)) {
    case JITIFY_OK:
    return (int)len;
    case JITIFY_AGAIN:
    return 0;
    default:
    return -1;
  }
}

void jitify_lexer_set_minify_rules(jitify_lexer_t *lexer, bool remove_space, bool remove_comments)
{
  lexer->remove_space = remove_space;
  lexer->remove_comments = remove_comments;
}

void jitify_lexer_set_max_setaside(jitify_lexer_t *lexer, size_t max)
{
  lexer->setaside_max = max;
}

static void setaside_buffer(jitify_lexer_t *lexer, size_t remaining)
{
  if (!lexer->setaside) {
    lexer->setaside = jitify_malloc(lexer->pool, lexer->setaside_max);
  }
  if (lexer->setaside_len == 0) {
    lexer->setaside_offset = lexer->starting_offset + (lexer->token_start - lexer->buf);
  }
  memcpy(lexer->setaside + lexer->setaside_len, lexer->token_start, remaining);
  lexer->setaside_len += remaining;
}

int jitify_lexer_scan(jitify_lexer_t *lexer, const void *data, size_t len, bool is_eof)
{
  int rc;
  struct timeval start_time, end_time;
  long elapsed_usec;
  int bytes_scanned;

  lexer->setaside_overflow = false;
  lexer->buf = data;
  lexer->err = NULL;
  gettimeofday(&start_time, NULL);
  if (lexer->failsafe_mode) {
    rc = failsafe_send(lexer, data, len, lexer->starting_offset);
    bytes_scanned = len;
  }
  else {
    lexer->token_start = data;
    bytes_scanned = lexer->scan(lexer, data, len, is_eof);
    if (bytes_scanned < 0) {
      rc = bytes_scanned;
    }
    else if (bytes_scanned == (int)len) {
      if (lexer->token_start) {
        size_t remaining = (const char *)data + len - lexer->token_start;
        if (remaining) {
          /* Partially matched token at end of buffer */
          if (remaining + lexer->setaside_len <= lexer->setaside_max) {
            /* We have enough space to set aside this partial token until we get more data */
            setaside_buffer(lexer, remaining);
          }
          else {
            /* Not enough space to set aside this token, so send it unmodified */
            lexer->token_type = jitify_token_type_misc;
            if (lexer->setaside_len) {
              lexer->transform(lexer, lexer->setaside, lexer->setaside_len, lexer->setaside_offset);
              lexer->setaside_len = 0;
            }
            lexer->transform(lexer, lexer->token_start, remaining, CURRENT_OFFSET(lexer->token_start));
            /* TODO check transform return code */
            lexer->setaside_overflow = true;
          }
        }
      }
      rc = bytes_scanned;
    }
    else {
      if (lexer->failsafe_mode) {
        rc = failsafe_send(lexer, data + bytes_scanned, len - bytes_scanned, lexer->starting_offset + bytes_scanned);
        bytes_scanned = len;
      }
      else {
        rc = bytes_scanned;
      }
    }
  }
  gettimeofday(&end_time, NULL);
  elapsed_usec = (end_time.tv_sec * 1000000 + end_time.tv_usec) - (start_time.tv_sec * 1000000 + start_time.tv_usec);
  lexer->duration += elapsed_usec;
  lexer->bytes_in += bytes_scanned;
  if (bytes_scanned > 0) {
    lexer->starting_offset += bytes_scanned;
  }
  return rc;
}

void jitify_lexer_destroy(jitify_lexer_t *lexer)
{
  if (lexer) {
    if (lexer->cleanup) {
      lexer->cleanup(lexer);
    }
    jitify_array_destroy(lexer->attrs);
    jitify_free(lexer->pool, lexer->setaside);
    jitify_free(lexer->pool, lexer);
  }
}

#define DEFAULT_MAX_SETASIDE 1024

jitify_lexer_t *jitify_lexer_create(jitify_pool_t *pool, jitify_output_stream_t *out)
{
  jitify_lexer_t *lexer = jitify_calloc(pool, sizeof(*lexer));
  lexer->pool = pool;
  lexer->out = out;
  lexer->initialized = false;
  lexer->failsafe_mode = false;
  lexer->remove_space = false;
  lexer->remove_comments = false;
  lexer->setaside_max = DEFAULT_MAX_SETASIDE;
  lexer->token_type = jitify_token_type_misc;
  lexer->attrs = jitify_array_create(pool, sizeof(jitify_attr_t));
  return lexer;
}

typedef struct {
  const char *content_type;
  jitify_lexer_t *(*create_lexer)(jitify_pool_t *pool, jitify_output_stream_t *out);
} content_type_to_lexer_t;

static content_type_to_lexer_t content_type_map[] = {
  { "text/css", jitify_css_lexer_create },
  { "text/html", jitify_html_lexer_create },
  { "text/javascript", jitify_js_lexer_create },
  { "application/javascript", jitify_js_lexer_create },
  { "application/x-javascript", jitify_js_lexer_create },
  { NULL, NULL }
};

jitify_lexer_t *jitify_lexer_for_content_type(const char *content_type, jitify_pool_t *pool, jitify_output_stream_t *out)
{
  const char *delimiter;
  content_type_to_lexer_t *mapping;
  if (!content_type) {
    return NULL;
  }
  delimiter = strchr(content_type, ';');
  if (delimiter) {
    size_t length = delimiter - content_type;
    char *new_content_type = jitify_malloc(pool, length + 1);
    memcpy(new_content_type, content_type, length);
    new_content_type[length] = 0;
    content_type = new_content_type;
  }
  // TODO: replace with a sub-O(n) lookup if the number of map entries ever exceeds single digits
  for (mapping = content_type_map; mapping->content_type; mapping++) {
    if (!strcasecmp(content_type, mapping->content_type)) {
      return mapping->create_lexer(pool, out);
    }
  }
  return NULL;
}

size_t jitify_lexer_get_bytes_in(jitify_lexer_t *lexer)
{
  return lexer->bytes_in;
}

size_t jitify_lexer_get_bytes_out(jitify_lexer_t *lexer)
{
  return lexer->bytes_out;
}

size_t jitify_lexer_get_processing_time(jitify_lexer_t *lexer)
{
  return lexer->duration;
}

const char *jitify_lexer_get_err(jitify_lexer_t *lexer)
{
  return lexer->err;
}

void jitify_transform_with_setaside(jitify_lexer_t *lexer, const char *p)
{
  size_t length = p - lexer->token_start;
  if (length + lexer->setaside_len <= lexer->setaside_max) {
    memcpy(lexer->setaside + lexer->setaside_len, lexer->token_start, length);
    lexer->transform(lexer, lexer->setaside, lexer->setaside_len + length, lexer->setaside_offset);
    // TODO: check transform return code
  }
  else {
    lexer->token_type = jitify_token_type_misc;
    lexer->transform(lexer, lexer->setaside, lexer->setaside_len, lexer->setaside_offset);
    lexer->transform(lexer, lexer->token_start, length, lexer->starting_offset); // TODO: double-check the offset
  }
  lexer->setaside_len = 0;
}

#define INITIAL_ARRAY_SIZE 1

void jitify_lexer_add_cdnify_rule(jitify_lexer_t *lexer, const char *prefix, const char *replacement)
{
  jitify_cdnify_rule_t *rule;
  if (lexer->cdnify_rules_size <= lexer->cdnify_rules_len) {
    jitify_cdnify_rule_t *new_rules;
    if (lexer->cdnify_rules_size) {
      lexer->cdnify_rules_size *= 2;
    }
    else {
      lexer->cdnify_rules_size = INITIAL_ARRAY_SIZE;
    }
    new_rules = jitify_malloc(lexer->pool, lexer->cdnify_rules_size * sizeof(jitify_cdnify_rule_t));
    if (lexer->cdnify_rules_len) {
      memcpy(new_rules, lexer->cdnify_rules, lexer->cdnify_rules_len * sizeof(jitify_cdnify_rule_t));
    }
    if (lexer->cdnify_rules) {
      jitify_free(lexer->pool, lexer->cdnify_rules);
    }
    lexer->cdnify_rules = new_rules;
  }
  rule = &(lexer->cdnify_rules[lexer->cdnify_rules_len++]);
  rule->prefix.data = prefix;
  rule->prefix.len = strlen(prefix);
  if (replacement) {
    rule->replacement = jitify_malloc(lexer->pool, sizeof(jitify_str_t));
    rule->replacement->data = replacement;
    rule->replacement->len = strlen(replacement);
  }
  else {
    rule->replacement = NULL;
  }
}
