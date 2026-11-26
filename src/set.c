#include "set.h"
#include "change.h"
#include "cly.h"
#include "edit.h"
#include <srutils/srepo/data.h>

/*
 * Make sure that CLY_CHANGE_BATCH_NR may be encoded using a positive signed
 * integer so that cly_set_createn_add_keys() and cly_set_createn_add_parms()
 * may return a consistent number of possible changes.
 */
#if CLY_CHANGE_BATCH_NR > INT_MAX
#error Maximum number of changes too large !
#endif

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
	cly_assert((_cmd)->len < SREPO_XPATH_SIZE)

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

struct cly_set_list_cmd {
	struct cly_cmd         super;
	struct cly_change_path path;
	unsigned int           max_pred;
	unsigned int           max_chg;
};

#define cly_set_assert_list_cmd(_cmd) \
	cly_assert(_cmd); \
	cly_cmd_assert(&(_cmd)->super); \
	cly_change_path_assert(&(_cmd)->path)

struct cly_set_list_work {
	struct cly_set_work       super;
	struct cly_change_preds * preds;
};

#define cly_set_assert_list_work(_wk) \
	cly_assert(_wk); \
	cly_set_assert_work(&(_wk)->super); \
	cly_change_assert_preds((_wk)->preds)

/******************************************************************************
 * Set commands work
 ******************************************************************************/

static inline void
cly_set_release_work(struct cly_work * work)
{
	cly_set_assert_work((struct cly_set_work *)work);

	cly_change_destroy_batch(((struct cly_set_work *)work)->changes);
}

static int
cly_set_exec_work(struct cly_work * work, struct cly_context * context)
{
	cly_set_assert_work((const struct cly_set_work *)work);
	cly_set_assert_cmd((const struct cly_set_cmd *)
	                   ((const struct cly_set_work *)work)->cmd);
	cly_assert(((const struct cly_set_cmd *)
	            ((const struct cly_set_work *)work)->cmd)->max_chg);
	cly_assert_context(context);

	sr_session_ctx_t *          sess = *cly_cand_session(context);
	const struct cly_set_work * wk = (const struct cly_set_work *)work;
	const struct cly_set_cmd *  cmd = (const struct cly_set_cmd *)wk->cmd;

	if (sess)
		return cly_change_exec_batch(wk->changes,
		                             cmd->path,
		                             cmd->len,
		                             sess,
		                             wk->cmd);

	cly_cmd_talk(wk->cmd, "no ongoing edition");

	return -EPERM;
}

static const struct cly_work_ops cly_set_work_ops = {
	.exec    = cly_set_exec_work,
	.release = cly_set_release_work
};

