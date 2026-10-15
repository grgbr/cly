#include "dir.h"
#include "cly.h"
#include "match.h"
#include <srutils/srepo/schema.h>
#include <errno.h>

static int
cly_dir_walk_recurs(struct cly_dir *   directory,
                    cly_dir_visit_fn * visit,
                    void *             data)
{
#warning Remove recursion
	cly_dir_assert(directory);
	cly_assert(visit);

	int ret;

	ret = visit(directory, CLY_PRE_WALK_EVT, data);
	if (ret == CLY_CONT_WALK_RET) {
		struct cly_dir * child;

		cly_dir_foreach_child(directory, child) {
			ret = cly_dir_walk_recurs(child, visit, data);
			if (ret < 0)
				return ret;
		}

		ret = visit(directory, CLY_POST_WALK_EVT, data);
		cly_assert(ret <= 0);
	}

	return (ret >= 0) ? 0 : ret;
}

int
cly_dir_walk(struct cly_dir *   directory,
             cly_dir_visit_fn * visit,
             void *             data)
{
	cly_dir_assert(directory);
	cly_assert(visit);

	struct cly_dir * child;
	int              ret = 0;

	cly_dir_foreach_child(directory, child) {
		ret = cly_dir_walk_recurs(child, visit, data);
		if (ret < 0)
			return ret;
	}

	cly_assert(!ret);
	return 0;
}

static int
cly_dir_walk_recurs_safe(struct cly_dir *   directory,
                         cly_dir_visit_fn * visit,
                         void *             data)
{
#warning Remove recursion
	cly_dir_assert(directory);
	cly_assert(visit);

	int ret;

	ret = visit(directory, CLY_PRE_WALK_EVT, data);
	if (ret == CLY_CONT_WALK_RET) {
		struct cly_dir * child;
		struct cly_dir * tmp;

		cly_dir_foreach_child_safe(directory, child, tmp) {
			ret = cly_dir_walk_recurs_safe(child, visit, data);
			if (ret < 0)
				return ret;
		}

		ret = visit(directory, CLY_POST_WALK_EVT, data);
		cly_assert(ret <= 0);
	}

	return (ret >= 0) ? 0 : ret;
}

int
cly_dir_walk_safe(struct cly_dir *   directory,
                  cly_dir_visit_fn * visit,
                  void *             data)
{
	cly_dir_assert(directory);
	cly_assert(visit);

	struct cly_dir * child;
	struct cly_dir * tmp;
	int              ret = 0;

	cly_dir_foreach_child_safe(directory, child, tmp) {
		ret = cly_dir_walk_recurs_safe(child, visit, data);
		if (ret < 0)
			return ret;
	}

	cly_assert(!ret);
	return 0;
}

ssize_t
cly_dir_mkrel(const struct cly_dir * directory,
              const struct cly_dir * ancestor,
              char *                 path,
              size_t                 size)
{
	cly_dir_assert(directory);
	cly_dir_assert(ancestor);
	cly_assert(directory != ancestor);
	cly_assert(path);
	cly_assert(size);
	cly_assert(size <= CLY_PATH_MAX);

	struct cly_path pth;
	ssize_t         len;

	cly_path_init(&pth);

	do {
		cly_dir_assert(directory);

		cly_path_push_head(&pth,
		                   directory->name,
		                   strnlen(directory->name, CLY_PATH_NAME_MAX));
		directory = directory->parent;
	} while (directory != ancestor);

	len = cly_path_mkrel(&pth, path, size);

	cly_path_fini(&pth);

	return len;
}

ssize_t
cly_dir_mkabs(const struct cly_dir * directory, char * path, size_t size)
{
	cly_dir_assert(directory);
	cly_assert(path);
	cly_assert(size);
	cly_assert(size <= CLY_PATH_MAX);

	struct cly_path pth;
	ssize_t         len;

	cly_path_init(&pth);

	while (directory->parent) {
		cly_dir_assert(directory);

		cly_path_push_head(&pth,
		                   directory->name,
		                   strnlen(directory->name, CLY_PATH_NAME_MAX));
		directory = directory->parent;
	}

	len = cly_path_mkabs(&pth, path, size);
	cly_assert(len);

	cly_path_fini(&pth);

	return len;
}

