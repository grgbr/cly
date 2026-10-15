#include "arg.h"
#include "shell.h"
#include "cmd.h"
#include "dir.h"
#include "cly.h"
#include <string.h>

/******************************************************************************
 * Argument utilities
 ******************************************************************************/

static const char cly_arg_valid_chrs[] = "abcdefghijklmnopqrstuvwxyz"
                                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "0123456789"
                                         " /-_.=,:@";

#if 0
/* Keep this just in case we would need it. */
static bool
cly_arg_ischr_valid(int chr)
{
	return !!memchr(cly_arg_valid_chrs,
	                chr,
	                sizeof(cly_arg_valid_chrs) - 1);
}
#endif

size_t
_cly_arg_isstr_valid(const char * string, size_t length)
{
	cly_assert(string);
	cly_assert(length);
	cly_assert(length < CLY_ARG_MAX);
	cly_assert(strnlen(string, length) == length);

	size_t len = strspn(string, cly_arg_valid_chrs);

	return (string[len] == '\0') ? 0 : len;
}

/******************************************************************************
 * Base argument handling
 ******************************************************************************/

#define cly_arg_foreach_child(_arg, _child) \
	for (_child = (const struct cly_arg *)((_arg)->super.child); \
	     _child; \
	     _child = (const struct cly_arg *)((_child)->super.next))

static void
cly_arg_setup(struct cly_arg * argument, const struct cly_arg_ops * opers)
{
	cly_assert(argument);
	cly_arg_assert_ops(opers);

	cly_node_setup(&argument->super);
	argument->ops = opers;
}

struct cly_arg *
cly_arg_create(size_t size, const struct cly_arg_ops * opers)
{
	cly_assert(size >= sizeof(struct cly_arg));
	cly_arg_assert_ops(opers);

	struct cly_arg * arg;

	arg = cly_malloc(size);
	cly_assert(arg);

	cly_arg_setup(arg, opers);

	return arg;
}

/******************************************************************************
 * Parameter argument handling
 ******************************************************************************/

#define cly_arg_assert_parm(_parm) \
	cly_assert(_parm); \
	cly_arg_assert(&(_parm)->super); \
	cly_assert((_parm)->name); \
	cly_assert((_parm)->len); \
	cly_assert((_parm)->len < CLY_ARG_MAX); \
	cly_assert(strnlen((_parm)->name, CLY_ARG_MAX) == (_parm)->len)

static int
cly_arg_setup_parm(struct cly_arg_parm *      parameter,
                   const char *               name,
                   const struct cly_arg_ops * opers)
{
	cly_assert(parameter);
	cly_assert(name);
	cly_arg_assert_ops(opers);

	size_t len;

	len = strnlen(name, CLY_ARG_MAX);
	if (!len)
		return -ENODATA;
	if (len == CLY_ARG_MAX)
		return -ENAMETOOLONG;

	cly_arg_setup(&parameter->super, opers);
	parameter->name = name;
	parameter->len = len;

	return 0;
}

int
cly_arg_create_parm(struct cly_arg_parm **     parameter,
                    const char *               name,
                    const struct cly_arg_ops * opers)
{
	cly_assert(parameter);
	cly_assert(name);
	cly_arg_assert_ops(opers);

	struct cly_arg_parm * parm;
	int                   ret;

	parm = cly_malloc(sizeof(*parm));
	cly_assert(parm);

	ret = cly_arg_setup_parm(parm, name, opers);
	if (!ret) {
		*parameter = parm;
		return 0;
	}

	cly_free(parm);

	return ret;
}

/******************************************************************************
 * Choice argument handling
 ******************************************************************************/

static int
cly_arg_parse_choice(const struct cly_arg * choice,
                     const struct cly_cmd * command,
                     const struct cly_dir * directory,
                     struct cly_context *   context,
                     int                    argc,
                     const char * const     argv[],
                     void *                 data)
{
	cly_arg_assert(choice);
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argc >= 1);
	cly_assert(argv[0]);
	cly_assert(strnlen(argv[0], CLY_ARG_MAX) < CLY_ARG_MAX);

	const struct cly_node * opt;

	cly_node_foreach_child(&choice->super, opt) {
		int ret;

		ret = cly_arg_parse((const struct cly_arg *)opt,
		                    command,
		                    directory,
		                    context,
		                    1,
		                    &argv[0],
		                    data);
		cly_assert(ret <= 1);
		if (ret)
			return ret;
	}

	cly_cmd_log(command, "'%s': invalid argument.", argv[0]);

	return -EINVAL;
}

static void
cly_arg_complete_choice(const struct cly_arg * choice,
                        const struct cly_cmd * command,
                        const struct cly_dir * directory,
                        struct cly_context *   context,
                        const char *           word,
                        size_t                 length,
                        int                    argc,
                        const char * const     argv[],
                        struct cly_match *     matches)
{
	cly_arg_assert(choice);
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_match_assert(matches);

	const struct cly_node * opt;

	cly_node_foreach_child(&choice->super, opt)
		cly_arg_complete((const struct cly_arg *)opt,
		                 command,
		                 directory,
		                 context,
		                 word,
		                 length,
		                 argc,
		                 argv,
		                 matches);
}

