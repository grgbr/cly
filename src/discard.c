#include "discard.h"
#include "cmd.h"
#include "cly.h"

/******************************************************************************
 * `discard' command handling.
 * Discard current configuration editions and exit edition mode.
 ******************************************************************************/

struct cly_disc_work {
	struct cly_work        super;
	const struct cly_cmd * cmd;
};

static int
cly_disc_exec_work(struct cly_work * work, struct cly_context * context)
{
	const struct cly_disc_work * wk = (struct cly_disc_work *)work;
	sr_session_ctx_t **          sess = cly_cand_session(context);
	sr_error_t                   err;
	int                          ret = -EPERM;

	if (!*sess) {
		cly_cmd_talk(wk->cmd, "no ongoing edition");
		return 0;
	}

	if (srepo_has_changes(*sess)) {
		err = srepo_discard_changes(*sess);
		if (err != SR_ERR_OK)
			goto stop;
	}
	else
		cly_cmd_talk(wk->cmd, "no editions to discard");

	err = srepo_session_stop(*sess);
	if (err != SR_ERR_OK)
		goto err;

	ret = 0;
	goto out;

stop:
	srepo_session_stop(*sess);
err:
	cly_cmd_talk(wk->cmd, "cannot discard editions: %s", sr_strerror(err));
out:
	cly_info("exited edition mode");
	*sess = NULL;

	return ret;
}

static const struct cly_work_ops cly_disc_work_ops = {
	.exec    = cly_disc_exec_work,
	.release = cly_null_release_work
};

static int
cly_disc_parse_cmd(const struct cly_cmd * command,
                   const struct cly_dir * directory __unused,
                   struct cly_context *   context,
                   int                    argc __unused,
                   const char * const     argv[] __unused)
{
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argv);

	struct cly_disc_work * wk;
	int                    ret;

	/* Cannot fail. */
	wk = (struct cly_disc_work *)
	     cly_create_work(sizeof(*wk), &cly_disc_work_ops);
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

static const struct cly_cmd_ops cly_disc_cmd_ops = {
	.parse    = cly_disc_parse_cmd,
	.complete = cly_cmd_complete_args,
	.fini     = cly_cmd_null_fini
};

void
cly_disc_build_cmd(struct cly_dir * directory)
{
	cly_dir_assert(directory);

	struct cly_cmd * cmd;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with the "discard" name argument.
	 */
	cly_assert(sizeof("discard") <= CLY_ARG_MAX);
	cly_dir_create_cmdn_add(directory, &cmd, "discard", &cly_disc_cmd_ops);
}