char *
cly_dir_xpath(const struct cly_dir * directory)
{
	cly_dir_assert(directory);

	switch (directory->type) {
	case CLY_DIR_NODE_TYPE:
		return srepo_sch_node_path(directory->sch_node);
	case CLY_DIR_MOD_TYPE:
		return srepo_sch_module_path(directory->sch_mod);
	case CLY_DIR_NONE_TYPE:
		return cly_strdup("/");
	default:
		cly_assert(0);
	}
}

int
cly_dir_show_yang(const struct cly_dir *     directory,
                  const struct cly_context * context)
{
	cly_dir_assert(directory);
	cly_assert_context(context);

	int ret;

	switch (directory->type) {
	case CLY_DIR_NODE_TYPE:
		ret = srepo_sch_print_node_yang(context->lyout,
		                                directory->sch_node,
		                                0);
		if (ret != LY_SUCCESS)
			ret = -EBADR;
		break;

	case CLY_DIR_MOD_TYPE:
		ret = srepo_sch_print_module_yang(context->lyout,
		                                  directory->sch_mod,
		                                  0);
		if (ret != LY_SUCCESS)
			ret = -EBADR;
		break;

	case CLY_DIR_NONE_TYPE:
		ret = -EBADR;
		break;

	default:
		cly_assert(0);
	}

	return ret;
}

int
cly_dir_show_diag(const struct cly_dir *     directory,
                  const struct cly_context * context)
{
	cly_dir_assert(directory);
	cly_assert_context(context);

	int ret;

	switch (directory->type) {
	case CLY_DIR_NODE_TYPE:
		ret = srepo_sch_print_node_diag(context->lyout,
		                                directory->sch_node,
		                                cly_term_cols(context));
		if (ret != LY_SUCCESS)
			ret = -EBADR;
		break;

	case CLY_DIR_MOD_TYPE:
		ret = srepo_sch_print_module_diag(context->lyout,
		                                  directory->sch_mod,
		                                  cly_term_cols(context));
		if (ret != LY_SUCCESS)
			ret = -EBADR;
		break;

	case CLY_DIR_NONE_TYPE:
		ret = -EBADR;
		break;

	default:
		cly_assert(0);
	}

	return ret;
}

int
cly_dir_search_from_path(const struct cly_dir ** directory,
                         const struct cly_path * path)
{
	cly_assert(directory);
	cly_dir_assert(*directory);
	cly_assert(path);

	const struct cly_dir *       dir = *directory;
	unsigned int                 c;
	unsigned int                 cnt;
	const struct cly_path_comp * comp;

	cly_path_foreach_comp(path, c, cnt, comp) {
		if (cly_path_comp_kind(comp) !=
		    CLY_PATH_UPPER_COMP_KIND) {
			cly_assert(cly_path_comp_kind(comp) ==
			           CLY_PATH_REG_COMP_KIND);
			break;
		}

		if (dir->parent)
			dir = dir->parent;
	}

	cly_path_foreach_comp_from(path, c, cnt, comp) {
		/* Iterate over each path component... */
		const struct cly_dir * child;
		bool                   found = false;

		/*
		 * ...and search a child directory which name matches
		 * the current component.
		 */
		cly_dir_foreach_child(dir, child) {
			if (!cly_path_comp_ncmp(comp,
			                        child->name,
			                        strlen(child->name))) {
				dir = child;
				found = true;
				break;
			}
		}

		if (!found) {
			/*
			 * No matching child directory found: stop the
			 * search since the given path does not exist.
			 */
			return -ENOENT;
		}
	}

	*directory = dir;

	return 0;
}

int
cly_dir_search(const struct cly_dir ** directory, const char * path)
{
	cly_assert(directory);
	cly_dir_assert(*directory);
	cly_assert(path);

	struct cly_path        pth;
	const struct cly_dir * dir = *directory;
	int                    ret;

	cly_path_init(&pth);

	ret = cly_path_parse(&pth, path);
	if (ret)
		goto fini;

	if (path[0] == '/') {
		while (dir->parent)
			dir = dir->parent;
	}

	ret = cly_dir_search_from_path(&dir, &pth);
	if (ret)
		goto fini;

	*directory = dir;

fini:
	cly_path_fini(&pth);

	return ret;
}

