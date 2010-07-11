%%{
  machine jitify_common;
  
  access lexer->;
  
  action main_err {
    TOKEN_TYPE(jitify_token_type_misc);
    TOKEN_END;
    lexer->err = p;
    lexer->failsafe_mode = 1;
    jitify_err_checkpoint(lexer);
    p--;
    fbreak;
  }
  
  byte_order_mark = (
    0xEF 0xBB 0xBF
  ) >{ TOKEN_START(jitify_token_type_misc); } %{ TOKEN_END; };

}%%
