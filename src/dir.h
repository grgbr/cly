#ifndef _CLY_DIR_H
#define _CLY_DIR_H

#include "path.h"
#include "cmd.h"
#include <stdbool.h>
#include <string.h>

struct cly_context;

#define CLY_DIR_PATH_MAX (512)
#if CLY_DIR_PATH_MAX > CLY_LINE_MAX
/* The user would not be able to enter such a long path anymay... */
#undef CLY_DIR_NAME_MAX
#define CLY_DIR_NAME_MAX CLY_LINE_MAX
#endif

enum cly_dir_type {
	CLY_DIR_NONE_TYPE,
	CLY_DIR_NODE_TYPE,
	CLY_DIR_MOD_TYPE,
	CLY_DIR_TYPE_NR
};

struct cly_dir {
	/* This directory path component. */
	char                     name[CLY_PATH_NAME_MAX];
	/* Next directory entry sibling. */
	struct cly_dir *         next;
	/* Previous directory entry sibling. */
	struct cly_dir *         prev;
	/* First child directory entry. */
	struct cly_dir *         child;
	/* Parent directory entry. */
	struct cly_dir *         parent;
	/* Command linked list head. */
	struct cly_cmd *         cmds;
	enum cly_dir_type        type;
	/* Libyang schema module or node related to this directory entry. */
	union {
		const struct lysc_node *  sch_node;
		const struct lys_module * sch_mod;
		const void *              sch_void;
	};
};

#define cly_dir_assert(_dir) \
	cly_assert(_dir); \
	cly_assert(((_dir)->name[0] != '\0') && \
	           (strnlen((_dir)->name, CLY_PATH_NAME_MAX) < \
	            CLY_PATH_NAME_MAX)); \
	cly_assert(((_dir)->type == CLY_DIR_NONE_TYPE) || \
	           ((_dir)->type == CLY_DIR_MOD_TYPE) || \
	           ((_dir)->type == CLY_DIR_NODE_TYPE)); \
	cly_assert(((_dir)->type == CLY_DIR_NONE_TYPE) || (_dir)->sch_void);

static inline const char *
cly_dir_strerror(int error)
{
	switch (error) {
	case ENOENT:
		return "no such directory";
	case EEXIST:
		return "directory exists";
	case EBADR:
		return "internal YANG error";
	default:
		return cly_path_strerror(error);
	}
}

static inline enum cly_dir_type
cly_dir_type(const struct cly_dir * directory)
{
	cly_dir_assert(directory);

	return directory->type;
}

static inline bool
cly_dir_has_child(const struct cly_dir * directory)
{
	cly_dir_assert(directory);

	return !!directory->child;
}

#define cly_dir_foreach_child(_dir, _child) \
	for (_child = (_dir)->child; _child; _child = (_child)->next)

#define cly_dir_foreach_child_safe(_dir, _child, _tmp) \
	for (_child = (_dir)->child; \
	     _child && (_tmp = (_child)->next, 1); \
	     _child = _tmp)

/*
 * For the directory given in argument, compute a path relavite to an ancestor
 * directory.
 *
 * @directory:  Directory to compute the path for.
 * @ancestor:   An ancestor directory of @directory.
 * @path:       Pre-allocated string where to put the computed absolute path.
 * @size:       Size of @path including the terminating NULL byte.
 *
 * @return: Length of computed relative path, excluding the terminating NULL
 *          byte.
 */
extern ssize_t
cly_dir_mkrel(const struct cly_dir * directory,
              const struct cly_dir * ancestor,
              char *                 path,
              size_t                 size);

/*
 * Compute absolute path for the directory given in argument.
 *
 * @directory:  Directory to compute the path for.
 * @path:       Pre-allocated string where to put the computed absolute path.
 * @size:       Size of @path including the terminating NULL byte.
 *
 * @return: Length of computed absolute path, excluding the terminating NULL
 *          byte.
 */
extern ssize_t
cly_dir_mkabs(const struct cly_dir * directory, char * path, size_t size);

extern char *
cly_dir_xpath(const struct cly_dir * directory)
	__cly_nonull(1) __malloc(cly_free, 1) __returns_nonull __warn_result;

