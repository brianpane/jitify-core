/* CSS grammar based on W3C CSS 2.1 spec:
 * http://www.w3.org/TR/CSS2/grammar.html
 */

%%{
  
  machine css_grammar;
  
  css_comment = (
    '/*' ( any )* :>> '*/'
  ) >{ TOKEN_TYPE(jitify_type_css_comment); } %{ TOKEN_END; } ;

  optional_space = (
    space+
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_TYPE(jitify_type_css_optional_whitespace); TOKEN_END; };

  optional_space_or_comment = (
    optional_space |
    css_comment
  )+;
  
  required_space = (
    space+
    ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_TYPE(jitify_type_css_required_whitespace); TOKEN_END; };

  combinator = (
    '+' | '>'
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };

  _nmchar = (
    alnum | '_' | '-'
  );

  _single_quoted = (
    "'" ( [^'\\] | /\\./)* "'"
  );

  _double_quoted = (
    '"' ( [^"\\] | /\\./ )* '"'
  );

  _ident = (
    ( '-' | '*' | '!' )? ( alpha | '_') _nmchar*
  );

  _hash = (
    '#' _nmchar+
  );

  _css_class = (
    '.' _ident
  );

  _attrib = (
    '[' space*
    _ident space*
    ( ( '=' | "~=" | "|=" ) space* (_ident | _single_quoted | _double_quoted) )?
    space* ']'
  );

  _pseudo = (
    ':'+ _ident ( '(' space* _ident space* ')' )?
  );

  simple_selector = (
    ( (_ident | '*') (_hash | _css_class | _attrib | _pseudo)* )
    |
    (_hash | _css_class | _attrib | _pseudo)+
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };

  selector = (
    simple_selector ( combinator optional_space_or_comment? simple_selector )*
  );

  selectors = (
    selector ( css_comment? required_space ( css_comment optional_space? )* selector )*
  );

  comma = (
    ','
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };

  slash = (
    '/'
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };

  open_curly_brace = (
    '{'
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };

  close_curly_brace = (
    '}'
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };

  semicolon = (
    ';'
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };

  colon = (
    ':'
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };

  exclamation_point = (
    '!'
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };

  important = (
    /important/i
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };

  property = (
    _ident
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };

  priority = (
    exclamation_point optional_space_or_comment? important
  );

  unary_operator = (
    '-' | '+'
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };

  operator = (
    (comma | slash) optional_space?
  );

  single_quoted_uri = "'" @{ ATTR_SET_QUOTE('\''); }
  ( [^'\\] | /\\./)* >{ ATTR_VALUE_START; } %{ ATTR_VALUE_END; }
  "'";
  
  double_quoted_uri = '"' @{ ATTR_SET_QUOTE('"'); }
  ( [^"\\] | /\\./ )* >{ ATTR_VALUE_START; } %{ ATTR_VALUE_END; }
  '"';
  
  unquoted_uri = ( empty |
    ( ( any - ( space | '"' | "'" | ')' | ',' ) )  ( any - ( space | ')' | ',' ) )* )
    ) >{ ATTR_VALUE_START; } %{ ATTR_VALUE_END; };

  _misc_term = ( any - ( space | '(' | ')' | ';' | '}' | '"' | "'" | ',' | '/' ) )+;
    
  base_term = (
    _single_quoted | _double_quoted | _misc_term
  ) >{ TOKEN_START(jitify_type_css_term); } %{ TOKEN_END; };
  
  function_arg = ( unquoted_uri | double_quoted_uri | single_quoted_uri ) space*;
  
  function_args = '(' space* ( function_arg ( space* ',' function_arg )* space* ) :>> ')';
  
  term = base_term optional_space_or_comment? (function_args optional_space_or_comment?)?;

  declaration = (
    property optional_space_or_comment? colon optional_space_or_comment? term <: ((comma optional_space_or_comment?)? term)**
    (priority optional_space_or_comment?)?
  );

  ruleset = (
    selector ( required_space selector )** ( optional_space? comma optional_space? selectors )*
    optional_space_or_comment? open_curly_brace
    optional_space_or_comment? declaration? ( semicolon optional_space_or_comment? declaration? )*
    close_curly_brace
  );

  html_open_comment = (
    '<!--'
  ) %{ TOKEN_END; };

  html_close_comment = (
    '-->'
  ) %{ TOKEN_END; };

  media_list = (
    any - '{'
  )** >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };
  
  media = (
    ( '@' /media/i ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; }
    required_space
    media_list :>>
    open_curly_brace
    optional_space?
    ruleset*
    optional_space?
    close_curly_brace
  );
  
  css_document = (
    optional_space_or_comment |
    html_close_comment |
    html_open_comment |
    ruleset
#    media
  )** >{ TOKEN_START(jitify_token_type_misc); };

}%%
