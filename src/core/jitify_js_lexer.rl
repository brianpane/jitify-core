#include <stdio.h>
#define JITIFY_INTERNAL
#include "jitify_js.h"

/* JavaScript grammar based on the ECMA-262 5th Edition spec
 * http://www.ecma-international.org/publications/standards/Ecma-262.htm
 */

%%{
  machine jitify_js;
  include jitify_common "jitify_lexer_common.rl";
  
  js_space = /[ \t]/+
    >{ TOKEN_START(jitify_type_js_whitespace); } %{ TOKEN_END; };
  
  _line_end = ( /\r/? /\n/ );
  
  js_line_end = _line_end+
    >{ TOKEN_START(jitify_type_js_newline); } %{ TOKEN_END; };
  
  html_comment ='-->' %{ state->html_comment = 1; };
  
  single_quoted = (
    "'" ( [^'\\] | /\\./)* "'"
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };

  double_quoted = (
    '"' ( [^"\\] | /\\./ )* '"'
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };
  
  js_misc = (
    any - [ \t\r\n'"/]
  )+ >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };

  line_comment = (
    ( ( any | html_comment ) - _line_end )* :>> _line_end @{ state->slash_elem_complete = true; }
  ) >{ TOKEN_TYPE(jitify_type_js_line_comment); state->html_comment = 0; }
  $eof{ state->slash_elem_complete = true; }
  ;

  block_comment = (
    ( any )* :>> '*/' @{ state->slash_elem_complete = true; }
  ) >{ TOKEN_TYPE(jitify_type_js_comment); };

  regex = (
    ( [^\\/] | /\\./ )+ :>> '/' @{ state->slash_elem_complete = true; }
  );
  
  slash_element_perhaps_regex := (
    '/' @{ TOKEN_START(jitify_token_type_misc); state->slash_elem_complete = false; }
    (
      ( '*' block_comment ) |
      ( '/' line_comment ) |
      ( ( any - ( '*' | '/' ) ) @{ fhold; } regex )
    )
  )
  %{ // TODO: check if this is even reachable.  It could only happen at EOF
    TOKEN_END;
    fhold;
    fgoto program;
  }
  $err{
    if (state->slash_elem_complete) {
      /* If we got here by reaching the character after the end of
       * a syntactically valid slash_element, it's not actually an
       * error.  We've simply reached the end of what this sub-lexer
       * knows how to parse.
       */
      TOKEN_END;
      fhold;
      fgoto program;
    }
    else {
      // TODO: invoke main_err
    }
  }
  ;
  
  slash_element_not_regex := (
    '/' @{ TOKEN_START(jitify_token_type_misc); state->slash_elem_complete = true; }
    (
      ( '*' >{ state->slash_elem_complete = false; } block_comment ) |
      ( '/' >{ state->slash_elem_complete = false; } line_comment )
    )?
  )
  %{ // TODO: check if this is even reachable.  It could only happen at EOF
    TOKEN_END;
    fhold;
    fgoto program;
  }
  $err{
    if (state->slash_elem_complete) {
      /* Non-error case--see comments under slash_element_perhaps_regex for more info */
      TOKEN_END;
      fhold;
      fgoto program;
    }
    else {
      // TODO: invoke main_err
    }
  }
  ;
  
  # ECMAscript actually uses two different lexical grammars for different
  # syntactic contexts.  One of those lexical grammars allows the division
  # operator '/', while the other allows the regular expression format '/.../'.
  # The ECMAscript spec recommends that the parser simply choose the right
  # lexical grammar for every token based on the syntactic context.  Because
  # the Jitify Core doesn't use a separate parser on top of the lexer, the
  # lexer itself must somehow determine what grammar to use when it finds
  # a token starting with a slash.  Fortunately, the JS lexer already keeps
  # track of the last-written non-space character as part of the minification
  # algorithm, and that character provides enough context to choose the
  # right sub-lexer.`
  slash_element = '/'
    >{
       bool could_be_regex = false;
       switch (state->last_written) {
         case '(':
         case ',':
         case '=':
         case '[':
         case '!':
         case '&':
         case '|':
         case '?':
         case '{':
         case '}':
         case ';':
         case ':':
         case '\r':
         case '\n':
           could_be_regex = true;
       }
       fhold;
       if (could_be_regex) {
         fgoto slash_element_perhaps_regex;
       }
       else {
         fgoto slash_element_not_regex;
       }
     };
  
  program = (
       js_space |
       js_line_end |
       single_quoted |
       double_quoted |
       slash_element |
       js_misc
  )**;
  
  main := (
    byte_order_mark?
    program
  ) $err(main_err);
  
  write data;
}%%

int jitify_js_scan(jitify_lexer_t *lexer, const void *data, size_t length, bool is_eof)
{
  const char *p = data, *pe = data + length;
  const char *eof = is_eof ? pe : NULL;
  jitify_js_state_t *state = lexer->state;
  if (!lexer->initialized) {
    %% write init;
    lexer->initialized = true;
  }
  %% write exec;
  return p - (const char *)data;
}
