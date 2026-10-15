#include "quit.h"
#include "cmd.h"
#include "cly.h"

/******************************************************************************
 * `quit' command handling.
 * Terminate command line.
 ******************************************************************************/

static int
cly_quit_exec_work(struct cly_work *    work __unused,
                   struct cly_context * context __unused)
{
	/*
	 * We might use cly_shell_shutdown() but we don't want to depend on the
	 * shell since we might be called from the command line directly...
	 */
	return -ESHUTDOWN;
}

static const struct cly_work_ops cly_quit_work_ops = {
	.exec    = cly_quit_exec_work,
	.release = cly_null_release_work
};

static int
cly_quit_parse_cmd(const struct cly_cmd * command,
                  const struct cly_dir * directory,
                  struct cly_context *   context,
                  int                    argc __unused,
                  const char * const     argv[] __unused)
{
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argv);

	struct cly_work * wk;
	int               ret;

	/* Cannot fail. */
	wk = cly_create_work(sizeof(*wk), &cly_quit_work_ops);

	ret = cly_sched_work(context, wk);
	if (ret) {
		cly_cmd_log(command, "cannot schedule work.");
		goto destroy;
	}

	return 0;

destroy:
	cly_destroy_work(wk);

	return ret;
}

static const struct cly_cmd_ops cly_quit_cmd_ops = {
	.parse    = cly_quit_parse_cmd,
	.complete = cly_cmd_complete_args,
	.fini     = cly_cmd_null_fini
};

void
cly_quit_build_cmd(struct cly_dir * directory)
{
	cly_dir_assert(directory);

	struct cly_cmd * cmd;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with the "quit" name argument.
	 */
	cly_assert(sizeof("quit") <= CLY_ARG_MAX);
	cly_dir_create_cmdn_add(directory, &cmd, "quit", &cly_quit_cmd_ops);
}
