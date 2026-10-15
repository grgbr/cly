#include "pwd.h"
#include "cmd.h"
#include "cly.h"

/******************************************************************************
 * `pwd' command handling.
 * Show absolute path of current working directory.
 ******************************************************************************/

static int
cly_pwd_exec_work(struct cly_work *    work __unused,
                  struct cly_context * context)
{

	char *  path;
	ssize_t ret;

	path = cly_malloc(CLY_PATH_MAX);
	cly_assert(path);

	ret = cly_dir_mkabs(cly_cwd(context), path, CLY_PATH_MAX);
	if (ret > 0) {
		printf("%s\n", path);
		ret = 0;
		goto out;
	}

	cly_log("pwd: cannot show current working directory: %s.",
	        cly_dir_strerror(-ret));

out:
	cly_free(path);

	return ret;
}

static const struct cly_work_ops cly_pwd_work_ops = {
	.exec    = cly_pwd_exec_work,
	.release = cly_null_release_work
};

static int
cly_pwd_parse_cmd(const struct cly_cmd * command,
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
	wk = cly_create_work(sizeof(*wk), &cly_pwd_work_ops);

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

static const struct cly_cmd_ops cly_pwd_cmd_ops = {
	.parse    = cly_pwd_parse_cmd,
	.complete = cly_cmd_complete_args,
	.fini     = cly_cmd_null_fini
};

void
cly_pwd_build_cmd(struct cly_dir * directory)
{
	cly_dir_assert(directory);

	struct cly_cmd * cmd;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with the "pwd" name argument.
	 */
	cly_assert(sizeof("pwd") <= CLY_ARG_MAX);
	cly_dir_create_cmdn_add(directory, &cmd, "pwd", &cly_pwd_cmd_ops);
}
