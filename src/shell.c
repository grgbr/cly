#include "shell.h"
#include "expr.h"
#include "arg.h"
#include <readline/history.h>
#include <utils/path.h>

struct cly_shell {
	char *                       prompt;
	char *                       pref;
	void *                       data;
	cly_shell_collect_compl_fn * collect_compl;
	struct cly_match             matches;
	cly_shell_display_compl_fn * display_compl;
	void *                       display_data;
	volatile sig_atomic_t        redisplay;
	volatile sig_atomic_t        shutdown;
	char *                       hpath;
};

#define cly_shell_assert(_shell) \
	cly_assert((_shell)->prompt); \
	cly_assert((_shell)->pref); \
	cly_assert(!(_shell)->collect_compl || (_shell)->display_compl)

static struct cly_shell cly_the_shell;

static int
cly_shell_handle_readline_events(void)
{
	if (cly_the_shell.redisplay) {
		/* Move cursor to next line. */
		rl_crlf();
		/* Tell readline we have moved onto a new empty line. */
		rl_on_new_line();
		/* Wipe buffered line content out. */
		rl_replace_line("", 0);

		if (RL_ISSTATE(RL_STATE_ISEARCH | RL_STATE_NUMERICARG)) {
			/*
			 * We are in the middle of an incremental searching or
			 * numeric argument probing operation.
			 * Restore our own prompt since both operations
			 * temporarily modify current prompt and that we have
			 * just interrupted them.
			 */
			rl_restore_prompt();

			/*
			 * Cause a blocking readline() to return immediately
			 * instead of waiting for the next end-of-line input
			 * character.
			 * This gives readline() caller a chance to purge
			 * buffered input as soon as possible to prevent from
			 * mixing it with post interruption input characters.
			 * The reason why we cannot free these buffered
			 * characters here is that readline provides no public
			 * API (I know of) to purge characters buffered by its
			 * incremental searching logic. As a consequence we need
			 * to wait for readline() to return before being able to
			 * release them.
			 * This is the reason we leave cly_the_shell.redisplay
			 * untouched here...
			 */
			rl_done = 1;
		}
		else {
			/* Tell readline it should update screen. */
			rl_redisplay();

			/* Mark redisplay event as completed. */
			cly_the_shell.redisplay = 0;
		}
	}
	else if (cly_the_shell.shutdown)
		/* Request readline() to return immediately. */
		rl_done = 1;

	return 0;
}

static int
cly_shell_read_line(const struct cly_shell * shell, char ** line)
{
	cly_assert(line);

	char * ln;
	size_t len;
	int    ret;

	/* Reset redisplay event handling logic. */
	cly_the_shell.redisplay = 0;

	ln = readline(shell->prompt);
	if (!ln) {
		/* End of stream required using ^D. */
		return -ESHUTDOWN;
	}

	if (shell->shutdown) {
		/* We were explicitly requested to shutdown. */
		ret = -ESHUTDOWN;
		goto free;
	}

	if (cly_the_shell.redisplay || (ln[strspn(ln, " \t")] == '\0')) {
		/*
		 * Input line is empty...
		 *
		 * When cly_the_shell.redisplay is true, a redisplay event
		 * happened during the course of readline() and it has not been
		 * processed entirely, i.e. we were interrupted in the middle of
		 * an incremental searching and / or numeric argument probing
		 * operation.
		 * Complete the event by freeing readline's buffered input.
		 * See cly_shell_handle_readline_events();
		 */
		ret = -ENODATA;
		goto free;
	}

	len = strnlen(ln, CLY_LINE_MAX);
	if (len >= CLY_LINE_MAX) {
		cly_talk("invalid input line: too long.");
		ret = -ENOBUFS;
		goto free;
	}

	add_history(ln);

	*line = ln;

	return 0;

free:
	cly_free(ln);

	return ret;
}

#if 0
/* Keep this just in case we need to save parsed line into history. */
static void
cly_shell_hist_expr(const struct cly_expr_blk * block)
{
	cly_expr_blk_assert(block);

	char *  ln;
	ssize_t len;

	ln = cly_malloc(CLY_LINE_MAX);
	cly_assert(ln);

	len = cly_expr_blk_make_string(block, ln, CLY_LINE_MAX);
	if (len < 0) {
		cly_talk("cannot log into history: expression too long.");
		goto free;
	}

	add_history(ln);

free:
	cly_free(ln);
}
#endif

void
cly_shell_redisplay(void)
{
	cly_the_shell.redisplay = 1;
}

void
cly_shell_shutdown(void)
{
	cly_the_shell.shutdown = 1;
}

void
cly_shell_enroll_display_compl(cly_shell_display_compl_fn * display,
                               void *                       data)
{
	cly_shell_assert(&cly_the_shell);
	cly_assert(display);

	cly_the_shell.display_compl = display;
	cly_the_shell.display_data = data;
}

