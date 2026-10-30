#include "set.h"
#include "cly.h"
#include <srutils/srepo/data.h>

/******************************************************************************
 * Change work
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

struct cly_change_work {
	struct cly_work        super;
	const struct cly_cmd * cmd;
	unsigned int           cnt;
	unsigned int           nr;
	struct cly_change      changes[];
};

#define CLY_CHANGE_WORK_NR (128U)

#define cly_change_assert_work(_wk) \
	cly_assert(_wk); \
	cly_assert_work(&(_wk)->super); \
	cly_cmd_assert((_wk)->cmd); \
	cly_assert((_wk)->nr <= CLY_CHANGE_WORK_NR); \
	cly_assert((_wk)->cnt <= (_wk)->nr)

static int
cly_change_exec_work(struct cly_work * work, struct cly_context * context)
{
	cly_change_assert_work((const struct cly_change_work *)work);
	cly_cmd_assert(((const struct cly_change_work *)work)->cmd);
	cly_assert_context(context);

	sr_session_ctx_t *             sess = *cly_cand_session(context);
	const struct cly_change_work * wk = (const struct cly_change_work *)
	                                    work;

	if (sess) {
		unsigned int c;
		sr_error_t   err;

		for (c = 0; c < wk->cnt; c++) {
			cly_change_assert(&wk->changes[c]);

			const struct cly_change * chg = &wk->changes[c];
			char *                    path;
			const char *              msg;

			path = srepo_sch_node_path(chg->scn);

			switch (chg->type) {
			case CLY_UPDATE_CHANGE_TYPE:
				msg = "cannot update value";
				err = srepo_dat_change_bypath(
					sess,
					path,
					chg->val,
					NULL,
					SR_EDIT_NON_RECURSIVE);
				break;

			case CLY_DELETE_CHANGE_TYPE:
				msg = "cannot delete value";
				err = srepo_dat_delete_bypath(
					sess,
					path,
					SR_EDIT_NON_RECURSIVE);
				break;

			default:
				cly_assert(0);
			}

			srepo_free(path);

			if (err != SR_ERR_OK) {
				srepo_discard_changes(sess);
				cly_cmd_talk(wk->cmd,
				             "'%s': %s: %s",
				             srepo_sch_node_name(chg->scn),
				             msg,
				             sr_strerror(err));
				return -EPERM;
			}
		}

		err = srepo_apply_changes(sess);
		if (err != SR_ERR_OK) {
			cly_cmd_talk(wk->cmd,
			             "cannot apply changes: %s",
			             sr_strerror(err));
			return -EPERM;
		}

		return 0;
	}

	cly_cmd_talk(wk->cmd, "no ongoing edition");

	return -EPERM;
}

static const struct cly_work_ops cly_change_work_ops = {
	.exec    = cly_change_exec_work,
	.release = cly_null_release_work
};

static void
cly_change_work_push(struct cly_change_work * work,
                     enum cly_change_type     type,
                     const struct lysc_node * node,
                     const char *             val)
{
	cly_change_assert_work(work);
	cly_assert(work->cnt < work->nr);
	cly_change_assert_type(type);
	cly_assert(node);
	cly_assert((type == CLY_DELETE_CHANGE_TYPE) || val);

	struct cly_change * chg = &work->changes[work->cnt++];

	chg->type = type;
	chg->scn = node;
	chg->val = val;
}

static struct cly_change_work *
cly_change_create_work(unsigned int           nr,
                       const struct cly_cmd * command)
{
	cly_assert(nr);
	cly_assert(nr <= CLY_CHANGE_WORK_NR);
	cly_cmd_assert(command);

	struct cly_change_work * wk;

	wk = (struct cly_change_work *)
	     cly_create_work(sizeof(*wk) + (nr * sizeof(wk->changes[0])),
	                     &cly_change_work_ops);
	cly_assert(wk);

	wk->cmd = command;
	wk->cnt = 0;
	wk->nr = nr;

	return wk;
}

static inline void
cly_change_destroy_work(struct cly_change_work * work)
{
	cly_destroy_work(&work->super);
}

/******************************************************************************
 * Yang leaf parameter argument
 ******************************************************************************/

#include "arg.h"

struct cly_arg_leaf_parm {
	struct cly_arg_parm           super;
	const struct lysc_node_leaf * leaf;
};

#define cly_arg_assert_leaf_parm(_parm) \
	cly_assert(_parm); \
	cly_arg_assert_parm(&(_parm)->super); \
	cly_assert((_parm)->leaf)

