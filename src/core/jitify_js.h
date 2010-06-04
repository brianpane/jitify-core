#ifndef jitify_js_h
#define jitify_js_h

#include "jitify_lexer.h"

#ifdef JITIFY_INTERNAL

extern jitify_token_type_t jitify_type_js_whitespace;
extern jitify_token_type_t jitify_type_js_newline;
extern jitify_token_type_t jitify_type_js_comment;
extern jitify_token_type_t jitify_type_js_line_comment;

typedef struct {
  char last_written;
  char pending;
  bool html_comment;
  bool slash_elem_complete;
} jitify_js_state_t;

#endif /* JITIFY_INTERNAL */

#endif /* !defined(jitify_js_h) */
