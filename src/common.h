#ifndef _CLY_COMMON_H
#define _CLY_COMMON_H

#include "config.h"
#include <elog/elog.h>
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

#define cly_talk(_format, ...) \
	fprintf(stderr, \
	        "%s: " _format ".\n", \
	        program_invocation_short_name, \
	        ## __VA_ARGS__)

extern void
cly_init_log(enum elog_severity severity);

#if defined(CONFIG_CLY_LOG)

extern struct elog_stdio cly_logger;

#define cly_err(_format, ...) \
	elog_err(&cly_logger, _format ".", ## __VA_ARGS__)
#define cly_warn(_format, ...) \
	elog_warn(&cly_logger, _format ".", ## __VA_ARGS__)
#define cly_notice(_format, ...) \
	elog_notice(&cly_logger, _format ".", ## __VA_ARGS__)
#define cly_info(_format, ...) \
	elog_info(&cly_logger, _format ".", ## __VA_ARGS__)

#define cly_scn_log(_node, _svrt, _fmt, ...) \
	srepo_sch_log_node(_node, _svrt, "", _fmt, ## __VA_ARGS__)
#define cly_scn_err(_node, _fmt, ...) \
	cly_scn_log(_node, ELOG_ERR_SEVERITY, _fmt,  ## __VA_ARGS__)
#define cly_scn_warn(_node, _fmt, ...) \
	cly_scn_log(_node, ELOG_WARN_SEVERITY, _fmt, ## __VA_ARGS__)
#define cly_scn_notice(_node, _fmt, ...) \
	cly_scn_log(_node, ELOG_NOTICE_SEVERITY, _fmt, ## __VA_ARGS__)
#define cly_scn_info(_node, _fmt, ...) \
	cly_scn_log(_node, ELOG_INFO_SEVERITY, _fmt, ## __VA_ARGS__)

#define cly_dtn_log(_node, _svrt, _fmt, ...) \
	srepo_dat_log_node(_node, _svrt, "", _fmt, ## __VA_ARGS__)
#define cly_dtn_err(_node, _fmt, ...) \
	cly_dtn_log(_node, ELOG_ERR_SEVERITY, _fmt,  ## __VA_ARGS__)
#define cly_dtn_warn(_node, _fmt, ...) \
	cly_dtn_log(_node, ELOG_WARN_SEVERITY, _fmt, ## __VA_ARGS__)
#define cly_dtn_notice(_node, _fmt, ...) \
	cly_dtn_log(_node, ELOG_NOTICE_SEVERITY, _fmt, ## __VA_ARGS__)
#define cly_dtn_info(_node, _fmt, ...) \
	cly_dtn_log(_node, ELOG_INFO_SEVERITY, _fmt, ## __VA_ARGS__)

extern void
cly_fini_log(void);

#else  /* !defined(CONFIG_CLY_LOG) */

#define cly_err(_format, ...)
#define cly_warn(_format, ...)
#define cly_notice(_format, ...)
#define cly_info(_format, ...)

#define cly_scn_log(_node, _svrt, _fmt, ...)
#define cly_scn_err(_node, _fmt, ...)
#define cly_scn_warn(_node, _fmt, ...)
#define cly_scn_notice(_node, _fmt, ...)
#define cly_scn_info(_node, _fmt, ...)

#define cly_dtn_log(_node, _svrt, _fmt, ...)
#define cly_dtn_err(_node, _fmt, ...)
#define cly_dtn_warn(_node, _fmt, ...)
#define cly_dtn_notice(_node, _fmt, ...)
#define cly_dtn_info(_node, _fmt, ...)

static inline
void
cly_fini_log(void)
{
}

#endif /* defined(CONFIG_CLY_LOG) */

#if defined(CONFIG_CLY_DEBUG)

#define cly_debug(_format, ...) \
	elog_debug(&cly_logger, _format ".", ## __VA_ARGS__)

#define cly_scn_debug(_node, _fmt, ...) \
	cly_scn_log(_node, ELOG_DEBUG_SEVERITY, _fmt, ## __VA_ARGS__)

#define cly_dtn_debug(_node, _fmt, ...) \
	cly_dtn_log(_node, ELOG_DEBUG_SEVERITY, _fmt, ## __VA_ARGS__)

#else  /* defined(CONFIG_CLY_DEBUG) */

#define cly_debug(_format, ...)
#define cly_scn_debug(_node, _format, ...)
#define cly_dtn_debug(_node, _format, ...)

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
