#include "srepo_table.h"
#include "cly.h"
#include <srutils/srepo/data.h>
#include <srutils/srepo/schema.h>

static void
cly_srepo_table_set_node_cell(struct libscols_cell *  cell,
                              const char * const *    style,
                              const struct lyd_node * node)
{
	cly_assert(cell);
	cly_assert(node);

	const char * color;

	cly_table_cell_set_data(cell, srepo_dat_node_as_str(node));

	color = !srepo_dat_is_dflt(node)
	        ? cly_style_get_color(style, CLY_VALUE_STYLE_KIND)
	        : cly_style_get_color(style, CLY_DEFAULT_STYLE_KIND);

	cly_table_cell_set_color(cell, color);
}

static void
cly_srepo_table_set_err_cell(struct libscols_cell * cell,
                             const char * const *   style)
{
	cly_assert(cell);

	cly_table_cell_set_data(cell, "??");

	cly_table_cell_set_color(cell,
	                         cly_style_get_color(style,
	                                             CLY_ERROR_STYLE_KIND));
}

static int
cly_srepo_table_load(struct cly_table *         table,
                     const struct cly_context * context,
                     void *                     data)
{
	cly_srepo_table_assert((const struct cly_srepo_table *)table);
	cly_assert_context(context);
	srepo_dat_assert_get_flags((sr_get_oper_flag_t)data);

	sr_data_t *            cont;
	const char * const *   style = cly_get_style(context);
	unsigned int           l;
	struct libscols_line * ln;

	if (srepo_dat_load_data(context->sess,
	                        cly_table_get_userdata(table),
	                        2,
	                        (sr_get_oper_flag_t)data,
	                        &cont) == SR_ERR_OK) {
		cly_table_foreach_line(table, l, ln) {
			const struct lyd_node * leaf;
			struct libscols_cell *  cell =
				cly_table_line_get_cell(ln, 1);

			if (srepo_dat_find_node(
				cont->tree,
				cly_table_line_get_userdata(ln),
				(struct lyd_node **)&leaf) == SR_ERR_OK)
				cly_srepo_table_set_node_cell(cell,
				                              style,
				                              leaf);
			else
				cly_srepo_table_set_err_cell(cell, style);
		}

		srepo_dat_release_data(cont);
	}
	else {
		cly_table_foreach_line(table, l, ln)
			cly_srepo_table_set_err_cell(
				cly_table_line_get_cell(ln, 1),
				style);
	}

	return 0;
}

static void
_cly_srepo_table_fini(struct cly_table * table)
{
	cly_srepo_table_assert((const struct cly_srepo_table *)table);

	cly_free(cly_table_get_userdata(table));
}

static const struct cly_table_ops cly_srepo_table_ops = {
	.load = cly_srepo_table_load,
	.fini = _cly_srepo_table_fini
};

int
cly_srepo_table_init(struct cly_srepo_table *           table,
                     const struct lysc_node_container * container,
                     cly_srepo_table_filter_node_fn *   filter,
                     const struct cly_context *         context)
{
	cly_assert(table);
	cly_assert(container);
	cly_assert(filter);
	cly_assert_context(context);

	struct libscols_column * col;
	const struct lysc_node * child;
	int                      err;

	cly_table_init(&table->super, context, true, &cly_srepo_table_ops);

	cly_assert(sizeof("Attribute") <= CLY_TABLE_LABEL_MAX);
	col = cly_table_new_col(&table->super, "Attribute", 0.3, 0);

	cly_assert(sizeof("Value") <= CLY_TABLE_LABEL_MAX);
	cly_table_new_col(&table->super, "Value", 0.7, SCOLS_FL_WRAP);

	srepo_sch_foreach_child((const struct lysc_node *)container, child) {
		if ((child->nodetype == LYS_LEAF) && filter(child)) {
			struct libscols_line *        ln;
			struct libscols_cell *        cell;
			const struct lysc_node_leaf * leaf =
				(const struct lysc_node_leaf *)child;

			ln = cly_table_new_line(&table->super);
			cly_table_line_set_userdata(ln, (void *)leaf->name);

			cell = cly_table_line_get_cell(ln, 0);
			cly_table_cell_set_data(cell, leaf->name);
		}
	}

	if (!cly_table_get_line_count(&table->super)) {
		err = -ENOENT;
		goto fini;
	}

	cly_table_set_userdata(&table->super,
	                       srepo_sch_node_path((const struct lysc_node *)
	                                           container));
	cly_table_col_set_color(col,
	                        cly_style_get_color(cly_get_style(context),
	                                            CLY_LABEL_STYLE_KIND));

	return 0;

fini:
	cly_table_fini(&table->super);

	return err;
}

