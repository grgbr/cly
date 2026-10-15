#ifndef _CLY_ARG_H
#define _CLY_ARG_H

#include "node.h"
#include <string.h>

#define CLY_ARG_MAX (512U)
#if CLY_ARG_MAX > CLY_LINE_MAX
#error Inconsistent maximum argument and line lengths !
#endif

struct cly_arg;
struct cly_cmd;
struct cly_dir;
struct cly_context;
struct cly_match;

/******************************************************************************
 * Argument utilities
 ******************************************************************************/

extern size_t
_cly_arg_isstr_valid(const char * string, size_t length);

/******************************************************************************
 * Base argument handling
 ******************************************************************************/

typedef int cly_arg_parse_fn(const struct cly_arg *,
                             const struct cly_cmd *,
                             const struct cly_dir *,
                             struct cly_context *,
                             int,
                             const char * const [],
                             void *);

typedef void cly_arg_complete_fn(const struct cly_arg *,
                                 const struct cly_cmd *,
                                 const struct cly_dir *,
                                 struct cly_context *,
                                 const char *,
                                 size_t,
                                 int,
                                 const char * const [],
                                 struct cly_match *);

struct cly_arg_ops {
	cly_arg_parse_fn *    parse;
	cly_arg_complete_fn * complete;
};

#define cly_arg_assert_ops(_opers) \
	cly_assert(_opers); \
	cly_assert((_opers)->parse); \
	cly_assert((_opers)->complete)

struct cly_arg {
	struct cly_node            super;
	const struct cly_arg_ops * ops;
};

#define cly_arg_assert(_arg) \
	cly_assert(_arg); \
	cly_arg_assert_ops((_arg)->ops)

static inline int
cly_arg_parse(const struct cly_arg * argument,
              const struct cly_cmd * command,
              const struct cly_dir * directory,
              struct cly_context *   context,
              int                    argc,
              const char * const     argv[],
              void *                 data)
{
	cly_arg_assert(argument);
	cly_assert(command);
	cly_assert(directory);
	cly_assert(context);
	cly_assert(argc > 0);
	cly_assert(argv);
	cly_assert(argv[0]);

	return argument->ops->parse(argument,
	                            command,
	                            directory,
	                            context,
	                            argc,
	                            argv,
	                            data);
}

static inline void
cly_arg_complete(const struct cly_arg * argument,
                 const struct cly_cmd * command,
                 const struct cly_dir * directory,
                 struct cly_context *   context,
                 const char *           word,
                 size_t                 length,
                 int                    argc,
                 const char * const     argv[],
                 struct cly_match *     matches)
{
	cly_arg_assert(argument);
	cly_assert(command);
	cly_assert(directory);
	cly_assert(context);
	cly_assert(word);
	cly_assert(length < CLY_ARG_MAX);
	cly_assert(strnlen(word, CLY_ARG_MAX) == length);
	cly_assert(argc >= 0);
	cly_assert(!argc || argv);
	cly_assert(matches);

	return argument->ops->complete(argument,
	                               command,
	                               directory,
	                               context,
	                               word,
	                               length,
	                               argc,
	                               argv,
	                               matches);
}

static inline void
cly_arg_add_child(struct cly_arg * argument, struct cly_arg * child)
{
	cly_arg_assert(argument);
	cly_arg_assert(child);

	cly_node_add_child(&argument->super, &child->super);
}

extern struct cly_arg *
cly_arg_create(size_t size, const struct cly_arg_ops * opers);

static inline void
cly_arg_destroy(struct cly_arg * argument)
{
	cly_arg_assert(argument);

	cly_free(argument);
}

/******************************************************************************
 * Parameter argument handling
 ******************************************************************************/

struct cly_arg_parm {
	struct cly_arg super;
	const char *   name;
	size_t         len;
};

extern int
cly_arg_create_parm(struct cly_arg_parm **     parameter,
                    const char *               name,
                    const struct cly_arg_ops * opers);

extern int
cly_arg_createn_add_parm(struct cly_arg_parm **     parameter,
                         const char *               name,
                         const struct cly_arg_ops * opers,
                         struct cly_node *          cmd_or_arg);

/******************************************************************************
 * Choice argument handling
 ******************************************************************************/

extern struct cly_arg *
cly_arg_create_choice(void);

extern struct cly_arg *
cly_arg_createn_add_choice(struct cly_node * cmd_or_arg);

/******************************************************************************
 * Keyword parameter argument handling
 ******************************************************************************/

struct cly_arg_kword_term {
	const char *       value;
	size_t             len;
	cly_arg_parse_fn * on_match;
};

#define CLY_ARG_KWORD_TERM(_value, _on_match) \
	{ \
		.value    = _value, \
		.len      = compile_eval(_stroll_is_const(_value) && \
		                         _stroll_is_array(_value), \
		                         sizeof(_value) - 1, \
		                         "constant string expected"), \
		.on_match = _on_match \
	}

struct cly_arg_kword_parm {
	struct cly_arg_parm               super;
	unsigned int                      nr;
	const struct cly_arg_kword_term * terms;
};

extern int
cly_arg_create_kword_parm(struct cly_arg_kword_parm **      parameter,
                          const char *                      name,
                          const struct cly_arg_kword_term * terminals,
                          unsigned int                      nr);

extern int
cly_arg_createn_add_kword_parm(struct cly_arg_kword_parm **      parameter,
                               const char *                      name,
                               const struct cly_arg_kword_term * terminals,
                               unsigned int                      nr,
                               struct cly_node *                 cmd_or_arg);

#endif /* _CLY_ARG_H */
