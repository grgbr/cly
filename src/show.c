/******************************************************************************
 * Generic show command handling.
 *
 * Print configuration or operational state data related to the directory node
 * given in argument.
 ******************************************************************************/

#include "show.h"
#include "cly.h"
#include <srutils/srepo/data.h>
#include <srutils/srepo/schema.h>

enum cly_data_format {
	CLY_TABLE_DATA_FMT = 0,
	CLY_JSON_DATA_FMT,
	CLY_XML_DATA_FMT,
	CLY_DATA_FMT_NR
};

struct cly_show_work {
	struct cly_dir_work  super;
	enum cly_data_format format;
};

#define cly_show_assert_work(_work) \
	cly_assert(_work); \
	cly_assert((_work)->format >= 0); \
	cly_assert((_work)->format < CLY_DATA_FMT_NR)

struct cly_show_cmd {
	struct cly_cmd           super;
	struct cly_srepo_table * table;
	sr_get_oper_flag_t       flags;
};

#define cly_show_assert_cmd(_cmd) \
	cly_assert(_cmd); \
	cly_cmd_assert(&(_cmd)->super); \
	cly_srepo_table_assert((_cmd)->table); \
	srepo_dat_assert_get_flags((_cmd)->flags); \
	cly_assert(((_cmd)->flags & (SR_OPER_NO_STATE | SR_OPER_NO_CONFIG)) != \
	           (SR_OPER_NO_STATE | SR_OPER_NO_CONFIG))

#warning TODO: implement arbitrary output stream for pager support.
static int
cly_show_table(const struct cly_cmd *     command,
               struct cly_srepo_table *   table,
               sr_get_oper_flag_t         flags,
               const struct cly_context * context)
{
	cly_show_assert_cmd((const struct cly_show_cmd *)command);
	cly_srepo_table_assert(table);
	srepo_dat_assert_get_flags(flags);
	cly_assert((flags & (SR_OPER_NO_STATE | SR_OPER_NO_CONFIG)) !=
	           (SR_OPER_NO_STATE | SR_OPER_NO_CONFIG));
	cly_assert_context(context);

	int ret;

	ret = cly_table_load((struct cly_table *)table,
	                     context,
	                     (void *)flags);
	if (ret) {
		cly_cmd_log(command,
		            "cannot load table data: %s.",
		            sr_strerror(ret));
		return -ENOMSG;
	}

	ret = cly_table_show((const struct cly_table *)table, false, stdout);
	if (ret) {
		cly_cmd_log(command,
		            "cannot show table data: %s.",
		            strerror(-ret));
		return -ENOMSG;
	}

	return 0;
}

#warning TODO: implement arbitrary output stream for pager support.
static int
cly_show_format(const struct cly_cmd *     command,
                const struct lysc_node *   schema,
                sr_get_oper_flag_t         flags,
                LYD_FORMAT                 format,
                const struct cly_context * context)
{
	cly_show_assert_cmd((const struct cly_show_cmd *)command);
	cly_assert(schema);
	srepo_dat_assert_get_flags(flags);
	cly_assert((flags & (SR_OPER_NO_STATE | SR_OPER_NO_CONFIG)) !=
	           (SR_OPER_NO_STATE | SR_OPER_NO_CONFIG));
	cly_assert((format == LYD_JSON) || (format == LYD_XML));
	cly_assert_context(context);

	sr_data_t * data;
	sr_error_t  ret;

	ret = srepo_sch_load_data(context->sess,
	                          schema,
	                          2,
	                          flags,
	                          &data);
	if (ret != SR_ERR_OK) {
		cly_cmd_log(command, "cannot load data: %s.", sr_strerror(ret));
		return -ENOMSG;
	}

	ret = srepo_dat_print_data(data, format, context->lyout);
	if (ret != SR_ERR_OK)
		cly_cmd_log(command, "cannot show data: %s.", sr_strerror(ret));

	srepo_dat_release_data(data);

	return -EBADR;
}

