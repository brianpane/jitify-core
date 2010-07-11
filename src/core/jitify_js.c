#include <ctype.h>
#define JITIFY_INTERNAL
#include "jitify_js.h"

/* Javascript transformations
 * Minification logic based on the algorithms of JSMin: http://www.crockford.com/javascript/jsmin.html
 */

extern int jitify_js_scan(jitify_lexer_t *lexer, const void *data, size_t length, int is_eof);

jitify_token_type_t jitify_type_js_whitespace = "JS space";
jitify_token_type_t jitify_type_js_newline = "JS newline";
jitify_token_type_t jitify_type_js_comment = "JS block comment";
jitify_token_type_t jitify_type_js_line_comment = "JS line comment";

static int is_ident_char(char c)
{
  return isalnum(c) || (c == '_') || (c == '$') || (c == '\\') || (c >= 127) || (c < 0);
}

static jitify_status_t js_transform(jitify_lexer_t *lexer, const void *data, size_t length, size_t starting_offset)
{
  const char *buf = data;
  jitify_js_state_t *state = lexer->state;
  
  if (length == 0) {
    return JITIFY_OK;
  }
  
  /* If comment removal is enabled, replace any line comment with
   * a newline and any block comment with a space.  The minify logic
   * that happens later might further reduce these.
   */
  if (lexer->remove_comments) {
    if (lexer->token_type == jitify_type_js_comment) {
      buf = " ";
      length = 1;
      lexer->token_type = jitify_type_js_whitespace;
    }
    else if (lexer->token_type == jitify_type_js_line_comment) {
      buf = "\n";
      length = 1;
      lexer->token_type = jitify_type_js_newline;
    }
  }
  
  if (!lexer->remove_space) {
    const char *last = buf + length;
    while (--last >= buf) {
      if ((*last != ' ') && (*last != '\t')) {
        state->last_written = *last;
        break;
      }
    }
    if (jitify_write(lexer, buf, length) < 0) {
      return JITIFY_ERROR;
    }
    else {
      return JITIFY_OK;
    }
  }
  
  /* In JavaScript, whether it's safe to remove a space or newline
   * may depend on the characters before and after the whitespace.
   * To do minification in a streaming lexer, where the characters
   * before and after the whitespace might not even be in the same
   * buffer, we keep track of the last-written character in
   * state->last_written and hold any space or newline in
   * state->pending until the next token is seen.
   */
  if (lexer->token_type == jitify_type_js_whitespace) {
    if (state->pending) {
      /* There's already a space or newline pending, so we don't need this space */
    }
    else if (is_ident_char(state->last_written)) {
      /* We might need to output this space, so hold onto it */
      state->pending = ' ';
    }
    return JITIFY_OK;
  }
  else if (lexer->token_type == jitify_type_js_newline) {
    if (state->pending == '\n') {
      /* There's already a newline pending, so we don't need this one */
    }
    else if (state->pending == ' ') {
      /* There was a space at the end of the line; discard it */
      state->pending = '\n';
    }
    else if (is_ident_char(state->last_written) ||
             (state->last_written == '}') || (state->last_written == ']') || (state->last_written == ')') ||
             (state->last_written == '+') || (state->last_written == '-') || (state->last_written == '"') ||
             (state->last_written == '\'')) {
      /* We might need to output this newline, depending on what follows it */
      state->pending = '\n';
    }
    return JITIFY_OK;
  }
  else {
    if (state->pending == '\n') {
      if (is_ident_char(*buf) || (*buf == '{') || (*buf == '[') || (*buf == '(') || (*buf == '+') || (*buf == '-')) {
        if (jitify_write(lexer, &(state->pending), 1) < 0) {
          return JITIFY_ERROR;
        }
        state->last_written = state->pending;
      }
      state->pending = 0;
    }
    else if (state->pending == ' ') {
      if (is_ident_char(*buf)) {
        if (jitify_write(lexer, &(state->pending), 1) < 0) {
          return JITIFY_ERROR;
        }
      }
      state->pending = 0;
    }
    state->last_written = buf[length - 1];
    if (jitify_write(lexer, buf, length) < 0) {
      return JITIFY_ERROR;
    }
    else {
      return JITIFY_OK;
    }
  }
}

static void js_cleanup(jitify_lexer_t *lexer)
{
  jitify_free(lexer->pool, lexer->state);
}

jitify_lexer_t *jitify_js_lexer_create(jitify_pool_t *pool, jitify_output_stream_t *out)
{
  jitify_lexer_t *lexer = jitify_lexer_create(pool, out);
  jitify_js_state_t *state = jitify_calloc(pool, sizeof(*state));
  state->last_written = '\n';
  state->pending = 0;
  state->html_comment = 0;
  lexer->state = state;
  lexer->scan = jitify_js_scan;
  lexer->transform = js_transform;
  lexer->cleanup = js_cleanup;
  return lexer;
}
