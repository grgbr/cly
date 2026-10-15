#include "schema.h"
#include "dir.h"
#include "cly.h"

/******************************************************************************
 * `schema' command handling.
 * Print schema for current working command line node.
 ******************************************************************************/

struct cly_schema_work {
	struct cly_dir_work super;
	LYS_OUTFORMAT       format;
};

struct cly_schema_show {
	const struct cly_schema_work * work;
	const struct cly_context *     context;
};

static int
cly_schema_show_dir(const struct cly_dir *         directory,
                    const struct cly_schema_show * show)
{
	cly_dir_assert(directory);
	cly_assert((cly_dir_type(directory) == CLY_DIR_MOD_TYPE) ||
	           (cly_dir_type(directory) == CLY_DIR_NODE_TYPE));
	cly_assert(show);
	cly_assert(show->work);
	cly_assert_context(show->context);

	int ret;

	switch (show->work->format) {
#if defined(CONFIG_CLY_DEBUG)
	case LYS_OUT_TREE:
		ret = cly_dir_show_diag(directory, show->context);
		if (ret)
			cly_cmd_log(
				show->work->super.cmd,
				"schema: cannot show YANG tree diagram: %s.",
				cly_dir_strerror(-ret));
		break;
#endif /* defined(CONFIG_CLY_DEBUG) */

	case LYS_OUT_YANG_COMPILED:
		ret = cly_dir_show_yang(directory, show->context);
		if (ret)
			cly_cmd_log(
				show->work->super.cmd,
				"schema: cannot show YANG specification: %s.",
				cly_dir_strerror(-ret));
		break;

	default:
		cly_assert(0);
	}

	return ret;
}

static int
cly_schema_show_visit(struct cly_dir *    directory,
                      enum cly_walk_event event,
                      void *              data)
{
	cly_dir_assert(directory);
	cly_assert(data);

	switch (event) {
	case CLY_PRE_WALK_EVT:
		if (cly_dir_type(directory) != CLY_DIR_NONE_TYPE) {
			cly_schema_show_dir(directory, data);

			return CLY_SKIP_WALK_RET;
		}

		break;

	case CLY_POST_WALK_EVT:
		break;

	default:
		cly_assert(0);
	}

	return CLY_CONT_WALK_RET;
}

static int
cly_schema_exec_work(struct cly_work * work, struct cly_context * context)
{
	const struct cly_schema_work * wk = (const struct cly_schema_work *)
	                                    work;
	const struct cly_dir *         dir;
	int                            ret;
	const struct cly_schema_show   show = {
		.work    = wk,
		.context = context
	};

	ret = cly_dir_work_search(&wk->super, context, &dir);
	if (ret)
		return ret;

	/*
	 * Given the directory descriptor found above, display its schema if it
	 * matches a real libyang object (module or node)...
	 */
	if (cly_dir_type(dir) != CLY_DIR_NONE_TYPE)
		return cly_schema_show_dir(dir, &show);

	/* ... search the highest level children pointing to a real libyang
	 * object and show their corresponding schemas.
	 */
	return cly_dir_walk((struct cly_dir *)dir,
	                    cly_schema_show_visit,
	                    (void *)&show);
}

static const struct cly_work_ops cly_schema_work_ops = {
	.exec    = cly_schema_exec_work,
	.release = cly_dir_work_release
};

static int
cly_schema_parse_cmd(const struct cly_cmd * command,
                     const struct cly_dir * directory,
                     struct cly_context *   context,
                     int                    argc,
                     const char * const     argv[])
{
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argv);

	struct cly_schema_work * wk;
	int                      ret;

	/* Cannot fail. */
	wk = (struct cly_schema_work *)
	     cly_dir_work_create(sizeof(*wk), command, &cly_schema_work_ops);
	wk->format = LYS_OUT_YANG_COMPILED;

	ret = cly_cmd_parse_args(command, directory, context, argc, argv, wk);
	if (ret < 0)
		goto destroy;

	cly_assert(ret == argc);
	ret = cly_sched_work(context, (struct cly_work *)wk);
	if (ret) {
		cly_cmd_log(command, "cannot schedule work.");
		goto destroy;
	}

	return argc;

destroy:
	cly_dir_work_destroy((struct cly_dir_work *)wk);

	return ret;
}

static const struct cly_cmd_ops cly_schema_cmd_ops = {
	.parse    = cly_schema_parse_cmd,
	.complete = cly_cmd_complete_args,
	.fini     = cly_cmd_null_fini
};

static int
cly_schema_on_yang_match(const struct cly_arg * argument __unused,
                         const struct cly_cmd * command __unused,
                         const struct cly_dir * directory __unused,
                         struct cly_context *   context __unused,
                         int                    argc __unused,
                         const char * const     argv[] __unused,
                         void *                 data)
{
	((struct cly_schema_work *)data)->format = LYS_OUT_YANG_COMPILED;

	return 0;
}

#if defined(CONFIG_CLY_DEBUG)

static int
cly_schema_on_tree_match(const struct cly_arg * argument __unused,
                         const struct cly_cmd * command __unused,
                         const struct cly_dir * directory __unused,
                         struct cly_context *   context __unused,
                         int                    argc __unused,
                         const char * const     argv[] __unused,
                         void *                 data)
{
	((struct cly_schema_work *)data)->format = LYS_OUT_TREE;

	return 0;
}

#endif /* defined(CONFIG_CLY_DEBUG) */

static const struct cly_arg_kword_term cly_schema_format_terms[] = {
	CLY_ARG_KWORD_TERM("yang", cly_schema_on_yang_match),
#if defined(CONFIG_CLY_DEBUG)
	CLY_ARG_KWORD_TERM("tree", cly_schema_on_tree_match),
#endif /* defined(CONFIG_CLY_DEBUG) */
};

void
cly_schema_build_cmd(struct cly_dir * directory)
{
	cly_dir_assert(directory);

	struct cly_cmd *            cmd;
	struct cly_arg *            choice;
	struct cly_arg_kword_parm * fmt;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with the "schema" name argument.
	 */
	cly_assert(sizeof("schema") <= CLY_ARG_MAX);
	cly_dir_create_cmdn_add(directory, &cmd, "schema", &cly_schema_cmd_ops);

	/* Cannot fail. */
	choice = cly_arg_createn_add_choice((struct cly_node *)cmd);
	cly_dir_work_createn_add_arg(false, (struct cly_node *)choice);

	/*
	 * No need to check for returned code since this cannot fail with the
	 * "format" name argument.
	 */
	cly_assert(sizeof("format") <= CLY_ARG_MAX);
	cly_arg_createn_add_kword_parm(&fmt,
	                               "format",
	                               cly_schema_format_terms,
	                               stroll_array_nr(cly_schema_format_terms),
	                               (struct cly_node *)choice);
}
