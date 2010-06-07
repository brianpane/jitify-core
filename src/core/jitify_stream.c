#define JITIFY_INTERNAL
#include "jitify.h"
#include "jitify_lexer.h"


int jitify_write(jitify_lexer_t *lexer, const void *data, size_t length)
{
  int rv;
  rv = lexer->out->write(lexer->out, data, length);
  lexer->bytes_out += length;
  return rv;
}

void jitify_output_stream_destroy(jitify_output_stream_t *stream)
{
  if (stream) {
    if (stream->cleanup) {
      stream->cleanup(stream);
    }
    jitify_free(stream->pool, stream);
  }
}

static int file_write(jitify_output_stream_t *stream, const void *data, size_t length)
{
#if 1
  size_t bytes_written = fwrite(data, 1, length, (FILE *)stream->state);
  if (bytes_written < length) {
    return -1;
  }
  else {
    return (int)bytes_written;
  }
#else
  return (int)length;
#endif
}

jitify_output_stream_t *jitify_stdio_output_stream_create(jitify_pool_t *pool, FILE *out)
{
  jitify_output_stream_t *stream = jitify_calloc(pool, sizeof(*stream));
  stream->state = out;
  stream->pool = pool;
  stream->write = file_write;
  return stream;
}
