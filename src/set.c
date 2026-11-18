#include "set.h"
#include "change.h"
#include "cly.h"

struct cly_set_cmd {
	struct cly_cmd super;
	char *         path;
	size_t         len;
	unsigned int   max_chg;
};

#define cly_set_assert_cmd(_cmd) \
	cly_assert(_cmd); \
	cly_cmd_assert(&(_cmd)->super); \
	cly_assert((_cmd)->path); \
	cly_assert((_cmd)->len); \
	cly_assert((_cmd)->len < SREPO_XPATH_SIZE); \
	cly_assert((_cmd)->max_chg)

/******************************************************************************
 * Set commands work
 ******************************************************************************/

struct cly_set_work {
	struct cly_work           super;
	const struct cly_cmd *    cmd;
	struct cly_change_batch * changes;
};

#define cly_set_assert_work(_wk) \
	cly_assert(_wk); \
	cly_assert_work(&(_wk)->super); \
	cly_cmd_assert((_wk)->cmd); \
	cly_change_assert_batch((_wk)->changes)

static inline void
cly_set_work_push(struct cly_set_work *    work,
                  enum cly_change_type     type,
                  const struct lysc_node * node,
                  const char *             value)
{
	cly_set_assert_work(work);

	cly_change_push_batch(work->changes, type, node, value);
}

static inline void
cly_set_release_work(struct cly_work * work)
{
	cly_set_assert_work((struct cly_set_work *)work);

	cly_change_destroy_batch(((struct cly_set_work *)work)->changes);
}

static struct cly_set_work *
cly_set_create_work(const struct cly_work_ops * operations,
                    unsigned int                nr,
                    const struct cly_cmd *      command)
{
	cly_assert(nr);
	cly_assert(nr <= CLY_CHANGE_BATCH_NR);
	cly_cmd_assert(command);

	struct cly_set_work * wk;

	wk = (struct cly_set_work *)cly_create_work(sizeof(*wk), operations);
	cly_assert(wk);

	wk->cmd = command;

	wk->changes = cly_change_create_batch(nr);
	cly_assert(wk->changes);

	return wk;
}

static inline void
cly_set_destroy_work(struct cly_set_work * work)
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
	struct cly_set_work *            wk = data;

	len = strcspn(str, "=");
	if ((len != parm->super.len) ||
	    (str[len] != '=') ||
	    memcmp(str, parm->super.name, parm->super.len))
		return 0;