struct cly_cmd *
cly_dir_find_cmd(const struct cly_dir * directory, const char * name)
{
	cly_dir_assert(directory);
	cly_assert(cly_cmd_name_isok(name));

	struct cly_node * cmd;

	cly_node_foreach_sibling((struct cly_node *)directory->cmds, cmd) {
		if (!strcmp(name, ((const struct cly_cmd *)cmd)->name))
			return (struct cly_cmd *)cmd;
	}

	return NULL;
}

int
cly_dir_parse_cmd(const struct cly_dir * directory,
                  struct cly_context *   context,
                  int                    argc,
                  const char * const     argv[])
{
	cly_dir_assert(directory);
	cly_assert_args(argc, argv);

	const struct cly_node * cmd;
	int                     ret = 0;

	cly_node_foreach_sibling((struct cly_node *)directory->cmds, cmd) {
		ret = cly_cmd_parse((const struct cly_cmd *)cmd,
		                    directory,
		                    context,
		                    argc,
		                    argv);
		if (ret)
			break;
	}

	return ret;
}

void
cly_dir_complete_cmd(const struct cly_dir * directory,
                     struct cly_context *   context,
                     const char *           word,
                     size_t                 length,
                     int                    argc,
                     const char * const     argv[],
                     struct cly_match *     matches)
{
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(word);
	cly_assert(length < CLY_ARG_MAX);
	cly_assert(strnlen(word, CLY_ARG_MAX) == length);
	cly_assert(argc >= 0);
	cly_assert(!argc || argv);
	cly_match_assert(matches);

	const struct cly_node * cmd;

	cly_node_foreach_sibling((struct cly_node *)directory->cmds, cmd)
		cly_cmd_complete((const struct cly_cmd *)cmd,
		                 directory,
		                 context,
		                 word,
		                 length,
		                 argc,
		                 argv,
		                 matches);
}

void
cly_dir_add_child(struct cly_dir * directory, struct cly_dir * child)
{
#warning Make sure no other child with the same name exist (assertion).
	cly_dir_assert(directory);
	cly_dir_assert(child);

	struct cly_dir * head = directory->child;

	if (head) {
		struct cly_dir * tail = head->prev;

		child->prev = tail;
		tail->next = child;
		head->prev = child;
	}
	else {
		child->prev = child;
		directory->child = child;
	}

	child->parent = directory;
}

int
cly_dir_create_cmdn_add(struct cly_dir *           directory,
                        struct cly_cmd **          command,
                        const char *               name,
                        const struct cly_cmd_ops * opers)
{
	cly_dir_assert(directory);
	cly_assert(command);
	cly_assert(name);
	cly_assert(name[0] != '\0');
	cly_assert(!cly_dir_find_cmd(directory, name));
	cly_cmd_assert_ops(opers);

	int ret;

	ret = cly_cmd_create(command, name, opers);
	if (ret)
		return ret;

	cly_dir_add_cmd(directory, *command);

	return 0;
}

static void
_cly_dir_init(struct cly_dir *  directory,
              const char *      name,
              size_t            length,
              enum cly_dir_type type,
              const void *      schema)
{
	cly_assert(directory);
	cly_assert(name);
	cly_assert(length);
	cly_assert(length < CLY_PATH_NAME_MAX);
	cly_assert((type == CLY_DIR_NONE_TYPE) ||
	           (type == CLY_DIR_MOD_TYPE) ||
	           (type == CLY_DIR_NODE_TYPE));

	memcpy(directory->name, name, length + 1);
	directory->next = NULL;
	directory->prev = directory;
	directory->child = NULL;
	directory->parent = NULL;
	directory->cmds = NULL;
	directory->type = type;
	directory->sch_void = schema;
}

