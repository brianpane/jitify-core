#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <jitify.h>

static const char *PROGRAM_NAME = "jitify";
#define CONTENT_TYPE_CSS  1
#define CONTENT_TYPE_HTML 2
#define CONTENT_TYPE_JS   3

static size_t block_size = 8192;
static int max_setaside = -1;

static int content_type = 0;
static int remove_space = 0;
static int remove_comments = 0;

static void usage()
{
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s [options] (--css | --js | --html)  # read from stdin, write to stdout\n", PROGRAM_NAME);
  fprintf(stderr, "  %s [options] filename                 # read from file, write to stdout\n", PROGRAM_NAME);
  fprintf(stderr, "options:\n");
  fprintf(stderr, "  --remove-space      # remove unnecessary whitespace\n");
  fprintf(stderr, "  --remove-comments   # remove comments\n");
  fprintf(stderr, "  --minify            # equivalent to \"--remove-space --remove-comments\"\n");
  fprintf(stderr, "  --block-size=<n>    # process the input at most n bytes at a time\n");
}

static int get_content_type(const char *filename)
{
  const char *extension = strrchr(filename, '.');
  if (!extension) {
    return 0;
  }
  extension++;
  if (!strcasecmp("htm", extension) || !strcasecmp("html", extension)) {
    return CONTENT_TYPE_HTML;
  }
  if (!strcasecmp("css", extension)) {
    return CONTENT_TYPE_CSS;
  }
  if (!strcasecmp("js", extension)) {
    return CONTENT_TYPE_JS;
  }
  return 0;
}

static void process_file(int fd)
{
  jitify_pool_t *p = jitify_malloc_pool_create();
  jitify_output_stream_t *out = jitify_stdio_output_stream_create(p, stdout);
  jitify_lexer_t *lexer;
  int bytes_read;
  size_t bytes_in, bytes_out, duration;
  
  char *block;
  
  switch (content_type) {
    case CONTENT_TYPE_CSS:
      lexer = jitify_css_lexer_create(p, out);
      break;
    case CONTENT_TYPE_JS:
      lexer = jitify_js_lexer_create(p, out);
      break;
    case CONTENT_TYPE_HTML:
      lexer = jitify_html_lexer_create(p, out);
      break;
    default:
      fprintf(stderr, "%s: internal error\n", PROGRAM_NAME);
      return;
  }
  
  if (max_setaside >= 0) {
    jitify_lexer_set_max_setaside(lexer, (size_t)max_setaside);
  }
  jitify_lexer_set_minify_rules(lexer, remove_space, remove_comments);
  
  block = jitify_malloc(p, block_size);
  while ((bytes_read = read(fd, block, block_size)) > 0) {
    const char *err;
    jitify_lexer_scan(lexer, block, bytes_read, false);
    err = jitify_lexer_get_err(lexer);
    if (err) {
      size_t err_len = 20;
      size_t max_len = (block + bytes_read) - err;
      char *err_buf;
      if (err_len > max_len) {
        err_len = max_len;
      }
      err_buf = jitify_malloc(p, err_len + 1);
      memcpy(err_buf, err, err_len);
      err_buf[err_len] = 1;
      fprintf(stderr, "parsing error detected near '%s'\n", err_buf);
    }
  }
  if (bytes_read == 0) {
    jitify_lexer_scan(lexer, "NULL", 0, true);
  }
  bytes_in = jitify_lexer_get_bytes_in(lexer);
  bytes_out = jitify_lexer_get_bytes_out(lexer);
  duration = jitify_lexer_get_processing_time(lexer);
  if (bytes_in) {
    fprintf(stderr, "%lu bytes in, %lu bytes out, %lu usec (%lu nsec/byte)\n",
      (unsigned long)bytes_in, (unsigned long)bytes_out, (unsigned long)duration,
      (unsigned long)((1000 * duration)/bytes_in));
  }
  
  jitify_free(p, block);
  jitify_lexer_destroy(lexer);
  jitify_output_stream_destroy(out);
  jitify_pool_destroy(p);
}

#define OPT_MINIFY 1
#define OPT_BLOCK_SIZE 2
#define OPT_MAX_SETASIDE 3

int main(int argc, char **argv)
{
  struct option opts[] = {
    { "css", no_argument, &content_type, CONTENT_TYPE_CSS },
    { "html", no_argument, &content_type, CONTENT_TYPE_HTML },
    { "js", no_argument, &content_type, CONTENT_TYPE_JS },
    { "max-setaside", required_argument, NULL, OPT_MAX_SETASIDE },
    { "remove-space", no_argument, &remove_space, 1 },
    { "remove-comments", no_argument, &remove_comments, 1},
    { "minify", no_argument, NULL, OPT_MINIFY },
    { "block-size", required_argument, NULL, OPT_BLOCK_SIZE },
    { NULL, 0, 0, 0 }
  };
  int opt;
  int fd;
  
  do {
    opt = getopt_long(argc, argv, "", opts, NULL);
    switch (opt) {
      case OPT_MINIFY:
      remove_space = 1;
      remove_comments = 1;
      break;
      case OPT_BLOCK_SIZE:
      block_size = atoi(optarg);
      break;
      case OPT_MAX_SETASIDE:
      max_setaside = atoi(optarg);
      break;
    }
  } while (opt != -1);
  argc -= optind;
  argv += optind;
  if (argc > 1) {
    usage();
    return 1;
  }
  else if (argc < 1) {
    if (content_type == 0) {
      usage();
      return 1;
    }
    fd = 0;
  }
  else {
    if (content_type == 0) {
      content_type = get_content_type(*argv);
      if (content_type == 0) {
        fprintf(stderr, "%s: cannot determine content-type of %s\n", PROGRAM_NAME, *argv);
        fprintf(stderr, "  (use --css, --html, or --js to specify)\n");
        return 2;
      }
    }
    fd = open(*argv, O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "%s: cannot read %s\n", PROGRAM_NAME, *argv);
      return 3;
    }
  }
  process_file(fd);
  if (fd != 0) {
    close(fd);
  }
  return 0;
}
