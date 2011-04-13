#include <stdio.h>
#define JITIFY_INTERNAL
#include "jitify_css.h"

%%{
  machine jitify_css;
  include jitify_common "jitify_lexer_common.rl";
  include css_grammar   "jitify_css_lexer_common.rl";
  
  main := (
    byte_order_mark?
    css_document
  ) $err(main_err);
  
  write data;
}%%

int jitify_css_scan(jitify_lexer_t *lexer, const void *data, size_t length, int is_eof)
{
  const char *p = data;
  const char *pe = p + length;
  const char *eof = is_eof ? pe : NULL;
  jitify_css_state_t *state = lexer->state;
  if (!lexer->initialized) {
    %% write init;
    lexer->initialized = 1;
  }
  %% write exec;
  return p - (const char *)data;
}
