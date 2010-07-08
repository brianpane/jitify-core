cat << END >> $NGX_MAKEFILE
MOD_JITIFY_SRC_DIR=$ngx_addon_dir
END

cat << 'END' >> $NGX_MAKEFILE
CFLAGS += -Werror -Wall -I$(MOD_JITIFY_SRC_DIR) -I$(MOD_JITIFY_SRC_DIR)/../core
END
