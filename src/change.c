#include "change.h"
#include "cmd.h"
#include <srutils/srepo/data.h>
#include <limits.h>

#if SREPO_XPATH_SIZE > SSIZE_MAX
/*
 * Make sure cly_change_path_make_with_preds() may return the maximum data path
 * size supported.
 */
#error Srepo maximum path size unsupported !
#endif /* SREPO_XPATH_SIZE > SSIZE_MAX */

/******************************************************************************
 * Change (x)path predicates handling.
 ******************************************************************************/

static
unsigned int
cly_change_preds_count(const struct cly_change_preds * predicates)
{
	cly_change_assert_preds(predicates);

	return predicates->cnt;
}

static const struct cly_change_key *
cly_change_peek_pred(const struct cly_change_preds * predicates,
                     unsigned int                    index)
{
	cly_change_assert_preds(predicates);
	cly_assert(index <= predicates->cnt);

	return (index < predicates->cnt) ? &predicates->keys[index] : NULL;
}

int
cly_change_push_pred(struct cly_change_preds * predicates,
                     const struct lysc_node *  node,
                     const char *              value)
{
	cly_change_assert_preds(predicates);
	cly_assert(predicates->cnt < predicates->nr);
	cly_assert(node);
	cly_assert(value);

	struct cly_change_key * key;
	unsigned int            k;

	for (k = 0; k < predicates->cnt; k++) {
		if (predicates->keys[k].scn == node)
			return -EEXIST;
	}

	cly_assert(k == predicates->cnt);
	predicates->cnt = k + 1;

	key = &predicates->keys[k];
	key->scn = node;
	key->val = value;

	return 0;
}

struct cly_change_preds *
cly_change_create_preds(unsigned int nr)
{
	cly_assert(nr);
	cly_assert(nr <= CLY_CHANGE_PRED_NR);

	struct cly_change_preds * preds;

	preds = cly_malloc(sizeof(*preds) + (nr * sizeof(preds->keys[0])));
	cly_assert(preds);

	preds->cnt = 0;
	preds->nr = nr;

	return preds;
}

/******************************************************************************
 * Change (x)path handling.
 ******************************************************************************/

static
unsigned int
cly_change_path_count(const struct cly_change_path * path)
{
	cly_change_path_assert(path);

	return path->cnt;
}

static
int
cly_change_path_push(struct cly_change_path * path,
                     const struct lysc_node * node)
{
	cly_change_path_assert(path);
	cly_assert(node);

	if (path->cnt == path->nr) {
		unsigned int nr = 2 * path->nr;

		if (nr >= CLY_CHANGE_PATH_MAX)
			return -ENOBUFS;

		path->nr = nr;
		path->nodes = cly_realloc(path->nodes,
		                          nr * sizeof(path->nodes[0]));
		cly_assert(path->nodes);
	}

	path->nodes[path->cnt++] = node;

	return 0;
}

static void
cly_change_path_invert(struct cly_change_path * path)
{
	cly_change_path_assert(path);

	unsigned int cnt = path->cnt / 2;
	unsigned int c;

	for (c = 0; c < cnt; c++) {
		unsigned int             end = path->cnt - c - 1;
		const struct lysc_node * tmp = path->nodes[c];

		path->nodes[c] = path->nodes[end];
		path->nodes[end] = tmp;
	}
}

static void
cly_change_path_shrink(struct cly_change_path * path)
{
	cly_change_path_assert(path);

	if (path->cnt) {
		path->nodes = cly_realloc(path->nodes,
		                          path->cnt * sizeof(path->nodes[0]));
		cly_assert(path->nodes);
		path->nr = path->cnt;
	}
}

#define CLY_CHANGE_PATH_INIT_NR (4U)

static void
cly_change_path_init(struct cly_change_path * path)
{
	cly_assert(path);

	path->cnt = 0;
	path->nr = CLY_CHANGE_PATH_INIT_NR;
	path->nodes = cly_malloc(CLY_CHANGE_PATH_INIT_NR *
	                         sizeof(path->nodes[0]));
	cly_assert(path->nodes);
}