int
cly_shell_read_expr(struct cly_expr_blk * expr_block)
{
	cly_shell_assert(&cly_the_shell);
	cly_expr_blk_assert(expr_block);

	char * ln;
	int    ret;

	ret = cly_shell_read_line(&cly_the_shell, &ln);
	if (ret < 0)
		return ret;

	ret = cly_expr_blk_parse_line(expr_block, ln);
	if (ret < 0)
		goto free;

	return 0;

free:
	cly_free(ln);

	return ret;
}

static void
_cly_shell_set_prompt(struct cly_shell * shell, const char * prompt)
{
	cly_assert(shell->pref);
	cly_assert(prompt);
	cly_assert(*prompt);

	int ret __unused;

	ret = cly_asprintf(&shell->prompt, "%s%s> ", shell->pref, prompt);
	cly_assert(ret >= 4);
}

void
cly_shell_set_prompt(const char * prompt)
{
	cly_shell_assert(&cly_the_shell);
	cly_assert(prompt);
	cly_assert(*prompt);

	cly_free(cly_the_shell.prompt);
	_cly_shell_set_prompt(&cly_the_shell, prompt);
}

static char *
cly_shell_init_hist(void)
{
	const char * bdir; /* Base directory path. */
	const char * name = program_invocation_short_name;
	size_t       len;
	char *       cdir; /* Configuration directory path. */
	char *       hpath;
	int          ret = -EINVAL;

	bdir = secure_getenv("XDG_CONFIG_HOME");
	if (!bdir) {
		bdir = secure_getenv("HOME");
		if (!bdir)
			goto out;
		len = strnlen(bdir, PATH_MAX);
		if (!len || (len == PATH_MAX))
			goto out;
		ret = cly_asprintf(&cdir, "%s/.config/%s", bdir, name);
		cly_assert(ret >= 11);
	}
	else {
		len = strnlen(bdir, PATH_MAX);
		if (!len || (len == PATH_MAX))
			goto out;
		ret = cly_asprintf(&cdir, "%s/%s", bdir, name);
		cly_assert(ret >= 3);
	}

	ret = upath_mkdir(cdir, S_IRUSR | S_IWUSR | S_IXUSR);
	if (ret && (ret != -EEXIST))
		goto free;

	ret = cly_asprintf(&hpath, "%s/history", cdir);
	cly_assert(ret > 8);
	cly_assert(hpath);

	ret = -read_history(hpath);
	if (ret && (ret != -ENOENT))
		cly_warn("cannot load history: %s", strerror(-ret));

	cly_free(cdir);

	return hpath;

free:
	cly_free(cdir);
out:
	cly_warn("cannot setup history: %s", strerror(-ret));

	return NULL;
}

static void
cly_shell_fini_hist(struct cly_shell * shell)
{
	cly_assert(shell->hpath);

	int err;

	err = write_history(shell->hpath);
	if (err)
		cly_warn("cannot save history: %s", strerror(err));

	cly_free(shell->hpath);
}

static void
cly_shell_display_match_list(char ** matches,
                             int     count,
                             int     max_length,
                             void *  data __unused)
{
	rl_display_match_list(matches, count, max_length);
}

static void
cly_shell_display_match(char ** matches, int count, int max_length)
{
	/*
	 * Do not honor the `rl_completion_query_items' setting for now since
	 * current readline(3)'s get_y_or_n() implementation is not ready for
	 * rl_callback_handler_install() / alternate interface support yet
	 * (as of version 8.3).
	 * Readline(3) pager logic will still be thrown when the number of
	 * completion items requires it.
	 */

	cly_the_shell.display_compl(matches,
	                            count,
	                            max_length,
	                            cly_the_shell.display_data);

	rl_forced_update_display();
	extern int rl_display_fixed;
	rl_display_fixed = 1;
}

static char *
cly_shell_generate_match(const char * word __unused, int state)
{
	cly_assert(word);
	cly_assert(state >= 0);

	return cly_match_get(&cly_the_shell.matches, state);
}

