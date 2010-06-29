#ifndef jitify_lexer_h
#define jitify_lexer_h

#include <string.h>

#include "jitify.h"

#ifdef JITIFY_INTERNAL

typedef const char *jitify_token_type_t;

extern jitify_token_type_t jitify_token_type_misc;

typedef struct {
  const char *data;
  size_t len;
} jitify_str_t;

typedef struct {
  union {
    size_t offset; /* Relative to start of entire document */
    const char *buf;
  } data;
  size_t len;
} jitify_str_ref_t;

typedef struct {
  jitify_str_ref_t key;
  jitify_str_ref_t value;
  bool key_setaside;
  bool value_setaside;
  char quote;
} jitify_attr_t;

typedef struct {
  jitify_str_t prefix;
  jitify_str_t *replacement; /* NULL means "don't CDNify a link that matches this prefix" */
} jitify_cdnify_rule_t;

struct jitify_lexer_s {
  void *state;
  jitify_pool_t *pool;
  jitify_output_stream_t *out;

  bool initialized;
  bool failsafe_mode;

  /* Minification rules */
  bool remove_space;
  bool remove_comments;
  
  /* Link rewriting rules */
  jitify_cdnify_rule_t *cdnify_rules;
  size_t cdnify_rules_len;   /* Number of items in cdnify_rules */
  size_t cdnify_rules_size;  /* Max @ of items that cdnify_rules is sized to hold */
  
  jitify_status_t (*transform)(jitify_lexer_t *lexer, const void *data, size_t length, size_t offset);
  int (*scan)(jitify_lexer_t *lexer, const void *data, size_t length, bool is_eof);
  void (*cleanup)(jitify_lexer_t *lexer);
  
  char *setaside;
  size_t setaside_max;
  size_t setaside_len;
  bool setaside_overflow; /* True iff a cross-buffer token exceeded setaside_max */
  size_t setaside_offset; /* Offset from start of document of 1st byte of setaside */
  
  jitify_token_type_t token_type;
  const char *token_start;
  
  long duration; /* Cumulative time spent in this lexer, in usec */
  size_t bytes_in; /* Cumulative input bytes processed by this lexer */
  size_t bytes_out; /* Cumulative bytes of output produced by this lexer */
  
  const char *err; /* Location of error within input buf, NULL if no error */
  
  const char *buf; /* Start of current buffer */
  size_t starting_offset; /* Offset from start of document of 1st byte of current buffer */
  
  size_t subtoken_offset; /* Offset from start of document of current subtoken */
  
  jitify_array_t *attrs; /* Array of jitify_attr_t* */
  jitify_attr_t *current_attr; /* Points into attrs or is NULL */
  bool attrs_resolved; /* whether the keys and values in attrs have been converted from offsets to char* */
  
  /* The following fields support Ragel-generated parsers */
  int cs;
  int act;
};

extern jitify_lexer_t *jitify_lexer_create(jitify_pool_t *pool, jitify_output_stream_t *out);

extern void jitify_transform_with_setaside(jitify_lexer_t *lexer, const char *p);

extern void jitify_lexer_resolve_attrs(jitify_lexer_t *lexer, const char *buf, size_t starting_offset);

#define CURRENT_OFFSET(ptr)                      \
  (lexer->starting_offset + (ptr - lexer->buf))

#define TOKEN_TYPE(t)                            \
  lexer->token_type = (t)

#define TOKEN_START(t)                           \
  lexer->token_start = p;                        \
  TOKEN_TYPE(t)

#define TOKEN_END                                \
  if (lexer->setaside_len) {                     \
    jitify_transform_with_setaside(lexer, p);    \
  }                                              \
  else {                                         \
    lexer->transform(lexer, lexer->token_start,  \
      p - lexer->token_start,                    \
      CURRENT_OFFSET(lexer->token_start));       \
  }                                              \
  TOKEN_START(jitify_token_type_misc);           \
  lexer->current_attr = NULL

#define ADD_ATTR                                 \
  if (!lexer->current_attr) {                    \
    lexer->current_attr =                        \
      jitify_array_push(lexer->attrs);           \
  }

#define ATTR_SET_QUOTE(q)                        \
  lexer->current_attr->quote = q

#define ATTR_KEY_START                           \
  ADD_ATTR;                                      \
  lexer->subtoken_offset = CURRENT_OFFSET(p)

#define ATTR_KEY_END                             \
  lexer->current_attr->key.data.offset =         \
    lexer->subtoken_offset;                      \
  lexer->current_attr->key.len =                 \
    CURRENT_OFFSET(p) - lexer->subtoken_offset

#define ATTR_VALUE_START                         \
  ADD_ATTR;                                      \
  lexer->subtoken_offset = CURRENT_OFFSET(p)

#define ATTR_END                                 \
  lexer->current_attr = NULL

#define ATTR_VALUE_END                           \
  lexer->current_attr->value.data.offset =       \
    lexer->subtoken_offset;                      \
  lexer->current_attr->value.len =               \
    CURRENT_OFFSET(p) - lexer->subtoken_offset;  \
  ATTR_END

#define RESET_ATTRS                              \
  jitify_array_clear(lexer->attrs);              \
  lexer->attrs_resolved = false

#endif /* JITIFY_INTERNAL */

#endif /* !defined(jitify_lexer_h) */