int
cly_change_path_init_from_node(struct cly_change_path * path,
                               const struct lysc_node * node)
{
	cly_assert(path);
	cly_assert(node);

	cly_change_path_init(path);

	do {
		cly_assert(srepo_sch_conf_flags(node) == LYS_CONFIG_W);
		cly_assert(!(srepo_sch_status_flags(node) &
		             (LYS_STATUS_OBSLT | LYS_STATUS_DEPRC)));

		if (!(srepo_sch_node_type(node) &
		      (LYS_CHOICE | LYS_CASE | LYS_INPUT | LYS_OUTPUT)))
			/* Skip schema only nodes. */
			cly_change_path_push(path, node);
		node = srepo_sch_node_parent(node);
	} while (node);

	if (!path->cnt)
		return -ENOENT;

	cly_change_path_shrink(path);
	cly_change_path_invert(path);

	return 0;
}

static
ssize_t
cly_change_join_path(char *                     path,
                     const struct lys_module ** module,
                     const struct lysc_node *   node,
                     size_t                     size)
{
	cly_assert(path);
	cly_assert(node);
	cly_assert(size <= SREPO_XPATH_SIZE);

	const char * name = srepo_sch_node_name(node);
	size_t       len = strnlen(name, SREPO_XPATH_NODE_SIZE);

	cly_assert(len);
	cly_assert(len <= SREPO_XPATH_SIZE);

	size = stroll_min(size, (size_t)SREPO_XPATH_NODE_SIZE);

	if (*module != node->module) {
		const char * pref = node->module->name;
		size_t       plen = strnlen(pref, SREPO_XPATH_NODE_SIZE);

		cly_assert(plen);
		cly_assert(plen <= SREPO_XPATH_NODE_SIZE);
		if ((1 + plen + 1 + len) >= size)
			return -ENOBUFS;

		path[0] = '/';
		memcpy(&path[1], pref, plen);
		path[1 + plen] = ':';
		memcpy(&path[1 + plen + 1], name, len);

		*module = node->module;

		return 1 + (ssize_t)plen + 1 + (ssize_t)len;
	}
	else {
		if ((1 + len) >= size)
			return -ENOBUFS;

		path[0] = '/';
		memcpy(&path[1], name, len);

		return 1 + (ssize_t)len;
	}
}

static
ssize_t
cly_change_fill_path_pred(char *                    path,
                          const struct lysc_node *  node,
                          const char *              value,
                          size_t                    size)
{
	cly_assert(path);
	cly_assert(node);
	cly_assert(srepo_sch_is_key(node));
	cly_assert(value);
	cly_assert(size <= SREPO_XPATH_SIZE);

	const char * name = srepo_sch_node_name(node);
	size_t       len = strnlen(name, SREPO_XPATH_NODE_SIZE);
	size_t       vlen = strnlen(value, SREPO_XPATH_SIZE);

	cly_assert(len);
	cly_assert(len <= SREPO_XPATH_NODE_SIZE);
	cly_assert(vlen);
	cly_assert(vlen <= SREPO_XPATH_SIZE);

	if ((1 + len + 2 + vlen + 1) >= size)
		return -ENOBUFS;

	path[0] = '[';
	memcpy(&path[1], name, len);
	path[1 + len] = '=';
	path[1 + len + 1] = '\'';
	memcpy(&path[1 + len + 2], value, vlen);
	path[1 + len + 2 + vlen] = '\'';
	path[1 + len + 2 + vlen + 1] = ']';

	return 1 + (ssize_t)len + 2 + (ssize_t)vlen + 1 + 1;
}

