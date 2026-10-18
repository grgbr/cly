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
	cly_scn_debug(node, "obsolete / deprecated node ignored");

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

	cly_scn_err(node,
	            "cannot create menu directory entry: %s",
	            cly_dir_strerror(-ret));

	return ret;
}

/******************************************************************************
 * Command line hierarchy build tracker.
 ******************************************************************************/

struct cly_build_stat;
struct cly_build;

typedef int cly_build_process_fn(struct cly_build_stat *,
                                 const struct lysc_node *,
                                 enum srepo_walk_event,
                                 struct cly_build *);

struct cly_build_stat {
	cly_build_process_fn * process;
	struct cly_dir *       dir;
};

struct cly_build {
	struct cly_context *    ctx;
	unsigned int            cnt;
	unsigned int            nr;
	struct cly_build_stat * stats;
};

#define CLY_BUILD_INIT_NR (4U)

#define cly_build_assert(_build) \
	cly_assert(_build); \
	cly_assert((_build)->ctx); \
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
                  enum srepo_walk_event    event)
{
	cly_build_assert(builder);
	cly_assert(builder->cnt);
	cly_assert(node);
	cly_assert((event == SREPO_PRE_WALK_EVT) ||
	           (event == SREPO_POST_WALK_EVT));

	struct cly_build_stat * stat = &builder->stats[builder->cnt - 1];

	cly_assert(stat->process);

	return stat->process(stat, node, event, builder);
}

static void
cly_build_clear(struct cly_build * builder)
{
	cly_build_assert(builder);

	builder->cnt = 0;
}

