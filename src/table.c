#include "table.h"
#include "cly.h"

#define cly_table_assert_col_label(_label) \
	cly_assert(_label); \
	cly_assert(_label[0] != '\0'); \
	cly_assert(strnlen(_label, CLY_TABLE_LABEL_MAX) < \
	           CLY_TABLE_LABEL_MAX)

#define cly_table_assert_col_whint(_whint) \
	cly_assert(_whint > 0.); \
	cly_assert(_whint <= CLY_TABLE_COLUMN_WIDTH_MAX)

#define cly_table_assert_col_flags(_flags) \
	cly_assert(!((_flags) & \
		     ~(SCOLS_FL_TRUNC | \
		       SCOLS_FL_TREE | \
		       SCOLS_FL_RIGHT | \
		       SCOLS_FL_STRICTWIDTH | \
		       SCOLS_FL_NOEXTREMES | \
		       SCOLS_FL_HIDDEN | \
		       SCOLS_FL_WRAP)))

#define cly_table_assert_col_desc(_col) \
	cly_assert(_col); \
	cly_table_assert_col_label((_col)->label); \
	cly_table_assert_col_whint((_col)->whint); \
	cly_table_assert_col_flags((_col)->flags)

#define cly_table_assert_desc(_table) \
	cly_assert(_table); \
	cly_assert((_table)->nr); \
	cly_assert((_table)->nr <= CLY_TABLE_COLUMN_MAX); \
	cly_assert((_table)->cols)

int
cly_table_show(const struct cly_table * table, bool maxout, FILE * stdio)
{
	cly_table_assert(table);
	cly_assert(stdio);

	int ret;

	ret = scols_table_set_stream(table->scols, stdio);
	cly_assert(!ret);

	if (!maxout) {
		scols_table_enable_maxout(table->scols, 0);
		scols_table_enable_minout(table->scols, 1);
	}
	else {
		scols_table_enable_minout(table->scols, 0);
		scols_table_enable_maxout(table->scols, 1);
	}

	ret = scols_print_table(table->scols);
	if (ret == -ENOMEM)
		abort();

	return ret;
}

void
cly_table_cell_set_data(struct libscols_cell * cell, const char * data)
{
	cly_assert(cell);

	int err;

	err = scols_cell_set_data(cell, data);
	if (err) {
		cly_assert(err == -ENOMEM);
		abort();
	}
}

void
cly_table_line_set_color(struct libscols_line * line, const char * color)
{
	cly_assert(line);

	int err;

	err = scols_line_set_color(line, color);
	if (err) {
		cly_assert(err == -ENOMEM);
		abort();
	}
}

void
cly_table_cell_set_color(struct libscols_cell * cell, const char * color)
{
	cly_assert(cell);

	int err;

	err = scols_cell_set_color(cell, color);
	if (err) {
		cly_assert(err == -ENOMEM);
		abort();
	}
}

void *
cly_table_col_get_userdata(struct libscols_column * column)
{
	cly_assert(column);

	struct libscols_cell * head;

	head = scols_column_get_header(column);
	cly_assert(head);

	return scols_cell_get_userdata(head);
}

void
cly_table_col_set_userdata(struct libscols_column * column, void * data)
{
	cly_assert(column);

	struct libscols_cell * head;
	int                    err __unused;

	head = scols_column_get_header(column);
	cly_assert(head);

	err = scols_cell_set_userdata(head, data);
	cly_assert(!err);
}

void
cly_table_col_set_color(struct libscols_column * column, const char * color)
{
	cly_assert(column);

	int err;

	err = scols_column_set_color(column, color);
	if (err) {
		cly_assert(err == -ENOMEM);
		abort();
	}
}

struct libscols_column *
cly_table_new_col(const struct cly_table * table,
                  const char *             label,
                  double                   whint,
                  int                      flags)
{
	cly_table_assert(table);
	cly_table_assert_col_label(label);
	cly_table_assert_col_whint(whint);
	cly_table_assert_col_flags(flags);

	size_t                   len;
	struct libscols_column * col;
	int                      err __unused;

	len = strnlen(label, CLY_TABLE_LABEL_MAX);
	if (!len) {
		errno = ENODATA;
		return NULL;
	}
	else if (len >= CLY_TABLE_LABEL_MAX) {
		errno = ENAMETOOLONG;
		return NULL;
	}

	if (scols_table_get_ncols(table->scols) >= CLY_TABLE_COLUMN_MAX) {
		errno = ENOBUFS;
		return NULL;
	}

	col = scols_table_new_column(table->scols, label, whint, flags);
	if (!col) {
		cly_assert(errno == ENOMEM);
		abort();
	}

	/* Highlight column header title. */
	err = scols_cell_set_color(scols_column_get_header(col), "bold");
	cly_assert(!err);

	return col;
}

struct libscols_line *
cly_table_new_line(const struct cly_table * table)
{
	cly_table_assert(table);

	struct libscols_line * ln;

	ln = scols_table_new_line(table->scols, NULL);
	if (!ln) {
		cly_assert(errno == ENOMEM);
		abort();
	}

	return ln;
}

void *
cly_table_get_userdata(struct cly_table * table)
{
	cly_table_assert(table);

	struct libscols_cell * ttl;

	ttl = scols_table_get_title(table->scols);
	cly_assert(ttl);

	return scols_cell_get_userdata(ttl);
}

void
cly_table_set_userdata(struct cly_table * table, void * data)
{
	cly_table_assert(table);

	struct libscols_cell * ttl;
	int                    err __unused;

	ttl = scols_table_get_title(table->scols);
	cly_assert(ttl);

	err = scols_cell_set_userdata(ttl, data);
	cly_assert(!err);
}

void
cly_table_init(struct cly_table *           table,
               const struct cly_context *   context,
               bool                         nohead,
               const struct cly_table_ops * opers)
{
	cly_assert(table);
	cly_assert_context(context);
	cly_table_assert_ops(opers);

	struct libscols_table * tbl;

	tbl = scols_new_table();
	if (!tbl) {
		cly_assert(errno == ENOMEM);
		abort();
	}

	scols_table_enable_ascii(tbl, 1);
	scols_table_enable_colors(tbl, (int)cly_has_colors(context));
	scols_table_enable_noheadings(tbl, nohead);

	table->ops = opers;
	table->scols = tbl;
}

void
cly_table_init_from_desc(struct cly_table *            table,
                         const struct cly_context *    context,
                         const struct cly_table_desc * descriptor,
                         const struct cly_table_ops *  opers)
{
	cly_assert(table);
	cly_assert_context(context);
	cly_table_assert_desc(descriptor);
	cly_table_assert_ops(opers);

	unsigned int c;

	cly_table_init(table, context, descriptor->nohead, opers);

	for (c = 0; c < descriptor->nr; c++) {
		const struct cly_table_column_desc * cdesc;
		const struct libscols_column *       col __unused;

		cly_table_assert_col_desc(cdesc);

		cdesc = &descriptor->cols[c];
		col = cly_table_new_col(table,
		                        cdesc->label,
		                        cdesc->whint,
		                        cdesc->flags);
		cly_assert(col);
	}
}
