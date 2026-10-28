#include "edit.h"
#include "cmd.h"
#include "cly.h"

/******************************************************************************
 * `edit' command handling.
 * Enter editon mode.
 ******************************************************************************/

struct cly_edit_work {
	struct cly_work        super;
	const struct cly_cmd * cmd;
};

static int
cly_edit_exec_work(struct cly_work *    work, struct cly_context * context)
{
	sr_error_t                   err;
	const struct cly_edit_work * wk = (struct cly_edit_work *)work;
	sr_session_ctx_t **          sess = cly_cand_session(context);

	if (*sess) {
		cly_cmd_talk(wk->cmd, "edition already ongoing");
		return 0;
	}

	err = srepo_session_start(context->conn, SR_DS_CANDIDATE, sess);
	if (err == SR_ERR_OK) {
		cly_info("entered edition mode");
		return 0;
	}

	cly_cmd_talk(wk->cmd,
	             "cannot enter edition mode: %s",
	             sr_strerror(err));

	return -EPERM;
}

static const struct cly_work_ops cly_edit_work_ops = {
	.exec    = cly_edit_exec_work,
	.release = cly_null_release_work
};

static int
cly_edit_parse_cmd(const struct cly_cmd * command,
                   const struct cly_dir * directory __unused,
                   struct cly_context *   context,
                   int                    argc __unused,
                   const char * const     argv[] __unused)
{
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argv);

	struct cly_edit_work * wk;
	int                    ret;

	/* Cannot fail. */
	wk = (struct cly_edit_work *)
	     cly_create_work(sizeof(*wk), &cly_edit_work_ops);
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

static const struct cly_cmd_ops cly_edit_cmd_ops = {
	.parse    = cly_edit_parse_cmd,
	.complete = cly_cmd_complete_args,
	.fini     = cly_cmd_null_fini
};

void
cly_edit_build_cmd(struct cly_dir * directory)
{
	cly_dir_assert(directory);

	struct cly_cmd * cmd;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with the "edit" name argument.
	 */
	cly_assert(sizeof("edit") <= CLY_ARG_MAX);
	cly_dir_create_cmdn_add(directory, &cmd, "edit", &cly_edit_cmd_ops);
}
