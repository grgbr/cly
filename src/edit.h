#ifndef _CLY_EDIT_H
#define _CLY_EDIT_H

#include "common.h"

struct cly_cmd;
struct cly_dir;
struct cly_context;
struct cly_match;

extern void
cly_edit_complete_args(const struct cly_cmd * command,
                       const struct cly_dir * directory,
                       struct cly_context *   context,
                       const char *           word,
                       size_t                 length,
                       int                    argc,
                       const char * const     argv[],
                       struct cly_match *     matches);

extern void
cly_edit_build_cmd(struct cly_dir * directory);

#endif /* _CLY_EDIT_H */
