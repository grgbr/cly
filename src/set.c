struct cly_assign {
	const struct lysc_node * scn;
	const char *             val;
};

struct cly_set_work {
	struct cly_work        super;
	const struct cly_cmd * cmd;
	unsigned int           cnt;
	unsigned int           nr;
	struct cly_assign      assign[];
};

static int
cly_set_exec_work(struct cly_work * work, struct cly_context * context)
{
	cly_set_assert_work((const struct cly_set_work *)work);
	cly_cmd_assert(((const struct cly_set_work *)work)->cmd);
	cly_assert_context(context);

	const struct cly_set_work * wk = (const struct cly_set_work *)work;
	int                         ret;

	ret = cly_dir_work_search(&wk->super, context, &dir);
	if (ret)
		return ret;

	/*
	 * Given the directory descriptor found above, display its state related
	 * informations.
	 */
	switch (wk->format) {
	case CLY_TABLE_DATA_FMT:
		ret = cly_show_table(&cmd->super,
		                     cmd->table,
		                     cmd->flags,
		                     context);
		break;

	case CLY_JSON_DATA_FMT:
		ret = cly_show_format(&cmd->super,
		                      dir->sch_node,
		                      cmd->flags,
		                      LYD_JSON,
		                      context);
		break;

	case CLY_XML_DATA_FMT:
		ret = cly_show_format(&cmd->super,
		                      dir->sch_node,
		                      cmd->flags,
		                      LYD_XML,
		                      context);
		break;

	default:
		cly_assert(0);
	}

	return ret;
}

static const struct cly_work_ops cly_set_work_ops = {
	.exec    = cly_set_exec_work,
	.release = cly_null_release_work
};

static struct cly_set_work *
cly_set_create_work(unsigned int nr, const struct cly_cmd * command)
{
#define CLY_SET_WORK_ASSIGN_NR (128U)
	cly_assert(nr);
	cly_assert(nr <= CLY_SET_WORK_ASSIGN_NR);
	cly_cmd_assert(command);

	struct cly_set_work * wk;

	wk = (struct cly_set_work *)
	     cly_create_work(sizeof(*wk) + (nr * sizeof(wk->assign[0])),
	                     &cly_set_work_ops);
	cly_assert(wk);

	wk->cmd = command;
	wk->cnt = 0;
	wk->nr = nr;

	return wk;
}

static inline void
cly_set_work_destroy(struct cly_set_work * work)
{
	cly_destroy_work(&work->super);
}















static int
cly_set_parse_leaf_parm(const struct cly_arg * argument,
                        const struct cly_cmd * command,
                        const struct cly_dir * directory,
                        struct cly_context *   context,
                        int                    argc __unused,
                        const char * const     argv[],
                        void *                 data)
{
	cly_arg_assert_kword_parm((const struct cly_arg_kword_parm *)argument);
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argc == 1);
	cly_assert(argv[0]);
	cly_assert(strnlen(argv[0], CLY_ARG_MAX) < CLY_ARG_MAX);

	const struct cly_arg_kword_parm * parm =
		(const struct cly_arg_kword_parm *)argument;
	const char *                      str = argv[0];
	size_t                            len;
	unsigned int                      t;

	len = strcspn(str, "=");
	if ((len != parm->super.len) ||
	    (str[len] != '=') ||
	    memcmp(str, parm->super.name, parm->super.len))
		return 0;

	str = &str[len + 1];
	if (*str == '\0') {
		cly_cmd_talk(command,
		             "'%s': missing parameter keyword.",
		             parm->super.name);
		return -EINVAL;
	}

	for (t = 0; t < parm->nr; t++) {
		int ret;

		ret = cly_arg_parse_kword_term(&parm->terms[t],
		                               parm,
		                               command,
		                               directory,
		                               context,
		                               str,
		                               data);
		cly_assert(ret <= 0);
		if (!ret)
			/* Matched successfully. */
			return 1;
	}

	cly_cmd_talk(command, "'%s': invalid %s parameter keyword.",
	             str,
	             parm->super.name);

	return -EINVAL;
}

static void
cly_set_complete_leaf_parm(const struct cly_arg_kword_term * terminal,
                           const struct cly_arg_kword_parm * parameter,
                           const char *                      word,
                           size_t                            length,
                           struct cly_match *                matches)
{
}

static const struct cly_arg_ops cly_set_leaf_parm_ops = {
	.parse    = cly_set_parse_leaf_parm
	.complete = cly_set_complete_leaf_parm
};

static bool
cly_set_filter_arg_node(const struct lysc_node * node)
{
	cly_assert(node);
	cly_assert(srepo_sch_node_type(node) == LYS_LEAF);

#warning refactor me with show command filters!!
	return (srepo_sch_conf_flags(node) == LYS_CONFIG_W) &&
	       !(srepo_sch_status_flags(node) &
	         (LYS_STATUS_OBSLT | LYS_STATUS_DEPRC));
}













static int
cly_show_parse_cmd(const struct cly_cmd * command,
                   const struct cly_dir * directory,
                   struct cly_context *   context,
                   int                    argc,
                   const char * const     argv[])
{
	cly_show_assert_cmd((const struct cly_show_cmd *)command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argv);

	struct cly_show_work * wk;
	int                    ret;

	/* Cannot fail. */
	wk = (struct cly_show_work *)
	     cly_dir_work_create(sizeof(*wk), command, &cly_show_work_ops);
	wk->format = CLY_TABLE_DATA_FMT;

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
	cly_dir_work_destroy((struct cly_dir_work *)wk);

	return ret;
}

static void
cly_set_fini_cmd(struct cly_cmd * command)
{
	struct cly_srepo_table * tbl = ((struct cly_show_cmd *)command)->table;

	cly_srepo_table_fini(tbl);
	cly_free(tbl);
}

static const struct cly_cmd_ops cly_set_cmd_ops = {
	.parse    = cly_set_parse_cmd,
	.complete = cly_cmd_complete_args,
	.fini     = cly_set_fini_cmd
};



















static int
cly_set_create_cmd(struct cly_show_cmd **     command,
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

	struct cly_cmd *   cmd;
	struct cly_arg *   choice;
	struct lysc_node * child;
	int                ret;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with a command name validated thanks to cly_cmd_name_isok().
	 */
	cly_cmd_sized_create(&cmd, sizeof(*cmd), name, &cly_set_cmd_ops);
	cly_assert(cmd);

	/* Cannot fail. */
	choice = cly_arg_createn_add_choice((struct cly_node *)cmd);
	cly_dir_work_createn_add_arg(true, (struct cly_node *)choice);

	srepo_sch_foreach_child(node, child) {
		if ((srepo_sch_node_type(child) == LYS_LEAF) && filter(child)) {
			struct cly_arg_parm * parm;

			ret = cly_arg_createn_add_parm(
				&parm,
				srepo_sch_node_name(child),
				&cly_set_leaf_parm_ops,
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
		}
	}

	if (!cly_node_has_child(&choice->super)) {
		cly_scn_debug(choice,
		              "'%s' set command creation ignored: "
		              "no child schema data node found",
		              name);
		ret = 0;
		goto destroy;
	}

	cly_dir_add_cmd(directory, cmd);

	*command = cmd;

	return 0;

destroy:
	cly_cmd_destroy(cmd);

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
	cly_assert(== LYS_CONTAINER);
	cly_assert(name);
	cly_assert_context(context);

	struct cly_cmd * cmd;
	const char *     msg;
	int              err;

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
	                          cly_set_filter_arg_node,
	                          context);

err:
	cly_scn_err(container, "cannot create set command '%s': %s", name, msg);

	return err;
}
