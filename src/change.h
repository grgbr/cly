#ifndef _CLY_CHANGE_H
#define _CLY_CHANGE_H

#include "common.h"

struct cly_cmd;
struct lysc_node;

/******************************************************************************
 * Change (x)path handling.
 ******************************************************************************/

#define CLY_CHANGE_PATH_MAX \
	((SREPO_XPATH_SIZE - 1) / 2U)

struct cly_change_path {
	unsigned int              cnt;
	unsigned int              nr;
	const struct lysc_node ** nodes;
};

#define cly_change_path_assert(_path) \
	cly_assert(_path); \
	cly_assert((_path)->nr <= CLY_CHANGE_PATH_MAX); \
	cly_assert((_path)->cnt <= (_path)->nr); \
	cly_assert((_path)->nodes)

#define cly_change_path_foreach(_path, _cnt, _node) \
	for (_cnt = 0; \
	     (_cnt < (_path)->cnt) && (_node = (_path)->nodes[_cnt], 1); \
	     _cnt++)

extern int
cly_change_path_init_from_node(struct cly_change_path * path,
                               const struct lysc_node * node)
	__cly_nonull(1, 2) __warn_result;

static inline __cly_nonull(1)
void
cly_change_path_fini(struct cly_change_path * path)
{
	cly_change_path_assert(path);

	cly_free(path->nodes);
}

/******************************************************************************
 * Change (x)path predicates handling.
 ******************************************************************************/

struct cly_change_key {
	const struct lysc_node * scn;
	const char *             val;
};

#define cly_change_assert_key(_key) \
	cly_assert(_key); \
	cly_assert((_key)->scn); \
	cly_assert((_key)->val)

struct cly_change_preds {
	unsigned int          cnt;
	unsigned int          nr;
	struct cly_change_key keys[];
};

#define CLY_CHANGE_PRED_NR \
	(CLY_CHANGE_PATH_MAX / SREPO_XPATH_NODE_SIZE)
#if CLY_CHANGE_PRED_NR < 1
#error Invalid maximum number of predicates !
#endif /* CLY_CHANGE_PRED_NR < 1 */

#define cly_change_assert_preds(_preds) \
	cly_assert(_preds); \
	cly_assert((_preds)->nr <= CLY_CHANGE_PRED_NR); \
	cly_assert((_preds)->cnt <= (_preds)->nr)

static inline
bool
cly_change_preds_full(const struct cly_change_preds * predicates)
{
	cly_change_assert_preds(predicates);

	return predicates->cnt == predicates->nr;
}

extern int
cly_change_push_pred(struct cly_change_preds * predicates,
                     const struct lysc_node *  node,
                     const char *              value)
	__cly_nonull(1, 2, 3) __warn_result;

extern struct cly_change_preds *
cly_change_create_preds(unsigned int nr);

static inline
void
cly_change_destroy_preds(struct cly_change_preds * predicates)
{
	cly_change_assert_preds(predicates);

	cly_free(predicates);
}

/******************************************************************************
 * Batch of changes handling.
 ******************************************************************************/

enum cly_change_type {
	CLY_UPDATE_CHANGE_TYPE = 0,
	CLY_DELETE_CHANGE_TYPE,
	CLY_CHANGE_TYPE_NR
};

#define cly_change_assert_type(_type) \
	cly_assert(_type >= 0); \
	cly_assert(_type < CLY_CHANGE_TYPE_NR)

struct cly_change {
	enum cly_change_type     type;
	const struct lysc_node * scn;
	const char *             val;
};

#define cly_change_assert(_chg) \
	cly_assert(_chg); \
	cly_assert((_chg)->scn); \
	cly_change_assert_type((_chg)->type); \
	cly_assert(((_chg)->type == CLY_DELETE_CHANGE_TYPE) || (_chg)->val)

struct cly_change_batch {
	unsigned int      cnt;
	unsigned int      nr;
	struct cly_change changes[];
};

#define CLY_CHANGE_BATCH_NR (128U)

#define cly_change_assert_batch(_batch) \
	cly_assert(_batch); \
	cly_assert((_batch)->nr <= CLY_CHANGE_BATCH_NR); \
	cly_assert((_batch)->cnt <= (_batch)->nr)

static inline
unsigned int
cly_change_batch_count(const struct cly_change_batch * batch)
{
	cly_change_assert_batch(batch);

	return batch->cnt;
}

static inline
bool
cly_change_batch_empty(const struct cly_change_batch * batch)
{
	cly_change_assert_batch(batch);

	return !batch->cnt;
}

static inline
bool
cly_change_batch_full(const struct cly_change_batch * batch)
{
	cly_change_assert_batch(batch);

	return batch->cnt == batch->nr;
}

extern int
cly_change_push_batch(struct cly_change_batch * batch,
                      enum cly_change_type      type,
                      const struct lysc_node *  node,
                      const char *              value)
	__cly_nonull(1, 3) __warn_result;

extern int
cly_change_exec_batch(const struct cly_change_batch * batch,
                      char *                          path,
                      size_t                          length,
                      sr_session_ctx_t *              session,
                      const struct cly_cmd *          command)
	__cly_nonull(1, 2, 4, 5) __warn_result;

extern int
cly_change_exec_list_batch(const struct cly_change_batch * batch,
                           const struct cly_change_path *  path,
                           const struct cly_change_preds * predicates,
                           sr_session_ctx_t *              session,
                           const struct cly_cmd *          command)
	__cly_nonull(1, 2, 3, 4, 5) __warn_result;

extern struct cly_change_batch *
cly_change_create_batch(unsigned int nr);

static inline
void
cly_change_destroy_batch(struct cly_change_batch * batch)
{
	cly_change_assert_batch(batch);

	cly_free(batch);
}

#endif /* _CLY_CHANGE_H */
