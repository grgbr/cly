#ifndef _CLY_SET_H
#define _CLY_SET_H

#include "srepo_table.h"

struct cly_dir;
struct cly_context;
struct lysc_node_container;

extern int
cly_set_make_cmd(struct cly_dir *                   directory,
                 const struct lysc_node_container * container,
                 const char *                       name,
                 const struct cly_context *         context);

#endif /* _CLY_SET_H */
