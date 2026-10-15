#ifndef _CLY_COMMON_H
#define _CLY_COMMON_H

#include <stroll/cdefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

/*
 * Maximum size available to store a command line including the terminating NULL
 * byte.
 */
#define CONFIG_CLY_LINE_MAX 1024
#if CONFIG_CLY_LINE_MAX > SSIZE_MAX
#error Invalid maximum input line size !
#endif /* CONFIG_CLY_LINE_MAX > SSIZE_MAX */

/*
 * Maximum size available to store a xpath including the terminating NULL byte.
 */
#define CONFIG_CLY_XPATH_MAX 128
#if CONFIG_CLY_XPATH_MAX > SSIZE_MAX
#error Invalid maximum XPATH size !
#endif /* CONFIG_CLY_XPATH_MAX > SSIZE_MAX */

#define CLY_LINE_MAX  STROLL_CONCAT(CONFIG_CLY_LINE_MAX, U)
#define CLY_XPATH_MAX STROLL_CONCAT(CONFIG_CLY_XPATH_MAX, U)

/******************************************************************************
 * Utilities
 ******************************************************************************/

#if defined(CONFIG_CLY_ASSERT)
#include <assert.h>

#define cly_assert(...) assert(__VA_ARGS__)

#else  /* !defined(CONFIG_CLY_ASSERT) */

#define cly_assert(...)

#endif /* defined(CONFIG_CLY_ASSERT) */

#define cly_log(_format, ...) \
	fprintf(stderr, \
	        "%s: " _format "\n", \
	        program_invocation_short_name, \
	        ## __VA_ARGS__)

#if defined(CONFIG_CLY_LOG)

#include <sysrepo_types.h>

extern sr_log_level_t cly_log_lvl;

#endif /* defined(CONFIG_CLY_LOG) */

#if defined(CONFIG_CLY_DEBUG)

#if !defined(CONFIG_CLY_LOG)
#error Inconsistent debug / logging build configuration !
#endif /* !defined(CONFIG_CLY_LOG) */

#define cly_dbg(_format, ...) \
	((cly_log_lvl >= SR_LL_DBG) ? (void)cly_log(_format, ## __VA_ARGS__) \
	                            : (void)0)

#else  /* defined(CONFIG_CLY_DEBUG) */

#define cly_dbg(_format, ...)

#endif /* defined(CONFIG_CLY_DEBUG) */

#if defined(CONFIG_CLY_ASSERT)

extern void
cly_assert_args(int argc, const char * const argv[]);

#else  /* !defined(CONFIG_CLY_ASSERT) */

static inline void
cly_assert_args(int argc __unused, const char * const argv[] __unused)
{
}

#endif /* defined(CONFIG_CLY_ASSERT) */

extern void *
cly_malloc(size_t size);

extern void *
cly_realloc(void * data, size_t size);

extern char *
cly_strdup(const char * string);

extern int
cly_asprintf(char ** string, const char * format, ...) __printf(2, 3);

static inline void
cly_free(void * data)
{
	free(data);
}

static inline int
cly_render_chr(int chr)
{
	return isprint(chr) ? chr : '?';
}

#define CLY_RENDER_MAX (16U)

extern const char *
cly_render_string(const char * input, char output[CLY_RENDER_MAX]);

#endif /* _CLY_COMMON_H */