static int
cly_arg_parse_leaf_parm(const struct cly_arg * argument,
                        const struct cly_cmd * command,
                        const struct cly_dir * directory,
                        struct cly_context *   context,
                        int                    argc __unused,
                        const char * const     argv[],
                        void *                 data)
{
	cly_arg_assert_leaf_parm((const struct cly_arg_leaf_parm *)argument);
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argc == 1);
	cly_assert(argv[0]);
	cly_assert(strnlen(argv[0], CLY_ARG_MAX) < CLY_ARG_MAX);

	const struct cly_arg_leaf_parm * parm =
		(const struct cly_arg_leaf_parm *)argument;
	const char *                     str = argv[0];
	size_t                           len;
	struct cly_change_work *         wk = data;

	len = strcspn(str, "=");
	if ((len != parm->super.len) ||
	    (str[len] != '=') ||
	    memcmp(str, parm->super.name, parm->super.len))
		return 0;

#warning Implement leaf value help and display it in case of error
	str = &str[len + 1];
	len = strlen(str);
	if (!len) {
		if (srepo_sch_is_mandatory((const struct lysc_node *)
		                           parm->leaf)) {
			cly_cmd_talk(command,
			             "missing %s parameter value",
			             parm->super.name);
			return -EINVAL;
		}

		cly_change_work_push(wk,
		                     CLY_DELETE_CHANGE_TYPE,
		                     (const struct lysc_node *)parm->leaf,
		                     NULL);
	}
	else if (!strcmp(str, "default")) {
		if (!srepo_sch_leaf_dflt(parm->leaf)) {
			cly_cmd_talk(command,
			             "%s parameter has no default value",
			             parm->super.name);
			return -EINVAL;
		}

		cly_change_work_push(wk,
		                     CLY_DELETE_CHANGE_TYPE,
		                     (const struct lysc_node *)parm->leaf,
		                     NULL);
	}
	else {
		sr_error_t err;

		err = srepo_sch_validate_value((const struct lysc_node *)
		                               parm->leaf,
		                               str,
		                               len,
		                               NULL);
		if (err != SR_ERR_OK) {
			cly_cmd_talk(command,
			             "'%s': invalid %s parameter value",
			             str,
			             parm->super.name);
			return -EINVAL;
		}

		cly_change_work_push(wk,
		                     CLY_UPDATE_CHANGE_TYPE,
		                     (const struct lysc_node *)parm->leaf,
		                     str);
	}

	return 1;
}

static void
_cly_arg_complete_leaf_parm(const struct cly_arg_leaf_parm * parameter,
                            const char *                     word,
                            size_t                           length,
                            struct cly_match *               matches)
{
	cly_arg_assert_leaf_parm(parameter);
	cly_match_assert(matches);

	if (cly_arg_complete_parm_name((const struct cly_arg_parm *)parameter,
	                               word,
	                               length,
	                               matches))
		return;

#warning generate completion matches for enum based / bool / ... leaves
}

static void
cly_arg_complete_leaf_parm(const struct cly_arg * argument,
                           const struct cly_cmd * command __unused,
                           const struct cly_dir * directory __unused,
                           struct cly_context *   context __unused,
                           const char *           word,
                           size_t                 length,
                           int                    argc __unused,
                           const char * const     argv[] __unused,
                           struct cly_match *     matches)
{
	cly_arg_assert_leaf_parm((const struct cly_arg_leaf_parm *)argument);
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_match_assert(matches);

	const struct cly_arg_leaf_parm * parm =
		(const struct cly_arg_leaf_parm *)argument;

	if (!cly_arg_parm_already_completed((const struct cly_arg_parm *)parm,
	                                    argc,
	                                    argv))
		_cly_arg_complete_leaf_parm(parm, word, length, matches);
}

static const struct cly_arg_ops cly_arg_leaf_parm_ops = {
	.parse    = cly_arg_parse_leaf_parm,
	.complete = cly_arg_complete_leaf_parm
};

static int
cly_arg_create_leaf_parm(struct cly_arg_leaf_parm **   parameter,
                         const struct lysc_node_leaf * leaf)
{
	cly_assert(parameter);
	cly_assert(leaf);

	struct cly_arg_leaf_parm * parm;
	int                        ret;

	parm = cly_malloc(sizeof(*parm));
	cly_assert(parm);

	ret = cly_arg_setup_parm(&parm->super,
	                         srepo_sch_node_name((const struct lysc_node *)
	                                             leaf),
	                         &cly_arg_leaf_parm_ops);
	if (!ret) {
		parm->leaf = leaf;
		*parameter = parm;

		return 0;
	}

	cly_free(parm);

	return ret;
}

static int
cly_arg_createn_add_leaf_parm(struct cly_arg_leaf_parm **   parameter,
                              const struct lysc_node_leaf * leaf,
                              struct cly_node *             cmd_or_arg)
{
	cly_assert(parameter);
	cly_assert(leaf);
	cly_node_assert(cmd_or_arg);

	int ret;

	ret = cly_arg_create_leaf_parm(parameter, leaf);
	if (!ret) {
		cly_node_add_child(cmd_or_arg,
		                   *(struct cly_node **)parameter);
		return 0;
	}