static int
cly_dir_init(struct cly_dir *  directory,
             const char *      name,
             enum cly_dir_type type,
             const void *      schema)
{
	cly_assert(directory);
	cly_assert(name);
	cly_assert((type == CLY_DIR_NONE_TYPE) ||
	           (type == CLY_DIR_MOD_TYPE) ||
	           (type == CLY_DIR_NODE_TYPE));
	cly_assert((type == CLY_DIR_NONE_TYPE) ^ !!schema);

	size_t len;
	int    ret;

	len = strnlen(name, CLY_PATH_NAME_MAX);
	ret = cly_path_comp_isok(name, len);
	if (ret)
		return ret;

	_cly_dir_init(directory, name, len, type, schema);

	return 0;
}

static void
cly_dir_fini(struct cly_dir * directory)
{
	cly_dir_assert(directory);

	struct cly_node * cmd;
	struct cly_node * tmp;

	cly_node_foreach_sibling_safe((struct cly_node *)directory->cmds,
	                              cmd,
	                              tmp)
		cly_cmd_destroy((struct cly_cmd *)cmd);
}

void
cly_dir_init_root(struct cly_dir * root)
{
	cly_assert(root);

	_cly_dir_init(root, "/", sizeof("/") - 1, CLY_DIR_NONE_TYPE, NULL);
}

void
cly_dir_fini_root(struct cly_dir * root)
{
	cly_dir_assert(root);

	cly_dir_fini(root);
}

struct cly_dir *
cly_dir_create(const char * name, enum cly_dir_type type, const void * schema)
{
	cly_assert(name);

	struct cly_dir * dir;
	int              err;

	dir = cly_malloc(sizeof(*dir));
	err = cly_dir_init(dir, name, type, schema);
	if (!err)
		return dir;

	cly_free(dir);

	errno = -err;
	return NULL;
}

void
cly_dir_destroy(struct cly_dir * directory)
{
	cly_dir_fini(directory);
	cly_free(directory);
}

/* Keep this just in case we would need it in the future. */
#if 0
/*
 * Create a new directory identified by `path' relatively to `directory'.
 * The newly created directory is returned into `directory'.
 */
static int
cly_dir_make(struct cly_dir ** directory,
             const char *      path,
             enum cly_dir_type type,
             const void *      schema)
{
	cly_assert(directory);
	cly_dir_assert(*directory);
	cly_assert(path);
	cly_assert((type == CLY_DIR_NONE_TYPE) ||
	           (type == CLY_DIR_MOD_TYPE) ||
	           (type == CLY_DIR_NODE_TYPE));
	cly_assert((type == CLY_DIR_NONE_TYPE) ^ !!schema);

	if (*path != '\0') {
		struct cly_path              pth;
		const struct cly_path_comp * base;
		struct cly_dir *             dir = *directory;
		struct cly_dir *             child;
		int                          ret;

		cly_path_init(&pth);

		/* Parse the given path entirely to tokenize path components. */
		ret = cly_path_parse(&pth, path);
		if (ret)
			goto fini;

		/* Extract last path component. */
		base = cly_path_pop_tail(&pth);
		if (!base) {
			/* Root and current directories already exist. */
			ret = -EEXIST;
			goto fini;
		}

		if (path[0] == '/') {
			/* Get up to root directory if requested. */
			while (dir->parent)
				dir = dir->parent;
		}

		/* Now search for the parent directory of the given path. */
		ret = cly_dir_search_from_path((const struct cly_dir **)&dir,
		                               &pth);
		if (ret)
			/* Parent directory not found... */
			goto fini;

		switch (cly_path_comp_kind(base)) {
		case CLY_PATH_REG_COMP_KIND:
			break;

		case CLY_PATH_UPPER_COMP_KIND:
		case CLY_PATH_CURR_COMP_KIND:
			/* Well, current and upper directory already exist... */
			ret = -EEXIST;
			goto fini;

		default:
			cly_assert(0);
		}

		/*
		 * Make sure that the parent directory found has no child with
		 * the same base name as the requested one.
		 */
		cly_dir_foreach_child(dir, child) {
			if (!cly_path_comp_ncmp(base,
			                        child->name,
			                        strlen(child->name)))
				return -EEXIST;
		}

		/*
		 * Finally create the new directory and register it to its
		 * parent.
		 */
		child = cly_malloc(sizeof(*child));
		_cly_dir_init(child, base->str, base->len, type, schema);
		cly_dir_add_child(dir, child);

		*directory = child;
		ret = 0;

fini:
		cly_path_fini(&pth);

		return ret;
	}
	else
		return -ENODATA;
}
#endif