static const struct cly_arg_ops cly_arg_choice_ops = {
	.parse    = cly_arg_parse_choice,
	.complete = cly_arg_complete_choice
};

struct cly_arg *
cly_arg_create_choice(void)
{
	return cly_arg_create(sizeof(struct cly_arg), &cly_arg_choice_ops);
}

struct cly_arg *
cly_arg_createn_add_choice(struct cly_node * cmd_or_arg)
{
	cly_node_assert(cmd_or_arg);

	struct cly_arg * choice;

	choice = cly_arg_create_choice();
	cly_assert(choice);
	cly_node_add_child(cmd_or_arg, &choice->super);

	return choice;
}

/******************************************************************************
 * Keyword parameter argument handling
 ******************************************************************************/

#define cly_arg_assert_kword_term(_term) \
	cly_assert(_term); \
	cly_assert((_term)->value); \
	cly_assert((_term)->len); \
	cly_assert((_term)->len < CLY_ARG_MAX); \
	cly_assert(strnlen((_term)->value, CLY_ARG_MAX) == (_term)->len)

#define cly_arg_assert_kword_parm(_parm) \
	cly_assert(_parm); \
	cly_arg_assert_parm(&(_parm)->super); \
	cly_assert((_parm)->nr); \
	cly_assert(&(_parm)->super); \
	cly_arg_assert_terms((_parm)->terms, (_parm)->nr)

#if defined(CONFIG_CLY_ASSERT)

static void
cly_arg_assert_terms(const struct cly_arg_kword_term * terminals,
                     unsigned int                      nr)
{
	cly_assert(terminals);
	cly_assert(nr);

	unsigned int t;

	for (t = 0; t < nr; t++) {
		cly_arg_assert_kword_term(&terminals[t]);
	}
}

#endif /* defined(CONFIG_CLY_ASSERT) */

static int
cly_arg_parse_kword_term(const struct cly_arg_kword_term * terminal,
                         const struct cly_arg_kword_parm * parameter,
                         const struct cly_cmd *            command,
                         const struct cly_dir *            directory,
                         struct cly_context *              context,
                         const char *                      string,
                         void *                            data)
{
	cly_arg_assert_kword_term(terminal);

	size_t len;

	len = strnlen(string, terminal->len + 1);
	if ((len != terminal->len) ||
	    memcmp(string, terminal->value, terminal->len))
		return -EINVAL;

	return terminal->on_match((const struct cly_arg *)parameter,
	                          command,
	                          directory,
	                          context,
	                          1,
	                          &string,
	                          data);
}

static int
cly_arg_parse_kword_parm(const struct cly_arg * argument,
                         const struct cly_cmd * command,
                         const struct cly_dir * directory,
                         struct cly_context *   context,
                         int                    argc,
                         const char * const     argv[],
                         void *                 data)
{
	cly_arg_assert_kword_parm((const struct cly_arg_kword_parm *)argument);
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argc == 1);
	cly_assert(argv[0]);
	cly_assert(strnlen(argv[0], CLY_ARG_MAX) < CLY_ARG_MAX);

	const struct cly_arg_kword_parm * parm =
		(const struct cly_arg_kword_parm *)argument;
	const char *                      str = argv[0];
	size_t                            len;
	unsigned int                      t;

	len = strcspn(str, "=");
	if ((len != parm->super.len) ||
	    (str[len] != '=') ||
	    memcmp(str, parm->super.name, parm->super.len))
		return 0;

	str = &str[len + 1];
	if (*str == '\0') {
		cly_cmd_log(command,
		            "'%s': missing parameter keyword.",
		            parm->super.name);
		return -EINVAL;
	}

	for (t = 0; t < parm->nr; t++) {
		int ret;

		ret = cly_arg_parse_kword_term(&parm->terms[t],
		                               parm,
		                               command,
		                               directory,
		                               context,
		                               str,
		                               data);
		cly_assert(ret <= 0);
		if (!ret)
			/* Matched successfully. */
			return 1;
	}

	cly_cmd_log(command, "'%s': invalid %s parameter keyword.",
	            str,
	            parm->super.name);

	return -EINVAL;
}

static void
cly_arg_display_kword_list(char ** matches,
                           int     count,
                           int     max_length,
                           void *  data)
{
	cly_arg_assert_kword_parm((const struct cly_arg_kword_parm *)data);

	char **                           list;
	int                               c;
	const struct cly_arg_kword_parm * parm =
		(const struct cly_arg_kword_parm *)data;

	list = cly_malloc((count + 2) * sizeof(list[0]));
	cly_assert(list);

	for (c = 0; c <= count; c++)
		list[c] = &matches[c][parm->super.len + 1];
	list[count + 1] = NULL;

	rl_display_match_list(list,
	                      count,
	                      max_length - (parm->super.len + 1));

	cly_free(list);
}

static void
cly_arg_complete_kword_term(const struct cly_arg_kword_term * terminal,
                            const struct cly_arg_kword_parm * parameter,
                            const char *                      word,
                            size_t                            length,
                            struct cly_match *                matches)
{
	cly_arg_assert_kword_term(terminal);

