#ifndef _CLY_COMMON_H
#define _CLY_COMMON_H

#include "config.h"
#include <srutils/srepo/xpath.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

/*
 * Maximum size available to store a command line including the terminating NULL
 * byte.
 */
#define CLY_LINE_MAX \
	STROLL_CONCAT(CONFIG_CLY_LINE_MAX, U)
#if CLY_LINE_MAX > SSIZE_MAX
#error Invalid maximum input line size !
#endif /* CLY_LINE_MAX > SSIZE_MAX */

/*
 * Maximum size available to store a xpath including the terminating NULL byte.
 */
#define CLY_XPATH_MAX \
	SREPO_XPATH_SIZE
#if CLY_XPATH_MAX > SSIZE_MAX
#error Invalid maximum XPATH size !
#endif /* CLY_XPATH_MAX > SSIZE_MAX */


/******************************************************************************
 * Utilities
 ******************************************************************************/

#if defined(CONFIG_CLY_ASSERT)

#include <stroll/assert.h>

#define __cly_nonull(...)

#define cly_assert(_cond) \
	stroll_assert("cly", _cond)

#else  /* !defined(CONFIG_CLY_ASSERT) */

#define __cly_nonull(...) \
	__nonull(__VA_ARGS__)

#define cly_assert(...)

#endif /* defined(CONFIG_CLY_ASSERT) */

#define cly_log(_format, ...) \
	fprintf(stderr, \
	        "%s: " _format ".\n", \
	        program_invocation_short_name, \
	        ## __VA_ARGS__)

extern void
cly_log_scn(const struct lysc_node * node, const char * format, ...)
	__cly_nonull(1, 2) __printf(2, 3);

#if defined(CONFIG_CLY_LOG)

#include <elog/elog.h>

extern enum elog_severity cly_log_lvl;

#endif /* defined(CONFIG_CLY_LOG) */

#if defined(CONFIG_CLY_DEBUG)

#define cly_debug(_format, ...) \
	((cly_log_lvl >= ELOG_DEBUG_SEVERITY) \
	 ? (void)cly_log(_format, ## __VA_ARGS__) \
	 : (void)0)

#define cly_debug_scn(_node, _format, ...) \
	((cly_log_lvl >= ELOG_DEBUG_SEVERITY) \
	 ? cly_log_scn(_node, _format, ## __VA_ARGS__) \
	 : (void)0)

#else  /* defined(CONFIG_CLY_DEBUG) */

#define cly_debug(_format, ...)
#define cly_debug_scn(_node, _format, ...)

#endif /* defined(CONFIG_CLY_DEBUG) */

static inline __noreturn
void
cly_abort(void)
{
	srepo_abort();
}

static inline __alloc_size(1) __returns_nonull __warn_result
void *
cly_malloc(size_t size)
{
	cly_assert(size);

	return srepo_malloc(size);
}

extern void *
cly_realloc(void * data, size_t size)
	__alloc_size(2) __returns_nonull __warn_result;

static inline
void
cly_free(void * data)
{
	srepo_free(data);
}

extern char *
cly_strdup(const char * string)
	__cly_nonull(1) __returns_nonull __warn_result;

static inline __srepo_nonull(1, 2) __printf(2, 3) __warn_result
int
cly_asprintf(char ** string, const char * format, ...)
{
	cly_assert(string);
	cly_assert(format);

	va_list args;
	int     ret;

	va_start(args, format);
	ret = srepo_vasprintf(string, format, args);
	va_end(args);

	return ret;
}

#if defined(CONFIG_CLY_ASSERT)

extern void
cly_assert_args(int argc, const char * const argv[]);

#else  /* !defined(CONFIG_CLY_ASSERT) */

static inline
void
cly_assert_args(int argc __unused, const char * const argv[] __unused)
{
}

#endif /* defined(CONFIG_CLY_ASSERT) */

static inline
int
cly_render_chr(int chr)
{
	return isprint(chr) ? chr : '?';
}

#define CLY_RENDER_MAX (16U)

extern const char *
cly_render_string(const char * input, char output[CLY_RENDER_MAX])
	__cly_nonull(1, 2) __returns_nonull;

#endif /* _CLY_COMMON_H */