int
cly_dir_make_child(struct cly_dir ** dir,
                   const char *      name,
                   enum cly_dir_type type,
                   const void *      schema)
{
	cly_assert(dir);
	cly_dir_assert(*dir);
	cly_assert(name);

	struct cly_dir * node = *dir;
	struct cly_dir * child;

	cly_dir_foreach_child(node, child) {
		if (!strcmp(child->name, name)) {
			if ((child->type == type) &&
			    (child->sch_void == schema))
				goto out;
			else
				return -EEXIST;
		}
	}

	child = cly_dir_create(name, type, schema);
	if (!child)
		return -errno;

	cly_dir_add_child(node, child);

out:
	*dir = child;

	return 0;
}

struct cly_dir *
cly_dir_make_from_node(const struct lysc_node * node,
                       struct cly_dir *         root,
                       enum cly_dir_type        type,
                       const void *             schema)
{
	cly_assert(node);
	cly_assert((node->nodetype == LYS_CONTAINER) ||
	           (node->nodetype == LYS_LIST));
	cly_assert((type == CLY_DIR_NONE_TYPE) ||
	           (type == CLY_DIR_MOD_TYPE) ||
	           (type == CLY_DIR_NODE_TYPE));
	cly_assert((type == CLY_DIR_NONE_TYPE) ^ !!schema);
	cly_assert((node != schema) || (type == CLY_DIR_NODE_TYPE));

	const struct lysc_node ** comps;
	const struct lysc_node *  scn = node;
	unsigned int              nr = CLY_PATH_INIT_NR;
	unsigned int              cnt = 0;
	int                       err;

	comps = cly_malloc(nr * sizeof(*comps));
	cly_assert(comps);

	/*
	 * Get up to the top-level schema node, stacking parent nodes down along
	 * the path.
	 */
	while (scn->parent) {
		if (cnt == nr) {
			nr *= 2;
			comps = cly_realloc(comps, nr * sizeof(*comps));
			cly_assert(comps);
		}

		scn = scn->parent;
		comps[cnt++] = scn;
	}

#if 0
	/*
	 * Now create the top-level directory related to the module given in
	 * argument.
	 * Top-level node owner module should be the one that is given in
	 * argument.
	 */
	err = cly_dir_make_child(&root,
	                         scn->module->name,
	                         CLY_DIR_MOD_TYPE,
	                         scn->module);
	if (err)
		goto free;
#endif

	/*
	 * Iterate over stacked parent nodes in reverse order and create
	 * child directories.
	 */
	while (cnt--) {
		scn = comps[cnt];
		err = cly_dir_make_child(&root,
		                         scn->name,
		                         CLY_DIR_NODE_TYPE,
		                         scn);
		if (err)
			goto free;
	}

	/* Finally, create directory for `node' given in argument. */
	err = cly_dir_make_child(&root, node->name, type, schema);
	if (err)
		goto free;

	cly_free(comps);

	return root;

free:
	cly_free(comps);

	errno = -err;
	return NULL;
}

/******************************************************************************
 * Directory search logic for commands usage.
 ******************************************************************************/

int
cly_dir_work_search(const struct cly_dir_work * work,
                    struct cly_context *        context,
                    const struct cly_dir **     directory)
{
	cly_assert(work);
	cly_assert(work->cmd);
	cly_assert(strnlen(work->norm, CLY_PATH_MAX) == work->len);

	const struct cly_dir * dir = cly_cwd(context);

	if (cly_path_comp_count(&work->path)) {
		int ret;

		cly_assert(work->orig);
		if (work->orig[0] == '/')
			dir = &context->root;

		ret = cly_dir_search_from_path(&dir, &work->path);
		if (ret) {
			/*
			 * Searching for the current directory cannot fail.
			 * Hence, `work->orig' should always be a non-empty
			 * string.
			 */
			cly_assert(work->orig && (work->orig[0] != '\0'));
			cly_cmd_talk(work->cmd,
			             "'%s': %s.",
			             work->orig,
			             cly_dir_strerror(-ret));

			return ret;
		}
	}
	else if (work->norm[0] == '/') {
		/* Search for the root directory... */
		dir = &context->root;
	}
	/* Else: search for the current working directory. */

	*directory = dir;

	return 0;
}

