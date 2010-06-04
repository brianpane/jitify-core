#ifndef jitify_apache_glue_h
#define jitify_apache_glue_h

#include <ap_config.h>
#include <httpd.h>

#include "jitify.h"

extern jitify_pool_t *jitify_apache_pool_create(apr_pool_t *pool);

extern jitify_output_stream_t *jitify_apache_output_stream_create(jitify_pool_t *pool);

#endif /* !defined(jitify_apache_glue_h) */
