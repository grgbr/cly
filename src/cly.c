#include "cly.h"
#include "build.h"
#include "list.h"
#include "cd.h"
#include "pwd.h"
#include "find.h"
#include "xpath.h"
#include "schema.h"
#include "quit.h"
#include <sys/ioctl.h>

/******************************************************************************
 * Utilities
 ******************************************************************************/

unsigned int
cly_term_cols(const struct cly_context * context)
{
#warning TODO: plug in a SIGWINCH signal handler

	cly_assert_context(context);

	if (context->isatty) {
		int cols;

		rl_get_screen_size(NULL, &cols);
		cly_assert(cols > 0);

		return cols;
	}
	else
		return 0;
}

/******************************************************************************
 * Work handling
 ******************************************************************************/

static void
cly_release_workq(struct cly_context * context)
{
	cly_assert_context(context);

	unsigned int w;

	for (w = 0; w < context->wkcnt; w++)
		cly_destroy_work(context->wkq[w]);

	context->wkcnt = 0;
}

static int
cly_exec_workq(struct cly_context * context)
{
	cly_assert_context(context);

	unsigned int w;
	int          ret;

	for (w = 0, ret = 0; (w < context->wkcnt) && !ret; w++) {
		struct cly_work * wk = context->wkq[w];

		ret = wk->ops->exec(wk, context);
	}

	cly_release_workq(context);

	return ret;
}

/******************************************************************************
 * Overall cli context handling
 ******************************************************************************/

static bool
cly_shell_ison(const struct cly_context * context)
{
	return context->interact;
}

void
cly_chdir(struct cly_context * context, const struct cly_dir * directory)
{
	cly_assert_context(context);
	cly_dir_assert(directory);

	if (cly_shell_ison(context)) {
		char *  path;
		ssize_t len;

		path = cly_malloc(CLY_PATH_MAX);
		cly_assert(path);
		len = cly_dir_mkabs(directory, path,  CLY_PATH_MAX);
		cly_assert(len > 0);

		cly_shell_set_prompt(path);

		cly_free(path);
	}

	context->cwd = directory;
}

static int
cly_parse(struct cly_context * context, int argc, const char * const argv[])
{
	cly_assert_context(context);
	cly_assert_args(argc, argv);

	int ret;

	ret = cly_dir_parse_cmd(context->cwd, context, argc, argv);
	if (!ret && (context->cwd != &context->root))
		ret = cly_dir_parse_cmd(&context->root, context, argc, argv);

	if (ret == argc)
		return 0;

	if (!ret) {
		cly_log("'%s': no such command.", argv[0]);
		return -EINVAL;
	}

	cly_assert(ret < 0);

	return ret;
}

static int
cly_parse_expr_blk(struct cly_context *        context,
                   const struct cly_expr_blk * expr_block)
{
	cly_assert_context(context);
	cly_assert(expr_block);

	const struct cly_expr * expr;

	cly_expr_blk_foreach(expr_block, expr) {
		int ret;

		ret = cly_parse(context,
		                cly_expr_arg_cnt(expr),
		                cly_expr_args(expr));
		if (ret)
			return ret;
	}

	return 0;
}

static void
cly_complete(struct cly_match * matches,
             const char *       word,
             size_t             length,
             int                argc,
             const char * const argv[],
             void *             data)
{
	cly_assert(matches);
	cly_assert(word);
	cly_assert(length < CLY_ARG_MAX);
	cly_assert(strnlen(word, CLY_ARG_MAX) == length);
	cly_assert(argc >= 0);
	cly_assert(!argc || argv);

	struct cly_context * ctx = data;

	cly_dir_complete_cmd(ctx->cwd,
	                     ctx,
	                     word,
	                     length,
	                     argc,
	                     argv,
	                     matches);
	if (ctx->cwd != &ctx->root)
		cly_dir_complete_cmd(&ctx->root,
		                     ctx,
		                     word,
		                     length,
		                     argc,
		                     argv,
		                     matches);
}

