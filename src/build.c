#include "build.h"
#include "cly.h"
#include "show.h"
#include <srutils/srepo/schema.h>

/******************************************************************************
 * Command line hierarchy builder utilities.
 ******************************************************************************/

static bool
cly_build_node_uptodate(const struct lysc_node * node)
{
	if (!(srepo_sch_status_flags(node) &
	      (LYS_STATUS_OBSLT | LYS_STATUS_DEPRC)))
		return true;

	/* Ignore obsolete and deprecated nodes. */
	cly_debug_scn(node, "obsolete / deprecated node ignored.");

	return false;
}

/*
 * Create a directory entry related to the node given in argument underneath the
 * one pointed to by `*directory'.
 */
static int
cly_build_child_dir(struct cly_dir **        directory,
                    const struct lysc_node * node,
                    struct cly_context *     context)
{
	cly_assert(directory);
	cly_assert(*directory);
	cly_assert(node);
	cly_assert_context(context);

	struct cly_dir * dir = *directory;
	int              ret;

	ret = cly_dir_make_child(&dir, node->name, CLY_DIR_NODE_TYPE, node);
	if (!ret) {
		*directory = dir;
		return 0;
	}

	cly_log_scn(node,
	            "cannot create menu directory entry: %s.",
	            cly_dir_strerror(-ret));

	return ret;
}

/******************************************************************************
 * Command line hierarchy build tracker.
 ******************************************************************************/

struct cly_build_stat;
struct cly_build;

typedef int cly_build_process_fn(struct cly_build_stat *,
                                 struct cly_context *,
                                 const struct lysc_node *,
                                 enum srepo_walk_event,
                                 struct cly_build *);

struct cly_build_stat {
	cly_build_process_fn * process;
	struct cly_dir *       dir;
};

struct cly_build {
	unsigned int            cnt;
	unsigned int            nr;
	struct cly_build_stat * stats;
};

#define CLY_BUILD_INIT_NR (4U)

#define cly_build_assert(_build) \
	cly_assert(_build); \
	cly_assert((_build)->nr >= CLY_BUILD_INIT_NR); \
	cly_assert((_build)->cnt <= _build->nr); \
	cly_assert((_build)->stats)

static unsigned int
cly_build_count(const struct cly_build * builder)
{
	cly_build_assert(builder);

	return builder->cnt;
}

static void
cly_build_push(struct cly_build *     builder,
               cly_build_process_fn * process,
               struct cly_dir *       directory)
{
	cly_build_assert(builder);
	cly_assert(process);

	if (builder->cnt == builder->nr) {
		builder->nr *= 2;
		builder->stats = cly_realloc(builder->stats,
		                             builder->nr *
		                             sizeof(builder->stats[0]));
		cly_assert(builder->stats);
	}

	builder->stats[builder->cnt].process = process;
	builder->stats[builder->cnt++].dir = directory;
}

static struct cly_build_stat *
cly_build_pop(struct cly_build * builder)
{
	cly_build_assert(builder);
	cly_assert(builder->cnt);

	return &builder->stats[--builder->cnt];
}

static int
cly_build_process(struct cly_build *       builder,
                  const struct lysc_node * node,
                  enum srepo_walk_event    event,
                  struct cly_context *     context)
{
	cly_build_assert(builder);
	cly_assert(builder->cnt);
	cly_assert(node);
	cly_assert((event == SREPO_PRE_WALK_EVT) ||
	           (event == SREPO_POST_WALK_EVT));
	cly_assert_context(context);

	struct cly_build_stat * stat = &builder->stats[builder->cnt - 1];

	cly_assert(stat->process);

	return stat->process(stat, context, node, event, builder);
}

static void
cly_build_clear(struct cly_build * builder)
{
	cly_build_assert(builder);

	builder->cnt = 0;
}

