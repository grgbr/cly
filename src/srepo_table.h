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

typedef bool cly_srepo_table_filter_node_fn(const struct lysc_node * node);

extern int
cly_srepo_table_init(struct cly_srepo_table *           table,
                     const struct lysc_node_container * container,
                     cly_srepo_table_filter_node_fn *   filter,
                     const struct cly_context *         context);

extern int
cly_srepo_table_init_list(struct cly_srepo_table *         table,
                          const struct lysc_node *         list,
                          cly_srepo_table_filter_node_fn * filter,
                          const struct cly_context *       context);

static inline void
cly_srepo_table_fini(struct cly_srepo_table * table)
{
	cly_srepo_table_assert(table);

	cly_table_fini(&table->super);
}

#endif /* _CLY_SREPO_TABLE_H */
