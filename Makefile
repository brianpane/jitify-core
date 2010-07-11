# Edit the definition of APACHE_HOME to point to the base of your Apache installation
APACHE_HOME=$(HOME)/local/apache-2.2

# Locations of programs needed during compilation
APXS=$(APACHE_HOME)/bin/apxs
RAGEL=ragel

CFLAGS=-g -O3 -Werror -Wall
APXS_CFLAGS=-Wc,"$(CFLAGS)"
RAGELFLAGS=-G2

build/%.o:	src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -Isrc/core -o $@ $<

CORE_SRCS= \
	src/core/jitify_array.c         \
	src/core/jitify_css.c		\
	src/core/jitify_css_lexer.c	\
	src/core/jitify_html.c		\
	src/core/jitify_html_lexer.c	\
	src/core/jitify_js.c		\
	src/core/jitify_js_lexer.c	\
	src/core/jitify_lexer.c		\
	src/core/jitify_pool.c		\
	src/core/jitify_stream.c

CORE_TMP_OBJS= $(CORE_SRCS:%.c=%.o)
CORE_OBJS=$(CORE_TMP_OBJS:src/%=build/%)

all:
	@echo
	@echo "Jitify build options:"
	@echo
	@echo "    To build the Jitify command-line application,"
	@echo "        make tools"
	@echo
	@echo "    To build mod_jitify for Apache,"
	@echo "        make apache"
	@#echo
	@#echo "    To build mod_jitify for nginx,"
	@#echo "        make nginx"
	@echo
	@echo "Developer build options (useful only if you're modifying the Jitify core):"
	@echo
	@echo "    To regenerate all C source files from the Ragel grammar files,"
	@echo "        make ragel"
	@echo

build-prep:
	@mkdir -p build/core build/tools

ragel:
	$(RAGEL) $(RAGELFLAGS) -o src/core/jitify_css_lexer.c  src/core/jitify_css_lexer.rl
	$(RAGEL) $(RAGELFLAGS) -o src/core/jitify_html_lexer.c  src/core/jitify_html_lexer.rl
	$(RAGEL) $(RAGELFLAGS) -o src/core/jitify_js_lexer.c   src/core/jitify_js_lexer.rl

TOOL_TARGETS=build/jitify
TOOL_OBJS=$(CORE_OBJS) build/tools/jitify.o

tools:	$(TOOL_TARGETS)

build/jitify:	build-prep $(TOOL_OBJS) 
	$(CC) -o $@ $(TOOL_OBJS)

APACHE_TARGETS=build/mod_jitify.so
APACHE_SRCS=$(CORE_SRCS) src/apache/mod_jitify.c src/apache/jitify_apache_glue.c
apache:	build-prep $(APACHE_TARGETS)

build/mod_jitify.so:	$(APACHE_SRCS)
	$(APXS) -c -Wc,-g -Wc,-O3 -Wc,-Werror -Wc,-Wall -o $@ -Isrc/core $(APACHE_SRCS)
	cp -f build/.libs/mod_jitify.so build

clean:
	-rm -rf build/*
	-find src \( -name '*.o' -o -name '*.lo' -o -name '*.slo' \) -exec rm -f {} \;
	-find src -name '.libs' -exec rm -rf {} \;
