#include "cd.h"
#include "cmd.h"
#include "cly.h"

/******************************************************************************
 * `cd' command handling.
 * Change current working directory.
 ******************************************************************************/

static int
cly_chdir_exec_work(struct cly_work * work, struct cly_context * context)
{
	struct cly_dir_work *  wk = (struct cly_dir_work *)work;
	const struct cly_dir * dir;
	int                    ret;

	ret = cly_dir_work_search(wk, context, &dir);
	if (ret)
		return ret;

	/*
	 * Given the directory descriptor found above, change current working
	 * directory.
	 */
	cly_chdir(context, dir);

	return 0;
}

static const struct cly_work_ops cly_chdir_work_ops = {
	.exec    = cly_chdir_exec_work,
	.release = cly_dir_work_release
};

static int
cly_chdir_parse_cmd(const struct cly_cmd * command,
                    const struct cly_dir * directory,
                    struct cly_context *   context,
                    int                    argc,
                    const char * const     argv[])
{
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argv);

	if (argc == 1) {
		struct cly_dir_work * wk;
		int                   ret;

		/* Cannot fail. */
		wk = cly_dir_work_create(sizeof(*wk),
		                         command,
		                         &cly_chdir_work_ops);

		ret = cly_cmd_parse_args(command,
		                         directory,
		                         context,
		                         argc,
		                         argv,
		                         wk);
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

	cly_cmd_log(command, "invalid number of argument(s).");

	return -EINVAL;
}

static const struct cly_cmd_ops cly_chdir_cmd_ops = {
	.parse    = cly_chdir_parse_cmd,
	.complete = cly_cmd_complete_args,
	.fini     = cly_cmd_null_fini
};

void
cly_chdir_build_cmd(struct cly_dir * directory)
{
	cly_dir_assert(directory);

	struct cly_cmd * cmd;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with the "cd" name argument.
	 */
	cly_assert(sizeof("cd") <= CLY_ARG_MAX);
	cly_dir_create_cmdn_add(directory, &cmd, "cd", &cly_chdir_cmd_ops);

	/* Cannot fail either. */
	cly_dir_work_createn_add_arg(true, (struct cly_node *)cmd);
}