	return ret;
}

/******************************************************************************
 * `set' command
 ******************************************************************************/

struct cly_set_cmd {
	struct cly_cmd super;
	unsigned int   max_chg;
};

#define cly_set_assert_cmd(_cmd) \
	cly_assert(_cmd); \
	cly_cmd_assert(&(_cmd)->super); \
	cly_assert((_cmd)->max_chg)

static int
cly_set_parse_cmd(const struct cly_cmd * command,
                  const struct cly_dir * directory,
                  struct cly_context *   context,
                  int                    argc,
                  const char * const     argv[])
{
	cly_set_assert_cmd((const struct cly_set_cmd *)command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argv);

	struct cly_change_work * wk;
	int                      ret;

	/* Cannot fail. */
	wk = (struct cly_change_work *)
	     cly_change_create_work(
		((const struct cly_set_cmd *)command)->max_chg,
		command);

	ret = cly_cmd_parse_args(command, directory, context, argc, argv, wk);
	if (ret < 0)
		goto destroy;

	cly_assert(ret == argc);
	ret = cly_sched_work(context, (struct cly_work *)wk);
	if (ret) {
		cly_cmd_talk(command, "cannot schedule work");
		goto destroy;
	}

	return argc;

destroy:
	cly_change_destroy_work(wk);

	return ret;
}

static const struct cly_cmd_ops cly_set_cmd_ops = {
	.parse    = cly_set_parse_cmd,
	.complete = cly_cmd_complete_args,
	.fini     = cly_cmd_null_fini
};

static int
cly_set_create_cmd(struct cly_set_cmd **      command,
                   struct cly_dir *           directory,
                   const struct lysc_node *   node,
                   const char *               name,
                   cly_filter_node_fn *       filter,
                   const struct cly_context * context)
{
	cly_assert(command);
	cly_dir_assert(directory);
	cly_assert(node);
	cly_assert(srepo_sch_node_type(node) == LYS_CONTAINER);
	cly_assert(cly_cmd_name_isok(name));
	cly_assert(filter);
	cly_assert_context(context);

	struct cly_set_cmd *     cmd;
	struct cly_arg *         choice;
	const struct lysc_node * child;
	unsigned int             cnt = 0;
	int                      ret;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with a command name validated thanks to cly_cmd_name_isok().
	 */
	cly_cmd_sized_create((struct cly_cmd **)&cmd,
	                     sizeof(*cmd),
	                     name,
	                     &cly_set_cmd_ops);
	cly_assert(cmd);

	/* Cannot fail. */
	choice = cly_arg_createn_add_choice((struct cly_node *)cmd);

	srepo_sch_foreach_child(node, child) {
		if ((srepo_sch_node_type(child) == LYS_LEAF) && filter(child)) {
			struct cly_arg_leaf_parm * parm;

			ret = cly_arg_createn_add_leaf_parm(
				&parm,
				(const struct lysc_node_leaf *)child,
				(struct cly_node *)choice);
			if (ret) {
				cly_scn_err(
					child,
					"cannot create set command '%s': "
					"invalid argument '%s': %s",
					name,
					srepo_sch_node_name(child),
					strerror(-ret));
				goto destroy;
			}

			cnt++;
		}
	}

	if (!cnt) {
		cly_scn_debug(node,
		              "'%s' set command creation ignored: "
		              "no child schema data node found",
		              name);
		ret = 0;
		goto destroy;
	}

	cmd->max_chg = cnt;

	cly_dir_add_cmd(directory, (struct cly_cmd *)cmd);

	*command = cmd;

	return 0;

destroy:
	cly_cmd_destroy((struct cly_cmd *)cmd);

	return ret;
}

int
cly_set_make_cmd(struct cly_dir *                   directory,
                 const struct lysc_node_container * container,
                 const char *                       name,
                 const struct cly_context *         context)
{
	cly_dir_assert(directory);
	cly_assert(container);
	cly_assert(srepo_sch_node_type((const struct lysc_node *)container) ==
	           LYS_CONTAINER);
	cly_assert(name);
	cly_assert_context(context);

	struct cly_set_cmd * cmd;
	const char *         msg;
	int                  err;

	if (!cly_cmd_name_isok(name)) {
		msg = "invalid name";
		err = -EINVAL;
		goto err;
	}

	if (cly_dir_find_cmd(directory, name)) {
		msg = "already exists";
		err = -EEXIST;
		goto err;
	}

	return cly_set_create_cmd(&cmd,
	                          directory,
	                          (const struct lysc_node *)container,
	                          name,
	                          cly_filter_config_leaf,
	                          context);

err:
	cly_scn_err((const struct lysc_node *)container,
	            "cannot create set command '%s': %s",
	            name,
	            msg);

	return err;
}
