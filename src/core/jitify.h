#ifndef jitify_h
#define jitify_h

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Jitify Core external API */

/* Basic types */

typedef enum { JITIFY_OK, JITIFY_ERROR, JITIFY_AGAIN } jitify_status_t;

typedef struct jitify_array_s jitify_array_t;

/* Memory management */

typedef struct jitify_pool_s jitify_pool_t;

extern void *jitify_malloc(jitify_pool_t *pool, size_t length);

extern void *jitify_calloc(jitify_pool_t *pool, size_t length);

extern void jitify_free(jitify_pool_t *pool, void *object);

extern void jitify_pool_destroy(jitify_pool_t *pool);

extern jitify_pool_t *jitify_malloc_pool_create();

/* Dynamically growing arrays */

extern jitify_array_t *jitify_array_create(jitify_pool_t *pool, size_t element_size);

extern size_t jitify_array_length(const jitify_array_t *array);

extern void *jitify_array_get(jitify_array_t *array, size_t index);

extern void *jitify_array_push(jitify_array_t *array);

extern void jitify_array_clear(jitify_array_t *array);

extern void jitify_array_destroy(jitify_array_t *array);

/* I/O */

typedef struct jitify_output_stream_s jitify_output_stream_t;

extern void jitify_output_stream_destroy(jitify_output_stream_t *stream);

extern jitify_output_stream_t *jitify_stdio_output_stream_create(jitify_pool_t *pool, FILE *out);

/* Content parsing */

typedef struct jitify_lexer_s jitify_lexer_t;

extern jitify_lexer_t *jitify_lexer_for_content_type(const char *content_type, jitify_pool_t *pool, jitify_output_stream_t *out);

extern jitify_lexer_t *jitify_css_lexer_create(jitify_pool_t *pool, jitify_output_stream_t *out);

extern jitify_lexer_t *jitify_html_lexer_create(jitify_pool_t *pool, jitify_output_stream_t *out);

extern jitify_lexer_t *jitify_js_lexer_create(jitify_pool_t *pool, jitify_output_stream_t *out);

extern void jitify_lexer_set_minify_rules(jitify_lexer_t *lexer, bool remove_space, bool remove_comments);

extern void jitify_lexer_add_cdnify_rule(jitify_lexer_t *lexer, const char *prefix, const char *replacement);

extern void jitify_lexer_set_max_setaside(jitify_lexer_t *lexer, size_t max);

extern size_t jitify_lexer_get_bytes_in(jitify_lexer_t *lexer);

extern size_t jitify_lexer_get_bytes_out(jitify_lexer_t *lexer);

extern size_t jitify_lexer_get_processing_time(jitify_lexer_t *lexer);

extern const char *jitify_lexer_get_err(jitify_lexer_t *lexer);

extern int jitify_write(jitify_lexer_t *lexer, const void *data, size_t length);

/**
 * @return number of bytes scanned, or a negative number if an unrecoverable error occurs
 */
extern int jitify_lexer_scan(jitify_lexer_t *lexer, const void *data, size_t len, bool is_eof);

extern void jitify_lexer_destroy(jitify_lexer_t *lexer);

#ifdef JITIFY_INTERNAL

struct jitify_pool_s {
  void *state;
  void *(*malloc)(jitify_pool_t *p, size_t size);
  void *(*calloc)(jitify_pool_t *p, size_t size);
  void (*free)(jitify_pool_t *p, void *object);
  void (*cleanup)(jitify_pool_t *p);
};

struct jitify_output_stream_s {
  void *state;
  jitify_pool_t *pool;
  int (*write)(jitify_output_stream_t *stream, const void *data, size_t length);
  void (*cleanup)(jitify_output_stream_t *stream);
};

#endif /* JITIFY_INTERNAL */

#endif /* !defined(jitify_h) */
