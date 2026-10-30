#include "exit.h"
#include "cmd.h"
#include "cly.h"

/******************************************************************************
 * `exit' command handling.
 * Exit edition mode.
 ******************************************************************************/

struct cly_exit_work {
	struct cly_work        super;
	const struct cly_cmd * cmd;
};

static int
cly_exit_exec_work(struct cly_work * work, struct cly_context * context)
{
	const struct cly_exit_work * wk = (struct cly_exit_work *)work;
	sr_session_ctx_t **          sess = cly_cand_session(context);
	sr_error_t                   err;

	if (!*sess) {
		cly_cmd_talk(wk->cmd, "no ongoing edition");
		return 0;
	}

	cly_assert(!srepo_has_changes(*sess));
	err = srepo_session_stop(*sess);
	if (err != SR_ERR_OK)
		cly_warn("cannot stop edition session: %s", sr_strerror(err));

	cly_info("exited edition mode");
	*sess = NULL;

	return 0;
}

static const struct cly_work_ops cly_exit_work_ops = {
	.exec    = cly_exit_exec_work,
	.release = cly_null_release_work
};

static int
cly_exit_parse_cmd(const struct cly_cmd * command,
                     const struct cly_dir * directory __unused,
                     struct cly_context *   context,
                     int                    argc __unused,
                     const char * const     argv[] __unused)
{
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argv);

	struct cly_exit_work * wk;
	int                      ret;

	/* Cannot fail. */
	wk = (struct cly_exit_work *)
	     cly_create_work(sizeof(*wk), &cly_exit_work_ops);
	wk->cmd = command;

	ret = cly_sched_work(context, (struct cly_work *)wk);
	if (ret) {
		cly_cmd_talk(command, "cannot schedule work.");
		goto destroy;
	}

	return 0;

destroy:
	cly_destroy_work((struct cly_work *)wk);

	return ret;
}

static const struct cly_cmd_ops cly_exit_cmd_ops = {
	.parse    = cly_exit_parse_cmd,
	.complete = cly_cmd_complete_args,
	.fini     = cly_cmd_null_fini
};

void
cly_exit_build_cmd(struct cly_dir * directory)
{
	cly_dir_assert(directory);

	struct cly_cmd * cmd;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with the "exit" name argument.
	 */
	cly_assert(sizeof("exit") <= CLY_ARG_MAX);
	cly_dir_create_cmdn_add(directory, &cmd, "exit", &cly_exit_cmd_ops);
}

