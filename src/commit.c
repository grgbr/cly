#include "commit.h"
#include "cmd.h"
#include "cly.h"

/******************************************************************************
 * `commit' command handling.
 * Commit configuration editions and exit edition mode when successful.
 ******************************************************************************/

struct cly_commit_work {
	struct cly_work        super;
	const struct cly_cmd * cmd;
};

static int
cly_commit_exec_work(struct cly_work * work, struct cly_context * context)
{
	const struct cly_commit_work * wk = (struct cly_commit_work *)work;
	sr_session_ctx_t **            sess = cly_cand_session(context);
	sr_error_t                     err;

	if (!*sess) {
		cly_cmd_talk(wk->cmd, "no ongoing edition");
		return 0;
	}

	if (srepo_has_changes(*sess)) {
		err = srepo_apply_changes(*sess);
		if (err != SR_ERR_OK)
			goto err;
	}
	else
		cly_cmd_talk(wk->cmd, "no edition to commit");

	err = srepo_session_stop(*sess);
	if (err != SR_ERR_OK)
		goto err;

	cly_info("exited edition mode");
	*sess = NULL;

	return 0;

err:
	/*
	 *  Leave edition as-is so that the user may continue in edition mode to
	 * fix things.
	 * He will still be able to discard current editions thanks to the
	 * `discard' command.
	 */
	cly_cmd_talk(wk->cmd, "cannot commit editions: %s", sr_strerror(err));

	return -EPERM;
}

static const struct cly_work_ops cly_commit_work_ops = {
	.exec    = cly_commit_exec_work,
	.release = cly_null_release_work
};

static int
cly_commit_parse_cmd(const struct cly_cmd * command,
                     const struct cly_dir * directory __unused,
                     struct cly_context *   context,
                     int                    argc __unused,
                     const char * const     argv[] __unused)
{
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argv);

	struct cly_commit_work * wk;
	int                      ret;

	/* Cannot fail. */
	wk = (struct cly_commit_work *)
	     cly_create_work(sizeof(*wk), &cly_commit_work_ops);
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

static const struct cly_cmd_ops cly_commit_cmd_ops = {
	.parse    = cly_commit_parse_cmd,
	.complete = cly_cmd_complete_args,
	.fini     = cly_cmd_null_fini
};

void
cly_commit_build_cmd(struct cly_dir * directory)
{
	cly_dir_assert(directory);

	struct cly_cmd * cmd;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with the "commit" name argument.
	 */
	cly_assert(sizeof("commit") <= CLY_ARG_MAX);
	cly_dir_create_cmdn_add(directory, &cmd, "commit", &cly_commit_cmd_ops);
}