static struct cly_set_work *
cly_set_create_work(unsigned int                nr,
                    const struct cly_cmd *      command)
{
	cly_assert(nr);
	cly_assert(nr <= CLY_CHANGE_BATCH_NR);
	cly_cmd_assert(command);

	struct cly_set_work * wk;

	wk = (struct cly_set_work *)
	     cly_create_work(sizeof(*wk), &cly_set_work_ops);
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
	int                              ret;

	if (cly_change_batch_full(wk->changes)) {
		cly_cmd_talk(command, "too many arguments");
		return -E2BIG;
	}

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

		ret = cly_change_push_batch(wk->changes,
		                            CLY_DELETE_CHANGE_TYPE,
		                            (const struct lysc_node *)
		                            parm->leaf,
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

		ret = cly_change_push_batch(wk->changes,
		                            CLY_DELETE_CHANGE_TYPE,
		                            (const struct lysc_node *)
		                            parm->leaf,
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

		ret = cly_change_push_batch(wk->changes,
		                            CLY_UPDATE_CHANGE_TYPE,
		                            (const struct lysc_node *)
		                            parm->leaf,
		                            str);
	}

	if (ret) {
		cly_assert(ret == -EEXIST);
		cly_cmd_talk(command,
		             "duplicate %s parameter",
		             parm->super.name);
		return -EEXIST;
	}

	return 1;
}

static void
cly_arg_complete_leaf_term(const char *                     terminal,
                           size_t                           tlen,
                           const struct cly_arg_leaf_parm * parameter,
                           const char *                     word,
                           size_t                           wlen,
                           struct cly_match *               matches)
{
	cly_assert(terminal);
	cly_assert(tlen);
	cly_assert(strlen(terminal) == tlen);
	cly_arg_assert_leaf_parm(parameter);

	if (!wlen || ((wlen <= tlen) && !memcmp(word, terminal, wlen))) {
		char * kw;
		int    ret __unused;

		ret = cly_asprintf(&kw,
		                   "%s=%s",
		                   parameter->super.name,
		                   terminal);
		cly_assert((size_t)ret == (parameter->super.len + 1 + tlen));

		/*
		 * Give ownership of `kw' to `matches'. Allocated string
		 * ownership will be transfered from `matches' to readline(3) by
		 * cly_shell_complete().
		 * Readline(3) will free(3) it at completion process termination
		 * time.
		 */
		cly_match_push(matches, kw);
	}
}

static void
cly_arg_complete_leaf_bool(const struct cly_arg_leaf_parm * parameter,
                           const char *                     word,
                           size_t                           length,
                           struct cly_match *               matches)
{
	cly_arg_complete_leaf_term("true",
	                           sizeof("true") - 1,
	                           parameter,
	                           word,
	                           length,
	                           matches);
	cly_arg_complete_leaf_term("false",
	                           sizeof("false") - 1,
	                           parameter,
	                           word,
	                           length,
	                           matches);
}

static void
cly_arg_complete_leaf_enum(const struct cly_arg_leaf_parm * parameter,
                           const struct lysc_type_enum *    type,
                           const char *                     word,
                           size_t                           length,
                           struct cly_match *               matches)
{
	const struct lysc_type_bitenum_item * enm;

	LY_ARRAY_FOR_ITER(type->enums, typeof(*enm), enm) {
		size_t len;

		len = strnlen(enm->name, CLY_ARG_MAX);
		if (len && (len < CLY_ARG_MAX))
			cly_arg_complete_leaf_term(enm->name,
			                           len,
			                           parameter,
			                           word,
			                           length,
			                           matches);
	}
}

static void
cly_arg_complete_leaf_bits(const struct cly_arg_leaf_parm * parameter,
                           const struct lysc_type_bits *    type,
                           const char *                     word,
                           size_t                           length,
                           struct cly_match *               matches)
{
	const struct lysc_type_bitenum_item * bts;

	LY_ARRAY_FOR_ITER(type->bits, typeof(*bts), bts) {
		size_t len;

		len = strnlen(bts->name, CLY_ARG_MAX);
		if (len && (len < CLY_ARG_MAX))
			cly_arg_complete_leaf_term(bts->name,
			                           len,
			                           parameter,
			                           word,
			                           length,
			                           matches);
	}
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

	if (!memcmp(word, parameter->super.name, parameter->super.len) &&
	    (word[parameter->super.len] == '=')) {
		/* Generate parameter value completion matches. */
		const struct lysc_type * type = parameter->leaf->type;

		word = &word[parameter->super.len + 1];
		length -= parameter->super.len + 1;

		switch (type->basetype) {
		case LY_TYPE_BOOL:
			cly_arg_complete_leaf_bool(parameter,
			                           word,
			                           length,
			                           matches);
			break;

		case LY_TYPE_ENUM:
			cly_arg_complete_leaf_enum(
				parameter,
				(const struct lysc_type_enum *)type,
				word,
				length,
				matches);
			break;

		case LY_TYPE_BITS:
			cly_arg_complete_leaf_bits(
				parameter,
				(const struct lysc_type_bits *)type,
				word,
				length,
				matches);
			break;

#warning Implement remaining value schema type completion support !!
		case LY_TYPE_UNION:
		case LY_TYPE_UNKNOWN:
		case LY_TYPE_BINARY:
		case LY_TYPE_UINT8:
		case LY_TYPE_UINT16:
		case LY_TYPE_UINT32:
		case LY_TYPE_UINT64:
		case LY_TYPE_STRING:
		case LY_TYPE_DEC64:
		case LY_TYPE_EMPTY:
		case LY_TYPE_IDENT:
		case LY_TYPE_INST:
		case LY_TYPE_LEAFREF:
		case LY_TYPE_INT8:
		case LY_TYPE_INT16:
		case LY_TYPE_INT32:
		case LY_TYPE_INT64:
			return;

		default:
			cly_assert(0);
		}

		if (cly_match_count(matches))
			cly_shell_enroll_display_compl(
				cly_arg_parm_display_list,
				(void *)parameter);
	}
}

static void
cly_arg_complete_leaf_parm(const struct cly_arg * argument,
                           const struct cly_cmd * command __unused,
                           const struct cly_dir * directory __unused,
                           struct cly_context *   context,
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

	if (*cly_cand_session(context)) {
		const struct cly_arg_leaf_parm * parm =
			(const struct cly_arg_leaf_parm *)argument;

		if (!cly_arg_parm_already_completed(
			(const struct cly_arg_parm *)parm,
			argc,
			argv))
			_cly_arg_complete_leaf_parm(parm,
			                            word,
			                            length,
			                            matches);
	}
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
cly_set_parse_cmd(const struct cly_cmd * command,
                  const struct cly_dir * directory,
                  struct cly_context *   context,
                  int                    argc,
                  const char * const     argv[])
{
	cly_set_assert_cmd((const struct cly_set_cmd *)command);
	cly_assert(((const struct cly_set_cmd *)command)->max_chg);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argv);

	struct cly_set_work * wk;
	int                   ret;

	/* Cannot fail. */
	wk = (struct cly_set_work *)
	     cly_set_create_work(((const struct cly_set_cmd *)command)->max_chg,
	                         command);

	ret = cly_cmd_parse_args(command, directory, context, argc, argv, wk);
	if (ret < 0)
		goto destroy;

	if (cly_change_batch_empty(wk->changes)) {
		cly_cmd_talk(command, "missing parameters(s)");
		ret = -EINVAL;
		goto destroy;
	}

	cly_assert(ret == argc);
	ret = cly_sched_work(context, (struct cly_work *)wk);
	if (ret) {
		cly_cmd_talk(command, "cannot schedule work");
		goto destroy;
	}

	return argc;

destroy:
	cly_set_destroy_work(wk);

	return ret;
}

static void
cly_set_fini_cmd(struct cly_cmd * command)
{
	cly_set_assert_cmd((struct cly_set_cmd *)command);

	srepo_free(((struct cly_set_cmd *)command)->path);
}

static const struct cly_cmd_ops cly_set_cmd_ops = {
	.parse    = cly_set_parse_cmd,
	.complete = cly_edit_complete_args,
	.fini     = cly_set_fini_cmd
};

static int
cly_set_createn_add_parms(struct cly_cmd *         command,
                          const struct lysc_node * node,
                          cly_filter_node_fn *     filter)
{
	cly_cmd_assert(command);
	cly_assert(node);
	cly_assert((srepo_sch_node_type(node) == LYS_CONTAINER) ||
	           (srepo_sch_node_type(node) == LYS_LIST));
	cly_assert(filter);

	struct cly_arg *         grp;
	const struct lysc_node * scn;
	unsigned int             cnt = 0;
	int                      ret;

	/* Cannot fail. */
	grp = cly_arg_createn_add_choice_group((struct cly_node *)command);

	srepo_sch_foreach_child(node, scn) {
		if ((srepo_sch_node_type(scn) == LYS_LEAF) && filter(scn)) {
			struct cly_arg_leaf_parm * parm;

			cly_assert(cnt <= CLY_CHANGE_BATCH_NR);
			if (cnt == CLY_CHANGE_BATCH_NR) {
				ret = -EOVERFLOW;
				goto err;
			}

			ret = cly_arg_createn_add_leaf_parm(
				&parm,
				(const struct lysc_node_leaf *)scn,
				(struct cly_node *)grp);
			if (ret)
				goto err;

			cnt++;
		}
	}

	if (cnt)
		return cnt;

	cly_scn_debug(node,
	              "'%s' set command creation ignored: "
	              "no schema parameter found",
	              command->name);

	return -ENOENT;

err:
	cly_scn_err(node,
	            "cannot create set command '%s': "
	            "invalid schema parameter '%s': %s",
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

	ret = cly_set_createn_add_parms((struct cly_cmd *)cmd, node, filter);
	cly_assert(ret);
	if (ret < 0)
		goto destroy;

	cmd->max_chg = (unsigned int)ret;
	cly_dir_add_cmd(directory, (struct cly_cmd *)cmd);
	*command = cmd;

	return 0;

destroy:
	cly_cmd_destroy((struct cly_cmd *)cmd);

	return (ret == -ENOENT) ? 0 : ret;
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
	cly_assert(!(srepo_sch_status_flags(container) &
	             (LYS_STATUS_OBSLT | LYS_STATUS_DEPRC)));
	cly_assert(name);
	cly_assert_context(context);

	struct cly_set_cmd * cmd;
	const char *         msg;
	int                  err;

	if (srepo_sch_conf_flags(container) != LYS_CONFIG_W)
		return 0;

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

/******************************************************************************
 * Yang list key argument
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
	cly_assert(srepo_sch_is_key((const struct lysc_node *)((_key)->leaf)))

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
	cly_assert(argc >= 1);
	cly_assert(argv[0]);
	cly_assert(strnlen(argv[0], CLY_ARG_MAX) < CLY_ARG_MAX);

	const struct cly_arg_key * key = (const struct cly_arg_key *)argument;
	const char *               str = argv[0];
	size_t                     len;
	struct cly_set_list_work * wk = data;

	if (cly_change_preds_full(wk->preds)) {
		cly_cmd_talk(command, "too many key arguments");
		return -E2BIG;
	}

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
		int        ret;

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

		ret = cly_change_push_pred(wk->preds,
		                           (const struct lysc_node *)key->leaf,
		                           str);
		if (ret) {
			cly_assert(ret == -EEXIST);
			cly_cmd_talk(
				command,
				"duplicate %s key value",
				srepo_sch_node_name(
					(const struct lysc_node *)key->leaf));
			return -EEXIST;
		}
	}

	return 1;
}

struct cly_arg_key_compl {
	const char *       word;
	size_t             len;
	struct cly_match * matches;
};

#define cly_arg_key_assert_compl(_compl) \
	cly_assert(_compl); \
	cly_assert(!(_compl)->len || (_compl)->word); \
	cly_match_assert((_compl)->matches)

static sr_error_t
cly_arg_process_key_compl(sr_session_ctx_t *      session __unused,
                          const char *            path __unused,
                          const struct lyd_node * entry,
                          void *                  data)
{
	cly_assert(session);
	cly_assert(path);
	cly_assert(entry);
	cly_arg_key_assert_compl((const struct cly_arg_key_compl *)data);

	const struct cly_arg_key_compl * compl = data;
	const char *                     val = srepo_dat_node_as_str(entry);
	size_t                           len = val ? strlen(val) : 0;

	if (compl->len <= len) {
		if (!compl->len || !memcmp(compl->word, val, compl->len)) {
			char * v = cly_strdup(val);

			/*
			 * Give ownership of `name' to `matches'. Allocated
			 * string ownership will be transfered from `matches' to
			 * readline(3) by cly_shell_complete().
			 * Readline(3) will free(3) it at completion process
			 * termination time.
			 */
			cly_match_push(compl->matches, v);

			/*
			 * Disable appending the completion character after a
			 * selected match.
			 */
			cly_shell_suppress_complete_char();
		}
	}

	return SR_ERR_OK;
}

