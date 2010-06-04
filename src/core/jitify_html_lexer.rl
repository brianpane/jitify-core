#include <stdio.h>
#define JITIFY_INTERNAL
#include "jitify_css.h"
#include "jitify_html.h"

/* HTML grammar based on http://www.w3.org/TR/WD-html-lex/
 */

%%{
  machine jitify_html;
  include jitify_common "jitify_lexer_common.rl";
  include css_grammar   "jitify_css_lexer_common.rl";
  
  tag_close = space* '/'? '>';

  name_char = (alnum | '-' | '_' | '.' | ':');
  name_start_char = (alpha | '_');
  name = name_start_char name_char*;

  action leave_content {
    TOKEN_END;
  }

  conditional_comment = '[if' %{ state->conditional_comment = 1; };

  comment = '--' %{ TOKEN_TYPE(jitify_type_html_comment); state->conditional_comment = false; }
    (any | conditional_comment)* :>> '-->' %{ TOKEN_END; };

  misc_directive = any* :>> '>';

  directive = (
    '!' (comment | misc_directive)
  );

  attr_name = (
    alpha (alnum | '-' | '_' | ':')*
  ) >{ ATTR_KEY_START; } %{ ATTR_KEY_END; };

  unquoted_attr_value = name_char+
    >{ ATTR_VALUE_START; ATTR_SET_QUOTE(0); } %{ ATTR_VALUE_END; };
  
  single_quoted_attr_value = "'" @{ ATTR_SET_QUOTE('\''); }
  ( /[^']*/ ) >{ ATTR_VALUE_START; } %{ ATTR_VALUE_END; }
  "'";
  
  double_quoted_attr_value = '"' @{ ATTR_SET_QUOTE('"'); }
  ( /[^"]*/ ) >{ ATTR_VALUE_START; } %{ ATTR_VALUE_END; }
  '"';
  
  attr_value = (
    unquoted_attr_value |
    single_quoted_attr_value  |
    double_quoted_attr_value
  );
  
  attr = (
    (attr_name space* '=' space* attr_value)
    |
    (attr_value)
  );

  unparsed_attr_name = (
    alpha (alnum | '-' | '_' | ':')*
  );

  unparsed_attr_value = (
    name_char+ |
    "'" /[^']*/ "'" |
    '"' /[^"]*/ '"'
  );

  unparsed_attr = (
    (unparsed_attr_name space* '=' space* unparsed_attr_value)
    |
    (unparsed_attr_value)
  );

  preformatted_close = '</' /(pre|textarea)/i '>' @{ state->nominify_depth--; };
  
  preformatted_open= (/pre/i | /textarea/i) @{ state->nominify_depth++; }
    (space+ unparsed_attr)* tag_close;
  
  script_close = '</' /script/i '>';

  script = (
    /script/i %{ TOKEN_TYPE(jitify_type_html_script_open); }
      (space+ attr)* tag_close %{ TOKEN_END; RESET_ATTRS; TOKEN_START(jitify_token_type_misc); }
      (any* - ( any* script_close any* ) ) script_close
  );

  img = /img/i %{ TOKEN_TYPE(jitify_type_html_img_open); } (space+ attr)* tag_close;

  link = /link/i %{ TOKEN_TYPE(jitify_type_html_link_open); } (space+ attr)* tag_close;

  style = (
    /style/i >{ TOKEN_TYPE(jitify_token_type_misc); } (space+ unparsed_attr)* tag_close %{ TOKEN_END; }
    css_document? ( '</' /style/i '>' ) >{ TOKEN_TYPE(jitify_token_type_misc); } %{ TOKEN_END; }
  );
  
  misc_tag = '/'? name >{ TOKEN_TYPE(jitify_token_type_misc); } (space+ unparsed_attr)* tag_close;

  element = (
    script
    |
    img
    |
    preformatted_open | preformatted_close
    |
    link
    |
    style
    |
    misc_tag
    |
    directive
  );

  html_space = (
    ( space - ( '\r' | '\n' ) ) |
    ( '\r' | '\n' ) @{ state->space_contains_newlines = true; }
  )+ >{ TOKEN_START(jitify_type_html_space); state->space_contains_newlines = false; } %{ TOKEN_END; };
  
  content = (
    any - (space | '<' )
  )+ >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };
  
  one = any >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };
  main := (
    byte_order_mark?
    (
      ( '<' >{ TOKEN_START(jitify_token_type_misc); } element %{ TOKEN_END; RESET_ATTRS; } )
      |
      html_space
      |
      content
    )**
  ) $err(main_err);
  
  write data;
}%%

int jitify_html_scan(jitify_lexer_t *lexer, const void *data, size_t length, bool is_eof)
{
  const char *p = data, *pe = data + length;
  const char *eof = is_eof ? pe : NULL;
  jitify_html_state_t *state = lexer->state;
  if (!lexer->initialized) {
    %% write init;
    lexer->initialized = true;
  }
  %% write exec;
  return p - (const char *)data;
}
