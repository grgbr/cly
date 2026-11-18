#ifndef _CLY_SET_H
#define _CLY_SET_H

struct cly_dir;
struct cly_context;
struct lysc_node;

extern int
cly_set_make_cmd(struct cly_dir *           directory,
                 const struct lysc_node *   container,
                 const char *               name,
                 const struct cly_context * context);

#endif /* _CLY_SET_H */