static void
cly_arg_complete_key_values(const struct cly_arg_key * key,
                            struct cly_context *       context,
                            const char *               word,
                            size_t                     length,
                            struct cly_match *         matches)
{
	cly_arg_assert_key(key);
	cly_assert_context(context);
	cly_match_assert(matches);

	sr_session_ctx_t * sess = *cly_cand_session(context);

	if (sess) {
		char *                         path;
		const struct cly_arg_key_compl compl = {
			.word    = word,
			.len     = length,
			.matches = matches
		};

		path = srepo_sch_node_path((const struct lysc_node *)key->leaf);
		srepo_dat_loadn_iter(sess,
		                     path,
		                     1,
		                     0,
		                     cly_arg_process_key_compl,
		                     (void *)&compl);
		srepo_free(path);
	}
}

static void
cly_arg_complete_key(const struct cly_arg * argument,
                     const struct cly_cmd * command __unused,
                     const struct cly_dir * directory __unused,
                     struct cly_context *   context,
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

	const struct cly_arg_key * key = (const struct cly_arg_key *)argument;

	/* if ! already completed */
	cly_arg_complete_key_values(key, context, word, length, matches);
}

static const struct cly_arg_ops cly_arg_key_ops = {
	.parse    = cly_arg_parse_key,
	.complete = cly_arg_complete_key
};

