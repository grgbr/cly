#ifndef _CLY_MATCH_H
#define _CLY_MATCH_H

#include "common.h"

struct cly_match {
	unsigned int cnt;
	unsigned int nr;
	char **      data;
};

/*
 * TODO: make sure CLY_MATCH_MAX is consistent with `rl_completion_query_items'
 *       if used.
 */
#define CLY_MATCH_MAX (1024U)

#define cly_match_assert(_match) \
	cly_assert(_match); \
	cly_assert((_match)->nr <= CLY_MATCH_MAX); \
	cly_assert((_match)->cnt <= (_match)->nr)

#define CLY_MATCH_INIT \
	{ \
		.cnt = 0, \
		.nr = 0, \
		.data = NULL \
	}

#define cly_match_foreach(_matches, _indx, _match) \
	for (_indx = 0; \
	     ((_indx) < (_matches)->cnt) && \
	     (_match = (_matches)->data[_indx]); \
	     (_indx)++)

static inline unsigned int
cly_match_count(const struct cly_match * matches)
{
	cly_match_assert(matches);

	return matches->cnt;
}

static inline char *
cly_match_get(const struct cly_match * matches, unsigned int index)
{
	cly_match_assert(matches);

	return (index < matches->cnt) ? matches->data[index] : NULL;
}

extern void
cly_match_push(struct cly_match * matches, char * string);

extern void
cly_match_init(struct cly_match * matches);

static inline void
cly_match_fini(struct cly_match * matches)
{
	cly_match_assert(matches);

	cly_free(matches->data);
}

#endif /* _CLY_MATCH_H */