static char **
cly_shell_complete(const char * word, int begin, int end)
{
	cly_shell_assert(&cly_the_shell);
	cly_assert(word);
	cly_assert(begin >= 0);
	cly_assert(end >= 0);
	cly_assert(begin <= end);
	cly_assert(end <= rl_end);
	cly_assert(strlen(word) == (size_t)(end - begin));

	char ** match = NULL;

	/* Restore default completion matches display behavior. */
	cly_the_shell.display_compl = cly_shell_display_match_list;

	if (((size_t)end < CLY_LINE_MAX) &&
	    ((size_t)(end - begin) < CLY_ARG_MAX)) {
		int              first = begin;
		int              last = begin;
		struct cly_expr  expr;

		/*
		 * Probe for the begining of last command within the current
		 * line, then get rid of leading spaces from there...
		 */
		first = begin;
		while (first && (rl_line_buffer[first - 1] != ';'))
			first--;
		first += strspn(&rl_line_buffer[first], " \t");
		cly_assert(first <= begin);

		/* ...in addition, also get rid of trailing spaces. */
		while ((last > first) &&
		       (rl_line_buffer[last - 1] == ' ' ||
		        rl_line_buffer[last - 1] == '\t'))
			last--;
		cly_assert(last <= begin);
		cly_assert(last >= first);

		cly_match_init(&cly_the_shell.matches);

		if (last - first) {
			char * ln;

			/*
			 * ...then duplicate the command up and including the
			 * word preceding the word to complete into a newly
			 * allocated string.
			 */
			ln = cly_malloc((last - first) + 1);
			cly_assert(ln);
			memcpy(ln, &rl_line_buffer[first], last - first);
			ln[last - first] = '\0';

			cly_expr_init(&expr);
			if (!cly_expr_parse_string(&expr, ln))
				cly_the_shell.collect_compl(
					&cly_the_shell.matches,
					word,
					(size_t)(end - begin),
					cly_expr_arg_cnt(&expr),
					cly_expr_args(&expr),
					cly_the_shell.data);
			cly_expr_fini(&expr);

			cly_free(ln);
		}
		else
			cly_the_shell.collect_compl(&cly_the_shell.matches,
			                            word,
			                            (size_t)(end - begin),
			                            0,
			                            NULL,
			                            cly_the_shell.data);
		if (cly_match_count(&cly_the_shell.matches))
			match = rl_completion_matches(word,
			                              cly_shell_generate_match);
		cly_match_fini(&cly_the_shell.matches);
	}

	/*
	 * Give ownership of matched strings to readline(3). It will free(3)
	 * them at completion process termination time.
	 */
	return match;
}

/* Disable readline's default completion logic. */
static char *
cly_shell_null_complete(const char * word __unused, int len __unused)
{
	return NULL;
}

static void
cly_shell_setup_compl(struct cly_shell *           shell,
                      cly_shell_collect_compl_fn * complete)
{
	cly_assert(shell);

	if (complete) {
		/*
		 * Install our own cly_shell_complete() completion function and
		 * disable readline's default completion logic.
		 */
		rl_completion_entry_function = cly_shell_null_complete;
		shell->collect_compl = complete;
		rl_attempted_completion_function = cly_shell_complete;

		/*
		 * In addition install our own completion display function to
		 * give the command / argument completion logic an opportunity
		 * to override default readline(3) behavior.
		 */
		shell->display_compl = cly_shell_display_match_list;
		rl_completion_display_matches_hook = cly_shell_display_match;

		/* Enable completion. */
		rl_inhibit_completion = 0;
	}
	else
		/* Disable completion entirely. */
		rl_inhibit_completion = 1;
}

int
cly_shell_init(bool                         history,
               const char *                 break_chars,
               cly_shell_collect_compl_fn * complete,
               void *                       data)

{
	cly_assert(!break_chars || (break_chars[0] != '\0'));

	const char * user;
	size_t       len;
	char *       host;
	int          ret __unused;

	user = secure_getenv("USER");
	if (!user)
		goto err;
	len = strnlen(user, LOGIN_NAME_MAX);
	if (!len || (len == LOGIN_NAME_MAX))
		goto err;

	host = cly_malloc(HOST_NAME_MAX + 1);
	cly_assert(host);
	ret = gethostname(host, HOST_NAME_MAX + 1);
	cly_assert(!ret);

	ret = cly_asprintf(&cly_the_shell.pref, "%s@%s:", user, host);
	cly_assert(ret >= 4);

	cly_free(host);

	_cly_shell_set_prompt(&cly_the_shell, "/");
	cly_the_shell.hpath = NULL;
	cly_the_shell.redisplay = 0;
	cly_the_shell.shutdown = 0;

	/* Setup the list of characters that signal a break between words. */
	if (break_chars)
		rl_basic_word_break_characters = break_chars;
	
	cly_shell_setup_compl(&cly_the_shell, complete);
	cly_the_shell.data = data;

	if (history)
		cly_the_shell.hpath = cly_shell_init_hist();

	rl_readline_name = program_invocation_short_name;

	rl_event_hook = cly_shell_handle_readline_events;

	return 0;

err:
	cly_talk("cannot setup shell: %s.", strerror(EINVAL));

	return -EINVAL;
}

void
cly_shell_fini(void)
{
	cly_shell_assert(&cly_the_shell);

	if (cly_the_shell.hpath)
		cly_shell_fini_hist(&cly_the_shell);

	cly_free(cly_the_shell.pref);
	cly_free(cly_the_shell.prompt);
}
