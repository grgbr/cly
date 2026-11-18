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
	     (_cnt < (_path)->cnt) && (_node = (_path)->nodes[_cnt], true); \
	     _cnt++)

static inline
unsigned int
cly_change_path_count(const struct cly_change_path * path)
{
	cly_change_path_assert(path);

	return path->cnt;
}

extern void
cly_change_path_init_from_node(struct cly_change_path * path,
                               const struct lysc_node * node);

static inline void
cly_change_path_fini(struct cly_change_path * path)
{
	cly_change_path_assert(path);

	cly_free(path->nodes);
}

/******************************************************************************
 * Batch of changes handling.
 ******************************************************************************/

enum cly_change_type {
	CLY_KEY_CHANGE_TYPE    = 0,
	CLY_UPDATE_CHANGE_TYPE,
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
const struct cly_change *
cly_change_peek_batch(const struct cly_change_batch * batch, unsigned int index)
{
	cly_change_assert_batch(batch);
	cly_assert(index <= batch->cnt);

	return &batch->changes[index];
}

extern void
cly_change_push_batch(struct cly_change_batch * batch,
                      enum cly_change_type      type,
                      const struct lysc_node *  node,
                      const char *              value);

extern int
cly_change_exec_batch(const struct cly_change_batch * batch,
                      sr_session_ctx_t *              session,
                      char *                          path,
                      size_t                          length,
                      const struct cly_cmd *          command);

extern int
cly_change_exec_list_batch(const struct cly_change_batch * batch,
                           sr_session_ctx_t *              session,
                           const struct cly_change_path *  path,
                           const struct cly_cmd *          command);

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