static void
cly_build_init(struct cly_build * builder)
{
	cly_assert(builder);

	builder->cnt = 0;
	builder->nr = CLY_BUILD_INIT_NR;
	builder->stats = cly_malloc(CLY_BUILD_INIT_NR *
	                            sizeof(builder->stats[0]));
}

static void
cly_build_fini(struct cly_build * builder)
{
	cly_build_assert(builder);

	cly_free(builder->stats);
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

#if 0
static int
cly_build_ext_cmd(const struct cly_context *  context,
                  struct cly_dir *            directory,
                  const struct lysc_node *    node,
                  const struct lysc_cly_cmd * command)
{
	cly_assert_context(context);
	cly_dir_assert(directory);
	cly_assert(node);
	cly_assert(command);
	cly_assert(command->name);

	int ret;

	switch (command->kind) {
	case LYSC_CLY_SHOW_CONFIG_CMD_KIND:
		ret = cly_show_make_config_cmd(
			directory,
			(const struct lysc_node_container *)node,
			command->name,
			context);
		break;

	case LYSC_CLY_SHOW_STATE_CMD_KIND:
		ret = cly_show_make_oper_cmd(
			directory,
			(const struct lysc_node_container *)node,
			command->name,
			context);
		break;

	case LYSC_CLY_SET_CONFIG_CMD_KIND:
		/* Implement me ! */
		assert(0);

	default:
		assert(0);
	}

	return ret;
}

static int
cly_build_handle_container(struct cly_context *     context,
                           const struct lysc_node * node)
{
	struct lysc_cly          cly;
	ly_bool                  has_cly;
	const struct lysc_node * menu = NULL;
	struct cly_dir *         dir;
	int                      ret;

	/* Probe for YANG cly extension set. */
	ret = lysc_cly_load(node, &cly);
	if (ret != LY_SUCCESS) {
		cly_log_scn(node,
		            "cannot process menu entry: %s.",
		            ly_strerr(ret));
		return -EBADR;
	}

	has_cly = lysc_cly_exists(&cly);
	if (has_cly) {
		/* A set of YANG cly extensions is present... */

		if (lysc_cly_is_ignored(&cly))
			/* ...but it has been requested to be ignored. */
			goto unload;

		/*
		 * Retrieve menu node: subsequent commands will be attached
		 * to it.
		 */
		menu = lysc_cly_get_menu(&cly);
		cly_assert(menu);
	}

	if (!menu) {
		/*
		 * Create a menu directory hierarchy described by the node's
		 * YANG path rooted under its module related directory menu
		 * entry.
		 */
		struct cly_dir * root = &context->root;

		/*
		 * Create the top-level directory related to the module owning
		 * the node.
		 */
		ret = cly_dir_make_child(&root,
		                         node->module->name,
		                         CLY_DIR_MOD_TYPE,
		                         node->module);
		if (ret)
			goto nodir;

		/*
		 * Then, create the menu directory entry hierarchy related to
		 * the node underneath.
		 */
		dir = cly_dir_make_from_node(node,
		                             root,
		                             CLY_DIR_NODE_TYPE,
		                             node);
	}
	else {
		/*
		 * Create the menu directory hierarchy as requested by the cly
		 * extensions under our root directory entry.
		 */
		dir = cly_dir_make_from_node(menu,
		                             &context->root,
		                             CLY_DIR_MOD_TYPE,
		                             node->module);
	}
	if (!dir) {
		ret = -errno;
		goto nodir;
	}

	/*
	 * Eventually, create and attach commands to the menu directory entry
	 * just created.
	 */
	if (has_cly) {
		/*
		 * Instantiate commands defined by the YANG cly extensions
		 * found.
		 */
		const struct lysc_cly_cmd * cmd;

		lysc_cly_foreach_command(&cly, cmd) {
			ret = cly_build_ext_cmd(context, dir, node, cmd);
			if (ret)
				goto unload;
		}
	}
	else {
		ret = cly_show_make_config_cmd(
			dir,
			(const struct lysc_node_container *)node,
			"config",
			context);
		if (ret)
			goto unload;

		ret = cly_show_make_oper_cmd(
			dir,
			(const struct lysc_node_container *)node,
			"status",
			context);
		if (ret)
			goto unload;
	}

	lysc_cly_unload(&cly);

	return 0;

nodir:
	cly_log_scn(node,
	            "cannot create menu directory entry: %s.",
	            cly_dir_strerror(-ret));
unload:
	lysc_cly_unload(&cly);

	return ret;
}

static int
cly_build_handle_list(struct cly_context *     context,
                      const struct lysc_node * node,
                      struct cly_build_stat *  builder)
{
	struct cly_dir * dir = builder->dir;

	if (!dir) {
		/*
		 * Create the top-level directory related to the module owning
		 * the node.
		 */

		int ret;

		dir = &context->root;
		ret = cly_dir_make_child(&dir,
		                         node->module->name,
		                         CLY_DIR_MOD_TYPE,
		                         node->module);
		if (ret) {
			cly_log_scn(node,
			            "cannot create menu directory entry: %s.",
			            cly_dir_strerror(-ret));
			return srepo_sys_error(ret);
		}

		builder->dir = dir;
	}

	ret = cly_show_make_config_list_cmd(dir,
	                                    (const struct lysc_node_list *)node,
	                                    "config",
	                                    context);
	if (ret)
		return srepo_sys_error(ret);

	ret = cly_show_make_oper_list_cmd(dir,
	                                  (const struct lysc_node_list *)node,
	                                  "status",
	                                  context);
	return srepo_sys_error(ret);
}

static int
cly_build_tree_dir(struct cly_context *     context,
                   const struct lysc_node * node,
                   enum cly_walk_event      event,
                   struct cly_build_stat *  builder)
{
	cly_assert_context(context);
	cly_assert(node);
	cly_assert((event == CLY_PRE_WALK_EVT) || (event == CLY_POST_WALK_EVT));

	int ret;

	if (srepo_sch_status_flags(node) &
	    (LYS_STATUS_OBSLT | LYS_STATUS_DEPRC)) {
		/* Ignore obsolete and deprecated nodes. */
		cly_debug_scn(node, "obsolete / deprecated node ignored.");
		return CLY_SKIP_WALK_RET;
	}

	switch (srepo_sch_node_type(node)) {
	case LYS_CONTAINER:
		if (event == CLY_PRE_WALK_EVT) {
			ret = cly_build_handle_container(context, node);
			if (ret)
				return ret;
		}

		break;

	case LYS_LIST:
		if (event == CLY_PRE_WALK_EVT) {
			ret = cly_build_handle_list(context, node, builder);
			if (ret)
				return ret;
		}

		break;

	case LYS_LEAF:
		return CLY_SKIP_WALK_RET;

	case LYS_CHOICE:
	case LYS_LEAFLIST:
	case LYS_ANYXML:
	case LYS_ANYDATA:
	case LYS_CASE:
	case LYS_RPC:
	case LYS_ACTION:
	case LYS_NOTIF:
	case LYS_INPUT:
	case LYS_OUTPUT:

	case LYS_USES:
	case LYS_GROUPING:
	case LYS_AUGMENT:
	case LYS_UNKNOWN:
	default:
		if (event == CLY_PRE_WALK_EVT)
			cly_debug_scn(
				node,
				"'%s' node type support not implemented !",
				cly_sch_node_type_str(node));
		return CLY_SKIP_WALK_RET;
	}

	return CLY_CONT_WALK_RET;
}

#endif

static int
cly_build_process_cont(struct cly_build_stat *  state,
                       struct cly_context *     context,
                       const struct lysc_node * node,
                       enum srepo_walk_event    event,
                       struct cly_build *       builder);

static int
cly_build_process_list(struct cly_build_stat *  state,
                       struct cly_context *     context,
                       const struct lysc_node * node,
                       enum srepo_walk_event    event,
                       struct cly_build *       builder);

/******************************************************************************
 * Container command line builder state handling.
 ******************************************************************************/

static int
cly_build_handle_cont(struct cly_build_stat *            state,
                      struct cly_context *               context,
                      const struct lysc_node_container * container,
                      enum srepo_walk_event              event,
                      struct cly_build *                 builder)
{
	cly_assert(state);
	cly_assert(state->dir);
	cly_assert_context(context);
	cly_assert(container);
	cly_assert(srepo_sch_node_type((const struct lysc_node *)container) ==
	           LYS_CONTAINER);
	cly_assert(event == SREPO_PRE_WALK_EVT);
	cly_build_assert(builder);

	if (cly_build_node_uptodate((const struct lysc_node *)container)) {
		struct cly_dir * dir = state->dir;
		int              ret;

		/*
		 * Create the directory entry related to the container
		 * given in argument.
		 */
		ret = cly_build_child_dir(&dir,
		                          (const struct lysc_node *)
		                          container,
		                          context);
		if (ret)
			return srepo_sys_error(ret);

		/*
		 * Create and attach a show configuration command to the
		 * directory just created.
		 */
		ret = cly_show_make_config_cmd(dir,
		                               container,
		                               "config",
		                               context);
		if (ret)
			return srepo_sys_error(ret);

		/*
		 * Create and attach a show operational state command to
		 * the directory just created.
		 */
		ret = cly_show_make_oper_cmd(dir,
		                             container,
		                             "status",
		                             context);
		if (ret)
			return srepo_sys_error(ret);

		cly_build_push(builder,
		               cly_build_process_cont,
		               dir);

		return SREPO_CONT_WALK_RET;
	}
	else
		return SREPO_SKIP_WALK_RET;
}

static int
cly_build_handle_list(struct cly_build_stat *       state,
                      struct cly_context *          context,
                      const struct lysc_node_list * list,
                      enum srepo_walk_event         event,
                      struct cly_build *            builder)
{
	cly_assert(state);
	cly_assert(state->dir);
	cly_assert_context(context);
	cly_assert(list);
	cly_assert(srepo_sch_node_type((const struct lysc_node *)list) ==
	           LYS_LIST);
	cly_assert(event == SREPO_PRE_WALK_EVT);
	cly_build_assert(builder);

	if (cly_build_node_uptodate((const struct lysc_node *)list)) {
		int ret;

		/*
		 * Create and attach a show configuration command to its
		 * parent directory.
		 */
		ret = cly_show_make_config_list_cmd(state->dir,
		                                    list,
		                                    "config",
		                                    context);
		if (ret)
			return srepo_sys_error(ret);

		/*
		 * Create and attach a show operational state command to
		 * its parent directory.
		 */
		ret = cly_show_make_oper_list_cmd(state->dir,
		                                  list,
		                                  "status",
		                                  context);
		if (ret)
			return srepo_sys_error(ret);

		cly_build_push(builder,
		               cly_build_process_list,
		               state->dir);

		return SREPO_CONT_WALK_RET;
	}
	else
		return SREPO_SKIP_WALK_RET;
}

static const char *
cly_sch_node_type_str(const struct lysc_node * node)
{
	cly_assert(node);

	return srepo_sch_node_type_str(srepo_sch_node_type(node));
}

static int
cly_build_process_cont(struct cly_build_stat *  state,
                       struct cly_context *     context,
                       const struct lysc_node * node,
                       enum srepo_walk_event    event,
                       struct cly_build *       builder)
{
	cly_assert(state);
	cly_assert_context(context);
	cly_assert(node);
	cly_assert(event == SREPO_PRE_WALK_EVT);
	cly_build_assert(builder);

	switch (srepo_sch_node_type(node)) {
	case LYS_CONTAINER:
		return cly_build_handle_cont(
			state,
			context,
			(const struct lysc_node_container *)node,
			event,
			builder);

	case LYS_LIST:
		return cly_build_handle_list(
			state,
			context,
			(const struct lysc_node_list *)node,
			event,
			builder);

	case LYS_LEAF:
		break;

	case LYS_LEAFLIST:
	case LYS_CHOICE:
	case LYS_ANYXML:
	case LYS_ANYDATA:
	case LYS_ACTION:
	case LYS_NOTIF:
		cly_debug_scn(node,
		              "'%s' node type not implemented !",
		              cly_sch_node_type_str(node));
		break;

	case LYS_RPC:
	default:
		/*
		 * Statement not allowed by RFC7950's "YANG ABNF Grammar"
		 * section 14.
		 * Basically, this should never happen since we are traversing
		 * a YANG compiled schema.
		 */
		cly_debug_scn(node,
		              "invalid '%s' node type.",
		              cly_sch_node_type_str(node));
	}

	return SREPO_SKIP_WALK_RET;
}

/******************************************************************************
 * List command line builder state handling.
 ******************************************************************************/

#if 0
#warning REMOVE ME and adapt cly_show_make_.*_list_cmd() so that it may cope with list sub containers...
static int
cly_build_handle_list_cont(struct cly_build_stat *            state,
                           struct cly_context *               context,
                           const struct lysc_node_container * container,
                           enum cly_walk_event                event,
                           struct cly_build *                 builder)
{
	cly_assert(state);
	cly_assert(state->dir);
	cly_assert_context(context);
	cly_assert(container);
	cly_assert(srepo_sch_node_type((const struct lysc_node *)container) ==
	           LYS_CONTAINER);
	cly_assert(event == CLY_PRE_WALK_EVT);
	cly_build_assert(builder);

	if (cly_build_node_uptodate((const struct lysc_node *)container)) {
		struct cly_dir * dir = state->dir;
		int              ret;

		/*
		 * Create the directory entry related to the list given
		 * in argument.
		 */
		ret = cly_build_child_dir(&dir,
		                          (const struct lysc_node *)container,
		                          context);
		if (ret)
			return ret;

		/*
		 * Create and attach a show configuration command to the
		 * directory just created.
		 */
		ret = cly_show_make_config_list_cmd(dir,
		                                    container,
		                                    "config",
		                                    context);
		if (ret)
			return ret;

		/*
		 * Create and attach a show operational state command to
		 * the directory just created.
		 */
		ret = cly_show_make_oper_list_cmd(dir,
		                                  container,
		                                  "status",
		                                  context);
		if (ret)
			return ret;

		cly_build_push(builder, cly_build_process_list, dir);

		return CLY_CONT_WALK_RET;
	}
	else
		return CLY_SKIP_WALK_RET;
}
#endif

static int
cly_build_process_list(struct cly_build_stat *  state,
                       struct cly_context *     context,
                       const struct lysc_node * node,
                       enum srepo_walk_event    event,
                       struct cly_build *       builder)
{
	cly_assert(state);
	cly_assert_context(context);
	cly_assert(node);
	cly_assert(event == SREPO_PRE_WALK_EVT);
	cly_build_assert(builder);

	switch (srepo_sch_node_type(node)) {
	case LYS_CONTAINER:
#warning REMOVE ME and adapt cly_show_make_.*_list_cmd() so that it may cope with list sub containers...
#if 1
		cly_debug_scn(node,
		              "'%s' node type not implemented !",
		              cly_sch_node_type_str(node));
		break;
#else
		return cly_build_handle_list_cont(
			state,
			context,
			(const struct lysc_node_container *)node,
			event,
			builder);
#endif

	case LYS_LEAF:
		break;

	case LYS_LIST:
	case LYS_LEAFLIST:
	case LYS_CHOICE:
	case LYS_ANYXML:
	case LYS_ANYDATA:
	case LYS_ACTION:
	case LYS_NOTIF:
		cly_debug_scn(node,
		              "'%s' node type not implemented !",
		              cly_sch_node_type_str(node));
		break;

	case LYS_RPC:
	default:
		/*
		 * Statement not allowed by RFC7950's "YANG ABNF Grammar"
		 * section 14.
		 * Basically, this should never happen since we are traversing
		 * a YANG compiled schema.
		 */
		cly_debug_scn(node,
		              "invalid '%s' node type.",
		              cly_sch_node_type_str(node));
	}

	return SREPO_SKIP_WALK_RET;
}

/******************************************************************************
 * Top-level module command line builder state handling.
 ******************************************************************************/

static int
cly_build_top_dir(struct cly_dir **        directory,
                  const struct lysc_node * node,
                  struct cly_context *     context)
{
	cly_assert(directory);
	cly_assert(node);
	cly_assert_context(context);

	struct cly_dir * dir = &context->root;
	int              ret;

	/*
	 * Create the top-level directory related to the module owning
	 * the node.
	 */
	ret = cly_dir_make_child(&dir,
	                         node->module->name,
	                         CLY_DIR_MOD_TYPE,
	                         node->module);
	if (ret) {
		cly_log_scn(
			node,
			"cannot create top-level menu directory entry: %s.",
			cly_dir_strerror(-ret));
		return ret;
	}

	/*
	 * Then, create the directory entry related to the container given in
	 * argument underneath.
	 */
	ret = cly_build_child_dir(&dir, node, context);
	if (ret)
		return ret;

	*directory = dir;

	return 0;
}

static int
cly_build_handle_topcont(struct cly_build_stat *            state __unused,
                         struct cly_context *               context,
                         const struct lysc_node_container * container,
                         enum srepo_walk_event              event,
                         struct cly_build *                 builder)
{
	cly_assert(state);
	cly_assert(!state->dir);
	cly_assert_context(context);
	cly_assert(container);
	cly_assert(srepo_sch_node_type((const struct lysc_node *)container) ==
	           LYS_CONTAINER);
	cly_assert(event == SREPO_PRE_WALK_EVT);
	cly_build_assert(builder);

	if (cly_build_node_uptodate((const struct lysc_node *)container)) {
		struct cly_dir * dir;
		int              ret;

		/*
		 * Create the directory entry related to the container
		 * given in argument.
		 */
		ret = cly_build_top_dir(&dir,
		                        (const struct lysc_node *)
		                        container,
		                        context);
		if (ret)
			return srepo_sys_error(ret);

		/*
		 * Create and attach a show configuration command to the
		 * directory just created.
		 */
		ret = cly_show_make_config_cmd(dir,
		                               container,
		                               "config",
		                               context);
		if (ret)
			return srepo_sys_error(ret);

		/*
		 * Create and attach a show operational state command to
		 * the directory just created.
		 */
		ret = cly_show_make_oper_cmd(dir,
		                             container,
		                             "status",
		                             context);
		if (ret)
			return srepo_sys_error(ret);

		cly_build_push(builder, cly_build_process_cont, dir);

		return SREPO_CONT_WALK_RET;
	}
	else
		return SREPO_SKIP_WALK_RET;
}

static int
cly_build_handle_toplist(struct cly_build_stat *       state __unused,
                         struct cly_context *          context,
                         const struct lysc_node_list * list,
                         enum srepo_walk_event         event,
                         struct cly_build *            builder)
{
	cly_assert(state);
	cly_assert(!state->dir);
	cly_assert_context(context);
	cly_assert(list);
	cly_assert(srepo_sch_node_type((const struct lysc_node *)list) ==
	           LYS_LIST);
	cly_assert(event == SREPO_PRE_WALK_EVT);
	cly_build_assert(builder);

	if (cly_build_node_uptodate((const struct lysc_node *)list)) {
		struct cly_dir * dir;
		int              ret;

		/*
		 * Create the directory entry related to the list given
		 * in argument.
		 */
		ret = cly_build_top_dir(&dir,
		                        (const struct lysc_node *)list,
		                        context);
		if (ret)
			return srepo_sys_error(ret);

		/*
		 * Create and attach a show configuration command to the
		 * directory just created.
		 */
		ret = cly_show_make_config_list_cmd(dir,
		                                    list,
		                                    "config",
		                                    context);
		if (ret)
			return srepo_sys_error(ret);

		/*
		 * Create and attach a show operational state command to
		 * the directory just created.
		 */
		ret = cly_show_make_oper_list_cmd(dir,
		                                  list,
		                                  "status",
		                                  context);
		if (ret)
			return srepo_sys_error(ret);

		cly_build_push(builder, cly_build_process_list, dir);

		return SREPO_CONT_WALK_RET;
	}
	else
		return SREPO_SKIP_WALK_RET;
}

static int
cly_build_process_module(struct cly_build_stat *  state,
                         struct cly_context *     context,
                         const struct lysc_node * node,
                         enum srepo_walk_event    event,
                         struct cly_build *       builder)
{
	cly_assert(state);
	cly_assert_context(context);
	cly_assert(node);
	cly_assert(event == SREPO_PRE_WALK_EVT);
	cly_build_assert(builder);

	switch (srepo_sch_node_type(node)) {
	case LYS_CONTAINER:
		return cly_build_handle_topcont(
			state,
			context,
			(const struct lysc_node_container *)node,
			event,
			builder);

	case LYS_LIST:
		return cly_build_handle_toplist(
			state,
			context,
			(const struct lysc_node_list *)node,
			event,
			builder);

	case LYS_RPC:
	case LYS_NOTIF:
		cly_debug_scn(node,
		              "'%s' node type not implemented !",
		              cly_sch_node_type_str(node));
		break;

	case LYS_LEAF:
	case LYS_LEAFLIST:
	case LYS_CHOICE:
	case LYS_ANYXML:
	case LYS_ANYDATA:
	default:
		/*
		 * We arbitrarily choose to ignore these top-level statements
		 * despite being legitimate (see section 14 "YANG ABNF Grammar"
		 * of RFC7950).
		 * We don't have to support them as long as none of the YANG
		 * modules we parse use them as top-level statements.
		 */
		cly_debug_scn(node,
		              "unexpected '%s' node type.",
		              cly_sch_node_type_str(node));
	}

	return SREPO_SKIP_WALK_RET;
}

/******************************************************************************
 * Command line builder logic boostrapping.
 ******************************************************************************/

static int
cly_build_tree(struct cly_context *     context,
               const struct lysc_node * node,
               enum srepo_walk_event    event,
               void *                   builder)
{
	cly_assert_context(context);
	cly_assert(node);
	cly_assert((event == SREPO_PRE_WALK_EVT) ||
	           (event == SREPO_POST_WALK_EVT));
	cly_assert(builder);

	if (event == SREPO_PRE_WALK_EVT)
		return cly_build_process((struct cly_build *)builder,
		                         node,
		                         event,
		                         context);

	/* else if (event == CLY_POST_WALK_EVT) */
	cly_build_pop(builder);

	return SREPO_CONT_WALK_RET;
}

int
cly_build_from_schema(struct cly_context * context)
{
	unsigned int              m;
	const struct lys_module * mod;
	struct cly_build          build;
	sr_error_t                ret = SR_ERR_OK;

	cly_build_init(&build);

	srepo_sch_foreach_module(context->lyctx, m, mod) {
		cly_build_push(&build, cly_build_process_module, NULL);

		if (mod->compiled->data) {
			ret = srepo_sch_walk_module(mod,
			                            cly_build_tree,
			                            &build);
			if (ret != SR_ERR_OK)
				break;
		}

		cly_assert(cly_build_count(&build) == 1);
		cly_build_clear(&build);
	}

	cly_build_fini(&build);

	return -EPERM;
}