static int
cly_init_context(struct cly_context * context, const char * const * style)
{
	cly_assert(context);
	cly_assert(style);

	int err;

	cly_init_log(CONFIG_CLY_LOG_SEVERITY_VALUE);

#if defined(CONFIG_CLY_DEBUG)
	/*
	 * SR_CTX_SET_PRIV_PARSED is required to print YANG diagram trees.
	 * In addition, SR_CTX_SET_PRIV_PARSED is activated for non-printed
	 * contexts only.
	 * This is why the SR_CTX_NO_PRINTED option must also be
	 * passed at the expense of reduced performances.
	 */
	err = sr_context_options(SR_CTX_NO_PRINTED | SR_CTX_SET_PRIV_PARSED,
	                         1,
	                         NULL);
	if (err != SR_ERR_OK) {
		cly_log("cannot setup repo context options: %s",
		        sr_strerror(err));
		return err;
	}
#endif /* defined(CONFIG_CLY_DEBUG) */

	err = sr_connect(SR_CONN_DEFAULT, &context->conn);
	if (err != SR_ERR_OK) {
		cly_log("cannot open repo connection: %s",
		        sr_strerror(err));
		return err;
	}

	err = sr_session_start(context->conn,
	                       SR_DS_OPERATIONAL,
	                       &context->sess);
	if (err != SR_ERR_OK) {
		cly_log("cannot start repo session: %s",
		        sr_strerror(err));
		goto disconnect;
	}

	context->lyctx = sr_session_acquire_context(context->sess);
	assert(context->lyctx);

	err = ly_out_new_file(stdout, &context->lyout);
	if (err != LY_SUCCESS) {
		cly_log("cannot open yang output printer: %s",
		        ly_strerr(err));
		err = SR_ERR_LY;
		goto release;
	}

	context->wkcnt = 0;
	cly_dir_init_root(&context->root);
	context->cwd = &context->root;
	context->isatty = !!isatty(STDOUT_FILENO);
	context->colored = !!context->isatty;
	context->style = context->colored ? style : NULL;
	context->interact = false;

	return SR_ERR_OK;

release:
	sr_session_release_context(context->sess);
	sr_session_stop(context->sess);
disconnect:
	sr_disconnect(context->conn);

	return err;
}

static int
cly_visitn_destroy_dir(struct cly_dir *    directory,
                       enum cly_walk_event event,
                       void *              data __unused)
{
	if (event == CLY_POST_WALK_EVT)
		cly_dir_destroy(directory);

	return CLY_CONT_WALK_RET;
}

static void
cly_fini_context(struct cly_context * context)
{
	cly_assert_context(context);

	cly_release_workq(context);
	cly_dir_walk_safe(&context->root, cly_visitn_destroy_dir, NULL);
	cly_dir_fini_root(&context->root);
	ly_out_free(context->lyout, NULL, 0);
	sr_session_release_context(context->sess);
	sr_session_stop(context->sess);
	sr_disconnect(context->conn);

	cly_fini_log();
}

/******************************************************************************
 * Top-level logic
 ******************************************************************************/

static const char * const cly_the_style[] = {
	[CLY_LABEL_STYLE_KIND]   = CLY_UNDERLINE_COLOR,
	[CLY_VALUE_STYLE_KIND]   = NULL,
	[CLY_DEFAULT_STYLE_KIND] = CLY_GRAY_COLOR,
	[CLY_ERROR_STYLE_KIND]   = CLY_BOLD_RED_COLOR
};

static int
cly_init(struct cly_context * context)
{
	cly_assert(context);

	int ret;

	ret = cly_init_context(context, cly_the_style);
	if (ret)
		return ret;

	cly_list_build_cmd(&context->root);
	cly_chdir_build_cmd(&context->root);
	cly_pwd_build_cmd(&context->root);
	cly_find_build_cmd(&context->root);
	cly_xpath_build_cmd(&context->root);
	cly_schema_build_cmd(&context->root);
	cly_quit_build_cmd(&context->root);

	ret = cly_build_from_schema(context);
	if (ret)
		goto fini;

	return 0;

fini:
	cly_fini_context(context);

	return ret;
}

static void
cly_fini(struct cly_context * context)
{
	cly_fini_context(context);
}

int
main(int argc, const char * const argv[])
{
	cly_assert(argc == 1);
	cly_assert(argv);

	struct cly_context ctx;
	int                ret;

	ret = cly_init(&ctx);
	if (ret)
		goto out;

	if (argc == 1) {
		if (!ctx.isatty)
			/* Cannot run in interactive mode... */
			goto fini;

		ret = cly_shell_init(true, " \t;\n", cly_complete, &ctx);
		if (ret)
			goto fini_shell;

		ctx.interact = true;

		do {
			struct cly_expr_blk eblk = CLY_EXPR_BLK_INIT(eblk);

			ret = cly_shell_read_expr(&eblk);
			cly_assert(ret <= 0);
			switch (ret) {
			case 0:
				/* General expression syntax ok. */
				ret = cly_parse_expr_blk(&ctx, &eblk);
				if (!ret)
					ret = cly_exec_workq(&ctx);
				break;

			case -ESHUTDOWN:
				/* Shell shutdown requested. */
				break;

			case -ENODATA:
				/* Empty input. */
			case -EINVAL:
				/* Invalid expression argument character. */
			case -ENAMETOOLONG:
				/* Expression argument too long. */
			case -ENOBUFS:
				/* Too many expressions within block. */
			default:
				/* Other input line fetching / parsing error. */
				break;
			}

			/* Release expression block resources. */
			cly_expr_blk_fini(&eblk);
		} while (ret != -ESHUTDOWN);
		if (ret == -ESHUTDOWN)
			ret = 0;

fini_shell:
		cly_shell_fini();
	}
	else {
		ret = cly_parse(&ctx, argc - 1, &argv[1]);
		if (ret)
			goto fini;

		ret = cly_exec_workq(&ctx);
	}

fini:
	cly_fini(&ctx);
out:
	return (!ret) ? EXIT_SUCCESS : EXIT_FAILURE;
}