static int
cly_show_exec_work(struct cly_work * work, struct cly_context * context)
{
	cly_show_assert_work((const struct cly_show_work *)work);
	cly_show_assert_cmd((const struct cly_show_cmd *)
	                    (((const struct cly_show_work *)work)->super.cmd));
	cly_assert_context(context);

	const struct cly_show_work * wk = (const struct cly_show_work *)work;
	const struct cly_show_cmd *  cmd = (const struct cly_show_cmd *)
	                                   wk->super.cmd;
	const struct cly_dir *       dir;
	int                          ret;

	ret = cly_dir_work_search(&wk->super, context, &dir);
	if (ret)
		return ret;

	/*
	 * Given the directory descriptor found above, display its state related
	 * informations.
	 */
	switch (wk->format) {
	case CLY_TABLE_DATA_FMT:
		ret = cly_show_table(&cmd->super,
		                     cmd->table,
		                     cmd->flags,
		                     context);
		break;

	case CLY_JSON_DATA_FMT:
		ret = cly_show_format(&cmd->super,
		                      dir->sch_node,
		                      cmd->flags,
		                      LYD_JSON,
		                      context);
		break;

	case CLY_XML_DATA_FMT:
		ret = cly_show_format(&cmd->super,
		                      dir->sch_node,
		                      cmd->flags,
		                      LYD_XML,
		                      context);
		break;

	default:
		cly_assert(0);
	}

	return ret;
}

static const struct cly_work_ops cly_show_work_ops = {
	.exec    = cly_show_exec_work,
	.release = cly_dir_work_release
};

static int
cly_show_parse_cmd(const struct cly_cmd * command,
                   const struct cly_dir * directory,
                   struct cly_context *   context,
                   int                    argc,
                   const char * const     argv[])
{
	cly_show_assert_cmd((const struct cly_show_cmd *)command);
	cly_dir_assert(directory);
	cly_assert_context(context);
	cly_assert(argv);

	struct cly_show_work * wk;
	int                    ret;

	/* Cannot fail. */
	wk = (struct cly_show_work *)
	     cly_dir_work_create(sizeof(*wk), command, &cly_show_work_ops);
	wk->format = CLY_TABLE_DATA_FMT;

	ret = cly_cmd_parse_args(command, directory, context, argc, argv, wk);
	if (ret < 0)
		goto destroy;

	cly_assert(ret == argc);
	ret = cly_sched_work(context, (struct cly_work *)wk);
	if (ret) {
		cly_cmd_log(command, "cannot schedule work.");
		goto destroy;
	}

	return argc;

destroy:
	cly_dir_work_destroy((struct cly_dir_work *)wk);

	return ret;
}

static void
cly_show_fini_cmd(struct cly_cmd * command)
{
	struct cly_srepo_table * tbl = ((struct cly_show_cmd *)command)->table;

	cly_srepo_table_fini(tbl);
	cly_free(tbl);
}

static const struct cly_cmd_ops cly_show_cmd_ops = {
	.parse    = cly_show_parse_cmd,
	.complete = cly_cmd_complete_args,
	.fini     = cly_show_fini_cmd
};

static int
cly_show_on_table_match(const struct cly_arg * argument __unused,
                        const struct cly_cmd * command __unused,
                        const struct cly_dir * directory __unused,
                        struct cly_context *   context __unused,
                        int                    argc __unused,
                        const char * const     argv[] __unused,
                        void *                 data)
{
	((struct cly_show_work *)data)->format = CLY_TABLE_DATA_FMT;

	return 0;
}

static int
cly_show_on_json_match(const struct cly_arg * argument __unused,
                       const struct cly_cmd * command __unused,
                       const struct cly_dir * directory __unused,
                       struct cly_context *   context __unused,
                       int                    argc __unused,
                       const char * const     argv[] __unused,
                       void *                 data)
{
	((struct cly_show_work *)data)->format = CLY_JSON_DATA_FMT;

	return 0;
}

static int
cly_show_on_xml_match(const struct cly_arg * argument __unused,
                       const struct cly_cmd * command __unused,
                       const struct cly_dir * directory __unused,
                       struct cly_context *   context __unused,
                       int                    argc __unused,
                       const char * const     argv[] __unused,
                       void *                 data)
{
	((struct cly_show_work *)data)->format = CLY_XML_DATA_FMT;

	return 0;
}

static const struct cly_arg_kword_term cly_show_format_terms[] = {
	CLY_ARG_KWORD_TERM("table", cly_show_on_table_match),
	CLY_ARG_KWORD_TERM("json",  cly_show_on_json_match),
	CLY_ARG_KWORD_TERM("xml",   cly_show_on_xml_match),
};

