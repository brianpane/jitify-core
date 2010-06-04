#define JITIFY_INTERNAL
#include "jitify_css.h"

extern int jitify_css_scan(jitify_lexer_t *lexer, const void *data, size_t length, bool is_eof);

jitify_token_type_t jitify_type_css_comment = "CSS comment";
jitify_token_type_t jitify_type_css_optional_whitespace = "CSS optional space";
jitify_token_type_t jitify_type_css_required_whitespace = "CSS required space";
jitify_token_type_t jitify_type_css_url = "CSS URL";

static jitify_status_t css_transform(jitify_lexer_t *lexer, const void *data, size_t length, size_t offset)
{
  if (lexer->token_type == jitify_type_css_optional_whitespace) {
    if (lexer->remove_space) {
      return JITIFY_OK;
    }
  }
  else if (lexer->token_type == jitify_type_css_comment) {
    if (lexer->remove_comments) {
      return JITIFY_OK;
    }
  }
  if (jitify_write(lexer->out, data, length) < 0) {
    return JITIFY_ERROR;
  }
  else {
    return JITIFY_OK;
  }
}

static void css_cleanup(jitify_lexer_t *lexer)
{
  jitify_free(lexer->pool, lexer->state);
}

jitify_lexer_t *jitify_css_lexer_create(jitify_pool_t *pool, jitify_output_stream_t *out)
{
  jitify_lexer_t *lexer = jitify_lexer_create(pool, out);
  lexer->scan = jitify_css_scan;
  lexer->transform = css_transform;
  lexer->cleanup = css_cleanup;
  return lexer;
}
