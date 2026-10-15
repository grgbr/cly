#include "cmd.h"
#include "cly.h"

static bool
cly_cmd_has_args(const struct cly_cmd * command)
{
	return cly_node_has_child(&command->super);
}

int
cly_cmd_parse_args(const struct cly_cmd * command,
                   const struct cly_dir * directory,
                   struct cly_context *   context,
                   int                    argc,
                   const char * const     argv[],
                   void *                 data)
{
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argv);

	if (!argc)
		return 0;

	if (cly_cmd_has_args(command)) {
		int cnt = 0;

		do {
			struct cly_node * child;
			int               ret = 0;

			cly_node_foreach_child(&command->super, child) {
				const struct cly_arg * arg =
					(const struct cly_arg *)child;

				ret = cly_arg_parse(arg,
				                    command,
				                    directory,
				                    context,
				                    argc - cnt,
				                    &argv[cnt],
				                    data);
				if (ret)
					break;
			}

			cly_assert(ret <= (argc - cnt));
			if (!ret) {
				cly_cmd_talk(command,
				             "'%s': invalid argument.",
				             argv[cnt]);
				return -EINVAL;
			}
			else if (ret < 0)
				return ret;

			cnt += ret;
		} while (cnt < argc);

		cly_assert(cnt == argc);

		return argc;
	}

	cly_cmd_talk(command, "too many arguments.");

	return -EINVAL;
}

void
cly_cmd_complete_args(const struct cly_cmd * command,
                      const struct cly_dir * directory,
                      struct cly_context *   context,
                      const char *           word,
                      size_t                 length,
                      int                    argc,
                      const char * const     argv[],
                      struct cly_match *     matches)
{
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(word);
	cly_assert(length < CLY_ARG_MAX);
	cly_assert(strnlen(word, CLY_ARG_MAX) == length);
	cly_assert(argc >= 0);
	cly_assert(!argc || argv);
	cly_match_assert(matches);

	struct cly_node * child;

	cly_node_foreach_child(&command->super, child)
		cly_arg_complete((const struct cly_arg *)child,
		                 command,
		                 directory,
		                 context,
		                 word,
		                 length,
		                 argc,
		                 argv,
		                 matches);
}

int
cly_cmd_parse(const struct cly_cmd * command,
              const struct cly_dir * directory,
              struct cly_context *   context,
              int                    argc,
              const char * const     argv[])
{
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert_args(argc, argv);

	int ret = 0;

	if (!strcmp(argv[0], command->name)) {
		ret = command->ops->parse(command,
		                          directory,
		                          context,
		                          argc - 1,
		                          &argv[1]);
		if (ret >= 0) {
			cly_assert((ret + 1) == argc);
			return argc;
		}
	}

	return ret;
}

void
cly_cmd_complete(const struct cly_cmd * command,
                 const struct cly_dir * directory,
                 struct cly_context *   context,
                 const char *           word,
                 size_t                 length,
                 int                    argc,
                 const char * const     argv[],
                 struct cly_match *     matches)
{
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(word);
	cly_assert(length < CLY_ARG_MAX);
	cly_assert(strnlen(word, CLY_ARG_MAX) == length);
	cly_assert(argc >= 0);
	cly_assert(!argc || argv);
	cly_match_assert(matches);

	if (argc) {
		if (!strcmp(argv[0], command->name))
			command->ops->complete(command,
			                       directory,
			                       context,
			                       word,
			                       length,
			                       argc - 1,
			                       &argv[1],
			                       matches);
	}
	else if (!length || !strncmp(word, command->name, length)) {
		char * name;

		name = cly_strdup(command->name);
		cly_assert(name);

		cly_match_push(matches, name);
	}
}

int
cly_cmd_init(struct cly_cmd *           command,
             const char *               name,
             const struct cly_cmd_ops * opers)
{
	cly_assert(command);
	cly_assert(name);
	cly_assert(name[0] != '\0');
	cly_cmd_assert_ops(opers);

	if (strnlen(name, CLY_ARG_MAX) == CLY_ARG_MAX)
		return -ENAMETOOLONG;

	cly_node_setup(&command->super);
	command->ops = opers;
	command->name = name;

	return 0;
}

static int
cly_cmd_destroy_arg(struct cly_node *   node,
                    enum cly_walk_event event,
                    void *              data __unused)
{
	cly_arg_assert((struct cly_arg *)node);

	switch (event) {
	case CLY_PRE_WALK_EVT:
		break;

	case CLY_POST_WALK_EVT:
		cly_arg_destroy((struct cly_arg *)node);
		break;

	default:
		cly_assert(0);
	}

	return 0;
}

void
cly_cmd_fini(struct cly_cmd * command)
{
	cly_cmd_assert(command);

	cly_node_walk_safe(&command->super, cly_cmd_destroy_arg, NULL);

	command->ops->fini(command);
}

int
cly_cmd_sized_create(struct cly_cmd **          command,
                     size_t                     size,
                     const char *               name,
                     const struct cly_cmd_ops * opers)
{
	cly_assert(command);
	cly_assert(size >= sizeof(**command));
	cly_assert(name);
	cly_assert(name[0] != '\0');
	cly_cmd_assert_ops(opers);

	struct cly_cmd * cmd;
	int              err;

	cmd = cly_malloc(size);

	err = cly_cmd_init(cmd, name, opers);
	if (err)
		return err;

	*command = cmd;

	return 0;
}
