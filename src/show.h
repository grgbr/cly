#ifndef _CLY_SHOW_H
#define _CLY_SHOW_H

#include "srepo_table.h"

struct cly_dir;
struct cly_context;
struct lysc_node_container;
struct lysc_node_list;

extern int
cly_show_make_config_cmd(struct cly_dir *                   directory,
                         const struct lysc_node_container * container,
                         const char *                       name,
                         const struct cly_context *         context);

extern int
cly_show_make_oper_cmd(struct cly_dir *                   directory,
                       const struct lysc_node_container * container,
                       const char *                       name,
                       const struct cly_context *         context);

extern int
cly_show_make_config_list_cmd(struct cly_dir *      directory,
                         const struct lysc_node *   list,
                         const char *               name,
                         const struct cly_context * context);
extern int
cly_show_make_oper_list_cmd(struct cly_dir *           directory,
                            const struct lysc_node *   list,
                            const char *               name,
                            const struct cly_context * context);

#endif /* _CLY_SHOW_H */
