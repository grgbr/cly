#ifndef _CLY_PATH_H
#define _CLY_PATH_H

#include "arg.h"

#define CLY_PATH_MAX \
	CLY_ARG_MAX

#define CLY_PATH_NAME_MAX (128)
#if CLY_PATH_NAME_MAX <= 64
#error Path name length MUST conform to section 6.2 of RFC 7950 !
#endif
#if CLY_PATH_MAX <= CLY_PATH_NAME_MAX
#error Maximum path length not large enough !
#endif /* CLY_PATH_MAX <= CLY_PATH_NAME_MAX */

#define CLY_PATH_INIT_NR (CLY_PATH_MAX / CLY_PATH_NAME_MAX)
#if CLY_PATH_INIT_NR < 4
#undef CLY_PATH_INIT_NR
#define CLY_PATH_INIT_NR (4)
#endif /* CLY_PATH_INIT_NR < 4 */

struct cly_path_comp {
	const char * str;
	size_t       len;
};

#define cly_path_assert_comp(_comp) \
	cly_assert(_comp); \
	cly_assert((_comp)->str); \
	cly_assert((_comp)->len); \
	cly_assert(!cly_path_comp_isok((_comp)->str, (_comp)->len))

extern const char *
cly_path_strerror(int error);

enum cly_path_comp_kind {
	CLY_PATH_REG_COMP_KIND,   /* Regular path component. */
	CLY_PATH_UPPER_COMP_KIND, /* Upper directory, i.e., `..' */
	CLY_PATH_CURR_COMP_KIND,  /* Current directory, i.e., `.' */
	CLY_PATH_COMP_KIND_NR     /* Invalid component type. */
};

extern enum cly_path_comp_kind
cly_path_comp_kind(const struct cly_path_comp * component);

/*
 * Validate a component over the length given in argument.
 *
 * To keep compliant with YANG identifiers, path component name :
 * - starts with a [a-zA-Z_] character ;
 * - is followed by zero or more [a-zA-Z0-9_-] characters ;
 * - and its entire length may be composed of up to (CLY_PATH_NAME_MAX - 1)
 *   characters.
 * See section 6.2 of RFC 7950 for more informations.
 */
extern int
cly_path_comp_isok(const char * component, size_t length);

extern int
cly_path_comp_ncmp(const struct cly_path_comp * component,
                   const char *                 string,
                   size_t                       length);

extern int
cly_path_comp_cmp(const struct cly_path_comp * component,
                  const char *                 string);

struct cly_path {
	unsigned int           head;
	unsigned int           cnt;
	unsigned int           nr;
	struct cly_path_comp * comps;
};

#define cly_path_assert(_stk) \
	cly_assert(_stk); \
	cly_assert((_stk)->nr); \
	cly_assert((_stk)->head < (_stk)->nr); \
	cly_assert((_stk)->cnt <= (_stk)->nr); \
	cly_assert((_stk)->comps)

#define cly_path_foreach_comp(_path, _indx, _cnt, _comp) \
	for (_indx = (_path)->head, \
	     _cnt = (_path)->cnt, \
	     _comp = &(_path)->comps[_indx]; \
	     \
	     _cnt; \
	     \
	     _indx = ((_indx) + 1) % (_path)->nr, \
	     _cnt = (_cnt) - 1, \
	     _comp = &(_path)->comps[_indx])

#define cly_path_foreach_comp_continue(_path, _indx, _cnt, _comp) \
	for (_indx = ((_indx) + 1) % (_path)->nr, \
	     _cnt = (_cnt) - 1, \
	     _comp = &(_path)->comps[_indx]; \
	     \
	     _cnt; \
	     \
	     _indx = ((_indx) + 1) % (_path)->nr, \
	     _cnt = (_cnt) - 1, \
	     _comp = &(_path)->comps[_indx])

#define cly_path_foreach_comp_from(_path, _indx, _cnt, _comp) \
	for (; \
	     _cnt; \
	     \
	     _indx = ((_indx) + 1) % (_path)->nr, \
	     _cnt = (_cnt) - 1, \
	     _comp = &(_path)->comps[_indx])

static inline unsigned int
cly_path_comp_count(const struct cly_path * path)
{
	cly_path_assert(path);

	return path->cnt;
}

extern ssize_t
cly_path_mkrel(struct cly_path * path, char * string, size_t size);

extern ssize_t
cly_path_mkabs(struct cly_path * path, char * string, size_t size);

extern void
cly_path_push_head(struct cly_path * path,
                   const char *      component,
                   size_t            length);

extern const struct cly_path_comp *
cly_path_pop_head(struct cly_path * path);

extern void
cly_path_push_tail(struct cly_path * path,
                   const char *      component,
                   size_t            length);

extern const struct cly_path_comp *
cly_path_pop_tail(struct cly_path * path);

extern ssize_t
cly_path_parse_comp(enum cly_path_comp_kind * kind,
                    const char *              string,
                    size_t                    size);

extern int
cly_path_parse(struct cly_path * path, const char * string);

extern void
cly_path_init(struct cly_path * path);

extern void
cly_path_fini(struct cly_path * path);

extern struct cly_path *
cly_path_create(void);

extern void
cly_path_destroy(struct cly_path * path);

extern ssize_t
cly_path_normalize(const char * path, char * norm, size_t size);

extern int
cly_path_isok(const char * string);

#endif /* _CLY_PATH_H */
