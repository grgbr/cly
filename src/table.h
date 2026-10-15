#ifndef _CLY_TABLE_H
#define _CLY_TABLE_H

#include "common.h"
#include <stdbool.h>
#include <libsmartcols/libsmartcols.h>

#define CLY_TABLE_COLUMN_MAX       (32U)
#if (CLY_TABLE_COLUMN_MAX < 2) || (CLY_TABLE_COLUMN_MAX > UINT_MAX)
#error Invalid maximum number of table columns !
#endif
#define CLY_TABLE_LABEL_MAX        (64U)
#define CLY_TABLE_COLUMN_WIDTH_MAX (128.)

struct cly_table_column_desc {
	const char * label;
	double       whint;
	int          flags;
};

/*
 * TODO: Add static assert support to make sure that the following is true.
 * - 0 < strnlen(_label, CLY_TABLE_LABEL_MAX) < CLY_TABLE_LABEL_MAX
 * - 0. < whint <= CLY_TABLE_COLUMN_WIDTH_MAX
 * - !((_flags) & \
 *     (SCOLS_FL_TRUNC | \
 *      SCOLS_FL_TREE | \
 *      SCOLS_FL_RIGHT | \
 *      SCOLS_FL_STRICTWIDTH | \
 *      SCOLS_FL_NOEXTREMES | \
 *      SCOLS_FL_HIDDEN | \
 *      SCOLS_FL_WRAP))
 */
#define CLY_TABLE_SETUP_COLUMN_DESC(_label, _whint, _flags) \
	{ \
		.label = _label, \
		.whint = _whint, \
		.flags = _flags \
	}

struct cly_table_desc {
	bool                                 nohead;
	unsigned int                         nr;
	const struct cly_table_column_desc * cols;
};

/*
 * TODO: Add static assert support to make sure that the following is true.
 * - 0 < stroll_array_nr(_cols) < CLY_TABLE_COLUMN_MAX
 */
#define CLY_TABLE_SETUP_DESC(_nohead, _cols) \
	{ \
		.nohead = _nohead, \
		.cnt    = stroll_array_nr(_cols), \
		.nr     = stroll_array_nr(_cols), \
		.cols   = _cols \
	}

struct cly_table;
struct cly_context;

typedef int (cly_table_load_fn)(struct cly_table *,
                                const struct cly_context *,
                                void *);

typedef void (cly_table_fini_fn)(struct cly_table *);

struct cly_table_ops {
	cly_table_load_fn * load;
	cly_table_fini_fn * fini;
};

#define cly_table_assert_ops(_ops) \
	cly_assert((_ops)->load); \
	cly_assert((_ops)->fini)

struct cly_table {
	const struct cly_table_ops * ops;
	struct libscols_table *      scols;
};

#define cly_table_assert(_table) \
	cly_assert(_table); \
	cly_table_assert_ops((_table)->ops); \
	cly_assert((_table)->scols)

extern int
cly_table_show(const struct cly_table * table, bool maxout, FILE * stdio);

static inline int
cly_table_load(struct cly_table *         table,
               const struct cly_context * context,
               void *                     data)
{
	cly_table_assert(table);
	cly_assert(context);

	return table->ops->load(table, context, data);
}

extern void
cly_table_cell_set_data(struct libscols_cell * cell, const char * data);

extern void
cly_table_cell_set_color(struct libscols_cell * cell, const char * color);

static inline struct libscols_cell *
cly_table_line_get_cell(struct libscols_line * line, unsigned int cell_index)
{
	cly_assert(line);
	cly_assert((size_t)cell_index < scols_line_get_ncells(line));

	struct libscols_cell * cell;

	cell = scols_line_get_cell(line, cell_index);
	cly_assert(cell);

	return cell;
}

static inline void *
cly_table_line_get_userdata(struct libscols_line * line)
{
	cly_assert(line);

	return scols_line_get_userdata(line);
}

static inline void
cly_table_line_set_userdata(struct libscols_line * line, void * data)
{
	cly_assert(line);

	int err;

	err = scols_line_set_userdata(line, data);
	cly_assert(!err);
}

extern void
cly_table_line_set_color(struct libscols_line * line, const char * color);

extern void *
cly_table_col_get_userdata(struct libscols_column * column);

extern void
cly_table_col_set_userdata(struct libscols_column * column, void * data);

extern void
cly_table_col_set_color(struct libscols_column * column, const char * color);

static inline struct libscols_cell *
cly_table_col_get_head(struct libscols_column * column)
{
	cly_assert(column);

	struct libscols_cell * cell;

	cell = scols_column_get_header(column);
	cly_assert(cell);

	return cell;
}

static inline unsigned int
cly_table_get_line_count(const struct cly_table * table)
{
	cly_table_assert(table);

	size_t cnt;

	cnt = scols_table_get_nlines(table->scols);
	cly_assert(cnt <= UINT_MAX);

	return (unsigned int)cnt;
}

static inline struct libscols_line *
cly_table_get_line(const struct cly_table * table, unsigned int line_index)
{
	cly_table_assert(table);
	cly_assert(line_index < scols_table_get_nlines(table->scols));

	struct libscols_line * ln;

	ln = scols_table_get_line(table->scols, line_index);
	cly_assert(ln);

	return ln;
}

#define cly_table_foreach_line(_table, _indx, _line) \
	for (_indx = 0; \
	     ((_indx) < cly_table_get_line_count(_table)) && \
	     (_line = cly_table_get_line(_table, _indx)); \
	     (_indx)++)

extern struct libscols_line *
cly_table_new_line(const struct cly_table * table);

static inline void
cly_table_remove_lines(const struct cly_table * table)
{
	cly_table_assert(table);

	scols_table_remove_lines(table->scols);
}

static inline unsigned int
cly_table_get_col_count(const struct cly_table * table)
{
	cly_table_assert(table);

	size_t cnt;

	cnt = scols_table_get_ncols(table->scols);
	cly_assert(cnt <= CLY_TABLE_COLUMN_MAX);

	return (unsigned int)cnt;
}

static inline struct libscols_column *
cly_table_get_col(const struct cly_table * table, unsigned int column_index)
{
	cly_table_assert(table);
	cly_assert(column_index < scols_table_get_ncols(table->scols));

	struct libscols_column * col;

	col = scols_table_get_column(table->scols, column_index);
	cly_assert(col);

	return col;
}

#define cly_table_foreach_col(_table, _indx, _col) \
	for (_indx = 0; \
	     ((_indx) < cly_table_get_col_count(_table)) && \
	     (_col = cly_table_get_col(_table, _indx)); \
	     (_indx)++)

extern struct libscols_column *
cly_table_new_col(const struct cly_table * table,
                  const char *             label,
                  double                   whint,
                  int                      flags);

extern void *
cly_table_get_userdata(struct cly_table * table);

extern void
cly_table_set_userdata(struct cly_table * table, void * data);

extern void
cly_table_init(struct cly_table *           table,
               const struct cly_context *   context,
               bool                         nohead,
               const struct cly_table_ops * opers);

extern void
cly_table_init_from_desc(struct cly_table *            table,
                         const struct cly_context *    context,
                         const struct cly_table_desc * descriptor,
                         const struct cly_table_ops *  opers);

static inline void
cly_table_fini(struct cly_table * table)
{
	cly_table_assert(table);

	table->ops->fini(table);
	scols_unref_table(table->scols);
}

#endif /* _CLY_TABLE_H */