static void
cly_arg_create_key(struct cly_arg_key **         argument,
                   const struct lysc_node_leaf * leaf)
{
	cly_assert(argument);
	cly_assert(leaf);

	struct cly_arg_key * arg;

	arg = (struct cly_arg_key *)
	      cly_arg_create(sizeof(*arg), &cly_arg_key_ops);
	cly_assert(arg);

	arg->leaf = leaf;

	*argument = arg;
}

static void
cly_arg_createn_add_key(struct cly_arg_key **         argument,
                        const struct lysc_node_leaf * leaf,
                        struct cly_node *             cmd_or_arg)
{
	cly_assert(argument);
	cly_assert(leaf);
	cly_node_assert(cmd_or_arg);

	cly_arg_create_key(argument, leaf);
	cly_node_add_child(cmd_or_arg, *(struct cly_node **)argument);
}

/******************************************************************************
 * List `set' work
 ******************************************************************************/

static int
cly_set_exec_list_work(struct cly_work * work, struct cly_context * context)
{
	cly_set_assert_work((const struct cly_set_work *)work);
	cly_set_assert_list_cmd((const struct cly_set_list_cmd *)
	                        ((const struct cly_set_work *)work)->cmd);
	cly_assert_context(context);

	sr_session_ctx_t *               sess = *cly_cand_session(context);
	const struct cly_set_list_work * wk = (const struct cly_set_list_work *)
	                                      work;
	const struct cly_set_list_cmd *  cmd = (const struct cly_set_list_cmd *)
	                                       wk->super.cmd;

	if (sess)
		return cly_change_exec_list_batch(wk->super.changes,
		                                  &cmd->path,
		                                  wk->preds,
		                                  sess,
		                                  wk->super.cmd);

	cly_cmd_talk(wk->super.cmd, "no ongoing edition");

	return -EPERM;
}

