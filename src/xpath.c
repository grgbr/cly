#include "xpath.h"
#include "dir.h"
#include "cly.h"

/******************************************************************************
 * `xpath' command handling.
 * Print XPATH for current working directory.
 ******************************************************************************/

static int
cly_xpath_exec_work(struct cly_work * work, struct cly_context * context)
{
	struct cly_dir_work *  wk = (struct cly_dir_work *)work;
	const struct cly_dir * dir;
	int                    ret;
	char *                 xpath;

	ret = cly_dir_work_search(wk, context, &dir);
	if (ret)
		return ret;

	/*
	 * Given the directory descriptor found above, compute the corresponding
	 * XPATH and display it.
	 */
	xpath = cly_dir_xpath(dir);
	cly_assert(xpath);
	printf("%s\n", xpath);
	cly_free(xpath);

	return 0;

}

static const struct cly_work_ops cly_xpath_work_ops = {
	.exec    = cly_xpath_exec_work,
	.release = cly_dir_work_release
};

static int
cly_xpath_parse_cmd(const struct cly_cmd * command,
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
	wk = cly_dir_work_create(sizeof(*wk), command, &cly_xpath_work_ops);

	ret = cly_cmd_parse_args(command, directory, context, argc, argv, wk);
	if (ret < 0)
		goto destroy;

	cly_assert(ret == argc);
	ret = cly_sched_work(context, &wk->super);
	if (ret) {
		cly_cmd_talk(command, "cannot schedule work");
		goto destroy;
	}

	return argc;

destroy:
	cly_dir_work_destroy(wk);

	return ret;
}

static const struct cly_cmd_ops cly_xpath_cmd_ops = {
	.parse    = cly_xpath_parse_cmd,
	.complete = cly_cmd_complete_args,
	.fini     = cly_cmd_null_fini
};

void
cly_xpath_build_cmd(struct cly_dir * directory)
{
	cly_dir_assert(directory);

	struct cly_cmd * cmd;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with the "xpath" name argument.
	 */
	cly_assert(sizeof("xpath") <= CLY_ARG_MAX);
	cly_dir_create_cmdn_add(directory, &cmd, "xpath", &cly_xpath_cmd_ops);

	/* Cannot fail either. */
	cly_dir_work_createn_add_arg(false, (struct cly_node *)cmd);
}