typedef int cly_show_init_cmd_fn(struct cly_srepo_table *,
                                 const struct lysc_node *,
                                 const struct cly_context *);

static int
cly_show_create_cmd(struct cly_show_cmd **     command,
                    struct cly_dir *           directory,
                    const struct lysc_node *   node,
                    const char *               name,
                    cly_show_init_cmd_fn *     init,
                    sr_get_oper_flag_t         flags,
                    const struct cly_context * context)
{
	cly_assert(command);
	cly_dir_assert(directory);
	cly_assert(node);
	cly_assert(cly_cmd_name_isok(name));
	cly_assert(init);
	srepo_dat_assert_get_flags(flags);
	cly_assert((flags & (SR_OPER_NO_STATE | SR_OPER_NO_CONFIG)) !=
	           (SR_OPER_NO_STATE | SR_OPER_NO_CONFIG));
	cly_assert_context(context);

	struct cly_show_cmd *       cmd = cmd;
	struct cly_srepo_table *    tbl;
	int                         err;
	struct cly_arg *            choice;
	struct cly_arg_kword_parm * fmt;

	/*
	 * No need to check for returned code since cly_cmd_create() cannot fail
	 * with a command name validated thanks to cly_cmd_name_isok().
	 */
	cly_cmd_sized_create((struct cly_cmd **)&cmd,
	                     sizeof(*cmd),
	                     name,
	                     &cly_show_cmd_ops);
	cly_assert(cmd);

	/*
	 * Cannot fail since calling cly_show_make_cmd() previously made sure
	 * that there is at least one candidate leaf for the directory given in
	 * argument.
	 */
	tbl = cly_malloc(sizeof(*tbl));
	err = init(tbl, node, context);
	if (err) {
		cly_assert(err == -ENOENT);
		goto free;
	}

	cmd->table = tbl;
	cmd->flags = flags;

	/* Cannot fail. */
	choice = cly_arg_createn_add_choice((struct cly_node *)cmd);
	cly_dir_work_createn_add_arg(false, (struct cly_node *)choice);

	/*
	 * No need to check for returned code since this cannot fail with the
	 * "format" name argument.
	 */
	cly_assert(sizeof("format") <= CLY_ARG_MAX);
	cly_arg_createn_add_kword_parm(&fmt,
	                               "format",
	                               cly_show_format_terms,
	                               stroll_array_nr(cly_show_format_terms),
	                               (struct cly_node *)choice);

	cly_dir_add_cmd(directory, &cmd->super);

	*command = cmd;

	return 0;

free:
	cly_free(tbl);
	cly_free(cmd);

	return err;
}

static int
cly_show_make_cmd(struct cly_dir *           directory,
                  const struct lysc_node *   node,
                  const char *               name,
                  cly_show_init_cmd_fn *     init,
                  sr_get_oper_flag_t         flags,
                  const struct cly_context * context)
{
	cly_dir_assert(directory);
	cly_assert(node);
	cly_assert(name);
	cly_assert(init);
	srepo_dat_assert_get_flags(flags);
	cly_assert((flags & (SR_OPER_NO_STATE | SR_OPER_NO_CONFIG)) !=
	           (SR_OPER_NO_STATE | SR_OPER_NO_CONFIG));
	cly_assert_context(context);

	struct cly_cmd * cmd;
	const char *     msg;
	int              ret;

	if (!cly_cmd_name_isok(name)) {
		msg = "invalid name";
		ret = -EINVAL;
		goto err;
	}

	if (cly_dir_find_cmd(directory, name)) {
		msg = "already exists";
		ret = -EEXIST;
		goto err;
	}

	ret = cly_show_create_cmd((struct cly_show_cmd **)&cmd,
	                          directory,
	                          node,
	                          name,
	                          init,
	                          flags,
	                          context);
	if (ret) {
		cly_assert(ret == -ENOENT);

		cly_scn_debug(node,
		              "'%s' show command creation ignored: "
		              "no child schema data node found.",
		              name);
	}

	return 0;

err:
	cly_scn_err(node, "cannot create show command '%s': %s.", name, msg);

	return ret;
}

static bool
cly_show_filter_config_node(const struct lysc_node * node)
{
	cly_assert(node);
	cly_assert(node->nodetype == LYS_LEAF);

	return (srepo_sch_conf_flags(node) == LYS_CONFIG_W) &&
	       !(srepo_sch_status_flags(node) &
	         (LYS_STATUS_OBSLT | LYS_STATUS_DEPRC));
}