int
cly_dir_work_parse(struct cly_dir_work * work,
                   const char *          path,
                   bool                  mandatory)
{
	cly_assert(work);
	cly_assert(work->cmd);
	cly_assert(path);

	if (!work->orig) {
		ssize_t ret = 0;

		if (*path != '\0') {
			ret = cly_path_parse(&work->path, path);
			if (ret)
				goto out;

			if (*path != '/')
				ret = cly_path_mkrel(&work->path,
				                     work->norm,
				                     sizeof(work->norm));
			else
				ret = cly_path_mkabs(&work->path,
				                     work->norm,
				                     sizeof(work->norm));

			cly_assert(ret >= 0);
			if (ret && (work->norm[ret - 1] != '/')) {
				if ((size_t)(ret + 1) >= sizeof(work->norm)) {
					ret = -ENAMETOOLONG;
					goto out;
				}

				work->norm[ret++] = '/';
			}
		}

		work->orig = path;
		work->len = (size_t)ret;
		work->norm[ret] = '\0';

		return 1;

out:
		if (!mandatory)
			return 0;

		cly_cmd_talk(work->cmd,
		             "'%s': invalid path argument: %s.",
		             path,
		             cly_path_strerror(-ret));
		return (int)ret;
	}
	else {
		/* Tell the caller that parsing has already been compeleted. */
		return -EALREADY;
	}
}

struct cly_dir_work *
cly_dir_work_create(size_t                      size,
                    const struct cly_cmd *      command,
                    const struct cly_work_ops * opers)
{
	cly_assert(size >= sizeof(struct cly_dir_work));
	cly_cmd_assert(command);
	cly_assert_work_ops(opers);

	struct cly_dir_work * wk;

	wk = (struct cly_dir_work *)cly_create_work(size, opers);
	cly_assert(wk);

	wk->cmd = command;
	cly_path_init(&wk->path);
	wk->orig = NULL;
	wk->len = 0;
	wk->norm[0] = '\0';

	return wk;
}

static void
cly_dir_work_fini(struct cly_dir_work * work)
{
	cly_assert(work);

	cly_path_fini(&work->path);
}

void
cly_dir_work_release(struct cly_work * work)
{
	cly_dir_work_fini((struct cly_dir_work *)work);
}

struct cly_dir_work_arg {
	struct cly_arg super;
	bool           mand;
};

static int
cly_dir_work_parse_arg(const struct cly_arg * argument,
                       const struct cly_cmd * command,
                       const struct cly_dir * directory __unused,
                       struct cly_context *   context __unused,
                       int                    argc,
                       const char * const     argv[],
                       void *                 data)
{
	cly_arg_assert(argument);
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argc);
	cly_assert(argv[0]);
	cly_assert(data);

	int ret;

	ret = cly_dir_work_parse(
		(struct cly_dir_work *)data,
		argv[0],
		((const struct cly_dir_work_arg *)argument)->mand);

	/* Ignore multiple path argument. */
	return (ret != -EALREADY) ? ret : 0;
}

static void
cly_dir_push_match(struct cly_match * matches, const char * dir_name)
{
	cly_match_assert(matches);
	cly_assert(!cly_path_isok(dir_name));
	cly_assert(dir_name[strlen(dir_name) - 1] == '/');

	char * path;

	path = cly_strdup(dir_name);
	cly_assert(path);

	cly_match_push(matches, path);
}