	if (!length ||
	    ((length <= terminal->len) &&
	     !memcmp(word, terminal->value, length))) {
		char * kw;
		int    ret;

		ret = cly_asprintf(&kw,
		                   "%s=%s",
		                   parameter->super.name,
		                   terminal->value);
		cly_assert((size_t)ret ==
		           (parameter->super.len + 1 + terminal->len));

		/*
		 * Give ownership of `kw' to `matches'. Allocated string
		 * ownership will be transfered from `matches' to readline(3) by
		 * cly_shell_complete().
		 * Readline(3) will free(3) it at completion process termination
		 * time.
		 */
		cly_match_push(matches, kw);
	}
}

static bool
cly_arg_kwork_already_completed(const struct cly_arg_kword_parm * parm,
                                int                               argc,
                                const char * const                argv[])
{
	cly_arg_assert_kword_parm(parm);

	int a;

	for (a = 0; a < argc; a++) {
		const char * arg = argv[a];

		if (!strncmp(arg, parm->super.name, parm->super.len) &&
		    arg[parm->super.len] == '=')
			return true;
	}

	return false;
}

static void
_cly_arg_complete_kword_parm(const struct cly_arg * argument,
                             const char *           word,
                             size_t                 length,
                             struct cly_match *     matches)
{
	cly_arg_assert_kword_parm((const struct cly_arg_kword_parm *)argument);
	cly_match_assert(matches);

	const struct cly_arg_kword_parm * parm =
		(const struct cly_arg_kword_parm *)argument;

	if (length <= parm->super.len) {
		if (!length || !memcmp(word, parm->super.name, length)) {
			char * name;
			int    ret;

			ret = cly_asprintf(&name, "%s=", parm->super.name);
			cly_assert((size_t)ret == (parm->super.len + 1));

			/*
			 * Give ownership of `name' to `matches'. Allocated
			 * string ownership will be transfered from `matches' to
			 * readline(3) by cly_shell_complete().
			 * Readline(3) will free(3) it at completion process
			 * termination time.
			 */
			cly_match_push(matches, name);

			/*
			 * Disable appending the completion character after a
			 * selected match.
			 */
			cly_shell_suppress_complete_char();
		}
	}
	else if (!memcmp(word, parm->super.name, parm->super.len) &&
		 (word[parm->super.len] == '=')) {
		unsigned int t;

		word = &word[parm->super.len + 1];
		length -= parm->super.len + 1;

		for (t = 0; t < parm->nr; t++)
			cly_arg_complete_kword_term(&parm->terms[t],
			                            parm,
			                            word,
			                            length,
			                            matches);

		if (cly_match_count(matches))
			cly_shell_enroll_display_compl(
				cly_arg_display_kword_list,
				(void *)parm);
	}
}

static void
cly_arg_complete_kword_parm(const struct cly_arg * argument,
                            const struct cly_cmd * command __unused,
                            const struct cly_dir * directory __unused,
                            struct cly_context *   context __unused,
                            const char *           word,
                            size_t                 length,
                            int                    argc __unused,
                            const char * const     argv[] __unused,
                            struct cly_match *     matches)
{
	cly_arg_assert_kword_parm((const struct cly_arg_kword_parm *)argument);
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_match_assert(matches);

	const struct cly_arg_kword_parm * parm =
		(const struct cly_arg_kword_parm *)argument;

	if (!cly_arg_kwork_already_completed(parm, argc, argv))
		_cly_arg_complete_kword_parm(argument, word, length, matches);
}

static const struct cly_arg_ops cly_arg_kword_parm_ops = {
	.parse    = cly_arg_parse_kword_parm,
	.complete = cly_arg_complete_kword_parm
};

int
cly_arg_create_kword_parm(struct cly_arg_kword_parm **      parameter,
                          const char *                      name,
                          const struct cly_arg_kword_term * terminals,
                          unsigned int                      nr)
{
	cly_assert(parameter);
	cly_assert(name);
	cly_arg_assert_terms(terminals, nr);

	struct cly_arg_kword_parm * parm;
	int                         ret;

	parm = cly_malloc(sizeof(*parm));
	cly_assert(parm);

	ret = cly_arg_setup_parm(&parm->super, name, &cly_arg_kword_parm_ops);
	if (!ret) {
		parm->nr = nr;
		parm->terms = terminals;
		*parameter = parm;

		return 0;
	}

	cly_free(parm);

	return ret;
}

int
cly_arg_createn_add_kword_parm(struct cly_arg_kword_parm **      parameter,
                               const char *                      name,
                               const struct cly_arg_kword_term * terminals,
                               unsigned int                      nr,
                               struct cly_node *                 cmd_or_arg)
{
	cly_assert(parameter);
	cly_assert(name);
	cly_arg_assert_terms(terminals, nr);
	cly_node_assert(cmd_or_arg);

	int ret;

	ret = cly_arg_create_kword_parm(parameter, name, terminals, nr);
	if (!ret) {
		cly_node_add_child(cmd_or_arg,
		                   *(struct cly_node **)parameter);
		return 0;
	}

	return ret;
}