static
int
cly_change_path_make_with_preds(char **                         data_path,
                                const struct cly_change_path *  change_path,
                                const struct cly_change_preds * predicates)
{
	cly_assert(data_path);
	cly_change_path_assert(change_path);
	cly_assert(cly_change_path_count(change_path));
	cly_change_assert_preds(predicates);
	cly_assert(cly_change_preds_count(predicates));
	cly_assert(cly_change_preds_full(predicates));

	char *                    pth;
	char *                    str;
	unsigned int              p;
	const struct lysc_node *  node;
	const struct lys_module * mod = NULL;
	size_t                    sz = SREPO_XPATH_SIZE;
	ssize_t                   ret;
	unsigned int              k = 0;

	pth = cly_malloc(SREPO_XPATH_SIZE);
	cly_assert(pth);

	str = pth;
	cly_change_path_foreach(change_path, p, node) {
		ret = cly_change_join_path(str, &mod, node, sz);
		cly_assert(ret);
		if (ret < 0)
			goto free;

		cly_assert((size_t)ret < sz);
		str += ret;
		sz -= ret;

		if (srepo_sch_node_type(node) == LYS_LIST) {
			while (true) {
				const struct cly_change_key * key;

				key = cly_change_peek_pred(predicates, k);
				if (!key ||
				    srepo_sch_node_parent(key->scn) != node)
					break;

				ret = cly_change_fill_path_pred(str,
				                                key->scn,
				                                key->val,
				                                sz);
				cly_assert(ret);
				if (ret < 0)
					goto free;

				cly_assert((size_t)ret < sz);
				str += ret;
				sz -= ret;

				k++;
			}
		}
	}

	cly_assert(p == cly_change_path_count(change_path));
	cly_assert(sz);
	cly_assert(sz < SREPO_XPATH_SIZE);
	cly_assert(k);
	cly_assert(k == cly_change_preds_count(predicates));
	*str = '\0';
	*data_path = pth;

	return (ssize_t)(SREPO_XPATH_SIZE - sz);

free:
	cly_free(pth);

	return ret;
}

/******************************************************************************
 * Batch of changes handling.
 ******************************************************************************/

#define cly_change_batch_foreach(_batch, _chg) \
	for (_chg = &(_batch)->changes[0]; \
	     _chg < &(_batch)->changes[(_batch)->cnt]; \
	     (_chg)++)

int
cly_change_push_batch(struct cly_change_batch * batch,
                      enum cly_change_type      type,
                      const struct lysc_node *  node,
                      const char *              value)
{
	cly_change_assert_batch(batch);
	cly_assert(batch->cnt < batch->nr);
	cly_change_assert_type(type);
	cly_assert(node);
	cly_assert((type == CLY_DELETE_CHANGE_TYPE) || value);

	struct cly_change * chg;
	unsigned int        c;

	for (c = 0; c < batch->cnt; c++) {
		if (batch->changes[c].scn == node)
			return -EEXIST;
	}

	cly_assert(c == batch->cnt);
	batch->cnt = c + 1;

	chg = &batch->changes[c];
	chg->type = type;
	chg->scn = node;
	chg->val = value;

	return 0;
}

static
sr_error_t
cly_change_exec(const struct cly_change * change,
                sr_session_ctx_t *        session,
                char *                    path,
                size_t                    length,
                const struct cly_cmd *    command)
{
	cly_change_assert(change);
	cly_assert(path);
	cly_assert(length);
	cly_assert(length < SREPO_XPATH_SIZE);
	cly_assert(session);
	cly_assert((srepo_session_dstore(session) == SR_DS_RUNNING) ||
	           (srepo_session_dstore(session) == SR_DS_CANDIDATE));
	cly_cmd_assert(command);

	const char * name;
	size_t       len;
	const char * msg;
	sr_error_t   ret = SR_ERR_INVAL_ARG;

	name = srepo_sch_node_name(change->scn);
	cly_assert(name);
	len = strnlen(name, SREPO_XPATH_NODE_SIZE);
	cly_assert(len);
	cly_assert(len < SREPO_XPATH_NODE_SIZE);
	if ((length + 1 + len) >= SREPO_XPATH_SIZE) {
		msg = "invalid change path";
		goto err;
	}

	path[length] = '/';
	memcpy(&path[length + 1], name, len);
	path[length + 1 + len] = '\0';

	switch (change->type) {
	case CLY_UPDATE_CHANGE_TYPE:
		cly_assert(srepo_sch_validate_value(change->scn,
		                                    change->val,
		                                    strlen(change->val),
		                                    NULL) == SR_ERR_OK);
		msg = "cannot update value";
		ret = srepo_dat_change_bypath(
			session,
			path,
			change->val,
			NULL,
			SR_EDIT_NON_RECURSIVE);
		break;

	case CLY_DELETE_CHANGE_TYPE:
		msg = "cannot delete value";
		ret = srepo_dat_delete_bypath(
			session,
			path,
			SR_EDIT_NON_RECURSIVE);
		break;

	default:
		cly_assert(0);
	}

	if (ret != SR_ERR_OK) {
err:
		cly_cmd_talk(command,
		             "'%s': %s: %s",
		             name,
		             msg,
		             sr_strerror(ret));
	}

	return ret;
}

