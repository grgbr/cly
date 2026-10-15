#ifndef _CLY_EXPR_H
#define _CLY_EXPR_H

#include "common.h"

/******************************************************************************
 * Base command line expression handling.
 ******************************************************************************/

struct cly_expr {
	struct cly_expr * next;
	unsigned int      cnt;
	unsigned int      nr;
	const char **     args;
};

#define CLY_EXPR_ARGS_MAX  (CLY_LINE_MAX / 2U)

#define cly_expr_assert(_expr) \
	cly_assert(_expr); \
	cly_assert((_expr)->nr); \
	cly_assert((_expr)->nr <= CLY_EXPR_ARGS_MAX); \
	cly_assert((_expr)->cnt <= (_expr)->nr); \
	cly_assert((_expr)->args)

static inline unsigned int
cly_expr_arg_cnt(const struct cly_expr * expression)
{
	cly_expr_assert(expression);
	cly_assert(expression->cnt);

	return expression->cnt;
}

static inline const char * const *
cly_expr_args(const struct cly_expr * expression)
{
	cly_expr_assert(expression);
	cly_assert(expression->cnt);

	return expression->args;
}

extern int
cly_expr_parse_string(struct cly_expr * expression, char * string);

extern void
cly_expr_init(struct cly_expr * expression);

extern void
cly_expr_fini(struct cly_expr * expression);

/******************************************************************************
 * Command line expression block / sequence handling.
 ******************************************************************************/

struct cly_expr_blk {
	unsigned int       cnt;
	struct cly_expr *  head;
	struct cly_expr ** tail;
	char *             line;
};

#define CLY_EXPR_BLK_MAX (CLY_LINE_MAX / 4U)

#define cly_expr_blk_assert(_blk) \
	cly_assert(_blk); \
	cly_assert((_blk)->cnt <= CLY_EXPR_BLK_MAX); \
	cly_assert((_blk)->head || ((_blk)->tail == &(_blk)->head)); \
	cly_assert(!(_blk)->line || (*(_blk)->line != '\0')); \
	cly_assert(!(_blk)->line || \
	           (strnlen((_blk)->line, CLY_LINE_MAX) < CLY_LINE_MAX))

#define CLY_EXPR_BLK_INIT(_blk) \
	{ \
		.cnt  = 0, \
		.head = NULL, \
		.tail = &(_blk).head, \
		.line = NULL \
	}

#define cly_expr_blk_foreach(_blk, _expr) \
	for (_expr = (_blk)->head; _expr; _expr = (_expr)->next)

#define cly_expr_blk_foreach_safe(_blk, _expr, _tmp) \
	for (_expr = (_blk)->head; \
	     (_expr) && (_tmp = (_expr)->next, 1); \
	     _expr = (_tmp))

extern int
cly_expr_blk_parse_line(struct cly_expr_blk * block, char * line);

#if 0
/* Keep this just in case we need to save parsed line into history. */
extern ssize_t
cly_expr_blk_make_string(const struct cly_expr_blk * block,
                         char *                      string,
                         size_t                      size);
#endif

extern void
cly_expr_blk_init(struct cly_expr_blk * block);

extern void
cly_expr_blk_fini(struct cly_expr_blk * block);

#endif /* _CLY_EXPR_H */