static void
cly_build_init(struct cly_build * builder, struct cly_context * context)
{
	cly_assert(builder);
	cly_assert_context(context);

	builder->ctx = context;
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

static int
cly_build_process_cont(struct cly_build_stat *  state,
                       const struct lysc_node * node,
                       enum srepo_walk_event    event,
                       struct cly_build *       builder);

static int
cly_build_process_list(struct cly_build_stat *  state,
                       const struct lysc_node * node,
                       enum srepo_walk_event    event,
                       struct cly_build *       builder);

/******************************************************************************
 * Container command line builder state handling.
 ******************************************************************************/

static int
cly_build_handle_cont(struct cly_build_stat *            state,
                      const struct lysc_node_container * container,
                      enum srepo_walk_event              event,
                      struct cly_build *                 builder)
{
	cly_assert(state);
	cly_assert(state->dir);
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
		                          (const struct lysc_node *)container,
		                          builder->ctx);
		if (ret)
			return srepo_sys_error(ret);

		/*
		 * Create and attach a show configuration command to the
		 * directory just created.
		 */
		ret = cly_show_make_config_cmd(dir,
		                               container,
		                               "config",
		                               builder->ctx);
		if (ret)
			return srepo_sys_error(ret);

		/*
		 * Create and attach a show operational state command to
		 * the directory just created.
		 */
		ret = cly_show_make_oper_cmd(dir,
		                             container,
		                             "status",
		                             builder->ctx);
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
cly_build_handle_list(struct cly_build_stat *  state,
                      const struct lysc_node * list,
                      enum srepo_walk_event    event,
                      struct cly_build *       builder)
{
	cly_assert(state);
	cly_assert(state->dir);
	cly_assert(list);
	cly_assert((srepo_sch_node_type(list) == LYS_LIST) ||
	           (srepo_sch_node_type(list) == LYS_CONTAINER));
	cly_assert(event == SREPO_PRE_WALK_EVT);
	cly_build_assert(builder);

	if (cly_build_node_uptodate(list)) {
		struct cly_dir * dir = state->dir;
		int              ret;

		/*
		 * Create the directory entry related to the container
		 * given in argument.
		 */
		ret = cly_build_child_dir(&dir, list, builder->ctx);
		if (ret)
			return srepo_sys_error(ret);

		/*
		 * Create and attach a show configuration command to its
		 * parent directory.
		 */
		ret = cly_show_make_config_list_cmd(dir,
		                                    list,
		                                    "config",
		                                    builder->ctx);
		if (ret)
			return srepo_sys_error(ret);

		/*
		 * Create and attach a show operational state command to
		 * its parent directory.
		 */
		ret = cly_show_make_oper_list_cmd(dir,
		                                  list,
		                                  "status",
		                                  builder->ctx);
		if (ret)
			return srepo_sys_error(ret);

		cly_build_push(builder, cly_build_process_list, dir);

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
                       const struct lysc_node * node,
                       enum srepo_walk_event    event,
                       struct cly_build *       builder)
{
	cly_assert(state);
	cly_assert(node);
	cly_assert(event == SREPO_PRE_WALK_EVT);
	cly_build_assert(builder);

	switch (srepo_sch_node_type(node)) {
	case LYS_CONTAINER:
		return cly_build_handle_cont(
			state,
			(const struct lysc_node_container *)node,
			event,
			builder);

	case LYS_LIST:
		return cly_build_handle_list(state, node, event, builder);

	case LYS_LEAF:
		break;

	case LYS_LEAFLIST:
	case LYS_CHOICE:
	case LYS_ANYXML:
	case LYS_ANYDATA:
	case LYS_ACTION:
	case LYS_NOTIF:
		cly_scn_debug(node,
		              "'%s' node type not implemented",
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
		cly_scn_debug(node,
		              "invalid '%s' node type",
		              cly_sch_node_type_str(node));
	}

	return SREPO_SKIP_WALK_RET;
}

/******************************************************************************
 * List command line builder state handling.
 ******************************************************************************/

static int
cly_build_process_list(struct cly_build_stat *  state,
                       const struct lysc_node * node,
                       enum srepo_walk_event    event,
                       struct cly_build *       builder)
{
	cly_assert(state);
	cly_assert(node);
	cly_assert(event == SREPO_PRE_WALK_EVT);
	cly_build_assert(builder);

	switch (srepo_sch_node_type(node)) {
	case LYS_CONTAINER:
	case LYS_LIST:
		return cly_build_handle_list(state, node, event, builder);

	case LYS_LEAF:
		break;

	case LYS_LEAFLIST:
	case LYS_CHOICE:
	case LYS_ANYXML:
	case LYS_ANYDATA:
	case LYS_ACTION:
	case LYS_NOTIF:
		cly_scn_debug(node,
		              "'%s' node type not implemented",
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
		cly_scn_debug(node,
		              "invalid '%s' node type",
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
		cly_scn_err(
			node,
			"cannot create top-level menu directory entry: %s",
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
                         const struct lysc_node_container * container,
                         enum srepo_walk_event              event,
                         struct cly_build *                 builder)
{
	cly_assert(state);
	cly_assert(!state->dir);
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
		                        builder->ctx);
		if (ret)
			return srepo_sys_error(ret);

		/*
		 * Create and attach a show configuration command to the
		 * directory just created.
		 */
		ret = cly_show_make_config_cmd(dir,
		                               container,
		                               "config",
		                               builder->ctx);
		if (ret)
			return srepo_sys_error(ret);

		/*
		 * Create and attach a show operational state command to
		 * the directory just created.
		 */
		ret = cly_show_make_oper_cmd(dir,
		                             container,
		                             "status",
		                             builder->ctx);
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
                         const struct lysc_node_list * list,
                         enum srepo_walk_event         event,
                         struct cly_build *            builder)
{
	cly_assert(state);
	cly_assert(!state->dir);
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
		                        builder->ctx);
		if (ret)
			return srepo_sys_error(ret);

		/*
		 * Create and attach a show configuration command to the
		 * directory just created.
		 */
		ret = cly_show_make_config_list_cmd(
			dir,
			(const struct lysc_node *)list,
			"config",
			builder->ctx);
		if (ret)
			return srepo_sys_error(ret);

		/*
		 * Create and attach a show operational state command to
		 * the directory just created.
		 */
		ret = cly_show_make_oper_list_cmd(
			dir,
			(const struct lysc_node *)list,
			"status",
			builder->ctx);
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
                         const struct lysc_node * node,
                         enum srepo_walk_event    event,
                         struct cly_build *       builder)
{
	cly_assert(state);
	cly_assert(node);
	cly_assert(event == SREPO_PRE_WALK_EVT);
	cly_build_assert(builder);

	switch (srepo_sch_node_type(node)) {
	case LYS_CONTAINER:
		return cly_build_handle_topcont(
			state,
			(const struct lysc_node_container *)node,
			event,
			builder);

	case LYS_LIST:
		return cly_build_handle_toplist(
			state,
			(const struct lysc_node_list *)node,
			event,
			builder);

	case LYS_RPC:
	case LYS_NOTIF:
		cly_scn_debug(node,
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
		cly_scn_debug(node,
		              "unexpected '%s' node type",
		              cly_sch_node_type_str(node));
	}

	return SREPO_SKIP_WALK_RET;
}

/******************************************************************************
 * Command line builder logic boostrapping.
 ******************************************************************************/

static int
cly_build_tree(const struct lysc_node * node,
               enum srepo_walk_event    event,
               void *                   builder)
{
	cly_assert(node);
	cly_assert((event == SREPO_PRE_WALK_EVT) ||
	           (event == SREPO_POST_WALK_EVT));
	cly_assert(builder);

	if (event == SREPO_PRE_WALK_EVT)
		return cly_build_process(builder, node, event);

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

	cly_build_init(&build, context);

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

	return (ret == SR_ERR_OK) ? 0 : -EPERM;
}