int
cly_change_exec_batch(const struct cly_change_batch * batch,
                      char *                          path,
                      size_t                          length,
                      sr_session_ctx_t *              session,
                      const struct cly_cmd *          command)
{
	cly_change_assert_batch(batch);
	cly_assert(batch->cnt);
	cly_assert(path);
	cly_assert(length);
	cly_assert(length < SREPO_XPATH_SIZE);
	cly_assert(session);
	cly_assert((srepo_session_dstore(session) == SR_DS_RUNNING) ||
	           (srepo_session_dstore(session) == SR_DS_CANDIDATE));
	cly_cmd_assert(command);

	const struct cly_change * chg;
	sr_error_t                err;

	cly_change_batch_foreach(batch, chg) {
		err = cly_change_exec(chg, session, path, length, command);
		if (err != SR_ERR_OK)
			goto discard;
	}

	err = srepo_apply_changes(session);
	if (err != SR_ERR_OK) {
		cly_cmd_talk(command,
		             "cannot apply changes: %s",
		             sr_strerror(err));
		goto discard;
	}

	return 0;

discard:
	srepo_discard_changes(session);

	return -EPERM;
}

int
cly_change_exec_list_batch(const struct cly_change_batch * batch,
                           const struct cly_change_path *  path,
                           const struct cly_change_preds * predicates,
                           sr_session_ctx_t *              session,
                           const struct cly_cmd *          command)
{
	cly_change_assert_batch(batch);
	cly_assert(batch->cnt);
	cly_change_path_assert(path);
	cly_assert(cly_change_path_count(path));
	cly_change_assert_preds(predicates);
	cly_assert(cly_change_preds_full(predicates));
	cly_assert(session);
	cly_assert((srepo_session_dstore(session) == SR_DS_RUNNING) ||
	           (srepo_session_dstore(session) == SR_DS_CANDIDATE));
	cly_cmd_assert(command);

	char *                    dpth;
	ssize_t                   len;
	const struct cly_change * chg;
	sr_error_t                err;

	len = cly_change_path_make_with_preds(&dpth, path, predicates);
	cly_assert(len);
	if (len < 0) {
		cly_cmd_talk(command,
		             "cannot make change path: %s",
		             strerror(-(int)len));
		return (int)len;
	}

	cly_change_batch_foreach(batch, chg) {
		err = cly_change_exec(chg, session, dpth, len, command);
		if (err != SR_ERR_OK)
			goto discard;
	}

	err = srepo_apply_changes(session);
	if (err != SR_ERR_OK) {
		cly_cmd_talk(command,
		             "cannot apply changes: %s",
		             sr_strerror(err));
		goto discard;
	}

	cly_free(dpth);

	return 0;

discard:
	srepo_discard_changes(session);
	cly_free(dpth);

	return -EPERM;
}

struct cly_change_batch *
cly_change_create_batch(unsigned int nr)
{
	cly_assert(nr);
	cly_assert(nr <= CLY_CHANGE_BATCH_NR);

	struct cly_change_batch * batch;

	batch = cly_malloc(sizeof(*batch) + (nr * sizeof(batch->changes[0])));
	cly_assert(batch);

	batch->cnt = 0;
	batch->nr = nr;

	return batch;
}