extern int
cly_dir_show_yang(const struct cly_dir *     directory,
                  const struct cly_context * context);

extern int
cly_dir_show_diag(const struct cly_dir *     directory,
                  const struct cly_context * context);

typedef int cly_dir_visit_fn(struct cly_dir *, enum cly_walk_event, void *);

/*
 * Perform a depth-first traversal of directory tree which root is given as the
 * `directory' argument.
 * 
 * Warning ! The visit() function is not called for the root directory passed in
 *           argument.
 */
extern int
cly_dir_walk(struct cly_dir *   directory,
             cly_dir_visit_fn * visit,
             void *             data);

extern int
cly_dir_walk_safe(struct cly_dir *   directory,
                  cly_dir_visit_fn * visit,
                  void *             data);

extern int
cly_dir_search_from_path(const struct cly_dir ** directory,
                         const struct cly_path * path);

extern int
cly_dir_search(const struct cly_dir ** directory, const char * path);

extern struct cly_cmd *
cly_dir_find_cmd(const struct cly_dir * directory, const char * name);

extern int
cly_dir_parse_cmd(const struct cly_dir * directory,
                  struct cly_context *   context,
                  int                    argc,
                  const char * const     argv[]);

struct cly_match;

extern void
cly_dir_complete_cmd(const struct cly_dir * directory,
                     struct cly_context *   context,
                     const char *           word,
                     size_t                 length,
                     int                    argc,
                     const char * const     argv[],
                     struct cly_match *     matches);

extern void
cly_dir_add_child(struct cly_dir * directory, struct cly_dir * child);

static inline void
cly_dir_add_cmd(struct cly_dir * directory, struct cly_cmd * command)
{
	cly_dir_assert(directory);
	cly_cmd_assert(command);
	cly_assert(!cly_dir_find_cmd(directory, command->name));

	cly_node_add_sibling((struct cly_node **)&directory->cmds,
	                     &command->super);
}

extern int
cly_dir_create_cmdn_add(struct cly_dir *           directory,
                        struct cly_cmd **          command,
                        const char *               name,
                        const struct cly_cmd_ops * opers);

extern void
cly_dir_init_root(struct cly_dir * root);

extern void
cly_dir_fini_root(struct cly_dir * root);

extern struct cly_dir *
cly_dir_create(const char * name, enum cly_dir_type type, const void * schema);

extern void
cly_dir_destroy(struct cly_dir * directory);

extern int
cly_dir_make_child(struct cly_dir ** dir,
                   const char *      name,
                   enum cly_dir_type type,
                   const void *      schema);

struct cly_dir *
cly_dir_make_from_node(const struct lysc_node *  node,
                       struct cly_dir *          root,
                       enum cly_dir_type         type,
                       const void *              schema);

/******************************************************************************
 * Directory search logic for commands usage.
 ******************************************************************************/

#include "work.h"

struct cly_dir_work {
	/* Base work structure. */
	struct cly_work        super;
	/* The command that initiated this work. */
	const struct cly_cmd * cmd;
	/* Internal state of requested path search. */
	struct cly_path        path;
	/* Pointer to original path argument. */
	const char *           orig;
	/* Length of `norm' field, excluding the terminating NULL byte. */
	size_t                 len;
	/* Validated normalized requested path. */
	char                   norm[CLY_PATH_MAX];
};

extern int
cly_dir_work_search(const struct cly_dir_work * work,
                    struct cly_context *        context,
                    const struct cly_dir **     directory);

extern int
cly_dir_work_parse(struct cly_dir_work *   work,
                   const char *            path,
                   bool                    mandatory);

extern struct cly_dir_work *
cly_dir_work_create(size_t                      size,
                    const struct cly_cmd *      command,
                    const struct cly_work_ops * opers);

static inline void
cly_dir_work_destroy(struct cly_dir_work * work)
{
	cly_destroy_work(&work->super);
}

extern void
cly_dir_work_release(struct cly_work * work);

struct cly_dir_work_arg;

extern struct cly_dir_work_arg *
cly_dir_work_create_arg(bool mandatory);

extern struct cly_dir_work_arg *
cly_dir_work_createn_add_arg(bool mandatory, struct cly_node * cmd_or_arg);

#endif /* _CLY_DIR_H */