static void
cly_dir_join_push_match(struct cly_match * matches,
                        const char *       dir_name,
                        const char *       base_name)
{
	char * path;
	int    ret;

	ret = cly_asprintf(&path, "%s%s/", dir_name, base_name);
	cly_assert(ret >= 2);
	cly_assert(path);

	cly_match_push(matches, path);
}

static void
cly_dir_generate_matches(const struct cly_dir * directory,
                         const char *           normalized,
                         struct cly_match *     matches)
{
	cly_dir_assert(directory);
	cly_assert(!normalized || !cly_path_isok(normalized));
	cly_assert(!normalized || (normalized[strlen(normalized) - 1] == '/'));
	cly_match_assert(matches);

	const struct cly_dir * child;

	if (cly_dir_has_child(directory)) {
		/*
		 * Here, we give ownership of newly allocated directory name to
		 * `matches'.
		 * Allocated string ownership will be transfered from `matches'
		 * to readline(3) by cly_shell_complete().
		 * Readline(3) will free(3) it at completion process termination
		 * time.
		 */
		cly_dir_foreach_child(directory, child)
			cly_dir_join_push_match(matches,
			                        normalized ? normalized : "",
			                        child->name);

		cly_shell_suppress_complete_char();
	}
	else if (normalized)
		cly_dir_push_match(matches, normalized);
}

static void
cly_dir_generate_named_matches(const struct cly_dir * directory,
                               const char *           normalized,
                               const char *           base_name,
                               size_t                 base_length,
                               struct cly_match *     matches)
{
	cly_dir_assert(directory);
	cly_assert(!normalized || !cly_path_isok(normalized));
	cly_assert(!normalized || (normalized[strlen(normalized) - 1] == '/'));
	cly_assert(base_name);
	cly_assert(base_length);
	cly_assert(base_length < CLY_PATH_NAME_MAX);
	cly_assert(strnlen(base_name, CLY_PATH_NAME_MAX) == base_length);
	cly_match_assert(matches);

	enum cly_path_comp_kind kind;

	if (cly_path_parse_comp(&kind, base_name, base_length + 1) > 0) {
		if (kind == CLY_PATH_REG_COMP_KIND) {
			const struct cly_dir * child;

			/*
			 * Here, we give ownership of newly allocated directory
			 * name to `matches'.
			 * Allocated string ownership will be transfered from
			 * `matches' to readline(3) by cly_shell_complete().
			 * Readline(3) will free(3) it at completion process
			 * termination time.
			 */
			if (cly_dir_has_child(directory)) {
				cly_dir_foreach_child(directory, child) {
					if (!strncmp(base_name,
					             child->name,
					             base_length))
						cly_dir_join_push_match(
							matches,
							normalized ? normalized
							           : "",
							           child->name);
				}
				cly_shell_suppress_complete_char();
			}
			else if (normalized)
				cly_dir_push_match(matches, normalized);
		}
		else {
			cly_dir_join_push_match(matches,
			                        normalized ? normalized : "",
			                        "..");
			cly_shell_suppress_complete_char();
		}
	}
}

static ssize_t
cly_dir_searchn_normalize(const struct cly_dir **     directory,
                          const char *                path,
                          char **                     normalized,
                          const struct  cly_context * context)
{
	cly_assert(directory);
	cly_dir_assert(*directory);
	cly_assert(path);
	cly_assert(normalized);
	cly_assert_context(context);

	struct cly_path        pth;
	int                    ret;
	char *                 norm;
	ssize_t                len;
	const struct cly_dir * dir = *directory;

	cly_path_init(&pth);

	ret = cly_path_parse(&pth, path);
	if (ret)
		goto fini;

	norm = cly_malloc(CLY_PATH_MAX);
	cly_assert(norm);

	if (path[0] != '/')
		len = cly_path_mkrel(&pth, norm, CLY_PATH_MAX - 1);
	else
		len = cly_path_mkabs(&pth, norm, CLY_PATH_MAX - 1);

	cly_assert(len >= 0);
	cly_assert((len + 1) < CLY_PATH_MAX);
	if (len) {
		if (norm[len - 1] != '/') {
			norm[len++] = '/';
			norm[len] = '\0';
		}

		if (norm[0] == '/')
			dir = &context->root;

		ret = cly_dir_search_from_path(&dir, &pth);
		if (ret)
			/* Directory not found: no match. */
			goto free;
	}
	else {
		cly_free(norm);
		norm = NULL;
	}

	cly_path_fini(&pth);

	*directory = dir;
	*normalized = norm;

	return len;

free:
	cly_free(norm);
fini:
	cly_path_fini(&pth);

	return ret;
}

