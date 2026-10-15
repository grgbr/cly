#ifndef _CLY_H
#define _CLY_H

#include "shell.h"
#include "expr.h"
#include "dir.h"
#include "work.h"
#include <stdbool.h>
#include <sysrepo.h>

/******************************************************************************
 * CLI style handling.
 ******************************************************************************/

#define CLY_RESET_COLOR        "\033[0m"
#define CLY_BOLD_COLOR         "\033[1m"
#define CLY_HALFBRIGHT_COLOR   "\033[2m"
#define CLY_UNDERLINE_COLOR    "\033[4m"
#define CLY_BLINK_COLOR        "\033[5m"
#define CLY_REVERSE_COLOR      "\033[7m"

/* Standard colors */
#define CLY_BLACK_COLOR        "\033[30m"
#define CLY_RED_COLOR          "\033[31m"
#define CLY_GREEN_COLOR        "\033[32m"
#define CLY_BROWN_COLOR        "\033[33m"
#define CLY_BLUE_COLOR         "\033[34m"
#define CLY_MAGENTA_COLOR      "\033[35m"
#define CLY_CYAN_COLOR         "\033[36m"
#define CLY_GRAY_COLOR         "\033[37m"

/* Bold variants */
#define CLY_DARK_GRAY_COLOR    "\033[1;30m"
#define CLY_BOLD_RED_COLOR     "\033[1;31m"
#define CLY_BOLD_GREEN_COLOR   "\033[1;32m"
#define CLY_BOLD_YELLOW_COLOR  "\033[1;33m"
#define CLY_BOLD_BLUE_COLOR    "\033[1;34m"
#define CLY_BOLD_MAGENTA_COLOR "\033[1;35m"
#define CLY_BOLD_CYAN_COLOR    "\033[1;36m"

#define CLY_WHITE_COLOR        "\033[1;37m"

enum cly_style_kind {
	CLY_LABEL_STYLE_KIND = 0,
	CLY_VALUE_STYLE_KIND,
	CLY_DEFAULT_STYLE_KIND,
	CLY_ERROR_STYLE_KIND,
	CLY_STYLE_KIND_NR
};

static inline const char *
cly_style_get_color(const char * const * style, enum cly_style_kind kind)
{
	cly_assert(kind >= 0);
	cly_assert(kind < CLY_STYLE_KIND_NR);

	if (style) {
		cly_assert(!style[kind] || style[kind][0] != '\0');
		return style[kind];
	}
	else
		return NULL;
}

/******************************************************************************
 * Global CLI context
 ******************************************************************************/

/*
 * The number of differed work MUST be large enough to support, at least , a
 * full block of expressions !
 */
#define CLY_WORK_NR (128U)
#if CLY_WORK_NR < CLY_EXPR_BLK_MAX
#undef CLY_WORK_NR
#define CLY_WORK_NR CLY_EXPR_BLK_MAX
#endif /* CLY_WORK_NR < CLY_EXPR_BLK_MAX */

struct cly_context {
	sr_conn_ctx_t *        conn;
	sr_session_ctx_t *     sess;
	const struct ly_ctx *  lyctx;
	unsigned int           wkcnt;
	struct cly_work *      wkq[CLY_WORK_NR];
	struct cly_dir         root;
	const struct cly_dir * cwd;
	struct ly_out *        lyout;
	bool                   isatty;
	bool                   colored;
	const char * const *   style;
	bool                   interact;
};

#define cly_assert_context(_ctx) \
	cly_assert(_ctx); \
	cly_assert((_ctx)->conn); \
	cly_assert((_ctx)->sess); \
	cly_assert((_ctx)->lyctx); \
	cly_dir_assert(&(_ctx)->root); \
	cly_dir_assert((_ctx)->cwd); \
	cly_assert((_ctx)->lyout)

extern unsigned int
cly_term_cols(const struct cly_context * context)
	__cly_nonull(1) __warn_result;

static inline bool
cly_isatty(const struct cly_context * context)
{
	cly_assert_context(context);

	return context->isatty;
}

static inline bool
cly_has_colors(const struct cly_context * context)
{
	cly_assert_context(context);

	return context->colored;
}

static inline const char * const *
cly_get_style(const struct cly_context * context)
{
	cly_assert_context(context);

	return context->style;
}

static inline bool
cly_isinteractive(const struct cly_context * context)
{
	cly_assert_context(context);

	return context->interact;
}

static inline const struct cly_dir *
cly_cwd(const struct cly_context * context)
{
	cly_assert_context(context);

	return context->cwd;
}

extern void
cly_chdir(struct cly_context * context, const struct cly_dir * directory);

static inline int
cly_sched_work(struct cly_context * context, struct cly_work * work)
{
	cly_assert_context(context);
	cly_assert_work(work);

	if (context->wkcnt >= CLY_WORK_NR)
		return -EBUSY;

	context->wkq[context->wkcnt++] = work;

	return 0;
}

#endif /* _CLY_H */
