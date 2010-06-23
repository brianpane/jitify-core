#include <string.h>
#define JITIFY_INTERNAL
#include "jitify_css.h"
#include "jitify_html.h"

jitify_token_type_t jitify_type_html_anchor_open = "HTML anchor";
jitify_token_type_t jitify_type_html_comment = "HTML comment";
jitify_token_type_t jitify_type_html_img_open = "HTML img";
jitify_token_type_t jitify_type_html_link_open = "HTML link";
jitify_token_type_t jitify_type_html_script_open = "HTML script";
jitify_token_type_t jitify_type_html_space = "HTML space";
jitify_token_type_t jitify_type_html_tag = "HTML tag";

extern int jitify_html_scan(jitify_lexer_t *lexer, const void *data, size_t length, bool is_eof);

static jitify_status_t html_tag_transform(jitify_lexer_t *lexer, const char *buf, size_t length,
  size_t starting_offset, const char *attr_name)
{
  bool modified = false;
  jitify_html_state_t *state = lexer->state;
  
  /* If we're minifying this HTML document, set
   * modified=true to force the tag to be
   * reconstructed with minimal spacing.
   */
  if (lexer->remove_space) {
    modified = true;
  }
  
  /* TODO: Add link rewriting here */
  
  if (!modified)
  {
    /* No modification needed; send the full token as-is */
    if (jitify_write(lexer, buf, length) < 0) {
      return JITIFY_ERROR;
    }
    else {
      return JITIFY_OK;
    }
  }
  else {
    size_t i, num_attrs;
    /* Reconstruct the tag based on the current attr values */
    jitify_lexer_resolve_attrs(lexer, buf, starting_offset);
    jitify_write(lexer, "<", 1);
    if (state->leading_slash) {
      jitify_write(lexer, "/", 1);
    }
    jitify_write(lexer, buf + state->tagname_offset - starting_offset, state->tagname_len);
    num_attrs = jitify_array_length(lexer->attrs);
    for (i = 0; i < num_attrs; i++) {
      jitify_attr_t *attr = jitify_array_get(lexer->attrs, i);
      jitify_write(lexer, " ", 1);
      if (attr->key.len) {
        jitify_write(lexer, attr->key.data.buf, attr->key.len);
        jitify_write(lexer, "=", 1);
      }
      if (attr->value.len) {
        if (attr->quote) {
          jitify_write(lexer, &(attr->quote), 1);
        }
        jitify_write(lexer, attr->value.data.buf, attr->value.len);
        if (attr->quote) {
          jitify_write(lexer, &(attr->quote), 1);
        }
      }
    }
    if (state->trailing_slash) {
      jitify_write(lexer, "/", 1);
    }
    jitify_write(lexer, ">", 1);
    return JITIFY_OK;
  }
}

static jitify_status_t html_transform(jitify_lexer_t *lexer, const void *data, size_t length, size_t starting_offset)
{
  const char *buf = data;
  jitify_html_state_t *state = lexer->state;
  
  if (lexer->token_type == jitify_type_html_space) {
    if (lexer->remove_space &&
        (state->nominify_depth == 0)) {
      if (state->space_contains_newlines) {
        buf = "\n";
      }
      else {
        buf = " ";
      }
      length = 1;
    }
  }
  else if (lexer->token_type == jitify_type_html_comment) {
    if (lexer->remove_comments && !state->conditional_comment) {
      buf = " ";
      length = 1;
    }
  }
  else if (lexer->token_type == jitify_type_css_comment) {
    if (lexer->remove_comments) {
      return JITIFY_OK;
    }
  }
  else if (lexer->token_type == jitify_type_css_optional_whitespace) {
    if (lexer->remove_space) {
      return JITIFY_OK;
    }
  }
  else if ((lexer->token_type == jitify_type_css_selector) || (lexer->token_type == jitify_type_css_term)) {
    if (lexer->remove_space && (state->last_token_type == lexer->token_type)) {
      /* The space we just skipped was actually necessary, so add a space back in */
      if (jitify_write(lexer, " ", 1) < 0) {
        return JITIFY_ERROR;
      }
    }
  }
  else if (lexer->token_type == jitify_type_html_tag) {
    return html_tag_transform(lexer, buf, length, starting_offset, "?");
  }
  else if (lexer->token_type == jitify_type_html_anchor_open) {
    return html_tag_transform(lexer, buf, length, starting_offset, "href");
  }
  else if (lexer->token_type == jitify_type_html_img_open) {
    return html_tag_transform(lexer, buf, length, starting_offset, "src");
  }
  else if (lexer->token_type == jitify_type_html_link_open) {
    return html_tag_transform(lexer, buf, length, starting_offset, "href");
  }
  else if (lexer->token_type == jitify_type_html_script_open) {
    return html_tag_transform(lexer, buf, length, starting_offset, "src");
  }
  
  state->last_token_type = lexer->token_type;
  if (jitify_write(lexer, buf, length) < 0) {
    return JITIFY_ERROR;
  }
  else {
    return JITIFY_OK;
  }
}

static void html_cleanup(jitify_lexer_t *lexer)
{
  jitify_free(lexer->pool, lexer->state);
}

jitify_lexer_t *jitify_html_lexer_create(jitify_pool_t *pool, jitify_output_stream_t *out)
{
  jitify_lexer_t *lexer = jitify_lexer_create(pool, out);
  jitify_html_state_t *state = jitify_calloc(pool, sizeof(*state));
  state->conditional_comment = false;
  state->space_contains_newlines = false;
  lexer->state = state;
  lexer->scan = jitify_html_scan;
  lexer->transform = html_transform;
  lexer->cleanup = html_cleanup;
  return lexer;
}
