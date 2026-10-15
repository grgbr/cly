#ifndef _CLY_WORK_H
#define _CLY_WORK_H

#include "common.h"

struct cly_work;
struct cly_context;

typedef int cly_work_exec_fn(struct cly_work *, struct cly_context *);

typedef void cly_work_release_fn(struct cly_work *);

struct cly_work_ops {
	cly_work_exec_fn *    exec;
	cly_work_release_fn * release;
};

#define cly_assert_work_ops(_ops) \
	cly_assert(_ops); \
	cly_assert((_ops)->exec); \
	cly_assert((_ops)->release)

struct cly_work {
	const struct cly_work_ops * ops;
};

#define cly_assert_work(_work) \
	cly_assert(_work); \
	cly_assert_work_ops((_work)->ops)

static inline struct cly_work *
cly_create_work(size_t size, const struct cly_work_ops * ops)
{
	cly_assert(size >= sizeof(struct cly_work));
	cly_assert_work_ops(ops);

	struct cly_work * wk;

	wk = cly_malloc(size);
	wk->ops = ops;

	return wk;
}

static inline void
cly_null_release_work(struct cly_work * work __unused)
{
}

static inline void
cly_destroy_work(struct cly_work * work)
{
	cly_assert_work(work);

	work->ops->release(work);
	cly_free(work);
}

#endif /* _CLY_WORK_H */