#warning Implement leaf value help and display it in case of error
	str = &str[len + 1];
	len = strlen(str);
	if (!len) {
		if (srepo_sch_is_key((const struct lysc_node *)parm->leaf) ||
		    srepo_sch_is_mandatory((const struct lysc_node *)
		                           parm->leaf)) {
			cly_cmd_talk(command,
			             "missing %s parameter value",
			             parm->super.name);
			return -EINVAL;
		}

		cly_set_work_push(wk,
		                  CLY_DELETE_CHANGE_TYPE,
		                  (const struct lysc_node *)parm->leaf,
		                  NULL);
	}
	else if (!strcmp(str, "default")) {
		if (srepo_sch_is_key((const struct lysc_node *)parm->leaf) ||
		    !srepo_sch_leaf_dflt(parm->leaf)) {
			cly_cmd_talk(command,
			             "%s parameter has no default value",
			             parm->super.name);
			return -EINVAL;
		}

		cly_set_work_push(wk,
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

		cly_set_work_push(wk,
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
 * Basic `set' command
 ******************************************************************************/

static int
cly_set_exec_work(struct cly_work * work, struct cly_context * context)
{
	cly_set_assert_work((const struct cly_set_work *)work);
	cly_set_assert_cmd((const struct cly_set_cmd *)
	                   ((const struct cly_set_work *)work)->cmd);
	cly_assert_context(context);

	sr_session_ctx_t *          sess = *cly_cand_session(context);
	const struct cly_set_work * wk = (const struct cly_set_work *)work;
	const struct cly_set_cmd *  cmd = (const struct cly_set_cmd *)wk->cmd;

	if (sess)
		return cly_change_exec_batch(wk->changes,
		                             sess,
		                             cmd->path,
		                             cmd->len,
		                             wk->cmd);

	cly_cmd_talk(wk->cmd, "no ongoing edition");

	return -EPERM;
}

static const struct cly_work_ops cly_set_work_ops = {
	.exec    = cly_set_exec_work,
	.release = cly_set_release_work
};

static int
cly_set_parsen_sched_work(const struct cly_cmd * command,
                          const struct cly_dir * directory,
                          struct cly_set_work *  work,
                          struct cly_context *   context,
                          int                    argc,
                          const char * const     argv[])
{
	int ret;

	ret = cly_cmd_parse_args(command, directory, context, argc, argv, work);
	if (ret < 0)
		goto destroy;

	cly_assert(ret == argc);
	ret = cly_sched_work(context, (struct cly_work *)work);
	if (ret) {
		cly_cmd_talk(command, "cannot schedule work");
		goto destroy;
	}

	return argc;

destroy:
	cly_set_destroy_work(work);

	return ret;
}

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

	struct cly_set_work * wk;

	/* Cannot fail. */
	wk = (struct cly_set_work *)
	     cly_set_create_work(&cly_set_work_ops,
	                         ((const struct cly_set_cmd *)command)->max_chg,
	                         command);

	return cly_set_parsen_sched_work(command,
	                                 directory,
	                                 wk,
	                                 context,
	                                 argc,
	                                 argv);
}

static void
cly_set_fini_cmd(struct cly_cmd * command)
{
	cly_set_assert_cmd((struct cly_set_cmd *)command);

	srepo_free(((struct cly_set_cmd *)command)->path);
}

static const struct cly_cmd_ops cly_set_cmd_ops = {
	.parse    = cly_set_parse_cmd,
	.complete = cly_cmd_complete_args,
	.fini     = cly_set_fini_cmd
};

static int
cly_set_createn_add_parm(struct cly_cmd *         command,
                         const struct lysc_node * node,
                         cly_filter_node_fn *     filter)
{
	cly_assert(node);
	cly_assert((srepo_sch_node_type(node) == LYS_CONTAINER) ||
	           (srepo_sch_node_type(node) == LYS_LIST));
	cly_assert(filter);
	cly_cmd_assert(command);

	struct cly_arg *         choice;
	const struct lysc_node * scn;
	unsigned int             cnt = 0;
	int                      ret;

	/* Cannot fail. */
	choice = cly_arg_createn_add_choice((struct cly_node *)command);

	srepo_sch_foreach_child(node, scn) {
		if ((srepo_sch_node_type(scn) == LYS_LEAF) && filter(scn)) {
			struct cly_arg_leaf_parm * parm;

			ret = cly_arg_createn_add_leaf_parm(
				&parm,
				(const struct lysc_node_leaf *)scn,
				(struct cly_node *)choice);
			if (ret)
				goto err;

			cnt++;
		}
	}

	if (cnt) {
		cly_assert(cnt <= (unsigned int)INT_MAX);
		return cnt;
	}

	cly_scn_err(node,
	            "cannot create set command '%s': no schema parameter found"
	            command->name);

	return -ENOENT;

err:
	cly_scn_err(scn,
	            "cannot create set command '%s': "
	            "invalid parameter '%s': %s",
	            command->name,
	            srepo_sch_node_name(scn),
	            strerror(-ret));

	return ret;
}

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

	struct cly_set_cmd * cmd;
	int                  ret;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with a command name validated thanks to cly_cmd_name_isok().
	 */
	cly_cmd_sized_create((struct cly_cmd **)&cmd,
	                     sizeof(*cmd),
	                     name,
	                     &cly_set_cmd_ops);

	cmd->path = srepo_sch_node_path(node);
	cly_assert(cmd->path);
	cmd->len = srepo_xpath_validate(cmd->path);
	cly_assert(cmd->len);
	cly_assert(cmd->len <= SREPO_XPATH_SIZE);
	if (cmd->len == SREPO_XPATH_SIZE) {
		cly_scn_err(node,
		            "cannot create set command '%s': "
		            "schema path too long",
		            name);
		ret = -EINVAL;
		goto destroy;
	}

	ret = cly_set_createn_add_parm((struct cly_cmd *)cmd,
	                               node,
	                               filter);
	cly_assert(ret);
	if (ret < 0)
		goto destroy;

	cmd->max_chg = (unsigned int)ret;
	cly_dir_add_cmd(directory, (struct cly_cmd *)cmd);
	*command = cmd;

	return 0;

destroy:
	cly_cmd_destroy((struct cly_cmd *)cmd);

	return ret;
}

int
cly_set_make_cmd(struct cly_dir *           directory,
                 const struct lysc_node *   container,
                 const char *               name,
                 const struct cly_context * context)
{
	cly_dir_assert(directory);
	cly_assert(container);
	cly_assert(srepo_sch_node_type(container) == LYS_CONTAINER);
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
	                          container,
	                          name,
	                          cly_filter_config_leaf,
	                          context);

err:
	cly_scn_err(container, "cannot create set command '%s': %s", name, msg);

	return err;
}

#if 0
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

/******************************************************************************
 * Yang list key parameter argument
 ******************************************************************************/

#include "arg.h"

struct cly_arg_key {
	struct cly_arg                super;
	const struct lysc_node_leaf * leaf;
};

#define cly_arg_assert_key(_key) \
	cly_assert(_key); \
	cly_arg_assert(&(_key)->super); \
	cly_assert((_key)->leaf); \
	cly_assert(srepo_sch_is_key((const struct lysc_node *)parm->leaf))

static int
cly_arg_parse_key(const struct cly_arg * argument,
                  const struct cly_cmd * command,
                  const struct cly_dir * directory,
                  struct cly_context *   context,
                  int                    argc __unused,
                  const char * const     argv[],
                  void *                 data)
{
	cly_arg_assert_key((const struct cly_arg_key *)argument);
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argc == 1);
	cly_assert(argv[0]);
	cly_assert(strnlen(argv[0], CLY_ARG_MAX) < CLY_ARG_MAX);

	const struct cly_arg_key * key = (const struct cly_arg_key *)argument;
	const char *               str = argv[0];
	size_t                     len;
	struct cly_set_work *   wk = data;

#warning Implement key value help and display it in case of error
	len = strlen(str);
	if (!len) {
		/*
		 * As stated into section 7.8.2 `The list's "key" Statement' of
		 * RFC 7950, all key values MUST be present when specifying a
		 * configuration list entry.
		 * In addition, `default' and `mandatory' YANG statements are
		 * ignored when specified within list key definitions.
		 */
		cly_cmd_talk(command,
		             "missing %s key value",
		             srepo_sch_node_name((const struct lysc_node *)
		                                 key->leaf));
			return -EINVAL;
	}
	else {
		sr_error_t err;

		err = srepo_sch_validate_value((const struct lysc_node *)
		                               key->leaf,
		                               str,
		                               len,
		                               NULL);
		if (err != SR_ERR_OK) {
			cly_cmd_talk(command,
				"'%s': invalid %s key value",
				str,
				srepo_sch_node_name(
					(const struct lysc_node *)key->leaf));
			return -EINVAL;
		}

		cly_set_work_push(wk,
		                  CLY_KEY_CHANGE_TYPE,
		                  (const struct lysc_node *)parm->leaf,
		                  str);
	}

	return 1;
}

static void
cly_arg_complete_key(const struct cly_arg * argument,
                     const struct cly_cmd * command __unused,
                     const struct cly_dir * directory __unused,
                     struct cly_context *   context __unused,
                     const char *           word,
                     size_t                 length,
                     int                    argc __unused,
                     const char * const     argv[] __unused,
                     struct cly_match *     matches)
{
	cly_arg_assert_key((const struct cly_arg_key *)argument);
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_match_assert(matches);

#warning implement key value completion
}

#if 0
FINISH ME!: implement CLY_KEY_CHANGE_TYPE support in cly_set_work logic!
static inline bool
cly_change_filter_keyed_leaf(const struct lysc_node * node)
{
	cly_assert(node);
	cly_assert(srepo_sch_node_type(node) == LYS_LEAF);

	if (srepo_sch_status_flags(node) &
	    (LYS_STATUS_OBSLT | LYS_STATUS_DEPRC))
		return false;

	return (srepo_sch_conf_flags(node) == LYS_CONFIG_W) ||
	       srepo_sch_is_key(node);
}

static void
cly_change_path_init_from_keyed_node(struct cly_change_path * path,
                                     const struct lysc_node * node)
{
	cly_assert(path);
	cly_assert(node);

	cly_change_path_init(path);

	do {
		cly_assert(cly_filter_config_leaf(node));

		if (!(srepo_sch_node_type(node) &
		      (LYS_CHOICE | LYS_CASE | LYS_INPUT | LYS_OUTPUT)))
			/* Skip schema only nodes. */
			cly_change_path_push(path, node);
		node = srepo_sch_node_parent(node);
	} while (node);

	cly_change_invert_path(path);
}
FINISH ME!: implement CLY_KEY_CHANGE_TYPE support in cly_set_work logic!
#endif

#endif

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

/******************************************************************************
 * List `set' command
 ******************************************************************************/

struct cly_set_list_cmd {
	struct cly_cmd         super;
	struct cly_change_path path;
	unsigned int           max_chg;
};

#define cly_set_assert_list_cmd(_cmd) \
	cly_assert(_cmd); \
	cly_cmd_assert(&(_cmd)->super); \
	cly_change_path_assert(&(_cmd)->path); \
	cly_assert((_cmd)->max_chg)

static int
cly_set_exec_list_work(struct cly_work * work, struct cly_context * context)
{
	cly_set_assert_work((const struct cly_set_work *)work);
	cly_set_assert_list_cmd((const struct cly_set_list_cmd *)
	                        ((const struct cly_set_work *)work)->cmd);
	cly_assert_context(context);

	sr_session_ctx_t *              sess = *cly_cand_session(context);
	const struct cly_set_work *     wk = (const struct cly_set_work *)work;
	const struct cly_set_list_cmd * cmd = (const struct cly_set_list_cmd *)
	                                      wk->cmd;

	if (sess)
		return cly_change_exec_list_batch(wk->changes,
		                                  sess,
		                                  &cmd->path,
		                                  wk->cmd);

	cly_cmd_talk(wk->cmd, "no ongoing edition");

	return -EPERM;
}

static const struct cly_work_ops cly_set_work_list_ops = {
	.exec    = cly_set_exec_list_work,
	.release = cly_set_release_work
};

static int
cly_set_parse_list_cmd(const struct cly_cmd * command,
                       const struct cly_dir * directory,
                       struct cly_context *   context,
                       int                    argc,
                       const char * const     argv[])
{
	cly_set_assert_list_cmd((const struct cly_set_list_cmd *)command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argv);

	struct cly_set_work * wk;

	/* Cannot fail. */
	wk = (struct cly_set_work *)
	     cly_set_create_work(&cly_set_work_list_ops,
	                         ((const struct cly_set_cmd *)command)->max_chg,
	                         command);

	return cly_set_parsen_sched_work(command,
	                                 directory,
	                                 wk,
	                                 context,
	                                 argc,
	                                 argv);
}

static void
cly_set_fini_list_cmd(struct cly_cmd * command)
{
	cly_set_assert_list_cmd((struct cly_set_list_cmd *)command);

	cly_change_path_fini(&((struct cly_set_list_cmd *)command)->path);
}

static const struct cly_cmd_ops cly_set_cmd_list_ops = {
	.parse    = cly_set_parse_list_cmd,
	.complete = cly_cmd_complete_args,
	.fini     = cly_set_fini_list_cmd
};

static bool
cly_set_filter_key_leaf(const struct lysc_node * node)
{
	cly_assert(node);

	if (srepo_sch_node_type(node) != LYS_LEAF)
		return false;

	if (srepo_sch_conf_flags(node) != LYS_CONFIG_W)
		return false;

	if (srepo_sch_status_flags(node) &
	    (LYS_STATUS_OBSLT | LYS_STATUS_DEPRC))
		return false;

	if (!srepo_sch_is_key(node))
		return false;

	return true;
}

static int
cly_set_createn_add_key(struct cly_cmd *         command,
                        struct cly_change_path * path,
                        const struct lysc_node * node)
{
	cly_cmd_assert(command);
	cly_assert(path);
	cly_assert(node);
	cly_assert((srepo_sch_node_type(list) == LYS_LIST) ||
	           (srepo_sch_node_type(list) == LYS_CONTAINER));

	const struct lysc_node * list;
	unsigned int             n;
	const struct lysc_node * leaf;
	unsigned int             cnt = 0;
	int                      ret;

	cly_change_path_init_from_node(path, node);

	cly_change_path_foreach(path, n, list) {
		if (srepo_sch_node_type(list) == LYS_LIST) {
			srepo_sch_foreach_child(list, leaf) {
				if (cly_set_filter_key_leaf(leaf)) {
					struct cly_arg_key * key;

					ret = cly_arg_createn_add_key(
						&key,
						leaf,
						(struct cly_node *)command);
					if (ret)
						goto err;

					cnt++;
				}
			}
		}
	}
	cly_assert(n);

	if (cnt) {
		cly_assert(cnt <= (unsigned int)INT_MAX);
		return (int)cnt;
	}

	cly_scn_err(node,
	            "cannot create set command '%s': "
	            "no schema key argument found"
	            command->name);

	return -ENOENT;

err:
	cly_scn_err(leaf,
	            "cannot create set command '%s': "
	            "invalid key argument '%s': %s",
	            command->name,
	            srepo_sch_node_name(leaf),
	            strerror(-ret));

	return ret;
}

static int
cly_set_create_list_cmd(struct cly_set_cmd **      command,
                        struct cly_dir *           directory,
                        const struct lysc_node *   node,
                        const char *               name,
                        cly_filter_node_fn *       filter,
                        const struct cly_context * context)
{
	cly_assert(command);
	cly_dir_assert(directory);
	cly_assert(node);
	cly_assert((srepo_sch_node_type(node) == LYS_CONTAINER) ||
	           (srepo_sch_node_type(node) == LYS_LIST));
	cly_assert(cly_cmd_name_isok(name));
	cly_assert(filter);
	cly_assert_context(context);

	struct cly_set_list_cmd * cmd;
	const struct lysc_node *  scn;
	int                       ret;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with a command name validated thanks to cly_cmd_name_isok().
	 */
	cly_cmd_sized_create((struct cly_cmd **)&cmd,
	                     sizeof(*cmd),
	                     name,
	                     &cly_set_cmd_list_ops);

	ret = cly_set_createn_add_key((struct cly_cmd *)cmd,
	                              &cmd->path,
	                              node);
	cly_assert(ret);
	if (ret < 0)
		goto destroy;

	finish me !! Check that cmd->max_chg <= CLY_CHANGE_BATCH_NR !!
	cmd->max_chg = (unsigned int)ret;

	ret = cly_set_createn_add_parm((struct cly_cmd *)cmd,
	                               node,
	                               filter);
	cly_assert(ret);
	if (ret < 0)
		goto destroy;

	finish me !! Check that cmd->max_chg + ret does not wrap and <= CLY_CHANGE_BATCH_NR !!

	cmd->max_chg += (unsigned int)ret;
	cly_dir_add_cmd(directory, (struct cly_cmd *)cmd);
	*command = cmd;

	return 0;

destroy:
	cly_cmd_destroy((struct cly_cmd *)cmd);

	return ret;
}
