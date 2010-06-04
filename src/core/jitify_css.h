#ifndef jitify_css_h
#define jitify_css_h

#include "jitify_lexer.h"

#ifdef JITIFY_INTERNAL

extern jitify_token_type_t jitify_type_css_term;
extern jitify_token_type_t jitify_type_css_comment;
extern jitify_token_type_t jitify_type_css_optional_whitespace;
extern jitify_token_type_t jitify_type_css_required_whitespace;
extern jitify_token_type_t jitify_type_css_url;

typedef struct {
  jitify_token_type_t last_token_type;
} jitify_css_state_t;

#endif /* JITIFY_INTERNAL */

#endif /* !defined(jitify_css_h) */