static inline void
cly_set_release_list_work(struct cly_work * work)
{
	cly_set_assert_list_work((struct cly_set_list_work *)work);

	cly_change_destroy_preds(((struct cly_set_list_work *)work)->preds);
	cly_set_release_work(work);
}

static const struct cly_work_ops cly_set_work_list_ops = {
	.exec    = cly_set_exec_list_work,
	.release = cly_set_release_list_work
};

static struct cly_set_list_work *
cly_set_create_list_work(unsigned int           pred_nr,
                         unsigned int           change_nr,
                         const struct cly_cmd * command)
{
	cly_assert(pred_nr);
	cly_assert(pred_nr <= CLY_CHANGE_PRED_NR);
	cly_assert(change_nr);
	cly_assert(change_nr <= CLY_CHANGE_BATCH_NR);
	cly_cmd_assert(command);

	struct cly_set_list_work * wk;

	wk = (struct cly_set_list_work *)
	     cly_create_work(sizeof(*wk), &cly_set_work_list_ops);
	cly_assert(wk);

	wk->super.cmd = command;

	wk->super.changes = cly_change_create_batch(change_nr);
	cly_assert(wk->super.changes);

	wk->preds = cly_change_create_preds(pred_nr);
	cly_assert(wk->preds);

	return wk;
}

static inline void
cly_set_destroy_list_work(struct cly_set_list_work * work)
{
	cly_destroy_work((struct cly_work *)work);
}

/******************************************************************************
 * List `set' command
 ******************************************************************************/

static int
cly_set_parse_list_cmd(const struct cly_cmd * command,
                       const struct cly_dir * directory,
                       struct cly_context *   context,
                       int                    argc,
                       const char * const     argv[])
{
	cly_set_assert_list_cmd((const struct cly_set_list_cmd *)command);
	cly_assert(((const struct cly_set_list_cmd *)command)->max_pred);
	cly_assert(((const struct cly_set_list_cmd *)command)->max_chg);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argv);

	struct cly_set_list_work * wk;
	int                        ret;

	/* Cannot fail. */
	wk = cly_set_create_list_work(
		((const struct cly_set_list_cmd *)command)->max_pred,
		((const struct cly_set_list_cmd *)command)->max_chg,
		command);

	ret = cly_cmd_parse_args(command, directory, context, argc, argv, wk);
	if (ret < 0)
		goto destroy;

	if (!cly_change_preds_full(wk->preds)) {
		cly_cmd_talk(command, "missing key argument(s)");
		ret = -EINVAL;
		goto destroy;
	}

	if (cly_change_batch_empty(wk->super.changes)) {
		cly_cmd_talk(command, "missing parameters(s)");
		ret = -EINVAL;
		goto destroy;
	}

	cly_assert(ret == argc);
	ret = cly_sched_work(context, (struct cly_work *)wk);
	if (ret) {
		cly_cmd_talk(command, "cannot schedule work");
		goto destroy;
	}

	return argc;

destroy:
	cly_set_destroy_list_work(wk);

	return ret;
}

static void
cly_set_fini_list_cmd(struct cly_cmd * command)
{
	cly_set_assert_list_cmd((struct cly_set_list_cmd *)command);

	cly_change_path_fini(&((struct cly_set_list_cmd *)command)->path);
}