static bool
cly_show_filter_oper_node(const struct lysc_node * node)
{
	cly_assert(node);
	cly_assert(node->nodetype == LYS_LEAF);

	return (srepo_sch_conf_flags(node) == LYS_CONFIG_R) &&
	       !(srepo_sch_status_flags(node) &
	         (LYS_STATUS_OBSLT | LYS_STATUS_DEPRC));
}

static int
cly_show_init_config_cmd(struct cly_srepo_table *   table,
                         const struct lysc_node *   node,
                         const struct cly_context * context)
{
	cly_assert(table);
	cly_assert(node);
	cly_assert(node->nodetype == LYS_CONTAINER);
	cly_assert_context(context);

	return cly_srepo_table_init(table,
	                            (const struct lysc_node_container *)node,
	                            cly_show_filter_config_node,
	                            context);
}

int
cly_show_make_config_cmd(struct cly_dir *                   directory,
                         const struct lysc_node_container * container,
                         const char *                       name,
                         const struct cly_context *         context)
{
	cly_dir_assert(directory);
	cly_assert(container);
	cly_assert(name);
	cly_assert_context(context);

	return cly_show_make_cmd(directory,
	                         (const struct lysc_node *)container,
	                         name,
	                         cly_show_init_config_cmd,
	                         SR_OPER_NO_STATE,
	                         context);
}

static int
cly_show_init_oper_cmd(struct cly_srepo_table *   table,
                       const struct lysc_node *   node,
                       const struct cly_context * context)
{
	cly_assert(table);
	cly_assert(node);
	cly_assert(node->nodetype == LYS_CONTAINER);
	cly_assert_context(context);

	return cly_srepo_table_init(table,
	                            (const struct lysc_node_container *)node,
	                            cly_show_filter_oper_node,
	                            context);
}

int
cly_show_make_oper_cmd(struct cly_dir *                   directory,
                       const struct lysc_node_container * container,
                       const char *                       name,
                       const struct cly_context *         context)
{
	cly_dir_assert(directory);
	cly_assert(container);
	cly_assert(name);
	cly_assert_context(context);

	return cly_show_make_cmd(directory,
	                         (const struct lysc_node *)container,
	                         name,
	                         cly_show_init_oper_cmd,
	                         SR_OPER_NO_CONFIG,
	                         context);
}

static int
cly_show_init_config_list_cmd(struct cly_srepo_table *   table,
                              const struct lysc_node *   node,
                              const struct cly_context * context)
{
	cly_assert(table);
	cly_assert(node);
	cly_assert(node->nodetype == LYS_LIST);
	cly_assert_context(context);

	return cly_srepo_table_init_list(table,
	                                 (const struct lysc_node_list *)node,
	                                 cly_show_filter_config_node,
	                                 context);
}

int
cly_show_make_config_list_cmd(struct cly_dir *              directory,
                              const struct lysc_node_list * list,
                              const char *                  name,
                              const struct cly_context *    context)
{
	cly_dir_assert(directory);
	cly_assert(list);
	cly_assert(name);
	cly_assert_context(context);

	return cly_show_make_cmd(directory,
	                         (const struct lysc_node *)list,
	                         name,
	                         cly_show_init_config_list_cmd,
	                         SR_OPER_NO_STATE,
	                         context);
}

static int
cly_show_init_oper_list_cmd(struct cly_srepo_table *   table,
                            const struct lysc_node *   node,
                            const struct cly_context * context)
{
	cly_assert(table);
	cly_assert(node);
	cly_assert(node->nodetype == LYS_LIST);
	cly_assert_context(context);

	return cly_srepo_table_init_list(table,
	                                 (const struct lysc_node_list *)node,
	                                 cly_show_filter_oper_node,
	                                 context);
}

int
cly_show_make_oper_list_cmd(struct cly_dir *              directory,
                            const struct lysc_node_list * list,
                            const char *                  name,
                            const struct cly_context *    context)
{
	cly_dir_assert(directory);
	cly_assert(list);
	cly_assert(name);
	cly_assert_context(context);

	return cly_show_make_cmd(directory,
	                         (const struct lysc_node *)list,
	                         name,
	                         cly_show_init_oper_list_cmd,
	                         SR_OPER_NO_CONFIG,
	                         context);
}
