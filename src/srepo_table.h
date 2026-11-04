#ifndef _CLY_SREPO_TABLE_H
#define _CLY_SREPO_TABLE_H

#include "table.h"
#include <sysrepo_types.h>

struct lysc_node;
struct lysc_node_container;
struct lysc_node_list;

struct cly_srepo_table {
	struct cly_table super;
};

#define cly_srepo_table_assert(_table) \
	cly_assert(_table); \
	cly_table_assert(&(_table)->super)

struct cly_srepo_table_data {
	sr_session_ctx_t *   sess;
	sr_get_oper_flag_t   flags;
	const char * const * style;
};

extern int
cly_srepo_table_init(struct cly_srepo_table *           table,
                     const struct lysc_node_container * container,
                     cly_filter_node_fn *               filter,
                     bool                               color,
                     const char * const *               style);

extern int
cly_srepo_table_init_list(struct cly_srepo_table * table,
                          const struct lysc_node * list,
                          cly_filter_node_fn *     filter,
                          bool                     color,
                          const char * const *     style);

static inline void
cly_srepo_table_fini(struct cly_srepo_table * table)
{
	cly_srepo_table_assert(table);

	cly_table_fini(&table->super);
}

#endif /* _CLY_SREPO_TABLE_H */