static bool
cly_dir_work_already_completed(int argc, const char * const argv[])
{
	int a;

	for (a = 0; a < argc; a++) {
		if (!cly_path_isok(argv[a]))
			return true;
	}

	return false;
}

static void
_cly_dir_work_complete_arg(struct cly_context * context,
                           const char *         word,
                           size_t               length,
                           struct cly_match *   matches)
{
	cly_assert_context(context);
	cly_match_assert(matches);

	const struct cly_dir * pdir = cly_cwd(context);

	if (length) {
		/* Base name of requested path. */
		const char * bname;
		/* Base name length. */
		size_t       blen;
		/* Directory name length of requested path. */
		ssize_t      dlen;
		/* Normalized directory name of requested path. */
		char *       norm = NULL;

		if (length >= CLY_PATH_MAX)
			return;

		/* Compute required base name. */
		bname = &word[length];
		while ((bname > word) && (bname[-1] != '/'))
		       bname--;
		blen = &word[length] - bname;
		cly_assert(bname[blen] == '\0');

		dlen = (ssize_t)(length - blen);
		if (dlen) {
			/*
			 * Compute requested directory name and search for its
			 * corresponding directory descriptor.
			 */
			char * dname;

			dname = cly_malloc(dlen + 1);
			cly_assert(dname);
			memcpy(dname, word, dlen);
			dname[dlen] = '\0';

			dlen = cly_dir_searchn_normalize(&pdir,
			                                 dname,
			                                 &norm,
			                                 context);

			cly_free(dname);

			if (dlen < 0)
				return;

			cly_assert((!dlen && !norm) || (dlen && norm));
		}

		if (blen)
			cly_dir_generate_named_matches(pdir,
			                               norm,
			                               bname,
			                               blen,
			                               matches);
		else
			cly_dir_generate_matches(pdir, norm, matches);

		/* free(3) handles NULL arguments properly... */
		cly_free(norm);
	}
	else
		cly_dir_generate_matches(pdir, NULL, matches);

	rl_filename_completion_desired = 1;
}

static void
cly_dir_work_complete_arg(const struct cly_arg * argument __unused,
                          const struct cly_cmd * command __unused,
                          const struct cly_dir * directory __unused,
                          struct cly_context *   context,
                          const char *           word,
                          size_t                 length,
                          int                    argc,
                          const char * const     argv[],
                          struct cly_match *     matches)
{
	cly_arg_assert(argument);
	cly_cmd_assert(command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_match_assert(matches);

	/*
	 * Generate possible completion pathes only when a path has not been
	 * completed within the previous command line words.
	 * In other words, a path completion may only be performed once for the
	 * current command line.
	 */
	if (!cly_dir_work_already_completed(argc, argv))
		_cly_dir_work_complete_arg(context, word, length, matches);
}

static const struct cly_arg_ops cly_dir_work_arg_ops = {
	.parse    = cly_dir_work_parse_arg,
	.complete = cly_dir_work_complete_arg
};

struct cly_dir_work_arg *
cly_dir_work_create_arg(bool mandatory)
{
	struct cly_dir_work_arg * arg;

	arg = (struct cly_dir_work_arg *)
	      cly_arg_create(sizeof(*arg), &cly_dir_work_arg_ops);
	cly_assert(arg);

	arg->mand = mandatory;

	return arg;
}

struct cly_dir_work_arg *
cly_dir_work_createn_add_arg(bool mandatory, struct cly_node * cmd_or_arg)
{
	struct cly_dir_work_arg * arg;

	arg = cly_dir_work_create_arg(mandatory);
	cly_assert(arg);

	cly_node_add_child(cmd_or_arg, (struct cly_node *)arg);

	return arg;
}
