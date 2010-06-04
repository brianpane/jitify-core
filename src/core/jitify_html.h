#ifndef jitify_html_h
#define jitify_html_h

#include "jitify_lexer.h"

#ifdef JITIFY_INTERNAL

extern jitify_token_type_t jitify_type_html_comment;
extern jitify_token_type_t jitify_type_html_img_open;
extern jitify_token_type_t jitify_type_html_link_open;
extern jitify_token_type_t jitify_type_html_script_open;
extern jitify_token_type_t jitify_type_html_space;

typedef struct {
  bool conditional_comment;
  bool space_contains_newlines;
  int nominify_depth;
  jitify_token_type_t last_token_type;
} jitify_html_state_t;

#endif /* JITIFY_INTERNAL */

#endif /* !defined(jitify_html_h) */
