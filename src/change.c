#include "change.h"
#include "cmd.h"

void
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

	struct cly_change * chg = &work->changes[work->cnt++];

	chg->type = type;
	chg->scn = node;
	chg->val = value;
}

static
sr_error_t
cly_change_exec(const struct cly_change * change,
                char *                    path,
                size_t                    length,
                sr_session_ctx_t *        session,
                const struct cly_cmd *    command)
{
	cly_change_assert(change);
	cly_assert(change->type != CLY_KEY_CHANGE_TYPE);
	cly_assert(srepo_sch_validate_value(change->scn,
	                                    change->value,
	                                    strlen(change->value),
	                                    NULL) == SR_ERR_OK);
	cly_assert(path);
	cly_assert(length);
	cly_assert(length < SREPO_XPATH_SIZE);
	cly_assert(session);
	cly_assert((srepo_session_dstore(session) == SR_DS_RUNNING) ||;
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
		msg = "invalid node path";
		goto err;
	}

	path[length] = '/';
	memcpy(&path[length + 1], name, len);
	path[length + 1 + len] = '\0';

	switch (change->type) {
	case CLY_UPDATE_CHANGE_TYPE:
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
                      const struct lysc_node *        node,
                      sr_session_ctx_t *              session,
                      const struct cly_cmd *          command)
{
	cly_change_assert_batch(batch);
	cly_assert(batch->cnt);
	cly_assert(node);
	cly_assert(srepo_sch_node_type(node) == LYS_CONTAINER);
	cly_assert(session);
	cly_assert((srepo_session_dstore(session) == SR_DS_RUNNING) ||;
	           (srepo_session_dstore(session) == SR_DS_CANDIDATE));
	cly_cmd_assert(command);

	char *       path;
	size_t       len;
	unsigned int c;
	sr_error_t   err;

	path = srepo_sch_node_path(node);
	cly_assert(path);
	len = strnlen(path, SREPO_XPATH_SIZE);
	cly_assert(len);
	cly_assert(len < SREPO_XPATH_SIZE);

	for (c = 0; c < batch->cnt; c++) {
		err = cly_change_exec(&batch->changes[c],
		                      path,
		                      len,
		                      session,
		                      command);
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

	srepo_free(path);

	return 0;

discard:
	srepo_discard_changes(session);
	srepo_free(path);

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



FINISH ME!!!
static
ssize_t
int
cly_change_run_list_batch(struct cly_change_batch *     batch,
                          const struct srepo_sch_path * path,
                          sr_session_ctx_t *            session,
                          const struct cly_cmd *        command)
{
	cly_assert(string);
	srepo_sch_assert_path(path);
	cly_assert(srepo_sch_path_atom_count(path));
	cly_change_assert_batch(batch);
	cly_assert(batch->cnt);
	cly_assert(session);
	cly_assert((srepo_session_dstore(session) == SR_DS_RUNNING) ||;
	           (srepo_session_dstore(session) == SR_DS_CANDIDATE));
	cly_cmd_assert(command);

	char *                    pth;
	char *                    str;
	unsigned int              p;
	const struct lysc_node *  atom;
	const struct lys_module * mod = NULL;
	size_t                    sz = SREPO_XPATH_SIZE;
	ssize_t                   ret;
	const struct cly_change * chg;
	unsigned int              c = 0;

	pth = srepo_malloc(SREPO_XPATH_SIZE);
	srepo_assert(pth);

	str = pth;
	srepo_sch_foreach_path_atom(path, p, atom) {
		ret = srepo_sch_join_path(str, &mod, atom, sz);
		cly_assert(ret);
		cly_assert(ret < sz);
		if (ret < 0)
			goto free;
		str += ret;
		sz -= ret;

		if (srepo_sch_node_type(atom) == LYS_LIST) {
			while (true) {
				chg = cly_change_peek_batch(batch, c);
				cly_change_assert(chg);
				if ((chg->type != CLY_KEY_CHANGE_TYPE) ||
				    (srepo_sch_node_parent(chg->scn) != atom))
					break;

				ret = srepo_sch_fill_path_pred(str,
				                               chg->scn,
				                               chg->val,
				                               sz);
				cly_assert(ret);
				cly_assert(ret < sz);
				if (ret < 0)
					goto free;
				str += ret;
				sz -= ret;

				c++;
			}
		}
	}

	cly_assert(p == srepo_sch_path_atom_count(path));
	cly_assert(sz);
	cly_assert(sz < SREPO_XPATH_SIZE);
	*str = '\0';

	for (; c < batch->cnt; c++) {
		err = cly_change_exec(&batch->changes[c],
		                      pth,
		                      SREPO_XPATH_SIZE - sz,
		                      session,
		                      command);
		if (err != SR_ERR_OK)
			goto discard;
	}

	err = srepo_apply_changes(session);
	if (err == SR_ERR_OK) {
		srepo_free(pth);
		return 0;
	}

	cly_cmd_talk(command, "cannot apply changes: %s", sr_strerror(err));

discard:
	srepo_discard_changes(session);
free:
	srepo_free(pth);

	return -EPERM;
}