static void
cly_srepo_table_load_list_entry(const struct cly_table * table,
                                const char * const *     style,
                                const struct lyd_node *  entry)
{
	cly_srepo_table_assert((const struct cly_srepo_table *)table);
	cly_assert(entry);

	struct libscols_line *   ln = cly_table_new_line(table);
	unsigned int             c;
	struct libscols_column * col;

	cly_assert(ln);

	cly_table_foreach_col(table, c, col) {
		struct libscols_cell *  cell = cly_table_line_get_cell(ln, c);
		const struct lyd_node * leaf;

		cly_assert(cell);

		if (srepo_dat_find_node(entry,
		                        cly_table_col_get_userdata(col),
		                        (struct lyd_node **)&leaf) == SR_ERR_OK)
			cly_srepo_table_set_node_cell(cell, style, leaf);
		else
			cly_srepo_table_set_err_cell(cell, style);
	}
}

static void
cly_srepo_table_load_err_entry(const struct cly_table * table,
                               const char * const *     style)
{
	cly_srepo_table_assert((const struct cly_srepo_table *)table);

	struct libscols_line *   ln = cly_table_new_line(table);
	unsigned int             c;
	struct libscols_column * col;

	cly_assert(ln);

	cly_table_foreach_col(table, c, col)
		cly_srepo_table_set_err_cell(cly_table_line_get_cell(ln, c),
		                           style);
}

static int
cly_srepo_table_load_list(struct cly_table *         table,
                          const struct cly_context * context,
                          void *                     data)
{
	cly_srepo_table_assert((const struct cly_srepo_table *)table);
	cly_assert_context(context);
	srepo_dat_assert_get_flags((sr_get_oper_flag_t)data);

	sr_data_t *          list;
	const char * const * style = cly_get_style(context);

	cly_table_remove_lines(table);

	if (srepo_dat_load_data(context->sess,
	                        cly_table_get_userdata(table),
	                        2,
	                        (sr_get_oper_flag_t)data,
	                        &list) == SR_ERR_OK) {
		struct lyd_node * entry;

		srepo_dat_foreach_data_tree(list, entry)
			cly_srepo_table_load_list_entry(table, style, entry);

		srepo_dat_release_data(list);
	}
	else
		cly_srepo_table_load_err_entry(table, style);

	return 0;
}

static const struct cly_table_ops cly_srepo_table_list_ops = {
	.load = cly_srepo_table_load_list,
	.fini = _cly_srepo_table_fini
};

#define CLY_SREPO_TABLE_LIST_INIT_NR (4U)
#if CLY_SREPO_TABLE_LIST_INIT_NR > CLY_TABLE_COLUMN_MAX
#error Invalid maximum number of data list table columns !
#endif

int
cly_srepo_table_init_list(struct cly_srepo_table *         table,
                          const struct lysc_node_list *    list,
                          cly_srepo_table_filter_node_fn * filter,
                          const struct cly_context *       context)
{
	cly_assert(table);
	cly_assert(list);
	cly_assert(filter);
	cly_assert_context(context);

	unsigned int                   cnt = 0;
	unsigned int                   nr = CLY_SREPO_TABLE_LIST_INIT_NR;
	const struct lysc_node_leaf ** fields = NULL;
	const struct lysc_node *       child;
	int                            err;
	unsigned int                   c;

	fields = cly_malloc(nr * sizeof(fields[0]));
	cly_assert(fields);
	srepo_sch_foreach_child((struct lysc_node *)list, child) {
		if ((child->nodetype == LYS_LEAF) && filter(child)) {
			assert(nr <= CLY_TABLE_COLUMN_MAX);
			assert(cnt <= nr);

			if (cnt == nr) {
				nr *= 2;
				if (nr >= CLY_TABLE_COLUMN_MAX) {
					err = -ENOBUFS;
					goto fini;
				}

				fields = cly_realloc(
					fields,
					sizeof(nr * sizeof(fields[0])));
				cly_assert(fields);
			}

			fields[cnt++] = (const struct lysc_node_leaf *)child;
		}
	}

	if (!cnt) {
		err = -ENOENT;
		goto free;
	}

	cly_table_init(&table->super,
	               context,
	               false,
	               &cly_srepo_table_list_ops);

	for (c = 0; c < cnt; c++) {
		struct libscols_column * col;

		col = cly_table_new_col(&table->super,
		                        fields[c]->name,
		                        (double)cnt / 100.0,
		                        SCOLS_FL_WRAP);
		if (!col) {
			cly_assert(errno != ENOBUFS);
			err = -errno;
			goto fini;
		}

		cly_table_col_set_userdata(col, (void *)fields[c]->name);

		cly_table_cell_set_color(
			cly_table_col_get_head(col),
			cly_style_get_color(cly_get_style(context),
			                    CLY_LABEL_STYLE_KIND));
	}

	cly_table_set_userdata(&table->super,
	                       srepo_sch_node_path((const struct lysc_node *)
	                                           list));

	cly_free(fields);

	return 0;

fini:
	cly_table_fini(&table->super);
free:
	cly_free(fields);

	return err;
}
