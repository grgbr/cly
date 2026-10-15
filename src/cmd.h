#ifndef _CLY_CMD_H
#define _CLY_CMD_H

#include "arg.h"

struct cly_cmd;
struct cly_dir;
struct cly_context;
struct cly_match;

typedef int cly_cmd_parse_fn(const struct cly_cmd *,
                             const struct cly_dir *,
                             struct cly_context *,
                             int,
                             const char * const []);

typedef void cly_cmd_complete_fn(const struct cly_cmd *,
                                 const struct cly_dir *,
                                 struct cly_context *,
                                 const char *,
                                 size_t,
                                 int,
                                 const char * const [],
                                 struct cly_match *);

typedef void cly_cmd_fini_fn(struct cly_cmd *);

struct cly_cmd_ops {
	cly_cmd_parse_fn *    parse;
	cly_cmd_complete_fn * complete;
	cly_cmd_fini_fn *     fini;
};

#define cly_cmd_assert_ops(_ops) \
	cly_assert(_ops); \
	cly_assert((_ops)->parse); \
	cly_assert((_ops)->complete); \
	cly_assert((_ops)->fini)

struct cly_cmd {
	struct cly_node            super;
	const struct cly_cmd_ops * ops;
	const char *               name;
};

#define cly_cmd_assert(_cmd) \
	cly_assert(_cmd); \
	cly_node_assert(&(_cmd)->super); \
	cly_cmd_assert_ops((_cmd)->ops); \
	cly_assert(cly_cmd_name_isok((_cmd)->name))

#define cly_cmd_talk(_cmd, _format, ...) \
	cly_talk("%s: " _format, (_cmd)->name, ## __VA_ARGS__)

static inline bool
cly_cmd_name_isok(const char * name)
{
	cly_assert(name);

	size_t len;

	len = strnlen(name, CLY_ARG_MAX);

	return (len && (len < CLY_ARG_MAX)) ? true : false;
}

extern int
cly_cmd_parse_args(const struct cly_cmd * command,
                   const struct cly_dir * directory,
                   struct cly_context *   context,
                   int                    argc,
                   const char * const     argv[],
                   void *                 data);

extern int
cly_cmd_parse(const struct cly_cmd * command,
              const struct cly_dir * dir,
              struct cly_context *   context,
              int                    argc,
              const char * const     argv[]);

extern void
cly_cmd_complete_args(const struct cly_cmd * command,
                      const struct cly_dir * directory,
                      struct cly_context *   context,
                      const char *           word,
                      size_t                 length,
                      int                    argc,
                      const char * const     argv[],
                      struct cly_match *     matches);

extern void
cly_cmd_complete(const struct cly_cmd * command,
                 const struct cly_dir * directory,
                 struct cly_context *   context,
                 const char *           word,
                 size_t                 length,
                 int                    argc,
                 const char * const     argv[],
                 struct cly_match *     matches);

static inline void
cly_cmd_add_arg(struct cly_cmd * command, struct cly_arg * argument)
{
	cly_cmd_assert(command);
	cly_arg_assert(argument);

	cly_node_add_child(&command->super, &argument->super);
}

extern int
cly_cmd_init(struct cly_cmd *           command,
             const char *               name,
             const struct cly_cmd_ops * opers);

extern void
cly_cmd_fini(struct cly_cmd * command);

static inline void
cly_cmd_null_fini(struct cly_cmd * command __unused)
{
	cly_cmd_assert(command);
}

extern int
cly_cmd_sized_create(struct cly_cmd **          command,
                     size_t                     size,
                     const char *               name,
                     const struct cly_cmd_ops * opers);

static inline int
cly_cmd_create(struct cly_cmd **          command,
               const char *               name,
               const struct cly_cmd_ops * opers)
{
	cly_assert(command);
	cly_assert(name);
	cly_cmd_assert_ops(opers);

	return cly_cmd_sized_create(command, sizeof(**command), name, opers);
}

static inline void
cly_cmd_destroy(struct cly_cmd * command)
{
	cly_cmd_fini(command);
	cly_free(command);
}

#endif /* _CLY_CMD_H */
