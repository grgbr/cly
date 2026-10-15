#include "find.h"
#include "cly.h"
#include "cmd.h"

/******************************************************************************
 * `find' command handling.
 * List menu directory entries recursively.
 ******************************************************************************/

struct cly_find_show {
	const struct cly_dir * anc;
	char *                 path;
	size_t                 off;
};

static int
cly_find_show_dir(struct cly_dir *    directory,
                  enum cly_walk_event event,
                  void *              data)
{
	cly_dir_assert(directory);
	cly_assert(data);

	switch (event) {
	case CLY_PRE_WALK_EVT:
		{
			const struct cly_find_show * show = data;
			ssize_t                      ret;

			ret = cly_dir_mkrel(directory,
			                    show->anc,
			                    &show->path[show->off],
			                    CLY_PATH_MAX - show->off);

			cly_assert(ret);
			if (ret > 0)
				printf("%s\n", show->path);
			else
				cly_log("find: cannot show: %s.",
				        cly_dir_strerror(-ret));

			break;
		}

	case CLY_POST_WALK_EVT:
		break;

	default:
		cly_assert(0);
	}

	return CLY_CONT_WALK_RET;
}

static int
cly_find_exec_work(struct cly_work * work, struct cly_context * context)
{
	struct cly_dir_work *  wk = (struct cly_dir_work *)work;
	const struct cly_dir * dir;
	int                    ret;
	struct cly_find_show   show;

	ret = cly_dir_work_search(wk, context, &dir);
	if (ret)
		return ret;

	/*
	 * Given the directory descriptor found above, display its descendant
	 * directory entries.
	 */
	show.anc = dir;
	show.path = wk->norm;
	show.off = wk->len;

	return cly_dir_walk((struct cly_dir *)dir, cly_find_show_dir, &show);
}

static const struct cly_work_ops cly_find_work_ops = {
	.exec    = cly_find_exec_work,
	.release = cly_dir_work_release
};

static int
cly_find_parse_cmd(const struct cly_cmd * command,
                   const struct cly_dir * directory,
                   struct cly_context *   context,
                   int                    argc,
                   const char * const     argv[])
{
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argv);

	struct cly_dir_work * wk;
	int                   ret;

	/* Cannot fail. */
	wk = cly_dir_work_create(sizeof(*wk), command, &cly_find_work_ops);

	ret = cly_cmd_parse_args(command, directory, context, argc, argv, wk);
	if (ret < 0)
		goto destroy;

	cly_assert(ret == argc);
	ret = cly_sched_work(context, &wk->super);
	if (ret) {
		cly_cmd_log(command, "cannot schedule work.");
		goto destroy;
	}

	return argc;

destroy:
	cly_dir_work_destroy(wk);

	return ret;
}

static const struct cly_cmd_ops cly_find_cmd_ops = {
	.parse    = cly_find_parse_cmd,
	.complete = cly_cmd_complete_args,
	.fini     = cly_cmd_null_fini
};

void
cly_find_build_cmd(struct cly_dir * directory)
{
	cly_dir_assert(directory);

	struct cly_cmd * cmd;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with the "find" name argument.
	 */
	cly_assert(sizeof("find") <= CLY_ARG_MAX);
	cly_dir_create_cmdn_add(directory, &cmd, "find", &cly_find_cmd_ops);

	/* Cannot fail either. */
	cly_dir_work_createn_add_arg(false, (struct cly_node *)cmd);
}
