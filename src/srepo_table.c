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
cly_srepo_table_clear_node_cell(struct libscols_cell * cell)
{
	cly_assert(cell);

	cly_table_cell_set_data(cell, NULL);
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
			const struct lysc_node * scl;
			const struct lyd_node *  dtl;
			struct libscols_cell *   cell;
			sr_error_t               err;

			scl = cly_table_line_get_userdata(ln);
			cly_assert(scl);

			err = srepo_dat_find_node(
				cont->tree,
				scl->name,
				(struct lyd_node **)&dtl);

			cell = cly_table_line_get_cell(ln, 1);
			cly_assert(cell);

			if (err == SR_ERR_OK)
				cly_srepo_table_set_node_cell(cell,
				                              style,
				                              dtl);
			else if ((err == SR_ERR_NOT_FOUND) &&
			         !srepo_sch_is_mandatory(scl))
				cly_srepo_table_clear_node_cell(cell);
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
		if ((srepo_sch_node_type(child) == LYS_LEAF) && filter(child)) {
			struct libscols_line *        ln;
			struct libscols_cell *        cell;
			const struct lysc_node_leaf * leaf =
				(const struct lysc_node_leaf *)child;

			ln = cly_table_new_line(&table->super);
			cly_table_line_set_userdata(ln, (void *)leaf);

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
		const struct lysc_node * scl = cly_table_col_get_userdata(col);
		struct libscols_cell *   cell = cly_table_line_get_cell(ln, c);
		const struct lyd_node *  dtl;
		sr_error_t               err;

		cly_assert(scl);
		cly_assert(cell);

		err = srepo_dat_find_node(entry,
		                          scl->name,
		                          (struct lyd_node **)&dtl);
		if (err == SR_ERR_OK)
			cly_srepo_table_set_node_cell(cell, style, dtl);
		else if ((err == SR_ERR_NOT_FOUND) &&
		         !srepo_sch_is_mandatory(scl))
			cly_srepo_table_clear_node_cell(cell);
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

#warning UNDERSTAND WHY WE NEED TO TRAVERSE CHILDREN !!
		srepo_dat_foreach_data_child(list, entry)
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

struct cly_srepo_table_array {
	unsigned int              cnt;
	unsigned int              nr;
	const struct lysc_node ** items;
};

#define cly_srepo_table_array_foreach(_array, _cnt, _node) \
	for (_cnt = 0, _node = (_array)->items[_cnt]; \
	     _cnt < (_array)->cnt; \
	     _cnt++, _node = (_array)->items[_cnt])

static unsigned int
cly_srepo_table_array_count(const struct cly_srepo_table_array * array)
{
	cly_assert(array);
	cly_assert(array->nr <= CLY_TABLE_COLUMN_MAX);
	cly_assert(array->cnt <= array->nr);
	cly_assert(array->items);

	return array->cnt;
}
static int
cly_srepo_table_push_array(struct cly_srepo_table_array * array,
                           const struct lysc_node *       node)
{
	cly_assert(array);
	cly_assert(array->nr <= CLY_TABLE_COLUMN_MAX);
	cly_assert(array->cnt <= array->nr);
	cly_assert(array->items);
	cly_assert(node);

	if (array->cnt == array->nr) {
		array->nr *= 2;
		if (array->nr >= CLY_TABLE_COLUMN_MAX)
			return -ENOBUFS;

		array->items = cly_realloc(array->items,
		                           array->nr * sizeof(array->items[0]));
		cly_assert(array->items);
	}

	array->items[array->cnt++] = node;

	return 0;
}

static void
cly_srepo_table_init_array(struct cly_srepo_table_array * array)
{
	array->cnt = 0;
	array->nr = CLY_SREPO_TABLE_LIST_INIT_NR;
	array->items = cly_malloc(CLY_SREPO_TABLE_LIST_INIT_NR *
	                          sizeof(array->items[0]));
	cly_assert(array->items);
}

static void
cly_srepo_table_fini_array(struct cly_srepo_table_array * array)
{
	cly_assert(array);
	cly_assert(array->nr <= CLY_TABLE_COLUMN_MAX);
	cly_assert(array->cnt <= array->nr);
	cly_assert(array->items);

	cly_free(array->items);
}

static int
cly_srepo_table_new_col(struct cly_srepo_table * table,
                        const struct lysc_node * node,
                        double                   whint,
                        int                      flags,
                        const char *             color)
{
	cly_srepo_table_assert((const struct cly_srepo_table *)table);
	cly_assert(node);

	struct libscols_column * col;

	col = cly_table_new_col(&table->super, node->name, whint, flags);
	if (!col) {
		cly_assert(errno != ENOBUFS);
		return -errno;
	}

	cly_table_col_set_userdata(col, (void *)node);

	cly_table_cell_set_color(cly_table_col_get_head(col), color);

	return 0;
}

int
cly_srepo_table_init_list(struct cly_srepo_table *         table,
                          const struct lysc_node *         list,
                          cly_srepo_table_filter_node_fn * filter,
                          const struct cly_context *       context)
{
	cly_assert(table);
	cly_assert(list);
	cly_assert(filter);
	cly_assert_context(context);

	struct cly_srepo_table_array   keys;
	struct cly_srepo_table_array   fields;
	const struct lysc_node *       leaf;
	int                            err;
	unsigned int                   cnt;
	unsigned int                   c;
	const char *                   clr;

	cly_srepo_table_init_array(&keys);
	cly_srepo_table_init_array(&fields);

	srepo_sch_foreach_child(list, leaf) {
		if ((srepo_sch_node_type(leaf) == LYS_LEAF) && filter(leaf)) {
			if (srepo_sch_is_key(leaf))
				err = cly_srepo_table_push_array(&keys, leaf);
			else
				err = cly_srepo_table_push_array(&fields, leaf);

			if (err)
				goto fini_arrays;
		}
	}

	cnt = cly_srepo_table_array_count(&keys) +
	      cly_srepo_table_array_count(&fields);
	if (!cnt) {
		err = -ENOENT;
		goto fini_arrays;
	}

	cly_table_init(&table->super,
	               context,
	               false,
	               &cly_srepo_table_list_ops);
	clr = cly_style_get_color(cly_get_style(context), CLY_LABEL_STYLE_KIND);
	cly_srepo_table_array_foreach(&keys, c, leaf) {
		err = cly_srepo_table_new_col(table,
		                              leaf,
		                              (double)cnt / 100.0,
		                              SCOLS_FL_WRAP,
		                              clr);
		if (err)
			goto fini_table;
	}
	cly_srepo_table_array_foreach(&fields, c, leaf) {
		err = cly_srepo_table_new_col(table,
		                              leaf,
		                              (double)cnt / 100.0,
		                              SCOLS_FL_WRAP,
		                              clr);
		if (err)
			goto fini_table;
	}

	cly_table_set_userdata(&table->super, srepo_sch_node_path(list));

	cly_srepo_table_fini_array(&keys);
	cly_srepo_table_fini_array(&fields);

	return 0;

fini_table:
	cly_table_fini(&table->super);
fini_arrays:
	cly_srepo_table_fini_array(&keys);
	cly_srepo_table_fini_array(&fields);

	return err;
}