static void
cly_set_complete_list_args(const struct cly_cmd * command,
                           const struct cly_dir * directory,
                           struct cly_context *   context,
                           const char *           word,
                           size_t                 length,
                           int                    argc,
                           const char * const     argv[],
                           struct cly_match *     matches)
{
	if (*cly_cand_session(context)) {
		const struct cly_node * arg;
		int                     cnt = 0;

		cly_node_foreach_child(&command->super, arg) {
			if (cnt++ == argc || !arg->next)
				break;
		}

		cly_arg_complete((const struct cly_arg *)arg,
		                 command,
		                 directory,
		                 context,
		                 word,
		                 length,
		                 argc,
		                 argv,
		                 matches);
	}
	else
		cly_shell_compl_talk("no ongoing edition");
}

static const struct cly_cmd_ops cly_set_cmd_list_ops = {
	.parse    = cly_set_parse_list_cmd,
	.complete = cly_set_complete_list_args,
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
cly_set_createn_add_keys(struct cly_cmd *         command,
                         struct cly_change_path * path,
                         const struct lysc_node * node)
{
	cly_cmd_assert(command);
	cly_assert(path);
	cly_assert(node);
	cly_assert((srepo_sch_node_type(node) == LYS_LIST) ||
	           (srepo_sch_node_type(node) == LYS_CONTAINER));

	const struct lysc_node * list;
	unsigned int             n;
	const struct lysc_node * leaf;
	unsigned int             cnt = 0;

	if (cly_change_path_init_from_node(path, node)) {
		cly_scn_debug(node,
		              "'%s' list set command creation ignored: "
		              "unexpected schema node path",
		              command->name);
		return -ENOENT;
	}

	cly_change_path_foreach(path, n, list) {
		if (srepo_sch_node_type(list) == LYS_LIST) {
			srepo_sch_foreach_child(list, leaf) {
				if (cly_set_filter_key_leaf(leaf)) {
					struct cly_arg_key * key;

					cly_assert(cnt <= CLY_CHANGE_PRED_NR);
					if (cnt == CLY_CHANGE_PRED_NR)
						goto ovrfl;

					cly_arg_createn_add_key(
						&key,
						(struct lysc_node_leaf *)leaf,
						(struct cly_node *)command);

					cnt++;
				}
			}
		}
	}
	cly_assert(n);

	if (cnt)
		return (int)cnt;

	return -ENOENT;

ovrfl:
	cly_scn_err(list,
	            "cannot create list set command '%s': "
	            "too many available schema keys",
	            command->name);

	return -EOVERFLOW;
}

static int
cly_set_create_list_cmd(struct cly_set_list_cmd ** command,
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
	int                       ret;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with a command name validated thanks to cly_cmd_name_isok().
	 */
	cly_cmd_sized_create((struct cly_cmd **)&cmd,
	                     sizeof(*cmd),
	                     name,
	                     &cly_set_cmd_list_ops);

	ret = cly_set_createn_add_keys((struct cly_cmd *)cmd, &cmd->path, node);
	cly_assert(ret);
	if (ret < 0)
		goto destroy;

	cmd->max_pred = (unsigned int)ret;

	ret = cly_set_createn_add_parms((struct cly_cmd *)cmd, node, filter);
	cly_assert(ret);
	if (ret < 0)
		goto destroy;

	cmd->max_chg = (unsigned int)ret;

	cly_dir_add_cmd(directory, (struct cly_cmd *)cmd);
	*command = cmd;

	return 0;

destroy:
	cly_cmd_destroy((struct cly_cmd *)cmd);

	return (ret == -ENOENT) ? 0 : ret;
}

int
cly_set_make_list_cmd(struct cly_dir *           directory,
                      const struct lysc_node *   node,
                      const char *               name,
                      const struct cly_context * context)
{
	cly_dir_assert(directory);
	cly_assert(node);
	cly_assert(cly_node_is_list_entry(node));
	cly_assert(!(srepo_sch_status_flags(node) &
	             (LYS_STATUS_OBSLT | LYS_STATUS_DEPRC)));
	cly_assert(name);
	cly_assert_context(context);

	struct cly_set_list_cmd * cmd;
	const char *              msg;
	int                       err;

	if (srepo_sch_conf_flags(node) != LYS_CONFIG_W)
		return 0;

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

	return cly_set_create_list_cmd(&cmd,
	                               directory,
	                               node,
	                               name,
	                               cly_filter_config_leaf,
	                               context);

err:
	cly_scn_err(node, "cannot create set command '%s': %s", name, msg);

	return err;
}
